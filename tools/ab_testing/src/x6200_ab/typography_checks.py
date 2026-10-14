"""Typography and legibility contracts over the structural inspection.

The C harness measures the label text with LVGL's own font metrics
(`lv_txt_get_size`) and resolves every glyph against the font chain
(`lv_font_get_glyph_dsc`), so these rules need no OCR:

* text that overflows its label box (a WRAP label that grows taller than the box,
  or a fixed label whose single line is wider/taller than the object);
* text clipped on purpose (DOT/CLIP long modes cutting the line);
* a glyph missing from every font in the chain: LVGL draws a placeholder
  rectangle, the classic "empty box" icon defect;
* a glyph that falls back to another font. That is *expected* in this firmware
  (the alternate fonts are ASCII-only and the icons come from the Sony fallback),
  so it only fails when the contract says fallbacks are not allowed;
* a text/background pair below the WCAG 2.1 contrast ratio, when the label
  actually paints an opaque background (otherwise the pair is not knowable from
  the object tree).
"""

from __future__ import annotations

from .capture import Capture
from .findings import ContractFinding
from .inspection import InspectionNode, effectively_hidden, inspection_of, iter_tree
from .quality_config import TypographyRules


def run_typography_checks(capture: Capture, typography: TypographyRules) -> list[ContractFinding]:
    """Evaluate the typography rules on one capture's structural inspection."""
    inspection = inspection_of(capture)
    if inspection is None:
        return [
            ContractFinding(
                rule="typography_instrumented",
                fixture=capture.name,
                ok=True,
                detail="no structural inspection in this capture (not requested)",
            )
        ]

    findings: list[ContractFinding] = []
    for node, ancestors, path in iter_tree(inspection.nodes):
        if not node.is_label or effectively_hidden(node, ancestors):
            continue
        if node.visible is None or node.visible.empty:
            # Off screen: the layout contracts own that, not typography.
            continue
        findings.extend(_label(capture.name, node, path, typography))
    return findings


def _label(
    name: str, node: InspectionNode, path: str, rules: TypographyRules
) -> list[ContractFinding]:
    findings: list[ContractFinding] = []
    text = node.text.strip()
    if not text:
        return findings

    if node.missing_glyphs:
        findings.append(
            ContractFinding(
                rule="typography_missing_glyph",
                fixture=name,
                ok=False,
                detail=(
                    f"text {text!r} uses glyph(s) {list(node.missing_glyphs)} missing from "
                    f"every font in the chain (LVGL draws a placeholder box)"
                ),
                path=path,
            )
        )
    elif node.fallback_glyphs and not rules.allow_fallback_glyphs:
        findings.append(
            ContractFinding(
                rule="typography_fallback_glyph",
                fixture=name,
                ok=False,
                detail=f"text {text!r} falls back for {list(node.fallback_glyphs)}",
                path=path,
            )
        )

    if node.text_truncated:
        findings.append(
            ContractFinding(
                rule="typography_text_truncated",
                fixture=name,
                ok=False,
                detail=(
                    f"text {text!r} is truncated ({node.long_mode}) in a {node.w}x{node.h} box"
                ),
                path=path,
            )
        )
    elif node.text_overflow:
        findings.append(
            ContractFinding(
                rule="typography_text_overflow",
                fixture=name,
                ok=False,
                detail=(f"text {text!r} does not fit its {node.w}x{node.h} box ({node.long_mode})"),
                path=path,
            )
        )

    contrast = _contrast_ratio(node)
    if contrast is not None:
        threshold = _threshold(node, rules)
        if contrast < threshold:
            findings.append(
                ContractFinding(
                    rule="typography_contrast",
                    fixture=name,
                    ok=False,
                    detail=(
                        f"text {text!r} contrast {contrast:.2f}:1 < {threshold:.2f}:1 "
                        f"({node.text_color} on {node.bg_color})"
                    ),
                    path=path,
                )
            )
    return findings


def _threshold(node: InspectionNode, rules: TypographyRules) -> float:
    if _font_px(node.text_font) >= rules.large_text_px:
        return rules.large_text_min_contrast_ratio
    return rules.min_contrast_ratio


def _font_px(font_name: str) -> int:
    """The pixel size encoded in a font name (`sony_20` -> 20), else 0."""
    _, _, size = font_name.rpartition("_")
    return int(size) if size.isdigit() else 0


def _contrast_ratio(node: InspectionNode) -> float | None:
    """Text-on-background contrast, when both colours are actually painted.

    Only meaningful for an opaque, non-image background and fully opaque text:
    with a translucent background the real colour depends on what is behind it,
    which the object tree does not know.
    """
    if node.bg_opa < 255 or node.bg_image or node.text_opa < 255:
        return None
    return _ratio(_luminance(node.text_color), _luminance(node.bg_color))


def _luminance(colour: str) -> float:
    value = colour.lstrip("#")
    channels = [int(value[i : i + 2], 16) / 255.0 for i in (0, 2, 4)]
    linear = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def _ratio(a: float, b: float) -> float:
    high, low = max(a, b), min(a, b)
    return (high + 0.05) / (low + 0.05)
