"""Run the compiled harness over a scenario suite and keep its PNG + JSON.

One `Capture` is the measurement everything else compares: the deterministic
PNG the harness writes plus the JSON it emits (coverage, radio-call log, render
metrics, embedded revision). The JSON's `revision` field is checked against the
revision being captured, so a binary from another build cannot be reported as
this one's.
"""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .docker import DockerRunner
from .errors import HarnessError
from .revisions import Revision
from .scenarios import Scenario, ScenarioSuite, parse_scenario

CAPTURE_SCRIPT = """\
set -euo pipefail
cd tools/ui_harness/build
./ui_harness \
  --scenario /src/tools/ui_harness/fixtures/{fixture} \
  --png /out/{name}.png \
  --json /out/{name}.json{inspect}
"""

# `--inspect` adds the structural inspection of the live LVGL object tree to the
# JSON (see tools/ui_harness/src/ui_inspector.c). It never changes a pixel, so
# the plain capture stays byte-identical.
INSPECT_FLAG = " --inspect"

# `--bench` measures the real render cost of each frame with CLOCK_MONOTONIC
# (see tools/ui_harness/src/bench_metrics.h) and appends a `benchmark` block to
# the JSON. It never changes the PNG.
BENCHMARK_SCRIPT = """\
set -euo pipefail
cd tools/ui_harness/build
./ui_harness \
  --scenario /src/tools/ui_harness/fixtures/{fixture} \
  --png /out/{name}.png \
  --json /out/{name}.json \
  --bench --warmup {warmup} --repeats {repeats}
"""


@dataclass(frozen=True)
class FrameSample:
    """One measured frame: real-clock timings and the render counters."""

    render_ms: float
    flush_ms: float
    draw_ms: float
    flushes: int
    redrawn_area_px: int
    unique_redrawn_px: int

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> FrameSample:
        return cls(
            render_ms=float(data["render_ms"]),
            flush_ms=float(data["flush_ms"]),
            draw_ms=float(data["draw_ms"]),
            flushes=int(data["flushes"]),
            redrawn_area_px=int(data["redrawn_area_px"]),
            unique_redrawn_px=int(data["unique_redrawn_px"]),
        )


@dataclass(frozen=True)
class MemorySample:
    """Process memory read from /proc at the end of the benchmark run."""

    rss_bytes: int
    pss_bytes: int
    peak_rss_bytes: int
    peak_vm_bytes: int

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> MemorySample:
        return cls(
            rss_bytes=int(data.get("rss_bytes", 0)),
            pss_bytes=int(data.get("pss_bytes", 0)),
            peak_rss_bytes=int(data.get("peak_rss_bytes", 0)),
            peak_vm_bytes=int(data.get("peak_vm_bytes", 0)),
        )


@dataclass(frozen=True)
class BenchmarkRun:
    """One `ui_harness --bench` invocation for a scenario in one round."""

    scenario: Scenario
    revision: Revision
    round_index: int
    frames: tuple[FrameSample, ...]
    memory: MemorySample | None
    environment: str
    report: dict[str, Any]


@dataclass(frozen=True)
class Capture:
    """A scenario rendered by a specific revision."""

    scenario: Scenario
    revision: Revision
    png: Path
    report_path: Path
    report: dict[str, Any]
    png_sha256: str

    @property
    def name(self) -> str:
        return self.scenario.name

    @property
    def width(self) -> int:
        return int(self.report["framebuffer"]["width"])

    @property
    def height(self) -> int:
        return int(self.report["framebuffer"]["height"])

    @property
    def reported_revision(self) -> str:
        return str(self.report.get("revision", ""))

    @property
    def coverage(self) -> list[dict[str, str]]:
        return list(self.report.get("coverage", []))

    @property
    def metrics(self) -> dict[str, Any]:
        return dict(self.report.get("metrics", {}))

    def flushed_pixels(self) -> int:
        return int(self.metrics.get("flushed_pixels", 0))

    def flushes(self) -> int:
        return int(self.metrics.get("flushes", 0))

    @property
    def framebuffer_hash(self) -> str:
        return str(self.metrics.get("framebuffer_fnv1a", ""))

    def summary(self) -> dict[str, Any]:
        """Serialisable view of this capture, for reports and results.json."""
        return {
            "scenario": self.name,
            "revision": self.reported_revision,
            "png_sha256": self.png_sha256,
            "width": self.width,
            "height": self.height,
            "flushes": self.flushes(),
            "flushed_pixels": self.flushed_pixels(),
            "framebuffer_fnv1a": self.framebuffer_hash,
            "coverage": self.coverage,
        }


@dataclass(frozen=True)
class Capturer:
    """Runs the harness binary of a revision for every scenario."""

    runner: DockerRunner

    def capture_all(
        self,
        revision: Revision,
        suite: ScenarioSuite,
        out_dir: Path,
        logs_dir: Path,
        *,
        inspect: bool = False,
    ) -> list[Capture]:
        out_dir.mkdir(parents=True, exist_ok=True)
        return [
            self.capture(revision, scenario, out_dir, logs_dir, inspect=inspect)
            for scenario in suite.scenarios
        ]

    def capture(
        self,
        revision: Revision,
        scenario: Scenario,
        out_dir: Path,
        logs_dir: Path,
        *,
        inspect: bool = False,
    ) -> Capture:
        scenario_path = revision.fixtures_dir / scenario.filename
        if not scenario_path.is_file():
            raise HarnessError(
                f"scenario `{scenario.name}` ({scenario.filename}) is not in revision "
                f"{revision.short_sha}'s fixtures ({revision.fixtures_dir})"
            )
        parsed = parse_scenario(scenario_path)
        if parsed.name != scenario.name:
            raise HarnessError(
                f"{scenario_path} declares name `{parsed.name}`, expected `{scenario.name}`"
            )
        script = CAPTURE_SCRIPT.format(
            fixture=scenario.filename,
            name=scenario.name,
            inspect=INSPECT_FLAG if inspect else "",
        )
        log_path = logs_dir / f"capture-{revision.label}-{revision.short_sha}-{scenario.name}.log"
        result = self.runner.run_script(
            revision.worktree,
            out_dir,
            script,
            timeout=300,
            log_path=log_path,
        )
        png = out_dir / f"{scenario.name}.png"
        report_path = out_dir / f"{scenario.name}.json"
        if not result.ok or not png.is_file() or not report_path.is_file():
            raise HarnessError(
                f"harness failed for scenario {scenario.name} on {revision.short_sha} "
                f"(exit {result.returncode}): {result.tail(10)}"
            )
        report = json.loads(report_path.read_text())
        reported = str(report.get("revision", ""))
        if reported != revision.sha:
            raise HarnessError(
                f"harness for {scenario.name} reports revision {reported or 'unknown'} "
                f"but {revision.sha} was built"
            )
        return Capture(
            scenario=scenario,
            revision=revision,
            png=png,
            report_path=report_path,
            report=report,
            png_sha256=_sha256(png),
        )

    def benchmark(
        self,
        revision: Revision,
        scenario: Scenario,
        *,
        warmup: int,
        repeats: int,
        round_index: int,
        out_dir: Path,
        logs_dir: Path,
    ) -> BenchmarkRun:
        """Run one `--bench` pass and keep its per-frame timings.

        A revision whose harness does not emit the `benchmark` block (an older
        commit) is BLOCKED with a clear cause, never silently reported as
        measured.
        """
        out_dir.mkdir(parents=True, exist_ok=True)
        scenario_path = revision.fixtures_dir / scenario.filename
        if not scenario_path.is_file():
            raise HarnessError(
                f"scenario `{scenario.name}` ({scenario.filename}) is not in revision "
                f"{revision.short_sha}'s fixtures ({revision.fixtures_dir})"
            )
        parsed = parse_scenario(scenario_path)
        if parsed.name != scenario.name:
            raise HarnessError(
                f"{scenario_path} declares name `{parsed.name}`, expected `{scenario.name}`"
            )
        script = BENCHMARK_SCRIPT.format(
            fixture=scenario.filename,
            name=scenario.name,
            warmup=warmup,
            repeats=repeats,
        )
        log_path = logs_dir / (
            f"benchmark-{revision.label}-{revision.short_sha}-{scenario.name}-r{round_index}.log"
        )
        result = self.runner.run_script(
            revision.worktree,
            out_dir,
            script,
            timeout=600,
            log_path=log_path,
        )
        report_path = out_dir / f"{scenario.name}.json"
        if not result.ok or not report_path.is_file():
            raise HarnessError(
                f"harness --bench failed for scenario {scenario.name} on {revision.short_sha} "
                f"(exit {result.returncode}): {result.tail(10)}"
            )
        report = json.loads(report_path.read_text())
        reported = str(report.get("revision", ""))
        if reported != revision.sha:
            raise HarnessError(
                f"harness for {scenario.name} reports revision {reported or 'unknown'} "
                f"but {revision.sha} was built"
            )
        benchmark = report.get("benchmark")
        if not isinstance(benchmark, dict) or not benchmark.get("enabled"):
            raise HarnessError(
                f"revision {revision.short_sha} does not emit the benchmark block: it was "
                "built before the real-clock instrumentation (milestone 4)"
            )
        frames = tuple(FrameSample.from_dict(entry) for entry in benchmark.get("frames", []))
        if not frames:
            raise HarnessError(
                f"revision {revision.short_sha} reported no benchmark frame for {scenario.name}"
            )
        memory = benchmark.get("memory")
        return BenchmarkRun(
            scenario=scenario,
            revision=revision,
            round_index=round_index,
            frames=frames,
            memory=MemorySample.from_dict(memory) if isinstance(memory, dict) else None,
            environment=str(benchmark.get("environment", "")),
            report=report,
        )


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()
