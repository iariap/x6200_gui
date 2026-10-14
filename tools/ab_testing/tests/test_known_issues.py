"""Tests for the known-issue detectors, classification and exclusions."""

from __future__ import annotations

from datetime import date
from pathlib import Path

import pytest

from helpers import blank_image, draw_box, inspection, make_capture, node, revision
from x6200_ab.errors import ConfigError, HarnessError
from x6200_ab.known_issues import (
    Detector,
    Exclusion,
    IssueClassification,
    KnownIssue,
    classify,
    evaluate_detector,
    load_known_issues,
)
from x6200_ab.regions import RegionMap

QUALITY_DIR = Path(__file__).resolve().parents[1] / "quality"


def _issue(status: str = "verified") -> KnownIssue:
    return KnownIssue(
        id="UI-900",
        title="test",
        severity="major",
        fixture="home",
        region="multiplicador",
        status=status,
        note="",
        evidence=(),
        detector=Detector(
            kind="region_content_above",
            region="multiplicador",
            pixel_tolerance=24,
            max_content=0.05,
        ),
        exclusions=(),
    )


def test_classification_matrix() -> None:
    assert classify(_issue(), True, False)[0] is IssueClassification.FIXED
    assert classify(_issue(), True, True)[0] is IssueClassification.UNCHANGED_DEFECT
    assert classify(_issue("fixed"), False, True)[0] is IssueClassification.REGRESSION
    assert classify(_issue("verified"), False, True)[0] is IssueClassification.NEW_DEFECT
    assert classify(_issue(), False, False)[0] is IssueClassification.NOT_REPRODUCED
    assert classify(_issue(), None, None)[0] is IssueClassification.UNVERIFIED


def test_detector_fires_only_with_content(tmp_path: Path, region_map: RegionMap) -> None:
    detector = Detector(
        kind="region_content_above",
        region="multiplicador",
        pixel_tolerance=24,
        max_content=0.05,
    )
    clean = make_capture(tmp_path / "clean", "home", blank_image(), rev=revision())
    dirty_image = draw_box(blank_image(), 762, 12, 28, 8)
    dirty = make_capture(tmp_path / "dirty", "home", dirty_image, rev=revision())
    clean_outcome = evaluate_detector(detector, clean, region_map)
    dirty_outcome = evaluate_detector(detector, dirty, region_map)
    assert clean_outcome.reproduced is False
    assert dirty_outcome.reproduced is True
    assert dirty_outcome.measured > clean_outcome.measured


def test_exclusion_scoping_and_expiry() -> None:
    exclusion = Exclusion(fixture="home", region="botones", until="2030-01-01", reason="known")
    assert exclusion.active("home", date(2029, 12, 31)) is True
    assert exclusion.active("home", date(2031, 1, 1)) is False
    # never global: another fixture does not match
    assert exclusion.active("other", date(2029, 1, 1)) is False


def test_repo_known_issues_load(region_map: RegionMap) -> None:
    issues = load_known_issues(QUALITY_DIR / "known_issues.yaml", set(region_map.regions))
    by_id = {issue.id: issue for issue in issues}
    assert "UI-001" in by_id
    assert by_id["UI-001"].detector is not None
    # the repository seeds are recorded but not yet reproducible
    assert all(by_id[f"UI-00{i}"].detector is None for i in range(2, 9))
    # UI-009 is the first reproduced firmware defect with a text detector
    ui009 = by_id["UI-009"]
    assert ui009.detector is not None
    assert ui009.detector.kind == "leading_glyph_range"
    assert ui009.detector.glyph_range == (0xF200, 0xF2FF)
    assert ui009.status == "fixed"


def _glyph_detector() -> Detector:
    return Detector(
        kind="leading_glyph_range",
        region="reloj_potencia",
        text_pattern=r"\d\.\dv\n",
        glyph_range=(0xF200, 0xF2FF),
    )


def test_leading_glyph_detector_flags_ascii_prefix(tmp_path: Path, region_map: RegionMap) -> None:
    """A battery-icon prefix passes; the uninitialized-pointer garbage does not."""
    detector = _glyph_detector()
    battery = make_capture(
        tmp_path / "battery",
        "home",
        blank_image(),
        inspection=inspection([node(cls="lv_label", text="\uf240 12.1v\n\uf1e6 13.8v")]),
    )
    garbage = make_capture(
        tmp_path / "garbage",
        "home",
        blank_image(),
        inspection=inspection([node(cls="lv_label", text="EHE 12.1v\n\uf1e6 13.8v")]),
    )
    assert evaluate_detector(detector, battery, region_map).reproduced is False
    assert evaluate_detector(detector, garbage, region_map).reproduced is True


def test_leading_glyph_detector_needs_the_inspection(tmp_path: Path, region_map: RegionMap) -> None:
    """Without `--inspect` there is no text to read: BLOCKED, never a silent pass."""
    capture = make_capture(tmp_path / "plain", "home", blank_image())
    with pytest.raises(HarnessError, match="structural inspection"):
        evaluate_detector(_glyph_detector(), capture, region_map)


def test_unknown_region_is_rejected(tmp_path: Path, region_map: RegionMap) -> None:
    path = tmp_path / "issues.yaml"
    path.write_text(
        "issues:\n"
        "  - id: UI-1\n"
        "    title: t\n"
        "    severity: major\n"
        "    fixture: home\n"
        "    region: nope\n"
        "    status: verified\n"
        "    detector:\n"
        "      kind: region_content_above\n"
        "      region: nope\n"
    )
    with pytest.raises(ConfigError):
        load_known_issues(path, set(region_map.regions))


def test_bad_severity_is_rejected(tmp_path: Path, region_map: RegionMap) -> None:
    path = tmp_path / "issues.yaml"
    path.write_text(
        "issues:\n"
        "  - id: UI-1\n"
        "    title: t\n"
        "    severity: enormous\n"
        "    fixture: home\n"
        "    status: verified\n"
    )
    with pytest.raises(ConfigError, match="unknown severity"):
        load_known_issues(path, set(region_map.regions))
