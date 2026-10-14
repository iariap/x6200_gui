"""Layout contracts over the harness' structural inspection.

These are objective geometry rules that a pixel diff cannot state: is an element
that paints something completely off screen, is it cut by the viewport or a
clipping parent, does it have a degenerate box, do two text labels land on top of
each other, is an interactive control below the minimum touch target the field UX
asks for.

Two facts keep the rules honest on a surface that is not a single static screen
(the Ajustes window is a scrolling dialog drawn over the main screen):

* **scrolling containers.** Content the user can scroll to is not "off screen":
  an object that leaves an ancestor that is a scroll area on that axis is
  reachable, so `layout_offscreen`/`layout_clipped` do not fire for it and the
  touch target is measured on the control's real size, not on the sliver still on
  view. The viewport itself is never a scroll area — content pushed off it is
  the defect the rule is about.
* **occlusion.** A label an opaque object painted above it hides in the region it
  overlaps is not "two stacked texts": the user sees one label, not two.

Everything is read from the inspection the C harness emits (`--inspect`, see
`tools/ui_harness/src/ui_inspector.c`). Without that block the checks report
themselves as unavailable instead of guessing.
"""

from __future__ import annotations

from .capture import Capture
from .findings import ContractFinding
from .inspection import (
    Box,
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
        findings.extend(_geometry(capture.name, node, ancestors, path, inspection, layout))
    findings.extend(_overlaps(capture.name, inspection))
    return findings


def _is_scroll_area(node: InspectionNode, viewport: Box) -> bool:
    """A container the user scrolls, as opposed to the viewport or a default box.

    The object has to be scrollable, to name **one** axis it scrolls on, and to
    be a strict sub-region of the viewport. Both exclusions matter:

    * the screen reports `scroll_dir: all` (LVGL's default for any object that
      never configured scrolling), so a widget pushed off the *viewport* by a
      layout defect is not mistaken for list content that needs scrolling into
      view;
    * every sub-widget (a spinbox, a dropdown) also carries the default `all`,
      and one of those clipping *its own* label is a real clip, not scroll
      content. The firmware's scroll areas (the Ajustes grid, its dialog box)
      declare one axis.
    """
    if not node.scrollable or node.scroll_dir not in ("hor", "ver"):
        return False
    box = node.box
    return (
        box.x > viewport.x
        and box.y > viewport.y
        and box.x + box.w < viewport.x + viewport.w
        and box.y + box.h < viewport.y + viewport.h
    )


def _scroll_reach(
    node: InspectionNode, ancestors: tuple[InspectionNode, ...], viewport: Box
) -> tuple[bool, bool]:
    """(horizontally, vertically) reachable by scrolling into view.

    True per axis when the node leaves a scroll area on that axis: the container
    is a scroll area (see `_is_scroll_area`) and scrolls on the axis the node
    overflows it on. A fixed container (or the viewport) does not make content
    reachable, so a real clip/overflow stays a finding.
    """
    horizontal = vertical = False
    for ancestor in reversed(ancestors):
        if not _is_scroll_area(ancestor, viewport):
            continue
        box = ancestor.box
        horizontal = horizontal or (
            ancestor.scrolls_horizontally and (node.x < box.x or node.x + node.w > box.x + box.w)
        )
        vertical = vertical or (
            ancestor.scrolls_vertically and (node.y < box.y or node.y + node.h > box.y + box.h)
        )
    return horizontal, vertical


def _geometry(
    name: str,
    node: InspectionNode,
    ancestors: tuple[InspectionNode, ...],
    path: str,
    inspection: Inspection,
    layout: LayoutRules,
) -> list[ContractFinding]:
    findings: list[ContractFinding] = []
    box = node.box
    describe = f"{node.cls} at {node.x},{node.y} {node.w}x{node.h}"
    paints = node.draws_content or node.cls in INTERACTIVE
    reach_horizontal, reach_vertical = _scroll_reach(node, ancestors, inspection.viewport)

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
        if paints and not (reach_horizontal or reach_vertical):
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
    cut_horizontal = box.w - node.visible.w > tolerance
    cut_vertical = box.h - node.visible.h > tolerance
    # Every axis the box is cut on has to be reachable by scrolling; a cut on an
    # axis the container does not scroll stays a finding (a real clip).
    cut_reachable = all(
        reach
        for cut, reach in ((cut_horizontal, reach_horizontal), (cut_vertical, reach_vertical))
        if cut
    )
    if paints and (cut_horizontal or cut_vertical) and not cut_reachable:
        clipped_w = box.w - node.visible.w
        clipped_h = box.h - node.visible.h
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

    if _is_control(node):
        # A control that is scrolled out measures with its real size: the sliver
        # on view is not the target the user aims at.
        if (cut_horizontal or cut_vertical) and cut_reachable:
            target_w, target_h = box.w, box.h
        else:
            target_w, target_h = node.visible.w, node.visible.h
        if target_w < layout.min_touch_target_px or target_h < layout.min_touch_target_px:
            findings.append(
                ContractFinding(
                    rule="layout_touch_target",
                    fixture=name,
                    ok=False,
                    detail=(
                        f"{describe} is {target_w}x{target_h}, below the "
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
    """Two text labels stacked on each other: one hides the other's content.

    A pair whose overlapping region an opaque object painted above the lower
    label covers is not two texts the user sees stacked: it is one label on top
    of a surface (a dialog's text over the screen behind it), and the lower
    label's ink is not on screen there. The order is the tree's paint order
    (depth-first, children in order); ancestors paint before their children, so
    a surface only hides a label painted after it.
    """
    order: dict[str, int] = {
        path: index for index, (_node, _ancestors, path) in enumerate(iter_tree(inspection.nodes))
    }
    opaque: list[tuple[InspectionNode, str]] = [
        (node, path)
        for node, ancestors, path in iter_tree(inspection.nodes)
        if _opaque(node) and not effectively_hidden(node, ancestors)
    ]
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
            if covered < OVERLAP_RATIO:
                continue
            if _overlap_hidden(first, path_a, second, path_b, order, opaque):
                continue
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


def _opaque(node: InspectionNode) -> bool:
    """Whether the object covers what is behind it with a solid surface."""
    return node.bg_opa >= 255 or node.bg_image


def _overlap_hidden(
    first: InspectionNode,
    path_a: str,
    second: InspectionNode,
    path_b: str,
    order: dict[str, int],
    opaque: list[tuple[InspectionNode, str]],
) -> bool:
    """True when the pair's overlapping region is covered by a surface above the lower label."""
    left = max(first.x, second.x)
    top = max(first.y, second.y)
    right = min(first.x + first.w, second.x + second.w)
    bottom = min(first.y + first.h, second.y + second.h)
    if right <= left or bottom <= top:
        return False
    lower = min(order[path_a], order[path_b])
    return any(
        order[path] > lower
        and surface.x <= left
        and surface.y <= top
        and surface.x + surface.w >= right
        and surface.y + surface.h >= bottom
        for surface, path in opaque
    )


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
