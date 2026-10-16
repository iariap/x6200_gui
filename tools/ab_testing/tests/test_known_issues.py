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
    baseline_issue_for,
    classify,
    evaluate_detector,
    load_known_issues,
)
from x6200_ab.regions import RegionMap

QUALITY_DIR = Path(__file__).resolve().parents[1] / "quality"


def _baseline_issue(
    *,
    status: str = "verified",
    fixture: str = "home",
    region: str = "frecuencia",
    rule: str = "region_min_content:frecuencia",
    until: str = "2030-01-01",
    evidence: tuple[str, ...] = ("capture",),
    exclusions: tuple[Exclusion, ...] | None = None,
) -> KnownIssue:
    if exclusions is None:
        exclusions = (
            Exclusion(fixture=fixture, region=region, until=until, reason="known", rule=rule),
        )
    return KnownIssue(
        id="UI-010",
        title="frecuencia invisible",
        severity="critical",
        fixture=fixture,
        region=region,
        status=status,
        note="",
        evidence=evidence,
        detector=None,
        exclusions=exclusions,
    )


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
    assert all(by_id[f"UI-00{i}"].detector is None for i in (2, 3, 4, 5, 6, 8))
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


def test_baseline_issue_matches_only_the_exact_scope() -> None:
    issue = _baseline_issue()
    today = date(2027, 1, 1)
    assert baseline_issue_for("region_min_content:frecuencia", "home", (issue,), today) is issue
    # another region, another fixture or a non-region rule never match
    assert baseline_issue_for("region_min_content:waterfall", "home", (issue,), today) is None
    assert baseline_issue_for("region_min_content:frecuencia", "other", (issue,), today) is None
    assert baseline_issue_for("coverage_required", "home", (issue,), today) is None


def test_baseline_issue_requires_an_open_founded_defect() -> None:
    today = date(2027, 1, 1)
    for issue in (
        _baseline_issue(status="fixed"),
        _baseline_issue(status="unverified"),
        _baseline_issue(evidence=()),
    ):
        assert baseline_issue_for("region_min_content:frecuencia", "home", (issue,), today) is None


def test_baseline_issue_expiry_closes_the_waiver() -> None:
    issue = _baseline_issue(until="2026-06-01")
    assert (
        baseline_issue_for("region_min_content:frecuencia", "home", (issue,), date(2026, 5, 31))
        is issue
    )
    assert (
        baseline_issue_for("region_min_content:frecuencia", "home", (issue,), date(2026, 6, 2))
        is None
    )


def test_repo_ui010_ui013_are_retired_with_their_fixtures(region_map: RegionMap) -> None:
    """UI-010 and UI-013 left with the legacy themes (issue #74).

    UI-010 (white frequency digits on Outdoor's white background, region
    `frecuencia` of `main-otro-tema`) and UI-013 (`params.theme_name` truncating
    "Outdoor-Negative", `main-tema-outdoor-negative`) were anchored to legacy
    themes that #74 retired: the themes and their fixtures are gone, so the entries
    go with them. The fixes stay in the code (src/main_screen.c asks the theme for
    its `text` role; `params_str_t.x` is 32 bytes) and are guarded by the contract
    rules and `tests/native/test_theme.c`, not by a waived fixture.
    """
    issues = load_known_issues(QUALITY_DIR / "known_issues.yaml", set(region_map.regions))
    by_id = {issue.id: issue for issue in issues}
    assert "UI-010" not in by_id
    assert "UI-013" not in by_id


def test_exclusion_rule_must_be_region_scoped(tmp_path: Path, region_map: RegionMap) -> None:
    path = tmp_path / "issues.yaml"
    path.write_text(
        "issues:\n"
        "  - id: UI-1\n"
        "    title: t\n"
        "    severity: major\n"
        "    fixture: home\n"
        "    region: frecuencia\n"
        "    status: verified\n"
        "    evidence: [capture]\n"
        "    exclusions:\n"
        "      - fixture: home\n"
        "        region: frecuencia\n"
        "        rule: coverage_required\n"
        "        until: 2030-01-01\n"
        "        reason: nope\n"
    )
    with pytest.raises(ConfigError, match="not region-scoped"):
        load_known_issues(path, set(region_map.regions))


def test_exclusion_rule_region_must_match(tmp_path: Path, region_map: RegionMap) -> None:
    path = tmp_path / "issues.yaml"
    path.write_text(
        "issues:\n"
        "  - id: UI-1\n"
        "    title: t\n"
        "    severity: major\n"
        "    fixture: home\n"
        "    region: frecuencia\n"
        "    status: verified\n"
        "    evidence: [capture]\n"
        "    exclusions:\n"
        "      - fixture: home\n"
        "        region: frecuencia\n"
        "        rule: 'region_min_content:waterfall'\n"
        "        until: 2030-01-01\n"
        "        reason: nope\n"
    )
    with pytest.raises(ConfigError, match="scoped to region"):
        load_known_issues(path, set(region_map.regions))
