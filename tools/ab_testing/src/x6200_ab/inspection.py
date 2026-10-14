"""The harness' structural inspection of the live LVGL object tree.

With `--inspect` the C harness (`tools/ui_harness/src/ui_inspector.c`) emits the
object tree of the rendered screen: absolute bounding boxes, the box actually
visible after the parents' clipping, the widget class, the label text with its
real font metrics (from `lv_txt_get_size`, never OCR), the relevant style colours
and the glyphs that fall back or are missing from every font in the chain.

This module only parses that JSON and gives the checks a tree to walk. When a
capture has no inspection block the tree is `None`, and the structural checks
report themselves as unavailable instead of inventing a verdict.
"""

from __future__ import annotations

from collections.abc import Iterator
from dataclasses import dataclass
from typing import Any

from .capture import Capture
from .errors import HarnessError


@dataclass(frozen=True)
class Box:
    """A rectangle in absolute screen coordinates."""

    x: int
    y: int
    w: int
    h: int

    @property
    def empty(self) -> bool:
        return self.w <= 0 or self.h <= 0


@dataclass(frozen=True)
class InspectionNode:
    """One LVGL object: geometry, visibility, class and, for labels, text."""

    cls: str
    x: int
    y: int
    w: int
    h: int
    visible: Box | None
    hidden: bool
    opa: int
    scrollable: bool
    bg_color: str
    bg_opa: int
    bg_image: bool
    text_color: str
    text_opa: int
    text: str
    text_font: str
    long_mode: str
    text_overflow: bool
    text_truncated: bool
    missing_glyphs: tuple[str, ...]
    fallback_glyphs: tuple[str, ...]
    children: tuple[InspectionNode, ...]

    @property
    def box(self) -> Box:
        return Box(self.x, self.y, self.w, self.h)

    @property
    def draws_content(self) -> bool:
        """Whether the object paints something a user can miss."""
        return bool(self.text.strip()) or self.bg_opa > 0 or self.bg_image

    @property
    def is_label(self) -> bool:
        return self.cls == "lv_label"


@dataclass(frozen=True)
class Inspection:
    """The whole inspected tree of one screen."""

    viewport: Box
    nodes: tuple[InspectionNode, ...]

    @property
    def count(self) -> int:
        return sum(1 for _ in iter_tree(self.nodes))


def iter_tree(
    nodes: tuple[InspectionNode, ...],
    ancestors: tuple[InspectionNode, ...] = (),
    prefix: tuple[int, ...] = (),
) -> Iterator[tuple[InspectionNode, tuple[InspectionNode, ...], str]]:
    """Yield every node with its ancestor chain (root first) and its index path."""
    for index, node in enumerate(nodes):
        path = (*prefix, index)
        yield node, ancestors, "/".join(str(part) for part in path)
        yield from iter_tree(node.children, (*ancestors, node), path)


def effectively_hidden(node: InspectionNode, ancestors: tuple[InspectionNode, ...]) -> bool:
    """A node is invisible if it, or any ancestor, is hidden or fully transparent."""
    if node.hidden or node.opa == 0:
        return True
    return any(ancestor.hidden or ancestor.opa == 0 for ancestor in ancestors)


def parse_inspection(report: dict[str, Any]) -> Inspection | None:
    """Parse the `inspection` block of a capture JSON, or `None` if absent."""
    raw = report.get("inspection")
    if not isinstance(raw, dict):
        return None
    viewport = raw.get("viewport")
    if not isinstance(viewport, dict):
        raise HarnessError("inspection block has no `viewport`")
    nodes = raw.get("nodes")
    if not isinstance(nodes, list):
        raise HarnessError("inspection block has no `nodes` list")
    return Inspection(
        viewport=Box(
            x=0,
            y=0,
            w=int(viewport["width"]),
            h=int(viewport["height"]),
        ),
        nodes=tuple(_parse_node(entry) for entry in nodes),
    )


def inspection_of(capture: Capture) -> Inspection | None:
    """The parsed inspection of a capture, or `None` when it was not requested."""
    return parse_inspection(capture.report)


def _parse_box(raw: Any) -> Box | None:
    if raw is None:
        return None
    if not isinstance(raw, dict):
        return None
    return Box(x=int(raw["x"]), y=int(raw["y"]), w=int(raw["w"]), h=int(raw["h"]))


def _parse_node(entry: dict[str, Any]) -> InspectionNode:
    children = entry.get("children", [])
    if not isinstance(children, list):
        raise HarnessError("inspection node `children` must be a list")
    return InspectionNode(
        cls=str(entry.get("class", "")),
        x=int(entry["x"]),
        y=int(entry["y"]),
        w=int(entry["w"]),
        h=int(entry["h"]),
        visible=_parse_box(entry.get("visible")),
        hidden=bool(entry.get("hidden", False)),
        opa=int(entry.get("opa", 255)),
        scrollable=bool(entry.get("scrollable", False)),
        bg_color=str(entry.get("bg_color", "#000000")),
        bg_opa=int(entry.get("bg_opa", 0)),
        bg_image=bool(entry.get("bg_image", False)),
        text_color=str(entry.get("text_color", "#000000")),
        text_opa=int(entry.get("text_opa", 255)),
        text=str(entry.get("text", "")),
        text_font=str(entry.get("text_font", "")),
        long_mode=str(entry.get("long_mode", "")),
        text_overflow=bool(entry.get("text_overflow", False)),
        text_truncated=bool(entry.get("text_truncated", False)),
        missing_glyphs=tuple(str(item) for item in entry.get("missing_glyphs", [])),
        fallback_glyphs=tuple(str(item) for item in entry.get("fallback_glyphs", [])),
        children=tuple(_parse_node(child) for child in children),
    )
