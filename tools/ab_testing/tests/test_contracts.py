"""Tests for the objective quality contracts (the C of the plan)."""

from __future__ import annotations

from datetime import date
from pathlib import Path

from helpers import (
    blank_image,
    contracts,
    draw_box,
    inspection,
    make_capture,
    node,
    revision,
)
from x6200_ab.capture import Capture
from x6200_ab.contracts import ContractFinding, run_contracts
from x6200_ab.known_issues import Exclusion, KnownIssue
from x6200_ab.quality_config import RuleException
from x6200_ab.regions import RegionMap


def _finding(findings: list[ContractFinding], rule: str) -> ContractFinding:
    for finding in findings:
        if finding.rule == rule:
            return finding
    raise AssertionError(f"no finding for {rule}: {[f.rule for f in findings]}")


def test_framebuffer_size_contract(tmp_path: Path, region_map: RegionMap) -> None:
    capture = make_capture(tmp_path, "home", blank_image(800, 480), rev=revision())
    findings = run_contracts([capture], region_map, contracts())
    assert _finding(findings, "framebuffer_size").ok is True

    small = make_capture(tmp_path / "small", "home", blank_image(640, 480), rev=revision())
    findings = run_contracts([small], region_map, contracts())
    assert _finding(findings, "framebuffer_size").ok is False


def _button_strip():
    """Five button panels with gaps: a region with real, non-uniform content."""
    image = blank_image()
    for index in range(5):
        image = draw_box(image, index * 160 + 5, 423, 150, 52)
    return image


def test_region_min_content_detects_blank_component(tmp_path: Path, region_map: RegionMap) -> None:
    rule = "region_min_content:botones"
    blank = make_capture(tmp_path / "blank", "home", blank_image(), rev=revision())
    findings = run_contracts([blank], region_map, contracts(region_min_content={"botones": 0.02}))
    assert _finding(findings, rule).ok is False

    filled = make_capture(tmp_path / "filled", "home", _button_strip(), rev=revision())
    findings = run_contracts([filled], region_map, contracts(region_min_content={"botones": 0.02}))
    assert _finding(findings, rule).ok is True


# --- the shared app window (`modal_surface`) ---------------------------------
#
# `dialog_init()` puts one opaque window (796x348 at x2/y66) over the screen; the
# waterfall region [0,196,800,222] is covered from y196 to y413, so only the last
# 4 rows stay visible. The contract's question is "is the component being drawn",
# and the covered pixels cannot answer it (they are not visible), so they are left
# out of the measurement. These tests pin BOTH directions: the visible part still
# has to have content (a blank waterfall keeps failing), and content that is only
# under the window does not count.

MODAL = (2, 66, 796, 348)


def _modal_window() -> dict:
    """The app window as `--inspect` reports it: exact box, own opaque surface."""
    return node(x=2, y=66, w=796, h=348, bg_opa=255, bg_image=True)


def _capture_with_modal(tmp_path: Path, name: str, image) -> Capture:
    return make_capture(
        tmp_path / name,
        name,
        image,
        rev=revision(),
        inspection=inspection([node(x=0, y=0, w=800, h=480, bg_opa=255), _modal_window()]),
    )


def test_modal_surface_does_not_hide_a_blank_region(tmp_path: Path, region_map: RegionMap) -> None:
    """NEGATIVE CONTROL: a waterfall that is blank where it is visible still fails.

    This is the property the contract exists for. Excluding the window must not
    turn "the waterfall is not being drawn" into a pass.
    """
    rule = "region_min_content:waterfall"
    blank = _capture_with_modal(tmp_path, "blank", blank_image())
    findings = run_contracts(
        [blank],
        region_map,
        contracts(
            region_min_content={"waterfall": 0.02},
            modal_surface=MODAL,
            modal_visible_regions=("waterfall",),
        ),
    )
    assert _finding(findings, rule).ok is False

    # Content in the visible 4-row sliver (y414..417) is what the contract counts.
    visible = _capture_with_modal(
        tmp_path / "visible", "visible", draw_box(blank_image(), 0, 414, 800, 4)
    )
    findings = run_contracts(
        [visible],
        region_map,
        contracts(
            region_min_content={"waterfall": 0.02},
            modal_surface=MODAL,
            modal_visible_regions=("waterfall",),
        ),
    )
    assert _finding(findings, rule).ok is True


def test_modal_surface_ignores_content_that_the_window_covers(
    tmp_path: Path, region_map: RegionMap
) -> None:
    """Content under the window is not visible: it neither passes nor fails by itself."""
    rule = "region_min_content:waterfall"
    # A band of content only where the window sits (y250..350), nothing visible.
    covered = draw_box(blank_image(), 100, 250, 400, 100)
    with_modal = _capture_with_modal(tmp_path / "with", "with", covered)
    findings = run_contracts(
        [with_modal],
        region_map,
        contracts(
            region_min_content={"waterfall": 0.02},
            modal_surface=MODAL,
            modal_visible_regions=("waterfall",),
        ),
    )
    assert _finding(findings, rule).ok is False

    # The same pixels WITHOUT the window on top are visible, so the region has content:
    # the measurement of a dialog-less fixture is the whole region, as before.
    without_modal = make_capture(tmp_path / "without", "without", covered, rev=revision())
    findings = run_contracts(
        [without_modal],
        region_map,
        contracts(
            region_min_content={"waterfall": 0.02},
            modal_surface=MODAL,
            modal_visible_regions=("waterfall",),
        ),
    )
    assert _finding(findings, rule).ok is True


def test_modal_surface_declares_a_fully_covered_region(
    tmp_path: Path, region_map: RegionMap
) -> None:
    """A region the window covers entirely is declared, never failed nor counted."""
    # `ajustes_tema` = [20,79,764,60] is inside the window (y66..413): not one of its
    # pixels is on screen. The shipped config declares only `waterfall`; this pins the
    # guard for a region that a future config could declare.
    capture = _capture_with_modal(tmp_path, "covered", blank_image())
    findings = run_contracts(
        [capture],
        region_map,
        contracts(
            region_min_content={"ajustes_tema": 0.02},
            modal_surface=MODAL,
            modal_visible_regions=("ajustes_tema",),
        ),
    )
    finding = _finding(findings, "region_min_content:ajustes_tema")
    assert finding.ok is True
    assert "fully covered" in finding.detail


def test_modal_surface_declares_a_region_that_only_keeps_the_margin(
    tmp_path: Path, region_map: RegionMap
) -> None:
    """A band the window covers except the screen margin is declared, not measured.

    `frecuencia` = [0,160,800,36] falls ENTIRELY inside the window's vertical span
    (y=66..413) and the window is 796 of the 800 columns wide, so no row of the band
    is visible: the pixels left at the edges are the screen margin, and the rest is
    the window's own text/borders. Measuring the region whole counted those as the
    row's content (measured 1.53% on `bt-dialog-tema-redshift`, below the 2% floor
    and unrelated to the frequency row). Without the window the same capture
    measures the region whole, as before.
    """
    rule = "region_min_content:frecuencia"
    config = contracts(
        region_min_content={"frecuencia": 0.02},
        modal_surface=MODAL,
        modal_visible_regions=("frecuencia",),
    )
    covered = _capture_with_modal(tmp_path, "margin", blank_image())
    finding = _finding(run_contracts([covered], region_map, config), rule)
    assert finding.ok is True
    assert "not measured" in finding.detail

    without = make_capture(tmp_path / "no-modal", "no-modal", blank_image(), rev=revision())
    assert _finding(run_contracts([without], region_map, config), rule).ok is False


def test_modal_surface_requires_the_inspection_tree(tmp_path: Path, region_map: RegionMap) -> None:
    """Without `--inspect` nothing is assumed: the region is measured whole."""
    capture = make_capture(tmp_path, "no-tree", draw_box(blank_image(), 100, 250, 400, 100))
    findings = run_contracts(
        [capture],
        region_map,
        contracts(
            region_min_content={"waterfall": 0.02},
            modal_surface=MODAL,
            modal_visible_regions=("waterfall",),
        ),
    )
    assert _finding(findings, "region_min_content:waterfall").ok is True


def test_coverage_levels_and_required(tmp_path: Path, region_map: RegionMap) -> None:
    bad_coverage = [
        {"component": "main_screen.c", "level": "REAL", "note": ""},
        {"component": "theme.c", "level": "PROBABLY", "note": ""},
    ]
    capture = make_capture(tmp_path, "home", blank_image(), rev=revision(), coverage=bad_coverage)
    findings = run_contracts([capture], region_map, contracts())
    assert _finding(findings, "coverage_levels").ok is False
    # styles.c is required by the real contracts and is missing here
    findings = run_contracts(
        [capture],
        region_map,
        contracts(required_components=("main_screen.c", "theme.c", "styles.c")),
    )
    assert _finding(findings, "coverage_required").ok is False


def _defective_capture(tmp_path: Path):
    """A capture whose inspection already carries a missing-glyph defect."""
    label = node(
        cls="lv_label",
        x=0,
        y=0,
        w=56,
        h=26,
        text="\uf293",
        missing_glyphs=["U+F293"],
    )
    return make_capture(
        tmp_path,
        "home",
        blank_image(),
        rev=revision(),
        inspection=inspection([label]),
    )


def test_structural_checks_run_inside_the_contracts(tmp_path: Path, region_map: RegionMap) -> None:
    findings = run_contracts([_defective_capture(tmp_path)], region_map, contracts())
    assert _finding(findings, "typography_missing_glyph").ok is False


def test_active_exception_waives_a_structural_finding(
    tmp_path: Path, region_map: RegionMap
) -> None:
    exception = RuleException(
        rule="typography_missing_glyph",
        fixture="home",
        reason="recorded known issue",
        author="test",
        until="2999-01-01",
        path="0",
    )
    findings = run_contracts(
        [_defective_capture(tmp_path)], region_map, contracts(exceptions=(exception,))
    )
    waived = _finding(findings, "typography_missing_glyph")
    assert waived.ok is True
    assert "waived until 2999-01-01" in waived.detail


def test_expired_exception_does_not_waive(tmp_path: Path, region_map: RegionMap) -> None:
    exception = RuleException(
        rule="typography_missing_glyph",
        fixture="home",
        reason="old waiver",
        author="test",
        until="2000-01-01",
    )
    findings = run_contracts(
        [_defective_capture(tmp_path)], region_map, contracts(exceptions=(exception,))
    )
    assert _finding(findings, "typography_missing_glyph").ok is False


def _open_frequency_issue() -> KnownIssue:
    """UI-010 as the registry records it: open, evidenced, scoped to one rule."""
    return KnownIssue(
        id="UI-010",
        title="frecuencia invisible",
        severity="critical",
        fixture="home",
        region="frecuencia",
        status="verified",
        note="",
        evidence=("capture",),
        detector=None,
        exclusions=(
            Exclusion(
                fixture="home",
                region="frecuencia",
                until="2030-01-01",
                reason="known baseline defect",
                rule="region_min_content:frecuencia",
            ),
        ),
    )


def test_open_known_issue_turns_a_failure_into_an_unchanged_defect(
    tmp_path: Path, region_map: RegionMap
) -> None:
    blank = make_capture(tmp_path, "home", blank_image(), rev=revision())
    findings = run_contracts(
        [blank],
        region_map,
        contracts(region_min_content={"frecuencia": 0.02}),
        (_open_frequency_issue(),),
        today=date(2026, 10, 14),
    )
    finding = _finding(findings, "region_min_content:frecuencia")
    assert finding.ok is False
    assert finding.unchanged_defect is True
    assert finding.baseline_issue == "UI-010"
    assert "UI-010" in finding.detail


def test_violation_not_in_the_registry_stays_a_real_failure(
    tmp_path: Path, region_map: RegionMap
) -> None:
    # Same contract shape, but a region the registered issue does not cover.
    blank = make_capture(tmp_path, "home", blank_image(), rev=revision())
    findings = run_contracts(
        [blank],
        region_map,
        contracts(region_min_content={"waterfall": 0.02}),
        (_open_frequency_issue(),),
        today=date(2026, 10, 14),
    )
    finding = _finding(findings, "region_min_content:waterfall")
    assert finding.ok is False
    assert finding.unchanged_defect is False
    assert finding.baseline_issue is None


def test_known_issue_outside_its_expiry_stays_a_real_failure(
    tmp_path: Path, region_map: RegionMap
) -> None:
    blank = make_capture(tmp_path, "home", blank_image(), rev=revision())
    findings = run_contracts(
        [blank],
        region_map,
        contracts(region_min_content={"frecuencia": 0.02}),
        (_open_frequency_issue(),),
        today=date(2031, 1, 1),
    )
    finding = _finding(findings, "region_min_content:frecuencia")
    assert finding.unchanged_defect is False
    assert finding.baseline_issue is None
