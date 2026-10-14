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
from .capture import Capture, Capturer
from .contracts import run_contracts
from .docker import DockerRunner
from .errors import AbError, DockerUnavailable
from .evaluator import ComparisonData, evaluate, observe_issues
from .known_issues import KnownIssue, evaluate_detector, load_known_issues
from .quality_config import Budgets, QualityContracts, load_budgets, load_contracts
from .quality_gates import ALL_GATES, GateName, GateProfile, GateResult, overall
from .regions import RegionMap, load_regions
from .reports import ReportMetadata, write_report
from .revisions import Revision, RevisionManager, find_repo
from .scenarios import ScenarioSuite
from .status import Status
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
    capture.add_argument("--out", required=True, help="directory for the PNG + JSON captures")
    capture.set_defaults(func=cmd_capture)

    audit = sub.add_parser("audit", help="check the known-issue detectors on one revision")
    _common(audit)
    audit.add_argument("--revision", required=True)
    audit.add_argument("--scenario", action="append", default=[])
    audit.set_defaults(func=cmd_audit)

    compare = sub.add_parser("compare", help="A/B compare two revisions and write a report")
    _common(compare)
    compare.add_argument("--a", required=True, help="baseline rev")
    compare.add_argument("--b", required=True, help="candidate rev")
    compare.add_argument("--scenario", action="append", default=[])
    compare.add_argument("--experiment-id", default=None)
    compare.add_argument("--reports-dir", default=None)
    compare.set_defaults(func=cmd_compare)

    benchmark = sub.add_parser("benchmark", help="render-cost repetitions for one revision")
    _common(benchmark)
    benchmark.add_argument("--revision", required=True)
    benchmark.add_argument("--scenario", action="append", default=[])
    benchmark.add_argument("--repeats", type=int, default=3)
    benchmark.set_defaults(func=cmd_benchmark)

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
            revision, suite, out_dir, context.workspace / "logs"
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
    context = _build_context(args)
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    try:
        revision = _materialize(context, manager, args.revision, "audit")
        suite = ScenarioSuite.from_fixtures_dir(revision.fixtures_dir, args.scenario)
        out_dir = context.workspace / "captures" / revision.short_sha
        captures = {
            capture.name: capture
            for capture in Capturer(context.runner).capture_all(
                revision, suite, out_dir, context.workspace / "logs"
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
                        "detail": "unverified: no detector yet",
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
        payload = {
            "status": Status.PASS.value,
            "exit_code": Status.PASS.exit_code,
            "revision": revision.sha,
            "scenarios": suite.names,
            "reproduced": reproduced,
            "unverified": unverified,
            "issues": results,
            "blocked_instrumentation": (
                "structural inspection (object bounding boxes, glyphs, overflow) is not "
                "emitted by the harness yet; this audit is pixel-region only"
            ),
        }
        text = (
            f"audited {revision.short_sha}: {len(results)} issues — "
            f"{len(reproduced)} reproduced, {len(unverified)} unverified; "
            "structural inspection (object bounds, glyphs) BLOCKED: the harness does not "
            "emit it yet, so this audit is pixel-region only"
        )
        return _emit(args, payload, text, Status.PASS)
    finally:
        _teardown(context, manager)


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
        a_captures = capturer.capture_all(
            revision_a,
            suite,
            context.workspace / "captures" / f"a-{revision_a.short_sha}",
            context.workspace / "logs",
        )
        b_captures = capturer.capture_all(
            revision_b,
            suite,
            context.workspace / "captures" / f"b-{revision_b.short_sha}",
            context.workspace / "logs",
        )
        a_by = {capture.name: capture for capture in a_captures}
        b_by = {capture.name: capture for capture in b_captures}
        diffs = _compute_diffs(a_by, b_by, context)
        issues = observe_issues(context.known_issues, a_by, b_by, context.region_map)
        findings = run_contracts(b_captures, context.region_map, context.contracts)
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
        }
        text = _compare_text(evaluation, paths.root)
        return _emit(args, payload, text, evaluation.status)
    finally:
        _teardown(context, manager)


def _compare_text(evaluation: Any, report_root: Path) -> str:
    lines = [
        f"experiment {evaluation.experiment_id}: {evaluation.status.value} "
        f"(exit {evaluation.status.exit_code})"
    ]
    if evaluation.improvement:
        lines.append("IMPROVEMENT: a known issue present in A is fixed in B")
    for gate in evaluation.gates:
        lines.append(
            f"  {gate.name.value:18s} {gate.status.value:15s} "
            f"{'(required)' if gate.required else '(advisory)'} — {gate.summary}"
        )
    lines.append(f"report: {report_root}")
    return "\n".join(lines)


# --- benchmark ---------------------------------------------------------------


def cmd_benchmark(args: argparse.Namespace) -> int:
    context = _build_context(args)
    manager = RevisionManager(context.repo, context.workspace, context.cache_dir)
    try:
        revision = _materialize(context, manager, args.revision, "bench")
        suite = ScenarioSuite.from_fixtures_dir(revision.fixtures_dir, args.scenario)
        capturer = Capturer(context.runner)
        runs: dict[str, list[dict[str, Any]]] = {name: [] for name in suite.names}
        for repeat in range(max(1, args.repeats)):
            out_dir = context.workspace / "bench" / f"{revision.short_sha}-{repeat}"
            for capture in capturer.capture_all(
                revision, suite, out_dir, context.workspace / "logs"
            ):
                runs[capture.name].append(
                    {
                        "flushes": capture.flushes(),
                        "flushed_pixels": capture.flushed_pixels(),
                        "framebuffer_fnv1a": capture.framebuffer_hash,
                        "png_sha256": capture.png_sha256,
                    }
                )
        payload = {
            "status": Status.BLOCKED.value,
            "exit_code": Status.BLOCKED.exit_code,
            "revision": revision.sha,
            "cause": (
                "frame timing, FPS and p50/p95/p99 are not instrumented: the harness only "
                "exposes render counters under virtual time, which are not wall-clock "
                "durations. Energy cannot be derived from them either."
            ),
            "available": runs,
        }
        text = (
            f"benchmark of {revision.short_sha}: BLOCKED — no frame-timing instrumentation; "
            f"render counters collected for {len(runs)} scenario(s)"
        )
        return _emit(args, payload, text, Status.BLOCKED)
    finally:
        _teardown(context, manager)


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
