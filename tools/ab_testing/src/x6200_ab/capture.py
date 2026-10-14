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
  --json /out/{name}.json
"""


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
    ) -> list[Capture]:
        out_dir.mkdir(parents=True, exist_ok=True)
        return [self.capture(revision, scenario, out_dir, logs_dir) for scenario in suite.scenarios]

    def capture(
        self,
        revision: Revision,
        scenario: Scenario,
        out_dir: Path,
        logs_dir: Path,
    ) -> Capture:
        scenario_path = revision.fixtures_dir / scenario.filename
        parsed = parse_scenario(scenario_path)
        if parsed.name != scenario.name:
            raise HarnessError(
                f"{scenario_path} declares name `{parsed.name}`, expected `{scenario.name}`"
            )
        script = CAPTURE_SCRIPT.format(fixture=scenario.filename, name=scenario.name)
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


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()
