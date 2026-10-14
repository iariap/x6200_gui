"""Write the experiment report: Markdown, HTML, JSON and the evidence.

One directory per experiment, `reports/<experiment-id>/`, holding:

* `report.md` / `report.html` — the human-readable verdict, gates, issues and
  per-fixture diff summary;
* `results.json` — the whole machine-readable evaluation (what `evaluate`
  reads back in CI);
* `metadata.json` — commits, date, host, tool versions and the fixture list;
* `screenshots/`, `diffs/`, `metrics/`, `logs/` — the captured PNGs, the diff
  and overlay evidence, the render metrics and the build/capture logs.

Nothing here invents data: every number comes from a capture or a measurement,
and gates without instrumentation are printed as BLOCKED with their cause.
"""

from __future__ import annotations

import importlib
import json
import platform
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from . import __version__
from .capture import Capture
from .evaluator import Evaluation
from .status import Status

SUB_DIRS = ("screenshots", "diffs", "metrics", "logs")

_STATUS_COLOR = {
    Status.PASS: "#1b7f3b",
    Status.FAIL: "#b3261e",
    Status.BLOCKED: "#8a6d00",
    Status.INCONCLUSIVE: "#5a5a5a",
    Status.REVIEW_REQUIRED: "#8a4b00",
}


@dataclass(frozen=True)
class ReportMetadata:
    """Identification of the run: what, when, where and with which versions."""

    experiment_id: str
    date: str
    docker_image: str
    scenario_fixtures: dict[str, str]
    # A's fixtures that B does not ship (a screen B removes): they are left out of the
    # comparison, so the report has to say it instead of quietly comparing fewer fixtures.
    scenarios_removed_in_b: tuple[str, ...] = ()

    def as_dict(self) -> dict[str, Any]:
        return {
            "experiment_id": self.experiment_id,
            "date": self.date,
            "host": platform.node(),
            "platform": platform.platform(),
            "framework_version": __version__,
            "python_version": platform.python_version(),
            "pillow_version": _module_version("PIL"),
            "numpy_version": _module_version("numpy"),
            "docker_image": self.docker_image,
            "scenario_fixtures": dict(sorted(self.scenario_fixtures.items())),
            "scenarios_removed_in_b": list(self.scenarios_removed_in_b),
        }


@dataclass(frozen=True)
class ReportPaths:
    """Where the report and its evidence landed."""

    root: Path
    markdown: Path
    html: Path
    results: Path
    metadata: Path
    screenshots: Path
    diffs: Path
    metrics: Path
    logs: Path


def write_report(
    evaluation: Evaluation,
    *,
    reports_dir: Path,
    workspace: Path,
    metadata: ReportMetadata,
    side_a: dict[str, Capture],
    side_b: dict[str, Capture],
) -> ReportPaths:
    """Materialise the experiment directory and return its paths."""
    root = reports_dir / evaluation.experiment_id
    if root.exists():
        shutil.rmtree(root)
    paths = ReportPaths(
        root=root,
        markdown=root / "report.md",
        html=root / "report.html",
        results=root / "results.json",
        metadata=root / "metadata.json",
        screenshots=root / "screenshots",
        diffs=root / "diffs",
        metrics=root / "metrics",
        logs=root / "logs",
    )
    for directory in SUB_DIRS:
        (root / directory).mkdir(parents=True, exist_ok=True)
    _copy_screenshots({"a": side_a, "b": side_b}, paths.screenshots)
    _copy_diffs(evaluation, paths.diffs)
    _write_metrics({"a": side_a, "b": side_b}, paths.metrics)
    _copy_logs(workspace, paths.logs)
    paths.results.write_text(json.dumps(evaluation.as_dict(), indent=2) + "\n")
    paths.metadata.write_text(json.dumps(metadata.as_dict(), indent=2, sort_keys=True) + "\n")
    paths.markdown.write_text(render_markdown(evaluation, metadata, paths))
    paths.html.write_text(render_html(evaluation, metadata, paths))
    return paths


def _copy_screenshots(sides: dict[str, dict[str, Capture]], destination: Path) -> None:
    for side, captures in sides.items():
        for name, capture in sorted(captures.items()):
            if capture.png.is_file():
                shutil.copy2(capture.png, destination / f"{side}-{name}.png")


def _copy_diffs(evaluation: Evaluation, destination: Path) -> None:
    for diff in evaluation.diffs:
        for path in (diff.diff_png, diff.overlay_png):
            if path is not None and path.is_file():
                shutil.copy2(path, destination / path.name)


def _write_metrics(sides: dict[str, dict[str, Capture]], destination: Path) -> None:
    for side, captures in sides.items():
        payload = {
            name: {
                "revision": capture.reported_revision,
                "flushes": capture.flushes(),
                "flushed_pixels": capture.flushed_pixels(),
                "framebuffer_fnv1a": capture.framebuffer_hash,
                "png_sha256": capture.png_sha256,
                "virtual_time_ms": capture.report.get("virtual_time_ms"),
            }
            for name, capture in sorted(captures.items())
        }
        (destination / f"{side}.json").write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n"
        )


def _copy_logs(workspace: Path, destination: Path) -> None:
    logs = workspace / "logs"
    if not logs.is_dir():
        return
    for log in sorted(logs.glob("*.log")):
        shutil.copy2(log, destination / log.name)


def render_markdown(evaluation: Evaluation, metadata: ReportMetadata, paths: ReportPaths) -> str:
    """Human-readable report; every claim is tied to a measurement."""
    lines: list[str] = []
    lines.append(f"# A/B UI report — {evaluation.experiment_id}")
    lines.append("")
    lines.append(f"- Date: {evaluation.date}")
    lines.append(f"- Profile: `{evaluation.profile}`")
    lines.append(f"- A (baseline): `{evaluation.revision_a.sha}` ({evaluation.revision_a.label})")
    lines.append(f"- B (candidate): `{evaluation.revision_b.sha}` ({evaluation.revision_b.label})")
    scenarios = ", ".join(f"`{name}`" for name in evaluation.scenario_names)
    lines.append(f"- Scenarios: {scenarios}")
    lines.append("")
    verdict = evaluation.status.value
    if evaluation.improvement:
        verdict += " (IMPROVEMENT: a known issue present in A is fixed in B)"
    lines.append(f"## Verdict: **{verdict}** (exit {evaluation.status.exit_code})")
    lines.append("")
    lines.extend(_gates_table(evaluation))
    lines.append("")
    lines.extend(_diff_section(evaluation))
    lines.append("")
    lines.extend(_coverage_section(evaluation))
    lines.append("")
    lines.extend(_issues_section(evaluation))
    lines.append("")
    lines.extend(_contracts_section(evaluation))
    lines.append("")
    lines.extend(_evidence_section(evaluation, metadata, paths))
    lines.append("")
    return "\n".join(lines)


def _gates_table(evaluation: Evaluation) -> list[str]:
    rows = ["| Gate | Required | Status | Summary |", "|---|---|---|---|"]
    for gate in evaluation.gates:
        rows.append(
            f"| {gate.name.value} | {'yes' if gate.required else 'no (advisory)'} | "
            f"{gate.status.value} | {gate.summary} |"
        )
    return rows


def _diff_section(evaluation: Evaluation) -> list[str]:
    lines = ["## Visual diff", ""]
    if not evaluation.diffs:
        lines.append("_No scenarios were diffed._")
        return lines
    lines.append("| Scenario | Identical | Changed px | Area | Boxes | Regions |")
    lines.append("|---|---|---|---|---|---|")
    for diff in evaluation.diffs:
        regions = ", ".join(diff.changed_region_names()) or "—"
        area = "—" if diff.dimension_mismatch else f"{diff.changed_fraction * 100:.3f}%"
        changed = "—" if diff.dimension_mismatch else str(diff.changed_pixels)
        lines.append(
            f"| {diff.scenario} | {'yes' if diff.identical else 'no'} | {changed} | "
            f"{area} | {len(diff.boxes)} | {regions} |"
        )
        if diff.dimension_mismatch is not None:
            a_shape, b_shape = diff.dimension_mismatch
            lines.append(f"  - dimension mismatch: A={a_shape} B={b_shape}")
    return lines


def _coverage_section(evaluation: Evaluation) -> list[str]:
    """Coverage the harness declares for the candidate, never inflated."""
    lines = ["## Coverage (declared by the harness)", ""]
    captures = evaluation.captures.get("b", [])
    if not captures:
        lines.append("_No candidate captures._")
        return lines
    levels = ("REAL", "PARTIAL", "SYNTHETIC", "UNSUPPORTED")
    lines.append("| Scenario | " + " | ".join(levels) + " |")
    lines.append("|" + "---|" * (len(levels) + 1))
    partial: list[str] = []
    for capture in captures:
        counts = dict.fromkeys(levels, 0)
        for entry in capture.get("coverage", []):
            level = str(entry.get("level", ""))
            if level in counts:
                counts[level] += 1
            if level in ("PARTIAL", "SYNTHETIC"):
                partial.append(f"{entry.get('component')}={level}")
        row = " | ".join(str(counts[level]) for level in levels)
        lines.append(f"| {capture.get('scenario')} | {row} |")
    if partial:
        lines.append("")
        lines.append("Components declared below REAL (never written as REAL):")
        lines.append("")
        lines.append("- " + ", ".join(sorted(set(partial))))
    return lines


def _issues_section(evaluation: Evaluation) -> list[str]:
    lines = ["## Known issues", ""]
    if not evaluation.issues:
        lines.append("_No known issues registered._")
        return lines
    lines.append("| Issue | Fixture | Severity | Status | A | B | Classification |")
    lines.append("|---|---|---|---|---|---|---|")
    for issue in evaluation.issues:
        lines.append(
            f"| {issue.issue.id} | {issue.issue.fixture} | {issue.issue.severity} | "
            f"{issue.issue.status} | {_tri(issue.reproduced_a)} | {_tri(issue.reproduced_b)} | "
            f"{issue.classification.value} |"
        )
    unverified = [issue for issue in evaluation.issues if issue.reproduced_a is None]
    if unverified:
        lines.append("")
        lines.append(
            f"{len(unverified)} issue(s) are unverified: recorded from the repository but with "
            "no detector yet, so they neither block nor pass. They stay in the annex until the "
            "harness reproduces them."
        )
    return lines


def _contracts_section(evaluation: Evaluation) -> list[str]:
    lines = ["## Quality contracts", ""]
    failures = [
        finding
        for finding in evaluation.contract_findings
        if not finding.ok and finding.baseline_issue is None
    ]
    unchanged = [finding for finding in evaluation.contract_findings if finding.unchanged_defect]
    total = len(evaluation.contract_findings)
    lines.append(
        f"{total} contract checks, {len(failures)} failure(s), "
        f"{len(unchanged)} unchanged baseline defect(s)."
    )
    for failure in failures:
        lines.append(f"- FAIL `{failure.rule}` [{failure.fixture}]: {failure.detail}")
    for item in unchanged:
        lines.append(
            f"- **UNCHANGED DEFECT** `{item.rule}` [{item.fixture}] ({item.baseline_issue}): "
            f"{item.detail}"
        )
    return lines


def _evidence_section(
    evaluation: Evaluation, metadata: ReportMetadata, paths: ReportPaths
) -> list[str]:
    lines = ["## Evidence", ""]
    lines.append(f"- Screenshots: `{paths.screenshots.name}/`")
    lines.append(f"- Diffs: `{paths.diffs.name}/`")
    lines.append(f"- Logs: `{paths.logs.name}/`")
    lines.append(f"- Docker image: `{metadata.docker_image}`")
    if metadata.scenarios_removed_in_b:
        removed = ", ".join(f"`{name}`" for name in metadata.scenarios_removed_in_b)
        lines.append(
            f"- Fixtures of A that B does not ship (REMOVED in B, left out of the "
            f"comparison): {removed}"
        )
    lines.append("")
    lines.append("### Gates without instrumentation (BLOCKED, never PASS)")
    lines.append("")
    for gate in evaluation.blocked_gates:
        lines.append(f"- **{gate.name.value}**: {gate.summary}")
        cause = gate.details.get("cause")
        needed = gate.details.get("needed")
        if cause:
            lines.append(f"  - cause: {cause}")
        if needed:
            lines.append(f"  - needed: {needed}")
    return lines


def render_html(evaluation: Evaluation, metadata: ReportMetadata, paths: ReportPaths) -> str:
    """A self-contained page that links the evidence images next to it."""
    parts: list[str] = [
        "<!doctype html>",
        '<html lang="en"><head><meta charset="utf-8">',
        f"<title>A/B report {evaluation.experiment_id}</title>",
        "<style>body{font-family:system-ui,sans-serif;margin:2rem;max-width:60rem}"
        "table{border-collapse:collapse;width:100%;margin:1rem 0}"
        "th,td{border:1px solid #ccc;padding:.35rem .6rem;text-align:left;font-size:.92rem}"
        "img{max-width:100%;border:1px solid #ccc;margin:.4rem 0}"
        ".pill{padding:.2rem .7rem;border-radius:1rem;color:#fff;font-weight:600}</style>",
        "</head><body>",
        f"<h1>A/B UI report — {evaluation.experiment_id}</h1>",
    ]
    color = _STATUS_COLOR[evaluation.status]
    parts.append(
        f'<p><span class="pill" style="background:{color}">{evaluation.status.value}</span> '
        f"exit {evaluation.status.exit_code}</p>"
    )
    if evaluation.improvement:
        parts.append("<p><strong>IMPROVEMENT</strong>: a known issue in A is fixed in B.</p>")
    parts.append("<ul>")
    parts.append(f"<li>Date: {evaluation.date}</li>")
    parts.append(f"<li>A: <code>{evaluation.revision_a.sha}</code></li>")
    parts.append(f"<li>B: <code>{evaluation.revision_b.sha}</code></li>")
    parts.append(f"<li>Docker image: <code>{metadata.docker_image}</code></li>")
    parts.append(f"<li>Scenarios: {', '.join(evaluation.scenario_names)}</li>")
    if metadata.scenarios_removed_in_b:
        removed = ", ".join(metadata.scenarios_removed_in_b)
        parts.append(f"<li>Fixtures removed in B (not compared): {removed}</li>")
    parts.append("</ul>")
    parts.append("<h2>Gates</h2>")
    parts.append("<table><tr><th>Gate</th><th>Required</th><th>Status</th><th>Summary</th></tr>")
    for gate in evaluation.gates:
        required = "yes" if gate.required else "advisory"
        parts.append(
            f"<tr><td>{gate.name.value}</td><td>{required}</td>"
            f"<td>{gate.status.value}</td><td>{gate.summary}</td></tr>"
        )
    parts.append("</table>")
    parts.append("<h2>Visual diff</h2>")
    parts.append(
        "<table><tr><th>Scenario</th><th>Changed px</th><th>Area</th><th>Regions</th></tr>"
    )
    for diff in evaluation.diffs:
        area = "—" if diff.dimension_mismatch else f"{diff.changed_fraction * 100:.3f}%"
        changed = "—" if diff.dimension_mismatch else str(diff.changed_pixels)
        regions = ", ".join(diff.changed_region_names()) or "—"
        parts.append(
            f"<tr><td>{diff.scenario}</td><td>{changed}</td><td>{area}</td><td>{regions}</td></tr>"
        )
    parts.append("</table>")
    for diff in evaluation.diffs:
        if diff.diff_png is None:
            continue
        parts.append(f"<h3>{diff.scenario}</h3>")
        parts.append(f'<img src="{paths.diffs.name}/{diff.diff_png.name}" alt="diff">')
        if diff.overlay_png is not None:
            parts.append(f'<img src="{paths.diffs.name}/{diff.overlay_png.name}" alt="overlay">')
    parts.append("<h2>Coverage (declared by the harness)</h2>")
    parts.append(
        "<table><tr><th>Scenario</th><th>REAL</th><th>PARTIAL</th>"
        "<th>SYNTHETIC</th><th>UNSUPPORTED</th></tr>"
    )
    for capture in evaluation.captures.get("b", []):
        counts = {"REAL": 0, "PARTIAL": 0, "SYNTHETIC": 0, "UNSUPPORTED": 0}
        for entry in capture.get("coverage", []):
            level = str(entry.get("level", ""))
            if level in counts:
                counts[level] += 1
        parts.append(
            f"<tr><td>{capture.get('scenario')}</td><td>{counts['REAL']}</td>"
            f"<td>{counts['PARTIAL']}</td><td>{counts['SYNTHETIC']}</td>"
            f"<td>{counts['UNSUPPORTED']}</td></tr>"
        )
    parts.append("</table>")
    parts.append("<h2>Known issues</h2>")
    parts.append(
        "<table><tr><th>Issue</th><th>Fixture</th><th>A</th><th>B</th><th>Classification</th></tr>"
    )
    for issue in evaluation.issues:
        parts.append(
            f"<tr><td>{issue.issue.id}</td><td>{issue.issue.fixture}</td>"
            f"<td>{_tri(issue.reproduced_a)}</td><td>{_tri(issue.reproduced_b)}</td>"
            f"<td>{issue.classification.value}</td></tr>"
        )
    parts.append("</table>")
    parts.append("<h2>Quality contracts</h2>")
    failures = [
        finding
        for finding in evaluation.contract_findings
        if not finding.ok and finding.baseline_issue is None
    ]
    unchanged = [finding for finding in evaluation.contract_findings if finding.unchanged_defect]
    parts.append(
        f"<p>{len(evaluation.contract_findings)} checks, {len(failures)} failure(s), "
        f"{len(unchanged)} unchanged baseline defect(s).</p>"
    )
    if failures or unchanged:
        parts.append("<ul>")
        for failure in failures:
            parts.append(
                f"<li>FAIL <code>{failure.rule}</code> [{failure.fixture}]: {failure.detail}</li>"
            )
        for defect in unchanged:
            parts.append(
                f"<li><strong>UNCHANGED DEFECT</strong> <code>{defect.rule}</code> "
                f"[{defect.fixture}] ({defect.baseline_issue}): {defect.detail}</li>"
            )
        parts.append("</ul>")
    if evaluation.blocked_gates:
        parts.append("<h2>Gates without instrumentation</h2><ul>")
        for gate in evaluation.blocked_gates:
            parts.append(f"<li><strong>{gate.name.value}</strong>: {gate.summary}</li>")
        parts.append("</ul>")
    parts.append("</body></html>")
    return "\n".join(parts) + "\n"


def _tri(value: bool | None) -> str:
    if value is None:
        return "—"
    return "yes" if value else "no"


@dataclass(frozen=True)
class BenchmarkReportPaths:
    """Where a benchmark report and its evidence landed."""

    root: Path
    markdown: Path
    results: Path
    metadata: Path


def write_benchmark_report(
    *,
    experiment_id: str,
    reports_dir: Path,
    payload: dict[str, Any],
    metadata: ReportMetadata,
) -> BenchmarkReportPaths:
    """Write `reports/<experiment-id>/` for a render benchmark run.

    The payload is the machine-readable result (same shape the CLI prints) and
    the Markdown is the human-readable summary; both carry the x86-proxy label.
    """
    root = reports_dir / experiment_id
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True, exist_ok=True)
    paths = BenchmarkReportPaths(
        root=root,
        markdown=root / "report.md",
        results=root / "results.json",
        metadata=root / "metadata.json",
    )
    paths.results.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    paths.metadata.write_text(json.dumps(metadata.as_dict(), indent=2, sort_keys=True) + "\n")
    paths.markdown.write_text(render_benchmark_markdown(experiment_id, payload, metadata))
    return paths


def render_benchmark_markdown(
    experiment_id: str, payload: dict[str, Any], metadata: ReportMetadata
) -> str:
    status = str(payload.get("status", ""))
    lines = [f"# Render benchmark — {experiment_id}", ""]
    lines.append(f"- Status: **{status}** (exit {payload.get('exit_code', '')})")
    lines.append(f"- Date: {metadata.date}")
    revisions = payload.get("revisions", {})
    for label, sha in revisions.items():
        lines.append(f"- {label}: `{sha}`")
    comparisons = payload.get("comparisons", [])
    if comparisons:
        lines.append("")
        lines.append(
            "> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, "
            "not the Cortex-A7 frame budget nor battery life."
        )
        lines.append("")
        lines.append(
            "| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | "
            "p95 Δ | flushes Δ | area Δ |"
        )
        lines.append("|---|---|---|---|---|---|---|---|---|")
        for item in comparisons:
            b = item.get("b", {})
            lines.append(
                f"| {item['scenario']} | {item['status']} | "
                f"{b.get('render_p50_ms', '')} | {b.get('render_p95_ms', '')} | "
                f"{b.get('render_p99_ms', '')} | {b.get('fps', '')} | "
                f"{item.get('p95_delta_pct', '')}% | {item.get('flushes_delta_pct', '')}% | "
                f"{item.get('redrawn_area_delta_pct', '')}% |"
            )
        for item in comparisons:
            if item.get("reasons"):
                lines.append("")
                lines.append(f"- **{item['scenario']}** ({item['status']}):")
                for reason in item["reasons"]:
                    lines.append(f"  - {reason}")
    measurements = payload.get("measurements", [])
    if measurements:
        lines.append("")
        lines.append("> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.")
        lines.append("")
        lines.append(
            "| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | "
            "flushes | redrawn area | RSS | PSS |"
        )
        lines.append("|---|---|---|---|---|---|---|---|---|---|---|")
        for item in measurements:
            memory = item.get("memory") or {}
            lines.append(
                f"| {item['scenario']} | {item.get('frames', '')} | "
                f"{item.get('render_p50_ms', '')} | {item.get('render_p95_ms', '')} | "
                f"{item.get('render_p99_ms', '')} | {item.get('fps', '')} | "
                f"{item.get('dispersion_cv_pct', '')}% | {item.get('flushes_mean', '')} | "
                f"{item.get('redrawn_area_mean', '')} | "
                f"{memory.get('rss_bytes', '')} | {memory.get('pss_bytes', '')} |"
            )
    lines.append("")
    lines.append(f"- Environment: {payload.get('environment', 'x86 host')}")
    lines.append("")
    return "\n".join(lines)


def _module_version(name: str) -> str:
    try:
        module = importlib.import_module(name)
    except ImportError:  # pragma: no cover - dependency is declared
        return "unknown"
    return str(getattr(module, "__version__", "unknown"))
