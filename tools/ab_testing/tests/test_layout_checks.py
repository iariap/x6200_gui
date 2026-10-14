"""Tests for the structural layout contracts (no Docker)."""

from __future__ import annotations

from pathlib import Path

from helpers import blank_image, inspection, make_capture, node, revision
from x6200_ab.layout_checks import run_layout_checks
from x6200_ab.quality_config import LayoutRules

LAYOUT = LayoutRules(min_touch_target_px=44, clip_tolerance_px=0)


def _capture(tmp_path: Path, nodes: list[dict]):
    return make_capture(
        tmp_path, "home", blank_image(), rev=revision(), inspection=inspection(nodes)
    )


def _rules(findings, rule: str) -> list:
    return [finding for finding in findings if finding.rule == rule]


def test_clean_tree_has_no_findings(tmp_path: Path) -> None:
    capture = _capture(tmp_path, [node(cls="lv_label", x=10, y=10, w=100, h=20, text="OK")])
    findings = run_layout_checks(capture, LAYOUT)
    assert not [finding for finding in findings if not finding.ok]


def test_offscreen_element_is_reported(tmp_path: Path) -> None:
    # paints something (text) but the viewport shows none of it
    offscreen = node(cls="lv_label", x=-200, y=10, w=100, h=20, visible=None, text="hi")
    findings = run_layout_checks(_capture(tmp_path, [offscreen]), LAYOUT)
    assert _rules(findings, "layout_offscreen") and not _rules(findings, "layout_offscreen")[0].ok


def test_clipped_element_is_reported(tmp_path: Path) -> None:
    clipped = node(
        cls="lv_obj",
        x=0,
        y=460,
        w=160,
        h=62,
        visible={"x": 0, "y": 460, "w": 160, "h": 20},
        bg_image=True,
    )
    findings = run_layout_checks(_capture(tmp_path, [clipped]), LAYOUT)
    assert _rules(findings, "layout_clipped")


def test_zero_sized_element_that_paints_is_reported(tmp_path: Path) -> None:
    empty = node(cls="lv_label", x=0, y=0, w=0, h=12, visible=None, text="x")
    findings = run_layout_checks(_capture(tmp_path, [empty]), LAYOUT)
    assert _rules(findings, "layout_invalid_box")


def test_empty_label_is_not_a_defect(tmp_path: Path) -> None:
    empty = node(cls="lv_label", x=0, y=0, w=0, h=12, visible=None, text="")
    findings = run_layout_checks(_capture(tmp_path, [empty]), LAYOUT)
    assert not [finding for finding in findings if not finding.ok]


def test_small_touch_target_is_reported(tmp_path: Path) -> None:
    small = node(cls="lv_btn", x=10, y=10, w=20, h=20)
    findings = run_layout_checks(_capture(tmp_path, [small]), LAYOUT)
    assert _rules(findings, "layout_touch_target")


def test_big_enough_touch_target_is_fine(tmp_path: Path) -> None:
    big = node(cls="lv_btn", x=10, y=10, w=120, h=48)
    findings = run_layout_checks(_capture(tmp_path, [big]), LAYOUT)
    assert not _rules(findings, "layout_touch_target")


def test_custom_clickable_button_below_target_is_reported(tmp_path: Path) -> None:
    # the firmware's button bar is clickable lv_obj with a background image
    button = node(cls="lv_obj", x=0, y=0, w=160, h=30, clickable=True, bg_image=True)
    findings = run_layout_checks(_capture(tmp_path, [button]), LAYOUT)
    assert _rules(findings, "layout_touch_target")


def test_transparent_clickable_strip_is_not_a_control(tmp_path: Path) -> None:
    # a clickable object that paints nothing is a drag strip, not a control
    strip = node(cls="lv_obj", x=0, y=0, w=800, h=24, clickable=True)
    findings = run_layout_checks(_capture(tmp_path, [strip]), LAYOUT)
    assert not _rules(findings, "layout_touch_target")


def test_two_overlapping_text_labels_are_reported(tmp_path: Path) -> None:
    a = node(cls="lv_label", x=100, y=100, w=80, h=20, text="first")
    b = node(cls="lv_label", x=100, y=100, w=80, h=20, text="second")
    findings = run_layout_checks(_capture(tmp_path, [a, b]), LAYOUT)
    assert _rules(findings, "layout_overlap")


def test_disjoint_labels_are_fine(tmp_path: Path) -> None:
    a = node(cls="lv_label", x=0, y=0, w=80, h=20, text="first")
    b = node(cls="lv_label", x=200, y=0, w=80, h=20, text="second")
    findings = run_layout_checks(_capture(tmp_path, [a, b]), LAYOUT)
    assert not _rules(findings, "layout_overlap")


def test_hidden_element_is_ignored(tmp_path: Path) -> None:
    hidden = node(cls="lv_label", x=-200, y=0, w=100, h=20, visible=None, hidden=True, text="hi")
    findings = run_layout_checks(_capture(tmp_path, [hidden]), LAYOUT)
    assert not [finding for finding in findings if not finding.ok]


def _scroll_dialog(children: list[dict]) -> list[dict]:
    """A dialog-sized scroll area (like the Ajustes grid) with `children` inside."""
    grid = node(
        cls="lv_obj",
        x=2,
        y=66,
        w=796,
        h=330,
        scrollable=True,
        scroll_dir="ver",
        bg_opa=255,
        children=children,
    )
    return [grid]


def test_content_scrolled_out_of_a_scroll_area_is_not_offscreen(tmp_path: Path) -> None:
    # the Ajustes grid: rows below the fold are reachable by scrolling
    row = node(cls="lv_label", x=22, y=600, w=200, h=28, visible=None, text="Timeout")
    findings = run_layout_checks(_capture(tmp_path, _scroll_dialog([row])), LAYOUT)
    assert not _rules(findings, "layout_offscreen")


def test_content_pushed_off_the_viewport_is_still_offscreen(tmp_path: Path) -> None:
    # no scroll area: the object is simply off the screen
    row = node(cls="lv_label", x=22, y=600, w=200, h=28, visible=None, text="Timeout")
    findings = run_layout_checks(_capture(tmp_path, [row]), LAYOUT)
    assert _rules(findings, "layout_offscreen")


def test_the_screen_itself_is_not_a_scroll_area(tmp_path: Path) -> None:
    # the screen reports scrollable + scroll_dir all (LVGL default); a widget it
    # pushes off the viewport is a layout defect, not content to scroll into view
    screen = node(
        cls="lv_obj",
        x=0,
        y=0,
        w=800,
        h=480,
        scrollable=True,
        scroll_dir="all",
        children=[node(cls="lv_btn", x=0, y=478, w=160, h=62, visible=None, bg_image=True)],
    )
    findings = run_layout_checks(_capture(tmp_path, [screen]), LAYOUT)
    assert _rules(findings, "layout_offscreen")


def test_clip_along_the_scroll_axis_is_not_a_defect(tmp_path: Path) -> None:
    half = node(
        cls="lv_dropdown",
        x=499,
        y=391,
        w=285,
        h=56,
        visible={"x": 499, "y": 391, "w": 285, "h": 14},
    )
    findings = run_layout_checks(_capture(tmp_path, _scroll_dialog([half])), LAYOUT)
    assert not _rules(findings, "layout_clipped")


def test_clip_off_the_scroll_axis_is_still_a_defect(tmp_path: Path) -> None:
    # the grid scrolls vertically only: a box that overhangs it horizontally is a
    # real clip even though it lives in the scroll area
    wide = node(
        cls="lv_obj",
        x=740,
        y=113,
        w=84,
        h=28,
        visible={"x": 740, "y": 113, "w": 58, "h": 28},
        bg_opa=255,
    )
    findings = run_layout_checks(_capture(tmp_path, _scroll_dialog([wide])), LAYOUT)
    assert _rules(findings, "layout_clipped")


def test_default_scroll_dir_is_not_a_scroll_area(tmp_path: Path) -> None:
    # a spinbox clips its own 4-digit label by 4px (the Ajustes year field); it
    # carries LVGL's default `scroll_dir: all`, so it is a real clip, not content
    # the user can scroll to
    spinbox = node(
        cls="other",
        x=691,
        y=100,
        w=93,
        h=56,
        scrollable=True,
        scroll_dir="all",
        bg_opa=255,
        children=[
            node(
                cls="lv_label",
                x=704,
                y=113,
                w=84,
                h=28,
                visible={"x": 704, "y": 113, "w": 80, "h": 28},
                text="2026",
            )
        ],
    )
    findings = run_layout_checks(_capture(tmp_path, [spinbox]), LAYOUT)
    assert _rules(findings, "layout_clipped")


def test_scrolled_control_measures_with_its_real_size(tmp_path: Path) -> None:
    half = node(
        cls="lv_dropdown",
        x=499,
        y=391,
        w=285,
        h=56,
        visible={"x": 499, "y": 391, "w": 285, "h": 14},
    )
    findings = run_layout_checks(_capture(tmp_path, _scroll_dialog([half])), LAYOUT)
    assert not _rules(findings, "layout_touch_target")


def test_small_scrolled_control_is_still_reported(tmp_path: Path) -> None:
    # the switch of Ajustes is 30px tall wherever it is; scrolling it into view
    # does not make it a 44px target
    switch = node(cls="lv_switch", x=514, y=94, w=111, h=30)
    findings = run_layout_checks(_capture(tmp_path, _scroll_dialog([switch])), LAYOUT)
    assert _rules(findings, "layout_touch_target")


def test_label_hidden_by_a_surface_above_is_not_an_overlap(tmp_path: Path) -> None:
    behind = node(cls="lv_label", x=0, y=160, w=150, h=36, text="14.074.000")
    dialog_label = node(cls="lv_label", x=22, y=173, w=200, h=28, text="Hour, Min, Sec")
    surface = node(cls="lv_obj", x=2, y=66, w=796, h=330, bg_opa=255)
    # paint order: the screen behind, then the surface, then the label on it
    findings = run_layout_checks(_capture(tmp_path, [behind, surface, dialog_label]), LAYOUT)
    assert not _rules(findings, "layout_overlap")


def test_overlap_inside_the_surface_is_still_reported(tmp_path: Path) -> None:
    # two labels on the same surface: the surface is painted below both, so it
    # hides neither of them
    surface = node(
        cls="lv_obj",
        x=2,
        y=66,
        w=796,
        h=330,
        bg_opa=255,
        children=[
            node(cls="lv_label", x=22, y=100, w=200, h=28, text="first"),
            node(cls="lv_label", x=22, y=100, w=200, h=28, text="second"),
        ],
    )
    findings = run_layout_checks(_capture(tmp_path, [surface]), LAYOUT)
    assert _rules(findings, "layout_overlap")


def test_hidden_ancestor_hides_children(tmp_path: Path) -> None:
    child = node(cls="lv_label", x=-500, y=0, w=100, h=20, visible=None, text="hi")
    parent = node(cls="lv_obj", x=0, y=0, w=800, h=480, hidden=True, children=[child])
    findings = run_layout_checks(_capture(tmp_path, [parent]), LAYOUT)
    assert not [finding for finding in findings if not finding.ok]


def test_without_inspection_is_informative_not_a_failure(tmp_path: Path) -> None:
    capture = make_capture(tmp_path, "home", blank_image(), rev=revision())
    findings = run_layout_checks(capture, LAYOUT)
    assert [finding.rule for finding in findings] == ["layout_instrumented"]
    assert findings[0].ok
