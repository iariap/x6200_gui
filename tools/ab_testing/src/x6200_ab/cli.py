"""Command-line entry point: capture, audit, compare, benchmark, report, evaluate.

Exit codes follow the plan: 0 PASS, 1 FAIL, 2 BLOCKED, 3 INCONCLUSIVE,
4 REVIEW REQUIRED. Every subcommand accepts `--json` for CI and writes its
scratch work (git worktrees, builds, captures) into an isolated workspace that
is removed on the way out. The framework is never destructive with the user's
own worktrees: it only removes the ones it created.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import tempfile
from collections.abc import Sequence
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from . import __version__, proc
from .builds import HarnessBuilder
from .capture import BenchmarkRun, Capture, Capturer, FrameSample, MemorySample
from .contracts import run_contracts, run_structural_checks
from .docker import DockerRunner
from .errors import AbError, DockerUnavailable
from .evaluator import ComparisonData, evaluate, observe_issues
from .inspection import inspection_of
from .known_issues import KnownIssue, evaluate_detector, load_known_issues
from .nontext_checks import (
    MissingRender,
    NonTextResult,
    load_nontext_objects,
    run_nontext_checks,
)
from .performance import (
    BenchmarkVerdict,
    ScenarioStats,
    compare_benchmark,
    measure_verdict,
    stats_from_frames,
)
from .quality_config import Budgets, QualityContracts, load_budgets, load_contracts
from .quality_gates import ALL_GATES, GateName, GateProfile, GateResult, overall
from .regions import RegionMap, load_regions
from .reports import ReportMetadata, write_benchmark_report, write_report
from .revisions import Revision, RevisionManager, find_repo
from .scenarios import ScenarioSuite, split_by_fixtures_dir
from .status import Status
from .theme_files import (
    BASE_PALETTES,
    SHIPPED_THEMES_SUBDIR,
    ThemeReport,
    load_builtin_palettes,
    load_exemptions,
    load_theme_pairs,
    served_theme_reports,
)
from .visual_diff import DiffResult, compute_diff

DEFAULT_REPORTS_SUBDIR = ("tools", "ab_testing", "reports")
DEFAULT_QUALITY_SUBDIR = ("tools", "ab_testing", "quality")


@dataclass
class Context:
    """Everything a command needs that is not a git revision."""

    repo: Path
    quality_dir: Path
    workspace: Path
    cache_dir: Path
    keep_workspace: bool
    region_map: RegionMap
    contracts: QualityContracts
    budgets: Budgets
    known_issues: tuple[KnownIssue, ...]
    runner: DockerRunner


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="x6200-ab",
        description="A/B testing framework for the X6200 UI harness (host side).",
    )
    parser.add_argument("--version", action="version", version=f"x6200-ab {__version__}")
    sub = parser.add_subparsers(dest="command")

    capture = sub.add_parser("capture", help="render every scenario of one revision")
    _common(capture)
    capture.add_argument("--revision", required=True, help="git rev (SHA, branch, tag)")
    capture.add_argument("--label", default="rev", help="label used in paths and reports")
    capture.add_argument("--scenario", action="append", default=[], help="limit to a fixture")
    capture.add_argument(
        "--inspect",
        action="store_true",
        help="also emit the structural inspection of the object tree in the JSON",
    )
    capture.add_argument(
        "--font",
        default=None,
        help="render with this font family (the ids of src/fonts/font_families.h, "
        "e.g. inter): the font matrix of issue #9 renders every candidate with one "
        "build instead of one build per family",
    )
    capture.add_argument("--out", required=True, help="directory for the PNG + JSON captures")
    capture.set_defaults(func=cmd_capture)

    audit = sub.add_parser("audit", help="check the known-issue detectors on one revision")
    _common(audit)
    audit.add_argument("--revision", "--ref", dest="revision", required=True)
    audit.add_argument("--scenario", action="append", default=[])
    audit.add_argument(
        "--suite",
        choices=("visual", "all"),
        default=None,
        help="select the whole visual suite (all fixtures); same as no --scenario",
    )
    audit.set_defaults(func=cmd_audit)

    contracts = sub.add_parser(
        "contracts", help="evaluate the objective quality contracts on one revision"
    )
    _common(contracts)
    contracts.add_argument("--revision", required=True)
    contracts.add_argument("--scenario", action="append", default=[])
    contracts.set_defaults(func=cmd_contracts)

    compare = sub.add_parser("compare", help="A/B compare two revisions and write a report")
    _common(compare)
    compare.add_argument("--a", required=True, help="baseline rev")
    compare.add_argument("--b", required=True, help="candidate rev")
    compare.add_argument("--scenario", action="append", default=[])
    compare.add_argument("--experiment-id", default=None)
    compare.add_argument("--reports-dir", default=None)
    compare.set_defaults(func=cmd_compare)

    benchmark = sub.add_parser(
        "benchmark", help="real-clock render cost of one revision, or A vs B"
    )
    _common(benchmark)
    benchmark.add_argument("--revision", default=None, help="measure a single revision")
    benchmark.add_argument("--a", default=None, help="baseline revision (with --b)")
    benchmark.add_argument("--b", default=None, help="candidate revision (with --a)")
    benchmark.add_argument("--scenario", action="append", default=[])
    benchmark.add_argument("--repeats", type=int, default=None, help="override the budget")
    benchmark.add_argument("--warmup", type=int, default=None, help="override the budget")
    benchmark.add_argument("--rounds", type=int, default=None, help="override the budget")
    benchmark.add_argument("--experiment-id", default=None)
    benchmark.add_argument("--reports-dir", default=None)
    benchmark.set_defaults(func=cmd_benchmark)

    themes = sub.add_parser(
        "themes",
        help="static contrast over every served theme + non-text over the golden renders",
    )
    themes.add_argument("--repo", default=None, help="repository root (default: cwd)")
    themes.add_argument("--quality-dir", default=None, help="quality YAML directory")
    themes.add_argument(
        "--themes-dir",
        default=None,
        help="directory with the shipped theme files (default: rootfs/usr/share/x6200/themes)",
    )
    themes.add_argument(
        "--renders-dir",
        default=None,
        help="committed renders for the non-text check (default: tools/ab_testing/golden)",
    )
    themes.add_argument(
        "--no-nontext",
        action="store_true",
        help="skip the WCAG 1.4.11 non-text pixel check",
    )
    themes.add_argument("--json", action="store_true")
    themes.set_defaults(func=cmd_themes)

    report = sub.add_parser("report", help="print an existing experiment's verdict")
    report.add_argument("--experiment-id", required=True)
    report.add_argument("--reports-dir", default=None)
    report.add_argument("--repo", default=None)
    report.add_argument("--json", action="store_true")
    report.set_defaults(func=cmd_report)

    evaluate = sub.add_parser("evaluate", help="recompute the verdict from results.json")
    evaluate.add_argument("--results", required=True, help="path to results.json")
    evaluate.add_argument("--profile", choices=("host-ui", "full"), default="host-ui")
    evaluate.add_argument("--json", action="store_true")
    evaluate.set_defaults(func=cmd_evaluate)

    return parser


def _common(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--repo", default=None, help="repository root (default: cwd)")
    parser.add_argument("--quality-dir", default=None, help="quality YAML directory")
    parser.add_argument("--workspace", default=None, help="scratch dir for worktrees/builds")
    parser.add_argument(
        "--cache-dir",
        default=None,
        help="shared cache of the pinned lvgl submodule tree (default: ~/.cache/x6200-ab)",
    )
    parser.add_argument("--keep-workspace", action="store_true")
    parser.add_argument(
        "--profile",
        choices=("host-ui", "full"),
        default="host-ui",
        help="which gates decide the verdict (host-ui: four required; full: all six)",
    )
    parser.add_argument("--json", action="store_true")


def _profile(args: argparse.Namespace) -> GateProfile:
    if getattr(args, "profile", "host-ui") == "full":
        return GateProfile.full()
    return GateProfile.host_ui()


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "func"):
        parser.print_help()
        return Status.BLOCKED.exit_code
    try:
        return int(args.func(args))
    except proc.CommandNotFound as exc:
        return _emit_error(args, DockerUnavailable(str(exc)))
    except AbError as exc:
        return _emit_error(args, exc)
    except KeyboardInterrupt:  # pragma: no cover - interactive only
        return 130


# --- capture -----------------------------------------------------------------


def cmd_capture(args: argparse.Namespace) -> int:
    context = _build_context(args)
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    try:
        revision = _materialize(context, manager, args.revision, args.label)
        suite = ScenarioSuite.from_fixtures_dir(revision.fixtures_dir, args.scenario)
        out_dir = Path(args.out).resolve()
        captures = Capturer(context.runner).capture_all(
            revision,
            suite,
            out_dir,
            context.workspace / "logs",
            inspect=bool(getattr(args, "inspect", False)),
            font=getattr(args, "font", None),
        )
        payload = {
            "status": Status.PASS.value,
            "exit_code": Status.PASS.exit_code,
            "revision": revision.sha,
            "out": str(out_dir),
            "captures": [_capture_dict(capture) for capture in captures],
        }
        text = "\n".join(
            f"{capture.name}: {capture.png} ({capture.width}x{capture.height}, "
            f"{capture.png_sha256[:12]})"
            for capture in captures
        )
        return _emit(args, payload, text, Status.PASS)
    finally:
        _teardown(context, manager)


# --- audit -------------------------------------------------------------------


def cmd_audit(args: argparse.Namespace) -> int:
    """Known-issue detectors *and* the structural inspection of the live tree.

    Captures run with `--inspect`, so the audit measures the real object tree
    (bounding boxes, text metrics, glyphs) instead of being blocked on missing
    instrumentation. A structural finding that is not waived fails the audit.
    """
    context = _build_context(args)
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    try:
        revision = _materialize(context, manager, args.revision, "audit")
        suite = ScenarioSuite.from_fixtures_dir(revision.fixtures_dir, args.scenario)
        out_dir = context.workspace / "captures" / revision.short_sha
        captures = {
            capture.name: capture
            for capture in Capturer(context.runner).capture_all(
                revision, suite, out_dir, context.workspace / "logs", inspect=True
            )
        }
        results: list[dict[str, Any]] = []
        for issue in context.known_issues:
            capture = captures.get(issue.fixture)
            if issue.detector is None or capture is None:
                results.append(
                    {
                        "id": issue.id,
                        "title": issue.title,
                        "reproduced": None,
                        "detail": (
                            "out of scope: the UI harness cannot measure it (see "
                            "docs/UI_KNOWN_ISSUES.md)"
                            if issue.status == "out-of-scope"
                            else "unverified: no detector yet"
                        ),
                    }
                )
                continue
            outcome = evaluate_detector(issue.detector, capture, context.region_map)
            results.append(
                {
                    "id": issue.id,
                    "title": issue.title,
                    "reproduced": outcome.reproduced,
                    "detail": outcome.detail,
                }
            )
        reproduced = [item["id"] for item in results if item["reproduced"]]
        unverified = [item["id"] for item in results if item["reproduced"] is None]

        structural = run_structural_checks(
            [captures[name] for name in sorted(captures)], context.contracts
        )
        failures = [finding for finding in structural if not finding.ok]
        nodes = sum(
            inspection.count
            for capture in captures.values()
            if (inspection := inspection_of(capture)) is not None
        )
        status = Status.FAIL if failures else Status.PASS
        payload = {
            "status": status.value,
            "exit_code": status.exit_code,
            "revision": revision.sha,
            "scenarios": suite.names,
            "instrumentation": {
                "structural": True,
                "source": "live LVGL object tree (ui_harness --inspect); no OCR",
                "nodes_inspected": nodes,
            },
            "reproduced": reproduced,
            "unverified": unverified,
            "issues": results,
            "structural_failures": len(failures),
            "findings": [finding.as_dict() for finding in structural],
        }
        text = (
            f"audited {revision.short_sha}: {len(results)} known issues — "
            f"{len(reproduced)} reproduced, {len(unverified)} unverified; "
            f"structural inspection REAL over {nodes} objects on {len(captures)} "
            f"scenario(s): {len(failures)} structural finding(s)"
        )
        if failures:
            text += "\n" + "\n".join(
                f"  FAIL {finding.rule} [{finding.fixture}]: {finding.detail}"
                for finding in failures
            )
        return _emit(args, payload, text, status)
    finally:
        _teardown(context, manager)


# --- contracts ---------------------------------------------------------------


def cmd_contracts(args: argparse.Namespace) -> int:
    """Evaluate the candidate alone against the objective quality contracts.

    Independent from any baseline: geometry, minimum content per region and the
    honesty of the declared coverage. A rule the candidate violates is a FAIL;
    this is the C of the plan (quality, not A/B).
    """
    context = _build_context(args)
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    try:
        revision = _materialize(context, manager, args.revision, "contracts")
        suite = ScenarioSuite.from_fixtures_dir(revision.fixtures_dir, args.scenario)
        out_dir = context.workspace / "captures" / revision.short_sha
        captures = Capturer(context.runner).capture_all(
            revision, suite, out_dir, context.workspace / "logs", inspect=True
        )
        findings = run_contracts(
            captures, context.region_map, context.contracts, context.known_issues
        )
        failures = [
            finding for finding in findings if not finding.ok and finding.baseline_issue is None
        ]
        unchanged = [finding for finding in findings if finding.unchanged_defect]
        status = Status.FAIL if failures else Status.PASS
        payload = {
            "status": status.value,
            "exit_code": status.exit_code,
            "revision": revision.sha,
            "scenarios": suite.names,
            "checks": len(findings),
            "failures": [finding.as_dict() for finding in failures],
            "unchanged_defects": [finding.as_dict() for finding in unchanged],
            "findings": [finding.as_dict() for finding in findings],
        }
        text = (
            f"contracts {revision.short_sha}: {status.value} — "
            f"{len(findings)} checks, {len(failures)} failure(s), "
            f"{len(unchanged)} unchanged baseline defect(s)"
        )
        if failures:
            text += "\n" + "\n".join(
                f"  FAIL {finding.rule} [{finding.fixture}]: {finding.detail}"
                for finding in failures
            )
        for defect in unchanged:
            text += (
                f"\n  UNCHANGED DEFECT {defect.rule} [{defect.fixture}] "
                f"({defect.baseline_issue}): {defect.detail}"
            )
        return _emit(args, payload, text, status)
    finally:
        _teardown(context, manager)


# --- themes ------------------------------------------------------------------


def cmd_themes(args: argparse.Namespace) -> int:
    """Static WCAG contrast over every palette the ROM carries.

    Offline by design: it reads the base palettes of `src/theme.h` (the internal
    templates a file's `base` inherits from) and the shipped files of
    `rootfs/usr/share/x6200/themes`, resolves each file's `base` the way
    `src/theme.c` loads a theme, and measures the pairs
    `quality/theme_pairs.yaml` declares — the Settings pairs the rendered
    contracts cannot reach. The ten palettes (four base templates plus the six
    shipped files) are measured, so there is no UNCOVERED theme left. The Settings
    selector offers exactly the six themes of the collection (issues #73/#74); the
    flag `offered` in the payload says which ones are a menu choice.
    No capture, no revision, no Docker: the verdict only depends on files in the
    repository.
    """
    repo = _resolve_repo(args)
    quality_dir = (
        Path(args.quality_dir).resolve() if args.quality_dir else _resolve_quality_dir(repo)
    )
    themes_dir = (
        Path(args.themes_dir).resolve()
        if getattr(args, "themes_dir", None)
        else repo.joinpath(*SHIPPED_THEMES_SUBDIR)
    )
    pairs_path = quality_dir / "theme_pairs.yaml"
    theme_h = repo / "src" / "theme.h"
    pairs = load_theme_pairs(pairs_path)
    exemptions = load_exemptions(pairs_path)
    palettes = load_builtin_palettes(theme_h)
    reports = served_theme_reports(themes_dir, theme_h, palettes, pairs)

    failures = [result for report in reports for result in report.failures]
    errors = [report for report in reports if report.error is not None]

    nontext_results, nontext_missing = _nontext_section(args, repo, quality_dir)

    nontext_failures = [result for result in nontext_results if not result.ok]
    # A declared object with no render is a coverage hole, not a pass: the owner's
    # order is "measured, not declared".
    status = (
        Status.FAIL if (failures or errors or nontext_failures or nontext_missing) else Status.PASS
    )
    checked = sum(len(report.results) for report in reports)
    base_count = sum(1 for report in reports if report.source == "base")
    file_count = len(reports) - base_count
    # Paths go out relative to the repository when they live inside it: the
    # payload is an artifact (CI, a report), not a description of this machine.
    themes_json = [_theme_dict(report, repo) for report in reports]
    payload = {
        "status": status.value,
        "exit_code": status.exit_code,
        "themes_dir": _relative(themes_dir, repo),
        "theme_roles": len(palettes[BASE_PALETTES[0]]),
        "pairs": [pair.as_dict() for pair in pairs],
        "themes": themes_json,
        "pairs_checked": checked,
        "exempt": [exemption.as_dict() for exemption in exemptions],
        "failures": [result.as_dict() for result in failures],
        "nontext": {
            "objects": len(nontext_results),
            "checked": len(nontext_results),
            "failures": [result.as_dict() for result in nontext_failures],
            "missing": [missing.as_dict() for missing in nontext_missing],
            "results": [result.as_dict() for result in nontext_results],
        },
        "errors": [
            {
                "theme": report.name,
                "path": _relative(Path(report.path), repo),
                "error": report.error,
            }
            for report in errors
        ],
    }
    lines = [
        f"themes {status.value}: {len(reports)} palette(s) "
        f"({base_count} base + {file_count} file(s)), {len(pairs)} pair(s) declared, "
        f"{checked} pair(s) checked, {len(failures)} failure(s)"
    ]
    for report in reports:
        header = f"  {report.name} ({_relative(Path(report.path), repo)}"
        header += "; base palette" if report.source == "base" else f"; base {report.base}"
        header += ")"
        if report.error is not None:
            lines.append(f"{header}: {report.error}")
            continue
        where = (
            f"{len(report.inherited)}/{len(palettes[BASE_PALETTES[0]])} roles from the base"
            if report.source != "base"
            else f"all {len(palettes[BASE_PALETTES[0]])} roles from src/theme.c"
        )
        flags = []
        if not report.offered:
            flags.append("not offered in the menu")
        if report.binary_surfaces:
            flags.append(f"binary surfaces: {', '.join(report.binary_surfaces)}")
        suffix = f" [{'; '.join(flags)}]" if flags else ""
        lines.append(f"{header}: {where}{suffix}")
        for result in report.results:
            label = f"{result.pair.fg} on {result.pair.bg}"
            note = f" [bg = {result.image}]" if result.binary else ""
            lines.append(
                f"    {'OK  ' if result.ok else 'FAIL'} {label:<30}"
                f"{result.ratio:6.2f} >= {result.pair.min_ratio:4.1f}  {result.origin}{note}"
            )
    if nontext_results:
        lines.append(
            f"  non-text (WCAG 1.4.11, pixels): {len(nontext_results)} object(s) measured, "
            f"{len(nontext_failures)} failure(s)"
        )
        for result in nontext_results:
            lines.append(f"    {'OK  ' if result.ok else 'FAIL'} {result.detail}")
    for missing in nontext_missing:
        lines.append(
            f"  NON-TEXT UNMEASURED {missing.fixture}: {missing.detail} "
            f"({', '.join(missing.objects)})"
        )
    if exemptions:
        lines.append(
            f"  exempt (WCAG 1.4.3, disabled/incidental): {len(exemptions)} role(s) — "
            f"{', '.join(exemption.role for exemption in exemptions)}"
        )
    return _emit(args, payload, "\n".join(lines), status)


def _nontext_section(
    args: argparse.Namespace, repo: Path, quality_dir: Path
) -> tuple[tuple[NonTextResult, ...], tuple[MissingRender, ...]]:
    """Measure the WCAG 1.4.11 objects over the committed (or given) renders."""
    if bool(getattr(args, "no_nontext", False)):
        return (), ()
    config = quality_dir / "nontext.yaml"
    if not config.is_file():
        return (), ()
    renders_dir = (
        Path(args.renders_dir).resolve()
        if getattr(args, "renders_dir", None)
        else repo / "tools" / "ab_testing" / "golden"
    )
    renders = (
        {path.stem: path for path in sorted(renders_dir.glob("*.png"))}
        if renders_dir.is_dir()
        else {}
    )
    objects = load_nontext_objects(config)
    return run_nontext_checks(renders, objects)


def _theme_dict(report: ThemeReport, repo: Path) -> dict[str, Any]:
    entry = report.as_dict()
    entry["path"] = _relative(Path(report.path), repo)
    return entry


def _relative(path: Path, repo: Path) -> str:
    try:
        return str(path.relative_to(repo))
    except ValueError:
        return str(path)


# --- compare -----------------------------------------------------------------


def cmd_compare(args: argparse.Namespace) -> int:
    context = _build_context(args)
    experiment_id = args.experiment_id or _new_experiment_id()
    reports_dir = (
        Path(args.reports_dir).resolve()
        if args.reports_dir
        else context.repo.joinpath(*DEFAULT_REPORTS_SUBDIR)
    )
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    capturer = Capturer(context.runner)
    try:
        revision_a = _materialize(context, manager, args.a, "a")
        suite = ScenarioSuite.from_fixtures_dir(revision_a.fixtures_dir, args.scenario)
        revision_b = _materialize(context, manager, args.b, "b")
        # A defines the suite. A change may legitimately drop a fixture in B (a screen B
        # no longer ships): that scenario cannot be rendered on B, so it is left out of the
        # comparison and reported (payload, text and metadata) instead of aborting the whole
        # run on a missing file. See tests/test_scenarios.py.
        suite, removed_in_b = split_by_fixtures_dir(suite, revision_b.fixtures_dir)
        a_captures = capturer.capture_all(
            revision_a,
            suite,
            context.workspace / "captures" / f"a-{revision_a.short_sha}",
            context.workspace / "logs",
            # A needs the structural inspection too: the `leading_glyph_range`
            # detectors read the label text from it, and the pixels stay
            # byte-identical (`--inspect` never changes a rendered pixel).
            inspect=True,
        )
        b_captures = capturer.capture_all(
            revision_b,
            suite,
            context.workspace / "captures" / f"b-{revision_b.short_sha}",
            context.workspace / "logs",
            inspect=True,
        )
        a_by = {capture.name: capture for capture in a_captures}
        b_by = {capture.name: capture for capture in b_captures}
        diffs = _compute_diffs(a_by, b_by, context)
        issues = observe_issues(context.known_issues, a_by, b_by, context.region_map)
        findings = run_contracts(
            b_captures, context.region_map, context.contracts, context.known_issues
        )
        data = ComparisonData(
            revision_a=revision_a,
            revision_b=revision_b,
            scenario_names=tuple(suite.names),
            a_captures=a_by,
            b_captures=b_by,
            diffs=tuple(diffs),
            issues=issues,
            contract_findings=tuple(findings),
        )
        evaluation = evaluate(
            data,
            region_map=context.region_map,
            contracts=context.contracts,
            budgets=context.budgets,
            profile=_profile(args),
            experiment_id=experiment_id,
        )
        metadata = ReportMetadata(
            experiment_id=experiment_id,
            date=evaluation.date,
            docker_image=context.runner.image,
            scenario_fixtures={scenario.name: scenario.filename for scenario in suite.scenarios},
            scenarios_removed_in_b=removed_in_b,
        )
        paths = write_report(
            evaluation,
            reports_dir=reports_dir,
            workspace=context.workspace,
            metadata=metadata,
            side_a=a_by,
            side_b=b_by,
        )
        payload = {
            "status": evaluation.status.value,
            "exit_code": evaluation.status.exit_code,
            "experiment_id": experiment_id,
            "report": str(paths.root),
            "improvement": evaluation.improvement,
            "gates": [gate.as_dict() for gate in evaluation.gates],
            "scenarios_removed_in_b": list(removed_in_b),
        }
        text = _compare_text(evaluation, paths.root, removed_in_b)
        return _emit(args, payload, text, evaluation.status)
    finally:
        _teardown(context, manager)


def _compare_text(evaluation: Any, report_root: Path, removed_in_b: tuple[str, ...] = ()) -> str:
    lines = [
        f"experiment {evaluation.experiment_id}: {evaluation.status.value} "
        f"(exit {evaluation.status.exit_code})"
    ]
    if evaluation.improvement:
        lines.append("IMPROVEMENT: a known issue present in A is fixed in B")
    if removed_in_b:
        lines.append(
            "scenarios REMOVED in B (A's fixture, not shipped by B; left out of the "
            f"comparison): {', '.join(removed_in_b)}"
        )
    for gate in evaluation.gates:
        lines.append(
            f"  {gate.name.value:18s} {gate.status.value:15s} "
            f"{'(required)' if gate.required else '(advisory)'} — {gate.summary}"
        )
    lines.append(f"report: {report_root}")
    return "\n".join(lines)


# --- benchmark ---------------------------------------------------------------


def cmd_benchmark(args: argparse.Namespace) -> int:
    """Measure the real render cost: one revision, or A vs B with alternation.

    A single revision is a *measurement* (p50/p95/p99, FPS, dispersion): PASS
    when the sample is stable, INCONCLUSIVE when it is too variable. Two
    revisions are a *gate*: FAIL when the candidate's p95 render time (or flush
    count / redrawn area) exceeds the baseline beyond the fixed budget.
    """
    context = _build_context(args)
    budget = context.budgets.benchmark
    warmup = args.warmup or budget.warmup_frames
    repeats = args.repeats or budget.repeats
    rounds = args.rounds or budget.rounds
    experiment_id = args.experiment_id or ("BENCH-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
    reports_dir = (
        Path(args.reports_dir).resolve()
        if args.reports_dir
        else context.repo.joinpath(*DEFAULT_REPORTS_SUBDIR)
    )
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    capturer = Capturer(context.runner)
    logs_dir = context.workspace / "logs"

    if not args.revision and not (args.a and args.b):
        raise AbError("benchmark needs --revision, or both --a and --b")

    try:
        environment = ""

        if args.a and args.b:
            revision_a = _materialize(context, manager, args.a, "a")
            revision_b = _materialize(context, manager, args.b, "b")
            suite = ScenarioSuite.from_fixtures_dir(revision_a.fixtures_dir, args.scenario)
            frames_a: dict[str, list[FrameSample]] = {name: [] for name in suite.names}
            frames_b: dict[str, list[FrameSample]] = {name: [] for name in suite.names}
            memory_a: dict[str, MemorySample] = {}
            memory_b: dict[str, MemorySample] = {}
            for round_index in range(rounds):
                environment = _bench_round(
                    capturer,
                    revision_a,
                    suite,
                    round_index=round_index,
                    warmup=warmup,
                    repeats=repeats,
                    workspace=context.workspace,
                    logs_dir=logs_dir,
                    frames=frames_a,
                    memories=memory_a,
                )
                environment = _bench_round(
                    capturer,
                    revision_b,
                    suite,
                    round_index=round_index,
                    warmup=warmup,
                    repeats=repeats,
                    workspace=context.workspace,
                    logs_dir=logs_dir,
                    frames=frames_b,
                    memories=memory_b,
                )
            stats_a = _stats(frames_a, memory_a)
            stats_b = _stats(frames_b, memory_b)
            verdict = compare_benchmark(stats_a, stats_b, budget)
            payload: dict[str, Any] = {
                "status": verdict.status.value,
                "exit_code": verdict.status.exit_code,
                "experiment_id": experiment_id,
                "kind": "benchmark-comparison",
                "revisions": {"a": revision_a.sha, "b": revision_b.sha},
                "scenarios": suite.names,
                "environment": environment,
                "budget": verdict.as_dict()["budget"],
                "comparisons": [item.as_dict() for item in verdict.comparisons],
            }
            metadata = ReportMetadata(
                experiment_id=experiment_id,
                date=datetime.now().date().isoformat(),
                docker_image=context.runner.image,
                scenario_fixtures={
                    scenario.name: scenario.filename for scenario in suite.scenarios
                },
            )
            paths = write_benchmark_report(
                experiment_id=experiment_id,
                reports_dir=reports_dir,
                payload=payload,
                metadata=metadata,
            )
            payload["report"] = str(paths.root)
            text = _benchmark_comparison_text(verdict, revision_a, revision_b, paths.root)
            return _emit(args, payload, text, verdict.status)

        revision = _materialize(context, manager, args.revision, "bench")
        suite = ScenarioSuite.from_fixtures_dir(revision.fixtures_dir, args.scenario)
        frames: dict[str, list[FrameSample]] = {name: [] for name in suite.names}
        memories: dict[str, MemorySample] = {}
        for round_index in range(rounds):
            environment = _bench_round(
                capturer,
                revision,
                suite,
                round_index=round_index,
                warmup=warmup,
                repeats=repeats,
                workspace=context.workspace,
                logs_dir=logs_dir,
                frames=frames,
                memories=memories,
            )
        stats = _stats(frames, memories)
        status = measure_verdict(stats, budget)
        payload = {
            "status": status.value,
            "exit_code": status.exit_code,
            "experiment_id": experiment_id,
            "kind": "benchmark-measurement",
            "revisions": {"revision": revision.sha},
            "scenarios": suite.names,
            "environment": environment,
            "budget": {
                "warmup_frames": warmup,
                "repeats": repeats,
                "rounds": rounds,
                "min_frames": budget.min_frames,
                "max_dispersion_cv_pct": budget.max_dispersion_cv_pct,
            },
            "measurements": [stats[name].as_dict() for name in sorted(stats)],
        }
        metadata = ReportMetadata(
            experiment_id=experiment_id,
            date=datetime.now().date().isoformat(),
            docker_image=context.runner.image,
            scenario_fixtures={scenario.name: scenario.filename for scenario in suite.scenarios},
        )
        paths = write_benchmark_report(
            experiment_id=experiment_id,
            reports_dir=reports_dir,
            payload=payload,
            metadata=metadata,
        )
        payload["report"] = str(paths.root)
        text = _benchmark_measurement_text(revision, stats, status, paths.root)
        return _emit(args, payload, text, status)
    finally:
        _teardown(context, manager)


def _bench_round(
    capturer: Capturer,
    revision: Revision,
    suite: ScenarioSuite,
    *,
    round_index: int,
    warmup: int,
    repeats: int,
    workspace: Path,
    logs_dir: Path,
    frames: dict[str, list[FrameSample]],
    memories: dict[str, MemorySample],
) -> str:
    """Run one `--bench` pass per scenario and append its frames."""
    environment = ""
    for scenario in suite.scenarios:
        out_dir = workspace / "bench" / f"{revision.short_sha}-r{round_index}"
        run: BenchmarkRun = capturer.benchmark(
            revision,
            scenario,
            warmup=warmup,
            repeats=repeats,
            round_index=round_index,
            out_dir=out_dir,
            logs_dir=logs_dir,
        )
        frames[scenario.name].extend(run.frames)
        if run.memory is not None:
            memories[scenario.name] = run.memory
        if run.environment:
            environment = run.environment
    return environment


def _stats(
    frames: dict[str, list[FrameSample]], memories: dict[str, MemorySample]
) -> dict[str, ScenarioStats]:
    return {
        name: stats_from_frames(name, tuple(samples), memories.get(name))
        for name, samples in frames.items()
    }


def _benchmark_comparison_text(
    verdict: BenchmarkVerdict, revision_a: Revision, revision_b: Revision, report_root: Path
) -> str:
    lines = [
        f"benchmark {revision_a.short_sha} (A) vs {revision_b.short_sha} (B): "
        f"{verdict.status.value} (exit {verdict.status.exit_code})"
    ]
    for item in verdict.comparisons:
        lines.append(
            f"  {item.scenario:20s} {item.status.value:15s} "
            f"p95 {item.a.render_p95_ms:.3f} -> {item.b.render_p95_ms:.3f} ms "
            f"({item.p95_delta_pct:+.1f}%)"
        )
        for reason in item.reasons:
            lines.append(f"      {reason}")
    lines.append("  x86 host proxy: relative regression detection, not the Cortex-A7")
    lines.append(f"report: {report_root}")
    return "\n".join(lines)


def _benchmark_measurement_text(
    revision: Revision,
    stats: dict[str, ScenarioStats],
    status: Status,
    report_root: Path,
) -> str:
    lines = [
        f"benchmark {revision.short_sha}: {status.value} (exit {status.exit_code}) — "
        "x86 host proxy, not the Cortex-A7"
    ]
    for name in sorted(stats):
        scenario = stats[name]
        rss = scenario.memory.rss_bytes if scenario.memory else 0
        lines.append(
            f"  {name:20s} frames={scenario.frames} p50={scenario.render_p50_ms:.3f} "
            f"p95={scenario.render_p95_ms:.3f} p99={scenario.render_p99_ms:.3f} ms "
            f"fps={scenario.fps:.1f} cv={scenario.dispersion_cv_pct:.1f}% "
            f"flushes={scenario.flushes_mean:.1f} rss={rss} B"
        )
    lines.append(f"report: {report_root}")
    return "\n".join(lines)


# --- report / evaluate -------------------------------------------------------


def cmd_report(args: argparse.Namespace) -> int:
    repo = _resolve_repo(args)
    reports_dir = (
        Path(args.reports_dir).resolve()
        if args.reports_dir
        else repo.joinpath(*DEFAULT_REPORTS_SUBDIR)
    )
    results_path = reports_dir / args.experiment_id / "results.json"
    if not results_path.is_file():
        return _emit_error(args, AbError(f"no results.json at {results_path}"))
    results = json.loads(results_path.read_text())
    status = _status_from_value(str(results.get("status", "")))
    payload = {
        "status": status.value,
        "exit_code": status.exit_code,
        "experiment_id": results.get("experiment_id", args.experiment_id),
        "report": str(results_path.parent),
        "results": results,
    }
    text = f"{args.experiment_id}: {status.value} (exit {status.exit_code})"
    return _emit(args, payload, text, status)


def cmd_evaluate(args: argparse.Namespace) -> int:
    results_path = Path(args.results).resolve()
    if not results_path.is_file():
        return _emit_error(args, AbError(f"no results file at {results_path}"))
    results = json.loads(results_path.read_text())
    profile = _profile(args)
    gates = [_gate_from_dict(entry) for entry in results.get("gates", [])]
    recomputed = _combine_required(profile, gates)
    recorded = _status_from_value(str(results.get("status", "")))
    consistent = recomputed is recorded
    payload = {
        "status": recomputed.value,
        "exit_code": recomputed.exit_code,
        "consistent_with_recorded": consistent,
        "recorded_status": recorded.value,
        "gates": [gate.as_dict() for gate in gates],
    }
    text = (
        f"recomputed {recomputed.value} (exit {recomputed.exit_code}); "
        f"recorded {recorded.value}; {'consistent' if consistent else 'INCONSISTENT'}"
    )
    if not consistent:
        return _emit(args, payload, text, Status.BLOCKED)
    return _emit(args, payload, text, recomputed)


# --- shared helpers ----------------------------------------------------------


def _build_context(args: argparse.Namespace) -> Context:
    repo = _resolve_repo(args)
    quality_dir = (
        Path(args.quality_dir).resolve() if args.quality_dir else _resolve_quality_dir(repo)
    )
    region_map = load_regions(quality_dir / "regions.yaml")
    contracts = load_contracts(quality_dir / "contracts.yaml", set(region_map.regions))
    budgets = load_budgets(quality_dir / "budgets.yaml")
    known_issues = load_known_issues(quality_dir / "known_issues.yaml", set(region_map.regions))
    runner = DockerRunner()
    runner.check()
    workspace = _resolve_workspace(args)
    workspace.mkdir(parents=True, exist_ok=True)
    (workspace / "logs").mkdir(parents=True, exist_ok=True)
    cache_dir = _resolve_cache_dir(args)
    cache_dir.mkdir(parents=True, exist_ok=True)
    return Context(
        repo=repo,
        quality_dir=quality_dir,
        workspace=workspace,
        cache_dir=cache_dir,
        keep_workspace=bool(getattr(args, "keep_workspace", False)),
        region_map=region_map,
        contracts=contracts,
        budgets=budgets,
        known_issues=known_issues,
        runner=runner,
    )


def _resolve_repo(args: argparse.Namespace) -> Path:
    if getattr(args, "repo", None):
        return Path(args.repo).resolve()
    return find_repo(Path.cwd())


def _resolve_quality_dir(repo: Path) -> Path:
    candidate = repo.joinpath(*DEFAULT_QUALITY_SUBDIR)
    if candidate.is_dir():
        return candidate
    return Path(__file__).resolve().parents[2] / "quality"


def _resolve_workspace(args: argparse.Namespace) -> Path:
    if getattr(args, "workspace", None):
        return Path(args.workspace).resolve()
    base = Path("/tmp/opencode") if Path("/tmp/opencode").is_dir() else Path(tempfile.gettempdir())
    return Path(tempfile.mkdtemp(prefix="x6200-ab-", dir=base))


def _resolve_cache_dir(args: argparse.Namespace) -> Path:
    if getattr(args, "cache_dir", None):
        return Path(args.cache_dir).resolve()
    root = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return root / "x6200-ab"


def _materialize(context: Context, manager: RevisionManager, ref: str, label: str) -> Revision:
    revision = manager.materialize(ref, label)
    HarnessBuilder(context.runner, context.workspace / "logs").build(revision)
    return revision


def _compute_diffs(
    a_by: dict[str, Capture], b_by: dict[str, Capture], context: Context
) -> list[DiffResult]:
    diffs_dir = context.workspace / "diffs"
    diffs_dir.mkdir(parents=True, exist_ok=True)
    diffs: list[DiffResult] = []
    for name in sorted(set(a_by) & set(b_by)):
        diffs.append(
            compute_diff(
                a_by[name],
                b_by[name],
                context.region_map,
                pixel_tolerance=context.budgets.visual.pixel_tolerance,
                min_box_pixels=context.budgets.visual.min_box_pixels,
                diff_png=diffs_dir / f"{name}-diff.png",
                overlay_png=diffs_dir / f"{name}-overlay.png",
            )
        )
    return diffs


def _capture_dict(capture: Capture) -> dict[str, Any]:
    return {
        "scenario": capture.name,
        "png": str(capture.png),
        "png_sha256": capture.png_sha256,
        "width": capture.width,
        "height": capture.height,
        "flushes": capture.flushes(),
        "flushed_pixels": capture.flushed_pixels(),
        "coverage": capture.coverage,
    }


def _combine_required(profile: GateProfile, gates: list[GateResult]) -> Status:
    return overall(profile, gates)


def _gate_from_dict(entry: dict[str, Any]) -> GateResult:
    name = _gate_name(str(entry.get("gate", "")))
    return GateResult(
        name=name,
        status=_status_from_value(str(entry.get("status", ""))),
        summary=str(entry.get("summary", "")),
        required=bool(entry.get("required", True)),
        evidence=tuple(str(item) for item in entry.get("evidence", [])),
        details=dict(entry.get("details", {})),
    )


def _gate_name(value: str) -> GateName:
    for gate in ALL_GATES:
        if gate.value == value:
            return gate
    raise AbError(f"unknown gate `{value}` in results")


def _status_from_value(value: str) -> Status:
    for status in Status:
        if status.value == value:
            return status
    raise AbError(f"unknown status `{value}`")


def _new_experiment_id() -> str:
    return "AB-" + datetime.now().strftime("%Y%m%d-%H%M%S")


def _teardown(context: Context, manager: RevisionManager) -> None:
    manager.cleanup()
    if not context.keep_workspace:
        shutil.rmtree(context.workspace, ignore_errors=True)


def _emit(args: argparse.Namespace, payload: dict[str, Any], text: str, status: Status) -> int:
    if getattr(args, "json", False):
        sys.stdout.write(json.dumps(payload, indent=2, default=str) + "\n")
    else:
        sys.stdout.write(text.rstrip() + "\n")
    return status.exit_code


def _emit_error(args: argparse.Namespace, error: AbError) -> int:
    payload: dict[str, Any] = {
        "status": Status.BLOCKED.value,
        "exit_code": Status.BLOCKED.exit_code,
        "error": str(error),
    }
    log = getattr(error, "log", "")
    if log:
        payload["log_tail"] = log
    if getattr(args, "json", False):
        sys.stdout.write(json.dumps(payload, indent=2, default=str) + "\n")
    else:
        sys.stderr.write(f"BLOCKED: {error}\n")
        if log:
            sys.stderr.write(log + "\n")
    return Status.BLOCKED.exit_code


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
