"""Tests for the theme resolution contract (the theme the firmware can keep)."""

from __future__ import annotations

from datetime import date
from pathlib import Path

from conftest import RealQuality
from helpers import blank_image, make_capture, revision
from x6200_ab.contracts import ContractFinding, apply_waivers
from x6200_ab.theme_checks import RULE_THEME_NAME, run_theme_checks

# The `theme` block the C harness writes (harness_main.c, write_theme): the name
# the fixture rendered and the one the firmware's own params store holds.
FITS: dict[str, object] = {
    "requested": "IU1WXY",
    "params_name": "IU1WXY",
    "params_max_chars": 15,
    "active": "IU1WXY",
}
TRUNCATED: dict[str, object] = {
    "requested": "Outdoor-Negative",
    "params_name": "Outdoor-Negativ",
    "params_max_chars": 15,
    "active": "Outdoor-Negative",
}
# The store after UI-013's fix (`params_str_t.x` is 32 bytes, 31 useful): the
# whole name survives, so `requested == params_name`.
WIDE: dict[str, object] = {
    "requested": "Outdoor-Negative",
    "params_name": "Outdoor-Negative",
    "params_max_chars": 31,
    "active": "Outdoor-Negative",
}


def _capture(tmp_path: Path, name: str, theme: dict[str, object] | None):
    return make_capture(tmp_path, name, blank_image(), rev=revision(), theme=theme)


def test_a_theme_that_survives_the_params_store_passes(tmp_path: Path) -> None:
    findings = run_theme_checks(_capture(tmp_path, "home", FITS))
    assert [finding.rule for finding in findings] == [RULE_THEME_NAME]
    assert findings[0].ok is True
    assert "fits params.theme_name" in findings[0].detail


def test_a_theme_name_the_store_truncates_is_a_defect(tmp_path: Path) -> None:
    """The deliberate defect: 'Outdoor-Negative' (16 chars) in a 16-byte buffer.

    `theme_list()` offers the name, `params_str_set()` keeps 15 characters and
    `theme_init()` then falls back to IU1WXY, so the user picks a theme and gets
    another one. The finding has to name both names and the consequence.
    """
    finding = run_theme_checks(_capture(tmp_path, "main-tema-outdoor-negative", TRUNCATED))[0]
    assert finding.ok is False
    assert "'Outdoor-Negative'" in finding.detail
    assert "'Outdoor-Negativ'" in finding.detail
    assert "15 chars" in finding.detail


def test_a_capture_without_the_theme_block_is_reported_as_not_instrumented(
    tmp_path: Path,
) -> None:
    finding = run_theme_checks(_capture(tmp_path, "home", None))[0]
    assert finding.ok is True
    assert "no `theme` block" in finding.detail


def test_the_repo_does_not_waive_the_theme_finding(real_quality: RealQuality) -> None:
    """With the fix in, the registry carries no exception: the finding is real.

    Before UI-04/AB-UI04-EXP4 this rule was waived on its fixture by a dated
    exception; now that `params.theme_name` holds the whole name the exception is
    gone, so a theme the store cannot keep is a plain failure — the same shape as
    UI-010/UI-011/UI-012 after their fixes.
    """
    finding = ContractFinding(
        rule=RULE_THEME_NAME,
        fixture="main-tema-outdoor-negative",
        ok=False,
        detail="theme 'Outdoor-Negative' does not fit params.theme_name",
    )
    unchanged = apply_waivers([finding], real_quality.contracts, date(2026, 10, 14))
    assert unchanged == [finding]
    assert unchanged[0].ok is False


def test_no_fixture_is_waived_for_the_theme_rule(real_quality: RealQuality) -> None:
    """No fixture gets a pass for the theme rule: the register has no waiver."""
    for fixture in ("main-tema-outdoor-negative", "main-tema-classic"):
        finding = ContractFinding(
            rule=RULE_THEME_NAME,
            fixture=fixture,
            ok=False,
            detail="theme that does not fit",
        )
        assert apply_waivers([finding], real_quality.contracts, date(2026, 10, 14))[0].ok is False


def test_the_fixed_store_keeps_the_full_name(tmp_path: Path) -> None:
    """UI-013's fix: the store holds the 31 characters `theme_list()` admits.

    `params_str_t.x` is 32 bytes (src/params/types.h), so "Outdoor-Negative" (16)
    survives the round trip and the fixture measures `requested == params_name`.
    """
    finding = run_theme_checks(_capture(tmp_path, "main-tema-outdoor-negative", WIDE))[0]
    assert finding.ok is True
    assert "fits params.theme_name" in finding.detail


def test_every_repo_exception_is_scoped_dated_and_recorded(real_quality: RealQuality) -> None:
    """A waiver may only exist while it is scoped, dated, reasoned and behind a known issue.

    UI-013 was the last exception for a while: the store (`params_str_t.x`,
    src/params/types.h) holds the 31 characters `theme_list()` admits, so
    `theme_name_fits_params` passes on its own fixture and the dated waiver went
    away with the fix (UI-04/AB-UI04-EXP4) — UI-011's waiver was withdrawn in
    EXP-1, UI-012's in EXP-2, the UI-010 region exclusion in EXP-3 and UI-016's
    `layout_clipped` in AB-UI016-T1. The invariant is not "no waivers": it is that
    a waiver is never global and never permanent, and that the defect it holds back
    is recorded. The four left are the Ajustes window's remaining real finding
    (UI-015, the controls below the 44px field-UX target), one per fixture that
    renders the rows it lives in — settings-rs-theme (UI-007) renders the same
    Spectrum/waterfall rows as settings-wfbat, under the RS theme. Each one names
    exactly one rule and one fixture (no `path` shadow, so it cannot hide another
    node), carries a reason, an author and an expiry, and its reason names the open
    known issue that documents it.
    """
    exceptions = real_quality.contracts.exceptions
    assert {exception.rule for exception in exceptions} == {"layout_touch_target"}
    assert {exception.fixture for exception in exceptions} == {
        "settings-audio",
        "settings-home",
        "settings-rs-theme",
        "settings-wfbat",
    }
    for exception in exceptions:
        assert exception.path is None
        assert exception.reason.strip()
        assert exception.author
        assert date.fromisoformat(exception.until) >= date(2026, 10, 14)
        documented = [issue for issue in real_quality.known_issues if issue.id in exception.reason]
        assert documented, f"{exception.rule} [{exception.fixture}] names no known issue"
        assert all(issue.is_open_defect for issue in documented)


def test_the_repo_explains_the_top_bar_visual_changes(real_quality: RealQuality) -> None:
    """UI-011 and UI-012 have no pixel detector: their fixes are declared changes.

    Neither structural rule (`typography_missing_glyph`, `typography_text_overflow`)
    says where the pixels move, so `expected_changes` carries the top-bar change
    of each fix; the gate reads the entries of one region as a set.
    """
    changes = real_quality.contracts.expected_changes
    assert "barra_superior" in {change.region for change in changes}
    reasons = " ".join(change.reason for change in changes)
    assert "UI-011" in reasons
    assert "UI-012" in reasons
    for change in changes:
        assert change.reason.strip(), f"{change.region} has no reason"
        assert change.author, f"{change.region} has no author"
        assert change.until >= "2026-10-14"
