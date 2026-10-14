"""Tests for the objective quality contracts (the C of the plan)."""

from __future__ import annotations

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
from x6200_ab.contracts import ContractFinding, run_contracts
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
