"""Layout contracts over the harness' structural inspection.

These are objective geometry rules that a pixel diff cannot state: is an element
that paints something completely off screen, is it cut by the viewport or a
clipping parent, does it have a degenerate box, do two text labels land on top of
each other, is an interactive control below the minimum touch target the field UX
asks for.

Everything is read from the inspection the C harness emits (`--inspect`, see
`tools/ui_harness/src/ui_inspector.c`). Without that block the checks report
themselves as unavailable instead of guessing.
"""

from __future__ import annotations

from .capture import Capture
from .findings import ContractFinding
from .inspection import (
    Inspection,
    InspectionNode,
    effectively_hidden,
    inspection_of,
    iter_tree,
)
from .quality_config import LayoutRules

# LVGL widget classes a user taps, drags or types into.
INTERACTIVE = frozenset(
    {
        "lv_btn",
        "lv_btnmatrix",
        "lv_slider",
        "lv_switch",
        "lv_dropdown",
        "lv_checkbox",
        "lv_roller",
        "lv_textarea",
    }
)

# A text label is hidden if another text label covers most of it.
OVERLAP_RATIO = 0.5


def run_layout_checks(capture: Capture, layout: LayoutRules) -> list[ContractFinding]:
    """Evaluate the layout rules on one capture's structural inspection."""
    inspection = inspection_of(capture)
    if inspection is None:
        return [
            ContractFinding(
                rule="layout_instrumented",
                fixture=capture.name,
                ok=True,
                detail="no structural inspection in this capture (not requested)",
            )
        ]

    findings: list[ContractFinding] = []
    for node, ancestors, path in iter_tree(inspection.nodes):
        if effectively_hidden(node, ancestors):
            continue
        findings.extend(_geometry(capture.name, node, path, layout))
    findings.extend(_overlaps(capture.name, inspection))
    return findings


def _geometry(
    name: str, node: InspectionNode, path: str, layout: LayoutRules
) -> list[ContractFinding]:
    findings: list[ContractFinding] = []
    box = node.box
    describe = f"{node.cls} at {node.x},{node.y} {node.w}x{node.h}"
    paints = node.draws_content or node.cls in INTERACTIVE

    if box.w < 0 or box.h < 0:
        findings.append(
            ContractFinding(
                rule="layout_invalid_box",
                fixture=name,
                ok=False,
                detail=f"negative size: {describe}",
                path=path,
            )
        )
    elif box.empty and paints:
        findings.append(
            ContractFinding(
                rule="layout_invalid_box",
                fixture=name,
                ok=False,
                detail=f"zero-size element that paints: {describe}",
                path=path,
            )
        )

    if box.empty:
        return findings

    if node.visible is None:
        if paints:
            findings.append(
                ContractFinding(
                    rule="layout_offscreen",
                    fixture=name,
                    ok=False,
                    detail=f"{describe} paints nothing on screen (off the viewport)",
                    path=path,
                )
            )
        return findings

    tolerance = layout.clip_tolerance_px
    clipped_w = box.w - node.visible.w
    clipped_h = box.h - node.visible.h
    if paints and (clipped_w > tolerance or clipped_h > tolerance):
        findings.append(
            ContractFinding(
                rule="layout_clipped",
                fixture=name,
                ok=False,
                detail=(
                    f"{describe} cut to {node.visible.w}x{node.visible.h} "
                    f"(lost {clipped_w}x{clipped_h})"
                ),
                path=path,
            )
        )

    if _is_control(node) and (
        node.visible.w < layout.min_touch_target_px or node.visible.h < layout.min_touch_target_px
    ):
        findings.append(
            ContractFinding(
                rule="layout_touch_target",
                fixture=name,
                ok=False,
                detail=(
                    f"{describe} is {node.visible.w}x{node.visible.h}, below the "
                    f"{layout.min_touch_target_px}px touch target"
                ),
                path=path,
            )
        )
    return findings


def _is_control(node: InspectionNode) -> bool:
    """A standard LVGL control, or a clickable object that paints like one.

    The firmware's own button bar is clickable `lv_obj`s (with a background
    image), so `node.cls in INTERACTIVE` alone would miss the main control
    surface. A clickable object that paints nothing (a transparent drag strip) is
    not a control.
    """
    return node.cls in INTERACTIVE or (node.clickable and node.draws_content)


def _overlaps(name: str, inspection: Inspection) -> list[ContractFinding]:
    """Two text labels stacked on each other: one hides the other's content."""
    labels: list[tuple[InspectionNode, str]] = [
        (node, path)
        for node, ancestors, path in iter_tree(inspection.nodes)
        if node.is_label
        and node.text.strip()
        and node.visible is not None
        and not node.visible.empty
        and not effectively_hidden(node, ancestors)
    ]
    findings: list[ContractFinding] = []
    for index, (first, path_a) in enumerate(labels):
        for second, path_b in labels[index + 1 :]:
            covered = _covered_fraction(first, second)
            if covered >= OVERLAP_RATIO:
                findings.append(
                    ContractFinding(
                        rule="layout_overlap",
                        fixture=name,
                        ok=False,
                        detail=(
                            f"text labels overlap: {first.text.strip()!r} at "
                            f"{first.x},{first.y} and {second.text.strip()!r} at "
                            f"{second.x},{second.y} ({covered * 100:.0f}% covered)"
                        ),
                        path=f"{path_a}|{path_b}",
                    )
                )
    return findings


def _overlap_area(a: InspectionNode, b: InspectionNode) -> int:
    left = max(a.x, b.x)
    top = max(a.y, b.y)
    right = min(a.x + a.w, b.x + b.w)
    bottom = min(a.y + a.h, b.y + b.h)
    if right <= left or bottom <= top:
        return 0
    return (right - left) * (bottom - top)


def _covered_fraction(a: InspectionNode, b: InspectionNode) -> float:
    area = _overlap_area(a, b)
    smaller = min(a.w * a.h, b.w * b.h)
    if smaller <= 0:
        return 0.0
    return area / smaller
