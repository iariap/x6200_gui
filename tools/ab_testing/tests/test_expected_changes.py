"""`expected_changes` are scoped to fixtures: a rectangle is not a screen.

`quality/contracts.yaml` declares the visual changes that are known and
acceptable. Before #296 an entry named a *region* (a rectangle of the 800x480
screen) and the gate allowed that rectangle on every fixture, so an entry written
for the Ajustes dialog silently explained a change of the SWRScan dialog that is
drawn in the same box. The entry now carries the globs of the fixtures it covers:
the gate allows a region only on the fixtures its own entries name, and the report
prints which entry (reason, author, expiry) explained each change.

The tests below pin the three pieces: the loader refuses an entry without a scope
or with a scope that matches nothing, the gate counts pixels per fixture, and the
report names the entry — with a negative control that fails.
"""

from __future__ import annotations

from datetime import date
from pathlib import Path

import pytest

from helpers import (
    FIXTURE_NAMES,
    QUALITY_DIR,
    blank_image,
    budgets,
    contracts,
    draw_box,
    make_capture,
    revision,
)
from x6200_ab.errors import ConfigError
from x6200_ab.evaluator import Evaluation
from x6200_ab.quality_config import ExpectedChange, QualityContracts, load_contracts
from x6200_ab.quality_gates import FixtureAllowance, visual_regression_gate
from x6200_ab.regions import RegionMap
from x6200_ab.reports import ReportMetadata, ReportPaths, render_markdown
from x6200_ab.status import Status
from x6200_ab.visual_diff import DiffResult, compute_diff

# `ajustes_dialogo` is [2, 66, 796, 348]: the app dialog box, shared by the
# Ajustes grid and every other app window (the case of #296).
DIALOG_REGION = "ajustes_dialogo"
TODAY = date(2026, 10, 24)

# The two fixtures of the reproduction: the app whose change the entry declares
# and another app drawn in the same box that the entry never reviewed.
DECLARED = "swrscan-3pass-stop"
OTHER = "swrscan-quick"


def _change(
    region: str = DIALOG_REGION,
    fixtures: tuple[str, ...] = (DECLARED,),
    *,
    until: str = "2027-04-30",
    author: str = "ui-polish-app-copy-284",
    reason: str = "AB-APCOPY288 (#288): the SWRScan status line goes to English\n",
) -> ExpectedChange:
    return ExpectedChange(
        region=region, fixtures=fixtures, reason=reason, author=author, until=until
    )


def _diff(tmp_path: Path, region_map: RegionMap, fixture: str) -> DiffResult:
    """A one-box change inside the dialog box of `fixture`."""
    base = blank_image()
    before = make_capture(tmp_path / f"{fixture}-a", fixture, base, rev=revision("a", "a" * 40))
    after = make_capture(
        tmp_path / f"{fixture}-b",
        fixture,
        draw_box(base, 122, 77, 30, 14, (240, 240, 240)),
        rev=revision("b", "b" * 40),
    )
    return compute_diff(
        before,
        after,
        region_map,
        pixel_tolerance=budgets().visual.pixel_tolerance,
        min_box_pixels=1,
    )


def _allowance(changes: tuple[ExpectedChange, ...], fixture: str) -> FixtureAllowance:
    allowed = tuple(change for change in changes if change.covers(fixture))
    return FixtureAllowance(regions=frozenset(change.region for change in allowed), changes=allowed)


def _gate(diff: DiffResult, region_map: RegionMap, changes: tuple[ExpectedChange, ...]):
    allowance = _allowance(changes, diff.scenario)
    return visual_regression_gate(
        [diff],
        region_map,
        allowances={diff.scenario: allowance} if allowance.regions else {},
        introduced_issues=[],
        review_regions=frozenset(),
    )


def _evaluation(diff: DiffResult, gate) -> Evaluation:
    return Evaluation(
        experiment_id="AB-test",
        date=TODAY.isoformat(),
        profile="host-ui",
        revision_a=revision("a", "a" * 40),
        revision_b=revision("b", "b" * 40),
        scenario_names=(diff.scenario,),
        diffs=(diff,),
        issues=(),
        contract_findings=(),
        gates=(gate,),
        status=Status.PASS,
        improvement=False,
        captures={"a": [], "b": []},
    )


# --- the loader --------------------------------------------------------------


def _write(tmp_path: Path, body: str) -> Path:
    path = tmp_path / "contracts.yaml"
    path.write_text("expected_changes:\n" + body)
    return path


def test_the_loader_reads_the_fixture_globs(tmp_path: Path) -> None:
    path = _write(
        tmp_path,
        "  - region: ajustes_dialogo\n"
        "    fixtures: [settings-*, bt-dialog-*]\n"
        "    reason: r\n"
        "    author: a\n"
        "    until: 2027-04-30\n",
    )
    loaded = load_contracts(path, {DIALOG_REGION}, {"settings-home", "bt-dialog-tema-aurora"})
    entry = loaded.expected_changes[0]
    assert entry.fixtures == ("settings-*", "bt-dialog-*")
    assert entry.covers("settings-home")
    assert entry.covers("bt-dialog-tema-aurora")
    assert not entry.covers("swrscan-3pass-stop")
    assert entry.as_dict()["fixtures"] == ["settings-*", "bt-dialog-*"]


def test_an_entry_without_fixtures_is_a_configuration_error(tmp_path: Path) -> None:
    """No silent default to "every screen": that is the bug of #296."""
    path = _write(
        tmp_path,
        "  - region: ajustes_dialogo\n    reason: r\n    author: a\n    until: 2027-04-30\n",
    )
    with pytest.raises(ConfigError, match="missing `fixtures`"):
        load_contracts(path, {DIALOG_REGION}, {"settings-home"})


@pytest.mark.parametrize(
    "body",
    [
        "  - region: ajustes_dialogo\n    fixtures: []\n    reason: r\n    author: a\n"
        "    until: 2027-04-30\n",
        "  - region: ajustes_dialogo\n    fixtures: settings-home\n    reason: r\n    author: a\n"
        "    until: 2027-04-30\n",
        "  - region: ajustes_dialogo\n    fixtures: ['']\n    reason: r\n    author: a\n"
        "    until: 2027-04-30\n",
    ],
)
def test_a_scope_that_is_not_a_non_empty_list_of_globs_is_rejected(
    tmp_path: Path, body: str
) -> None:
    path = _write(tmp_path, body)
    with pytest.raises(ConfigError):
        load_contracts(path, {DIALOG_REGION}, {"settings-home"})


def test_a_glob_that_matches_no_fixture_is_rejected(tmp_path: Path) -> None:
    """A scope that names nothing explains nothing: it is a typo, not a scope."""
    path = _write(
        tmp_path,
        "  - region: ajustes_dialogo\n"
        "    fixtures: [settings-home, main-tema-vision]\n"
        "    reason: r\n"
        "    author: a\n"
        "    until: 2027-04-30\n",
    )
    with pytest.raises(ConfigError, match="matches no fixture"):
        load_contracts(path, {DIALOG_REGION}, {"settings-home"})


def test_matching_is_case_sensitive_and_anchored() -> None:
    """`fnmatchcase`: the same fixture names on every host, no accidental prefix match."""
    entry = _change(fixtures=("settings-home", "bt-dialog-*"))
    assert entry.covers("settings-home")
    assert not entry.covers("settings-home-2")
    assert not entry.covers("Settings-home")
    assert not entry.covers("old-bt-dialog")


# --- the per-fixture allowance ----------------------------------------------


def test_allowed_for_reads_only_the_entries_of_that_fixture() -> None:
    quality: QualityContracts = contracts(
        expected_changes=(_change(fixtures=("swrscan-3pass-stop",)),)
    )
    assert quality.allowed_for("swrscan-3pass-stop", TODAY) == frozenset({DIALOG_REGION})
    assert quality.allowed_for("swrscan-quick", TODAY) == frozenset()
    assert quality.allowed_for("settings-home", TODAY) == frozenset()


def test_an_expired_entry_explains_nothing() -> None:
    quality = contracts(expected_changes=(_change(until="2026-01-01"),))
    assert quality.allowed_for("swrscan-3pass-stop", TODAY) == frozenset()
    assert quality.allowed_expectations("swrscan-3pass-stop", TODAY) == ()


# --- the gate ---------------------------------------------------------------


def test_a_change_of_the_declared_fixture_passes_and_names_the_entry(
    tmp_path: Path, region_map: RegionMap
) -> None:
    diff = _diff(tmp_path, region_map, DECLARED)
    gate = _gate(diff, region_map, (_change(),))
    assert gate.status is Status.PASS
    record = gate.details["explained_by"][0]
    assert record["scenario"] == DECLARED
    assert record["regions"] == [DIALOG_REGION]
    assert record["changes"][0]["author"] == "ui-polish-app-copy-284"
    assert record["changes"][0]["until"] == "2027-04-30"
    assert record["changes"][0]["fixtures"] == [DECLARED]
    # The human line carries the author, the expiry and the reason on one line.
    line = gate.details["explained_changes"][0]
    assert "ui-polish-app-copy-284" in line
    assert "2027-04-30" in line
    assert "AB-APCOPY288 (#288)" in line
    assert "\n" not in line


def test_the_same_change_on_another_fixture_fails(tmp_path: Path, region_map: RegionMap) -> None:
    """Negative control: the entry names the app it was measured on, not the box.

    The pixels are identical to the previous test; only the fixture name changes.
    Before #296 the entry explained it anyway, because the dialog box is shared.
    """
    diff = _diff(tmp_path, region_map, OTHER)
    gate = _gate(diff, region_map, (_change(),))
    assert gate.status is Status.FAIL
    assert "unexplained changed pixels" in gate.details["problems"][0]
    assert "explained_by" not in gate.details


def test_a_fixture_with_no_entry_at_all_fails(tmp_path: Path, region_map: RegionMap) -> None:
    diff = _diff(tmp_path, region_map, OTHER)
    gate = _gate(diff, region_map, ())
    assert gate.status is Status.FAIL
    assert "unexplained changed pixels" in gate.details["problems"][0]
    assert "explained_by" not in gate.details


def test_the_region_of_another_fixture_does_not_leak(tmp_path: Path, region_map: RegionMap) -> None:
    """An entry for `s_metro` explains `s_metro` of its fixture, not the dialog box."""
    diff = _diff(tmp_path, region_map, DECLARED)
    gate = _gate(diff, region_map, (_change(region="s_metro"),))
    assert gate.status is Status.FAIL


# --- the report -------------------------------------------------------------


def test_the_report_prints_the_entry_behind_each_explained_change(
    tmp_path: Path, region_map: RegionMap
) -> None:
    diff = _diff(tmp_path, region_map, DECLARED)
    gate = _gate(diff, region_map, (_change(),))
    evaluation = _evaluation(diff, gate)
    root = tmp_path / "report"
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
    metadata = ReportMetadata(
        experiment_id="AB-test",
        date=TODAY.isoformat(),
        docker_image="x6200-uiharness",
        scenario_fixtures={DECLARED: f"{DECLARED}.scn"},
    )
    markdown = render_markdown(evaluation, metadata, paths)
    assert "## Visual changes explained by a dated entry" in markdown
    assert f"| {DECLARED} |" in markdown
    assert "ui-polish-app-copy-284" in markdown
    assert "2027-04-30" in markdown
    assert "AB-APCOPY288 (#288)" in markdown


# --- the repository's own file ----------------------------------------------

REGIONS = {
    "barra_superior",
    "s_metro",
    "espectro",
    "waterfall_ticks",
    "frecuencia",
    "waterfall",
    "botones",
    "multiplicador",
    "reloj_potencia",
    "ajustes_anio",
    "ajustes_perfil",
    "ajustes_scrollbar",
    "ajustes_audio_ganancia",
    "ajustes_tema",
    "ajustes_dialogo",
    "dfl_inline",
}


def _repo_entries() -> tuple[ExpectedChange, ...]:
    return load_contracts(
        QUALITY_DIR / "contracts.yaml", REGIONS, set(FIXTURE_NAMES)
    ).expected_changes


def test_every_entry_names_at_least_one_live_fixture() -> None:
    entries = _repo_entries()
    assert len(entries) == 40, "the five entries whose fixtures #74 retired are gone"
    for entry in entries:
        assert entry.fixtures, f"{entry.region} ({entry.author}) has no scope"


def test_no_entry_uses_the_catch_all_glob() -> None:
    """`*` would allow the whole rectangle on every fixture: that is #296 again."""
    assert all("*" not in entry.fixtures for entry in _repo_entries())


def test_the_dialog_entries_do_not_explain_a_screen_they_did_not_measure() -> None:
    """The regression test of #296: the Ajustes declarations stop at Ajustes."""
    entries = _repo_entries()
    copy = next(entry for entry in entries if entry.author == "ui-polish-app-copy-284")
    assert copy.region == DIALOG_REGION
    assert copy.covers("swrscan-3pass-stop")
    assert not copy.covers("swrscan-quick")
    assert not copy.covers("settings-home")
    for entry in entries:
        if entry.region != DIALOG_REGION or entry.author == "ui-polish-app-copy-284":
            continue
        assert not entry.covers("swrscan-3pass-stop"), (
            f"{entry.author} explains the SWRScan dialog it never reviewed"
        )


def test_the_vision_and_rs_declarations_left_with_their_fixtures() -> None:
    """THEMES v2 #74 retired VISION/RS and their fixtures, so the entries go too.

    The rule `test_known_issues.py::test_repo_ui010_ui013_are_retired_with_their_fixtures`
    applies: a declaration anchored to a fixture the suite no longer ships cannot
    be measured (A defines the suite, so the fixture is never compared) and its
    globs would match nothing.
    """
    authors = {entry.author for entry in _repo_entries()}
    assert "ui04-tipografia" not in authors
    assert "wcag-full" not in authors
