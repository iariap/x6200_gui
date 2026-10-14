"""End-to-end tests: real Docker build, real harness, the three milestone cases.

Marked `e2e` because they build the C harness with Docker and create ephemeral
git revisions. Run them with `pytest -m e2e`. They are skipped when Docker or
the `x6200-uiharness` image is missing.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from collections.abc import Callable, Iterator
from dataclasses import dataclass
from pathlib import Path

import pytest

from x6200_ab.cli import main
from x6200_ab.docker import HARNESS_IMAGE

REPO_ROOT = Path(__file__).resolve().parents[3]
FIXTURE = "home-usb-20m"
BENCH_FIXTURE = "bench-render"

pytestmark = pytest.mark.e2e


@dataclass(frozen=True)
class Revisions:
    """The base commit plus the ephemeral ones the tests compare."""

    base: str
    regression: str
    defect: str
    bench_regression: str
    cache_dir: Path


def _git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], capture_output=True, text=True, check=False)


def _docker_ready() -> bool:
    if shutil.which("docker") is None:
        return False
    probe = subprocess.run(
        ["docker", "image", "inspect", HARNESS_IMAGE],
        capture_output=True,
        text=True,
        check=False,
    )
    return probe.returncode == 0


def _seed_cache(cache_dir: Path) -> None:
    """Copy the repo's checked-out lvgl into the cache so tests stay offline."""
    lvgl = REPO_ROOT / "lvgl"
    if not (lvgl / "CMakeLists.txt").exists():
        return
    listing = _git("-C", str(REPO_ROOT), "ls-files", "-s", "--", "lvgl").stdout.split()
    if len(listing) < 2:
        return
    target = cache_dir / f"lvgl-{listing[1]}"
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(lvgl, target, symlinks=True)


def _ephemeral(repo: Path, base: str, slug: str, mutate: Callable[[Path], None]) -> str:
    """Commit a one-off patch on a throwaway branch and return its SHA."""
    scratch = Path(tempfile.mkdtemp(prefix=f"abtest-{slug}-"))
    try:
        _git("-C", str(repo), "worktree", "add", "--detach", str(scratch), base)
        mutate(scratch)
        _git("-C", str(scratch), "add", "-A")
        committed = _git("-C", str(scratch), "commit", "-m", f"abtest: {slug} (ephemeral)")
        assert committed.returncode == 0, committed.stderr
        sha = _git("-C", str(scratch), "rev-parse", "HEAD").stdout.strip()
        _git("-C", str(repo), "branch", "-f", f"abtest/{slug}", sha)
        return sha
    finally:
        _git("-C", str(repo), "worktree", "remove", "--force", str(scratch))
        shutil.rmtree(scratch, ignore_errors=True)


def _shift_button_bar(worktree: Path) -> None:
    """Deliberate regression: push the bottom button bar past the screen edge."""
    path = worktree / "src" / "buttons.cpp"
    text = path.read_text()
    needle = "    uint16_t y = 480 - btn_height;"
    assert needle in text
    path.write_text(text.replace(needle, "    uint16_t y = 480 - btn_height + 30;"))


def _show_multiplier_label(worktree: Path) -> None:
    """Deliberate defect: stop hiding the tuning-multiplier label (UI-001)."""
    path = worktree / "src" / "main_screen.c"
    text = path.read_text()
    needle = "    lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);"
    assert needle in text
    path.write_text(text.replace(needle, "    /* abtest e2e: label left visible */"))


def _raise_bench_cost(worktree: Path) -> None:
    """Deliberate render regression: repaint the screen three times per frame.

    `bench_redraw_passes` is a benchmark-only knob (it never changes the PNG):
    the same screen is redrawn three times, so the frame's render time, flush
    count and redrawn area all grow. This is the controlled cost regression the
    benchmark has to detect.
    """
    path = worktree / "tools" / "ui_harness" / "fixtures" / f"{BENCH_FIXTURE}.scn"
    text = path.read_text()
    needle = "bench_redraw_passes 1"
    assert needle in text
    path.write_text(text.replace(needle, "bench_redraw_passes 3"))


def _base_revision() -> str:
    """The commit the demo revisions are built from.

    `X6200_AB_E2E_BASE` pins it (the committed evidence uses the harness commit
    `cba8dad`, so it stays valid as later commits land on top); by default it is
    the repository's current HEAD.
    """
    override = os.environ.get("X6200_AB_E2E_BASE")
    if override:
        return override
    return _git("-C", str(REPO_ROOT), "rev-parse", "HEAD").stdout.strip()


@pytest.fixture(scope="session")
def revisions(tmp_path_factory: pytest.TempPathFactory) -> Iterator[Revisions]:
    if not _docker_ready():
        pytest.skip(f"Docker image {HARNESS_IMAGE} is not available")
    base = _base_revision()
    cache_dir = tmp_path_factory.mktemp("ab-cache")
    _seed_cache(cache_dir)
    regression = _ephemeral(REPO_ROOT, base, "regression", _shift_button_bar)
    defect = _ephemeral(REPO_ROOT, base, "defect", _show_multiplier_label)
    bench_regression = _ephemeral(REPO_ROOT, base, "bench", _raise_bench_cost)
    _record_patches(
        base,
        {"regression": regression, "defect": defect, "bench": bench_regression},
    )
    try:
        yield Revisions(
            base=base,
            regression=regression,
            defect=defect,
            bench_regression=bench_regression,
            cache_dir=cache_dir,
        )
    finally:
        _git(
            "-C",
            str(REPO_ROOT),
            "branch",
            "-D",
            "abtest/regression",
            "abtest/defect",
            "abtest/bench",
        )
        _git("-C", str(REPO_ROOT), "worktree", "prune")


def _record_patches(base: str, revisions: dict[str, str]) -> None:
    """Keep the ephemeral revisions self-describing next to the committed report."""
    override = os.environ.get("X6200_AB_E2E_REPORTS")
    if not override:
        return
    destination = Path(override).resolve() / "revisions"
    destination.mkdir(parents=True, exist_ok=True)
    for slug, sha in revisions.items():
        diff = _git("-C", str(REPO_ROOT), "diff", base, sha)
        (destination / f"{slug}.patch").write_text(diff.stdout)


def _reports_root(tmp_path: Path) -> Path:
    """Where the report goes: the repo's `reports/` when asked, else a temp dir.

    `X6200_AB_E2E_REPORTS` lets the same tests produce the committed evidence in
    `tools/ab_testing/reports/` without duplicating the flow in a script.
    """
    override = os.environ.get("X6200_AB_E2E_REPORTS")
    return Path(override).resolve() if override else tmp_path


def _compare(
    tmp_path: Path, revisions: Revisions, a: str, b: str, experiment_id: str
) -> tuple[int, Path]:
    reports = _reports_root(tmp_path)
    workspace = tmp_path / f"ws-{experiment_id}"
    code = main(
        [
            "compare",
            "--a",
            a,
            "--b",
            b,
            "--repo",
            str(REPO_ROOT),
            "--scenario",
            FIXTURE,
            "--reports-dir",
            str(reports),
            "--workspace",
            str(workspace),
            "--cache-dir",
            str(revisions.cache_dir),
            "--experiment-id",
            experiment_id,
        ]
    )
    return code, reports / experiment_id


def _results(report_dir: Path) -> dict:
    return json.loads((report_dir / "results.json").read_text())


def test_a_vs_a_is_pass(tmp_path: Path, revisions: Revisions) -> None:
    code, report = _compare(tmp_path, revisions, revisions.base, revisions.base, "AB-e2e-aa")
    assert code == 0
    results = _results(report)
    assert results["status"] == "PASS"
    assert results["improvement"] is False
    assert all(diff["identical"] for diff in results["diffs"])
    assert (report / "report.md").is_file()
    assert (report / "report.html").is_file()
    assert (report / "screenshots" / f"a-{FIXTURE}.png").is_file()
    assert (report / "screenshots" / f"b-{FIXTURE}.png").is_file()


def test_regression_is_detected_with_evidence(tmp_path: Path, revisions: Revisions) -> None:
    code, report = _compare(
        tmp_path, revisions, revisions.base, revisions.regression, "AB-e2e-regression"
    )
    assert code == 1
    results = _results(report)
    assert results["status"] == "FAIL"
    visual = next(gate for gate in results["gates"] if gate["gate"] == "Visual Regression")
    assert visual["status"] == "FAIL"
    diff = next(item for item in results["diffs"] if item["scenario"] == FIXTURE)
    assert diff["changed_pixels"] > 0
    assert diff["boxes"], "a bounding box is required as evidence"
    changed_regions = [r["region"] for r in diff["regions"] if r["changed_pixels"]]
    assert "botones" in changed_regions
    assert (report / "diffs" / f"{FIXTURE}-diff.png").is_file()
    assert (report / "diffs" / f"{FIXTURE}-overlay.png").is_file()


def test_fix_is_an_improvement(tmp_path: Path, revisions: Revisions) -> None:
    # A is the defective revision, B restores the correct screen.
    code, report = _compare(tmp_path, revisions, revisions.defect, revisions.base, "AB-e2e-fix")
    assert code == 0
    results = _results(report)
    assert results["status"] == "PASS"
    assert results["improvement"] is True
    issue = next(item for item in results["known_issues"] if item["id"] == "UI-001")
    assert issue["reproduced_a"] is True
    assert issue["reproduced_b"] is False
    assert issue["classification"] == "FIXED"


def test_regression_of_a_fixed_issue_is_a_regression(tmp_path: Path, revisions: Revisions) -> None:
    # A is the clean base, B re-shows a defect that was fixed -> REGRESSION.
    code, report = _compare(
        tmp_path, revisions, revisions.base, revisions.defect, "AB-e2e-reappear"
    )
    assert code == 1
    results = _results(report)
    assert results["status"] == "FAIL"
    issue = next(item for item in results["known_issues"] if item["id"] == "UI-001")
    assert issue["classification"] == "REGRESSION"


def test_all_fixtures_can_be_captured(tmp_path: Path, revisions: Revisions) -> None:
    out = _reports_root(tmp_path) / "captures-e2e"
    code = main(
        [
            "capture",
            "--revision",
            revisions.base,
            "--repo",
            str(REPO_ROOT),
            "--out",
            str(out),
            "--workspace",
            str(tmp_path / "ws-capture"),
            "--cache-dir",
            str(revisions.cache_dir),
            "--json",
        ]
    )
    assert code == 0
    assert len(sorted(out.glob("*.png"))) == 7
    assert len(sorted(out.glob("*.json"))) == 7


def _run(tmp_path: Path, revisions: Revisions, extra: list[str]) -> int:
    return main(
        [
            *extra,
            "--repo",
            str(REPO_ROOT),
            "--workspace",
            str(tmp_path / "ws-extra"),
            "--cache-dir",
            str(revisions.cache_dir),
            "--json",
        ]
    )


def test_audit_reports_the_missing_instrumentation(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    code = _run(
        tmp_path,
        revisions,
        ["audit", "--revision", revisions.base, "--scenario", FIXTURE],
    )
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["issues"], "the audit must evaluate the registered issues"
    assert "structural inspection" in payload["blocked_instrumentation"]


def test_benchmark_measures_the_real_render_cost(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    code = _run(
        tmp_path,
        revisions,
        [
            "benchmark",
            "--revision",
            revisions.base,
            "--scenario",
            BENCH_FIXTURE,
            "--repeats",
            "5",
            "--warmup",
            "3",
            "--rounds",
            "3",
        ],
    )
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "PASS"
    assert payload["kind"] == "benchmark-measurement"
    measurement = payload["measurements"][0]
    assert measurement["scenario"] == BENCH_FIXTURE
    assert measurement["frames"] == 15
    assert measurement["render_p50_ms"] > 0.0
    assert measurement["render_p95_ms"] >= measurement["render_p50_ms"]
    assert measurement["render_p99_ms"] >= measurement["render_p95_ms"]
    assert measurement["fps"] > 0.0
    assert measurement["flushes_mean"] >= 1.0
    assert measurement["redrawn_area_mean"] > 0.0
    assert measurement["memory"]["rss_bytes"] > 0


def test_benchmark_detects_a_controlled_regression(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """The milestone gate: a deliberate 3x redraw revision FAILs on p95."""
    reports = _reports_root(tmp_path)
    workspace = tmp_path / "ws-benchmark"
    code = main(
        [
            "benchmark",
            "--a",
            revisions.base,
            "--b",
            revisions.bench_regression,
            "--scenario",
            BENCH_FIXTURE,
            "--repeats",
            "5",
            "--warmup",
            "3",
            "--rounds",
            "3",
            "--repo",
            str(REPO_ROOT),
            "--reports-dir",
            str(reports),
            "--workspace",
            str(workspace),
            "--cache-dir",
            str(revisions.cache_dir),
            "--experiment-id",
            "AB-e2e-benchmark",
            "--json",
        ]
    )
    assert code == 1
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "FAIL"
    comparison = payload["comparisons"][0]
    assert comparison["scenario"] == BENCH_FIXTURE
    assert comparison["status"] == "FAIL"
    assert comparison["p95_delta_pct"] > 100.0
    assert comparison["b"]["render_p95_ms"] > comparison["a"]["render_p95_ms"]
    assert comparison["b"]["flushes_mean"] > comparison["a"]["flushes_mean"]
    assert any("p95 render" in reason for reason in comparison["reasons"])
    results = reports / "AB-e2e-benchmark" / "results.json"
    assert results.is_file()
    stored = json.loads(results.read_text())
    assert stored["status"] == "FAIL"
    assert (reports / "AB-e2e-benchmark" / "report.md").is_file()
