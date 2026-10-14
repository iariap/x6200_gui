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


def test_the_repo_waives_the_theme_finding_only_until_its_expiry(
    real_quality: RealQuality,
) -> None:
    """The registry's exception covers this rule on this fixture — and expires."""
    finding = ContractFinding(
        rule=RULE_THEME_NAME,
        fixture="main-tema-outdoor-negative",
        ok=False,
        detail="theme 'Outdoor-Negative' does not fit params.theme_name",
    )
    waived = apply_waivers([finding], real_quality.contracts, date(2026, 10, 14))
    assert waived[0].ok is True
    assert waived[0].detail.startswith("waived until ")
    # A dated waiver, not a permanent one: after the expiry it is a failure again.
    assert apply_waivers([finding], real_quality.contracts, date(2028, 1, 1))[0].ok is False


def test_the_exception_is_scoped_to_its_fixture(real_quality: RealQuality) -> None:
    finding = ContractFinding(
        rule=RULE_THEME_NAME,
        fixture="main-tema-classic",
        ok=False,
        detail="theme that does not fit",
    )
    assert apply_waivers([finding], real_quality.contracts, date(2026, 10, 14))[0].ok is False


def test_the_repo_contracts_carry_the_vision_and_theme_exceptions(
    real_quality: RealQuality,
) -> None:
    scoped = {
        (exception.rule, exception.fixture): exception
        for exception in real_quality.contracts.exceptions
    }
    assert ("theme_name_fits_params", "main-tema-outdoor-negative") in scoped
    assert ("typography_missing_glyph", "main-tema-vision") in scoped
    assert ("typography_text_overflow", "main-tema-vision") in scoped
    for exception in real_quality.contracts.exceptions:
        assert exception.reason.strip(), f"{exception.rule} has no reason"
        assert exception.until >= "2026-10-14"
