"""Tests for the typography and legibility contracts (no Docker)."""

from __future__ import annotations

from pathlib import Path

from helpers import blank_image, inspection, make_capture, node, revision
from x6200_ab.quality_config import TypographyRules
from x6200_ab.typography_checks import run_typography_checks

RULES = TypographyRules(
    min_contrast_ratio=4.5,
    large_text_min_contrast_ratio=3.0,
    large_text_px=24,
    allow_fallback_glyphs=True,
)


def _capture(tmp_path: Path, nodes: list[dict]):
    return make_capture(
        tmp_path, "home", blank_image(), rev=revision(), inspection=inspection(nodes)
    )


def _rules(findings, rule: str) -> list:
    return [finding for finding in findings if finding.rule == rule]


def test_overflowing_text_is_reported(tmp_path: Path) -> None:
    label = node(cls="lv_label", x=0, y=0, w=56, h=26, text="VERYLONG", text_overflow=True)
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert _rules(findings, "typography_text_overflow")


def test_truncated_text_is_reported(tmp_path: Path) -> None:
    label = node(
        cls="lv_label",
        x=0,
        y=0,
        w=56,
        h=26,
        text="VERYLONG",
        long_mode="CLIP",
        text_truncated=True,
    )
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert _rules(findings, "typography_text_truncated")


def test_missing_glyph_is_reported(tmp_path: Path) -> None:
    label = node(
        cls="lv_label",
        x=0,
        y=0,
        w=56,
        h=26,
        text="\uf293",
        missing_glyphs=["U+F293"],
    )
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert _rules(findings, "typography_missing_glyph")


def test_fallback_glyph_is_allowed_by_default(tmp_path: Path) -> None:
    label = node(cls="lv_label", x=0, y=0, w=56, h=26, text="\uf1eb", fallback_glyphs=["U+F1EB"])
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert not _rules(findings, "typography_fallback_glyph")


def test_fallback_glyph_can_be_forbidden(tmp_path: Path) -> None:
    strict = TypographyRules(
        min_contrast_ratio=4.5,
        large_text_min_contrast_ratio=3.0,
        large_text_px=24,
        allow_fallback_glyphs=False,
    )
    label = node(cls="lv_label", x=0, y=0, w=56, h=26, text="\uf1eb", fallback_glyphs=["U+F1EB"])
    findings = run_typography_checks(_capture(tmp_path, [label]), strict)
    assert _rules(findings, "typography_fallback_glyph")


def test_low_contrast_text_is_reported(tmp_path: Path) -> None:
    label = node(
        cls="lv_label",
        x=0,
        y=0,
        w=56,
        h=26,
        text="ATU1",
        bg_color="#3A3F47",
        bg_opa=255,
        text_color="#3A3F47",
    )
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert _rules(findings, "typography_contrast")


def test_high_contrast_text_is_fine(tmp_path: Path) -> None:
    label = node(
        cls="lv_label",
        x=0,
        y=0,
        w=56,
        h=26,
        text="ATU1",
        bg_color="#3A3F47",
        bg_opa=255,
        text_color="#FFFFFF",
    )
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert not _rules(findings, "typography_contrast")


def test_translucent_background_is_not_judged(tmp_path: Path) -> None:
    # the pair depends on what is behind it, so the object tree can't state it
    label = node(
        cls="lv_label",
        x=0,
        y=0,
        w=56,
        h=26,
        text="ATU1",
        bg_color="#FFFFFF",
        bg_opa=127,
        text_color="#FFFFFF",
    )
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert not _rules(findings, "typography_contrast")


def test_large_text_uses_the_relaxed_threshold(tmp_path: Path) -> None:
    # ~4.0:1 is below 4.5:1 (normal) but above 3.0:1 (large)
    large = node(
        cls="lv_label",
        x=0,
        y=0,
        w=200,
        h=60,
        text="13.882",
        text_font="sony_30",
        bg_color="#3A3F47",
        bg_opa=255,
        text_color="#9AA0A6",
    )
    normal = node(
        cls="lv_label",
        x=0,
        y=100,
        w=56,
        h=26,
        text="ATU1",
        text_font="sony_20",
        bg_color="#3A3F47",
        bg_opa=255,
        text_color="#9AA0A6",
    )
    findings = run_typography_checks(_capture(tmp_path, [large, normal]), RULES)
    contrasts = _rules(findings, "typography_contrast")
    assert len(contrasts) == 1
    assert contrasts[0].path == "1"


def test_offscreen_label_is_not_a_typography_finding(tmp_path: Path) -> None:
    label = node(
        cls="lv_label",
        x=-500,
        y=0,
        w=56,
        h=26,
        visible=None,
        text="VERYLONG",
        text_overflow=True,
    )
    findings = run_typography_checks(_capture(tmp_path, [label]), RULES)
    assert not [finding for finding in findings if not finding.ok]


def test_without_inspection_is_informative_not_a_failure(tmp_path: Path) -> None:
    capture = make_capture(tmp_path, "home", blank_image(), rev=revision())
    findings = run_typography_checks(capture, RULES)
    assert [finding.rule for finding in findings] == ["typography_instrumented"]
    assert findings[0].ok
