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
from x6200_ab.regions import load_regions

REPO_ROOT = Path(__file__).resolve().parents[3]
FIXTURE = "home-usb-20m"
BENCH_FIXTURE = "bench-render"
CLOCK_FIXTURE = "home-bateria-externa"

pytestmark = pytest.mark.e2e


@dataclass(frozen=True)
class Revisions:
    """The base commit plus the ephemeral ones the tests compare."""

    base: str
    regression: str
    defect: str
    clock_defect: str
    style_defect: str
    theme_defect: str
    bench_regression: str
    structural: str
    focus_defect: str
    cache_dir: Path


def _git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", *args], capture_output=True, text=True, check=False)


# Identity for the throwaway revisions: the Actions runner has no user.name or
# user.email configured and these commits are never pushed, so they carry a
# scaffolding identity instead of the project's. The repository rule about the
# owner's identity applies to real project commits, not to these self-test ones.
EPHEMERAL_IDENTITY = (
    "-c",
    "user.name=ab-test-ephemeral",
    "-c",
    "user.email=ab-test-ephemeral@example.invalid",
)


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
        committed = _git(
            "-C",
            str(scratch),
            *EPHEMERAL_IDENTITY,
            "commit",
            "-m",
            f"abtest: {slug} (ephemeral)",
        )
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


def _shift_button_bar_and_show_multiplier(worktree: Path) -> None:
    """Deliberate regression, in two parts.

    The pixel shift alone no longer fails the visual gate: the font migration
    (issue #9 / decision 1) declares every region text is drawn in
    (`quality/contracts.yaml`, `expected_changes` AB-FONTS), so a change *inside*
    one of those regions is explained by the declaration — that is what the
    declarations say. What the corpus must still fail, and what this revision
    exercises, is a change that introduces a defect: the same shift plus the
    tuning-multiplier label left visible (UI-001). The evidence assertions of the
    test (bounding boxes, region attribution, diff/overlay PNGs) are unchanged.
    """
    _shift_button_bar(worktree)
    _show_multiplier_label(worktree)


def _show_multiplier_label(worktree: Path) -> None:
    """Deliberate defect: stop hiding the tuning-multiplier label (UI-001)."""
    path = worktree / "src" / "main_screen.c"
    text = path.read_text()
    needle = "    lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);"
    assert needle in text
    path.write_text(text.replace(needle, "    /* abtest e2e: label left visible */"))


def _reintroduce_clock_defect(worktree: Path) -> None:
    """Deliberate defect: drop the battery symbol's initialization (UI-009).

    The fix initialised `bat_sym`; putting back the bare declaration reproduces
    the uninitialized-pointer read of `show_time()` on the `home-bateria-externa`
    fixture (12.1 V > 10.2 V, outside the switch's 0..100 range).
    """
    path = worktree / "src" / "clock.c"
    text = path.read_text()
    needle = "const char * bat_sym = LV_SYMBOL_BATTERY_FULL;"
    assert needle in text
    path.write_text(text.replace(needle, "const char * bat_sym;"))


def _reintroduce_frequency_white(worktree: Path) -> None:
    """Deliberate defect: put the frequency row back on a hard-coded white (UI-010).

    The fix made the recolor markup of the three labels ask the theme for their
    colour (`theme_hex(TC_text)`, `#%06X`); writing the white back is exactly the
    defect that under the light Outdoor theme painted the digits white on the
    white background, so the region measured content 0.0000.
    """
    path = worktree / "src" / "main_screen.c"
    text = path.read_text()
    needle = "uint32_t    color = theme_hex(subject_get_int(freq_lock) ? TC_text_soft : TC_text);"
    assert text.count(needle) == 2, "the two frequency-row paths must carry the theme role"
    text = text.replace(
        needle,
        "uint32_t    color = subject_get_int(freq_lock) ? 0xBBBBBB : 0xFFFFFF;",
    )
    for fmt in ('"#%06X %i.%03i.%03i / %i.%03i.%03i"', '"#%06X %i.%03i.%03i"'):
        assert fmt in text, fmt
        text = text.replace(fmt, fmt.replace("%06X", "%03X"))
    path.write_text(text)


def _reintroduce_theme_truncation(worktree: Path) -> None:
    """Deliberate defect: put the theme-name store back to 16 bytes (UI-013).

    The fix widened `params_str_t.x` (src/params/types.h) to the 31 characters
    `theme_list()` admits, so "Outdoor-Negative" survives the store. Shrinking it
    back to the 16 bytes of the defect truncates it to "Outdoor-Negativ" and
    `theme_name_fits_params` fires again on `main-tema-outdoor-negative`: the
    contract, with no waiver left, turns that into a plain failure.
    """
    path = worktree / "src" / "params" / "types.h"
    text = path.read_text()
    needle = "    char        x[32];"
    assert needle in text, "the width fix must be in the base revision"
    path.write_text(text.replace(needle, "    char        x[16];", 1))


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


def _inject_structural_defects(worktree: Path) -> None:
    """Deliberate structural defects, all in the real top bar (`src/info.c`):

    * the mode label of a fixed 56x26 cell holds a much longer text, so it wraps
      past the cell (overflowing text);
    * the WiFi cell draws U+F293, a glyph that no font in the chain has, so LVGL
      paints a placeholder box (missing glyph);
    * the WiFi cell paints its text with the cell's own background colour, so the
      pair is illegible (contrast 1:1).
    """
    path = worktree / "src" / "info.c"
    text = path.read_text()

    mode = "lv_label_set_text(items[INFO_MODE], info_params_mode_label_get());"
    assert mode in text
    text = text.replace(mode, 'lv_label_set_text(items[INFO_MODE], "VERYLONGMODENAME");')

    glyph = "lv_label_set_text(items[INFO_WIFI], LV_SYMBOL_WIFI);"
    assert glyph in text
    text = text.replace(glyph, 'lv_label_set_text(items[INFO_WIFI], "\\xEF\\x8A\\x93");')

    contrast = "lv_obj_set_style_text_color(items[INFO_WIFI], TC(text_dim), 0);"
    assert contrast in text
    text = text.replace(contrast, "lv_obj_set_style_text_color(items[INFO_WIFI], TC(bar), 0);")

    path.write_text(text)


def _break_focus_group(worktree: Path) -> None:
    """Deliberate defect: stop registering Ajustes widgets in the focus group (UI-004).

    `dialog_item()` (src/dialog.c) is the only thing that adds a control to
    `keyboard_group`; with the call removed every Ajustes switch/slider/dropdown is
    still drawn but the keys and the MFK cannot reach it, which is the defect of
    docs/HALLAZGOS.md 18. The rendered detector `layout_focus_unreachable` must fire
    on the settings fixture.
    """
    path = worktree / "src" / "dialog.c"
    text = path.read_text()
    needle = "    lv_group_add_obj(keyboard_group, obj);"
    assert needle in text
    path.write_text(text.replace(needle, "    /* abtest e2e: focus group not registered */"))


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
    regression = _ephemeral(REPO_ROOT, base, "regression", _shift_button_bar_and_show_multiplier)
    defect = _ephemeral(REPO_ROOT, base, "defect", _show_multiplier_label)
    clock_defect = _ephemeral(REPO_ROOT, base, "clock", _reintroduce_clock_defect)
    style_defect = _ephemeral(REPO_ROOT, base, "style", _reintroduce_frequency_white)
    theme_defect = _ephemeral(REPO_ROOT, base, "theme", _reintroduce_theme_truncation)
    bench_regression = _ephemeral(REPO_ROOT, base, "bench", _raise_bench_cost)
    structural = _ephemeral(REPO_ROOT, base, "structural", _inject_structural_defects)
    focus_defect = _ephemeral(REPO_ROOT, base, "focus", _break_focus_group)
    _record_patches(
        base,
        {
            "regression": regression,
            "defect": defect,
            "clock": clock_defect,
            "style": style_defect,
            "theme": theme_defect,
            "bench": bench_regression,
            "structural": structural,
            "focus": focus_defect,
        },
    )
    try:
        yield Revisions(
            base=base,
            regression=regression,
            defect=defect,
            clock_defect=clock_defect,
            style_defect=style_defect,
            theme_defect=theme_defect,
            bench_regression=bench_regression,
            structural=structural,
            focus_defect=focus_defect,
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
            "abtest/clock",
            "abtest/style",
            "abtest/theme",
            "abtest/bench",
            "abtest/structural",
            "abtest/focus",
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
    tmp_path: Path,
    revisions: Revisions,
    a: str,
    b: str,
    experiment_id: str,
    fixture: str = FIXTURE,
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
            fixture,
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


def test_ui009_battery_symbol_fix_is_an_improvement(tmp_path: Path, revisions: Revisions) -> None:
    """A is the clock with the uninitialized symbol, B the fix -> IMPROVEMENT.

    This is the first real firmware fix evaluated by the harness: on the
    `home-bateria-externa` fixture (12.1 V on an external supply) A prints the
    garbage first glyph of the uninitialized pointer and B prints a battery
    icon; the `leading_glyph_range` detector records UI-009 as FIXED.
    """
    code, report = _compare(
        tmp_path,
        revisions,
        revisions.clock_defect,
        revisions.base,
        "AB-e2e-ui009",
        fixture=CLOCK_FIXTURE,
    )
    assert code == 0
    results = _results(report)
    assert results["status"] == "PASS"
    assert results["improvement"] is True
    issue = next(item for item in results["known_issues"] if item["id"] == "UI-009")
    assert issue["reproduced_a"] is True
    assert issue["reproduced_b"] is False
    assert issue["classification"] == "FIXED"
    # Evidence: the A/B screenshots and the diff are kept with the report.
    assert (report / "screenshots" / f"a-{CLOCK_FIXTURE}.png").is_file()
    assert (report / "screenshots" / f"b-{CLOCK_FIXTURE}.png").is_file()
    assert (report / "diffs" / f"{CLOCK_FIXTURE}-diff.png").is_file()


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
    fixtures = REPO_ROOT / "tools" / "ui_harness" / "fixtures"
    expected = len(list(fixtures.glob("*.scn")))
    assert len(sorted(out.glob("*.png"))) == expected
    assert len(sorted(out.glob("*.json"))) == expected


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


def _record_audit(payload: dict, slug: str) -> None:
    """Keep the structural audit evidence next to the committed reports."""
    override = os.environ.get("X6200_AB_E2E_REPORTS")
    if not override:
        return
    destination = Path(override).resolve() / "AB-e2e-structural"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / f"audit-{slug}.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n"
    )


def test_audit_uses_the_real_instrumentation(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    # `--ref`/`--suite` are the aliases the plan's gate command uses.
    code = _run(
        tmp_path,
        revisions,
        ["audit", "--ref", revisions.base, "--suite", "visual", "--scenario", FIXTURE],
    )
    payload = json.loads(capsys.readouterr().out)
    assert code == 0
    assert payload["issues"], "the audit must evaluate the registered issues"
    assert payload["instrumentation"]["structural"] is True
    assert payload["instrumentation"]["nodes_inspected"] > 0
    assert payload["structural_failures"] == 0
    assert "blocked_instrumentation" not in payload
    _record_audit(payload, "base")


def test_audit_detects_a_clipped_element(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """The button bar pushed past the screen edge is a real structural defect."""
    code = _run(
        tmp_path,
        revisions,
        ["audit", "--revision", revisions.regression, "--scenario", FIXTURE],
    )
    payload = json.loads(capsys.readouterr().out)
    assert code == 1
    failed = {finding["rule"] for finding in payload["findings"] if not finding["ok"]}
    assert "layout_clipped" in failed
    _record_audit(payload, "clipped")


def test_audit_detects_text_glyph_and_contrast_defects(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """The three deliberate text defects of the milestone-2 self-test."""
    code = _run(
        tmp_path,
        revisions,
        ["audit", "--revision", revisions.structural, "--scenario", FIXTURE],
    )
    payload = json.loads(capsys.readouterr().out)
    assert code == 1
    failed = {finding["rule"] for finding in payload["findings"] if not finding["ok"]}
    assert {
        "typography_text_overflow",
        "typography_missing_glyph",
        "typography_contrast",
    } <= failed
    _record_audit(payload, "structural")


def test_audit_detects_a_widget_out_of_the_focus_group(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """UI-004: a control the keyboard cannot reach is a rendered finding.

    The ephemeral revision drops `lv_group_add_obj()` from `dialog_item()`
    (src/dialog.c), so every Ajustes switch/slider/dropdown is drawn but outside
    `keyboard_group`; `layout_focus_unreachable` must fire on the settings fixture
    (the same class of defect docs/HALLAZGOS.md 18 fixed).
    """
    code = _run(
        tmp_path,
        revisions,
        ["audit", "--revision", revisions.focus_defect, "--scenario", "settings-home"],
    )
    payload = json.loads(capsys.readouterr().out)
    assert code == 1
    failed = {
        (finding["rule"], finding["fixture"])
        for finding in payload["findings"]
        if not finding["ok"]
    }
    assert ("layout_focus_unreachable", "settings-home") in failed
    _record_audit(payload, "focus")


def test_contracts_pass_on_the_base(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    code = _run(
        tmp_path,
        revisions,
        ["contracts", "--revision", revisions.base, "--scenario", FIXTURE],
    )
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "PASS"
    assert payload["checks"] > 0
    assert payload["failures"] == []
    assert payload["unchanged_defects"] == []


def test_contracts_reject_the_outdoor_frequency_defect(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """UI-010 is fixed: the registry keeps no waiver, a reappearance fails.

    The ephemeral revision puts the hard-coded white of the frequency row back
    (`src/styles.c`), which under the light Outdoor theme renders the digits white
    on the white background. Nothing in the registry exempts the region any more
    (the UI-010 exclusion was withdrawn when the fix landed), so the contract is a
    plain failure with no baseline issue attached.
    """
    code = _run(
        tmp_path,
        revisions,
        ["contracts", "--revision", revisions.style_defect, "--scenario", "main-otro-tema"],
    )
    assert code == 1
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "FAIL"
    failure = next(
        item for item in payload["failures"] if item["rule"] == "region_min_content:frecuencia"
    )
    assert failure["fixture"] == "main-otro-tema"
    assert failure["baseline_issue"] is None
    assert payload["unchanged_defects"] == []


def test_contracts_pass_on_the_fixed_outdoor_frequency_row(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """The fixed frequency row keeps content: the contract and the registry agree."""
    code = _run(
        tmp_path,
        revisions,
        ["contracts", "--revision", revisions.base, "--scenario", "main-otro-tema"],
    )
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "PASS"
    assert payload["failures"] == []
    assert payload["unchanged_defects"] == []
    finding = next(
        item for item in payload["findings"] if item["rule"] == "region_min_content:frecuencia"
    )
    assert finding["ok"] is True


def test_contracts_reject_a_theme_the_store_truncates(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """UI-013 is fixed: a store that truncates the name fails, not silently.

    The ephemeral revision puts `params_str_t.x` back to the 16 bytes of the
    defect (src/params/types.h), so the harness' own store truncates
    "Outdoor-Negative" to "Outdoor-Negativ" while the fixture renders the whole
    name. Nothing waives `theme_name_fits_params` any more (the UI-013 exception
    was the last one and was withdrawn when the fix landed), so the contract is a
    plain failure: the defect cannot come back unnoticed.
    """
    code = _run(
        tmp_path,
        revisions,
        [
            "contracts",
            "--revision",
            revisions.theme_defect,
            "--scenario",
            "main-tema-outdoor-negative",
        ],
    )
    assert code == 1
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "FAIL"
    failure = next(item for item in payload["failures"] if item["rule"] == "theme_name_fits_params")
    assert failure["fixture"] == "main-tema-outdoor-negative"
    assert failure["baseline_issue"] is None
    assert "'Outdoor-Negativ'" in failure["detail"]
    assert payload["unchanged_defects"] == []


def test_contracts_pass_on_the_fixed_theme_store(
    tmp_path: Path, revisions: Revisions, capsys: pytest.CaptureFixture
) -> None:
    """The widened store keeps the whole name: the contract and the registry agree.

    On the fixed revision the fixture measures `requested == params_name` and no
    issue waives the rule, so the fixture that used to separate the two names now
    passes on its own.
    """
    code = _run(
        tmp_path,
        revisions,
        ["contracts", "--revision", revisions.base, "--scenario", "main-tema-outdoor-negative"],
    )
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "PASS"
    assert payload["failures"] == []
    assert payload["unchanged_defects"] == []
    finding = next(item for item in payload["findings"] if item["rule"] == "theme_name_fits_params")
    assert finding["ok"] is True
    assert "fits params.theme_name" in finding["detail"]


def test_ui010_fix_stays_inside_the_frequency_region(tmp_path: Path, revisions: Revisions) -> None:
    """A is the white frequency row, B the fix -> the only change is `frecuencia`.

    UI-010 has no pixel detector of its own (its detector is the contract rule, so
    A/B classifies it UNVERIFIED, never FIXED: the honest limit, the same one
    UI-011/UI-012 have). What the comparison does prove is the shape of the fix: on
    the Outdoor fixture the only pixels that move are the three labels of the
    frequency row, and the contract of B passes without any waiver.
    """
    code, report = _compare(
        tmp_path,
        revisions,
        revisions.style_defect,
        revisions.base,
        "AB-e2e-ui010",
        fixture="main-otro-tema",
    )
    assert code == 0
    results = _results(report)
    assert results["status"] == "PASS"
    diff = next(item for item in results["diffs"] if item["scenario"] == "main-otro-tema")
    assert diff["identical"] is False
    assert diff["changed_pixels"] > 0
    changed = [region["region"] for region in diff["regions"] if region["changed_pixels"] > 0]
    # `ajustes_audio_ganancia` (the Ajustes Audio row of issue #26) is the second
    # region the pixels fall in: its rectangle spans y=140..196, so it overlaps the
    # frequency band (y=160..196) the dialog is drawn over - the house regions may
    # overlap on purpose (regions.py), and on this fixture the Ajustes window is not
    # even open, so the row that region names is not on screen. What pins the *shape*
    # of the fix is every changed box, below, being inside the frequency row.
    assert changed == ["frecuencia", "ajustes_audio_ganancia"]

    region_map = load_regions(REPO_ROOT / "tools" / "ab_testing" / "quality" / "regions.yaml")
    frequency_region = region_map.get("frecuencia")
    assert diff["boxes"]
    for box in diff["boxes"]:
        assert frequency_region.contains((box["x0"], box["y0"], box["x1"], box["y1"])), box
    issue = next(item for item in results["known_issues"] if item["id"] == "UI-010")
    assert issue["classification"] == "UNVERIFIED"
    contract = results["contracts"]
    frequency = next(
        finding for finding in contract if finding["rule"] == "region_min_content:frecuencia"
    )
    assert frequency["ok"] is True
    # Evidence: the A/B screenshots and the diff are kept with the report.
    assert (report / "screenshots" / "a-main-otro-tema.png").is_file()
    assert (report / "screenshots" / "b-main-otro-tema.png").is_file()
    assert (report / "diffs" / "main-otro-tema-diff.png").is_file()


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
            "--reports-dir",
            str(tmp_path / "bench-reports"),
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
