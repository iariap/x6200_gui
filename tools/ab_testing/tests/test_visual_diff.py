"""Tests for the pixel diff, bounding boxes and semantic regions."""

from __future__ import annotations

from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from helpers import blank_image, draw_box, make_capture, revision
from x6200_ab.regions import RegionMap
from x6200_ab.visual_diff import DiffResult, compute_diff, unexplained_changed_pixels

Image = NDArray[np.uint8]


def _diff(
    tmp_path: Path,
    a_image: Image,
    b_image: Image,
    region_map: RegionMap,
) -> DiffResult:
    a = make_capture(tmp_path / "a", "home", a_image, rev=revision("a", "a" * 40))
    b = make_capture(tmp_path / "b", "home", b_image, rev=revision("b", "b" * 40))
    return compute_diff(
        a,
        b,
        region_map,
        pixel_tolerance=12,
        min_box_pixels=4,
        diff_png=tmp_path / "diffs" / "home-diff.png",
        overlay_png=tmp_path / "diffs" / "home-overlay.png",
    )


def test_identical_captures_have_no_diff(tmp_path: Path, region_map: RegionMap) -> None:
    image = draw_box(blank_image(), 100, 100, 40, 20)
    result = _diff(tmp_path, image, image, region_map)
    assert result.identical
    assert result.changed_pixels == 0
    assert result.boxes == ()
    # identical captures do not need noise evidence images
    assert result.diff_png is None
    assert result.overlay_png is None


def test_changed_box_is_localised(tmp_path: Path, region_map: RegionMap) -> None:
    a_image = blank_image()
    b_image = draw_box(a_image, 300, 430, 60, 30)
    result = _diff(tmp_path, a_image, b_image, region_map)
    assert not result.identical
    assert result.changed_pixels == 60 * 30
    assert len(result.boxes) == 1
    box = result.boxes[0]
    assert (box.x0, box.y0, box.x1, box.y1) == (300, 430, 360, 460)
    # (300, 430) falls in the button-bar region
    assert "botones" in result.changed_region_names()
    assert result.diff_png is not None and result.diff_png.is_file()
    assert result.overlay_png is not None and result.overlay_png.is_file()


def test_dimension_mismatch_is_reported(tmp_path: Path, region_map: RegionMap) -> None:
    a_image = blank_image(800, 480)
    b_image = blank_image(640, 480)
    result = _diff(tmp_path, a_image, b_image, region_map)
    assert result.dimension_mismatch == ((800, 480), (640, 480))
    assert result.boxes == ()
    assert not result.identical


def test_unexplained_pixels_respect_allowed_regions(tmp_path: Path, region_map: RegionMap) -> None:
    a_image = blank_image()
    # inside the `multiplicador` region
    b_image = draw_box(a_image, 762, 12, 28, 8)
    result = _diff(tmp_path, a_image, b_image, region_map)
    assert result.changed_pixels > 0
    assert unexplained_changed_pixels(result, region_map, frozenset({"multiplicador"})) == 0
    assert unexplained_changed_pixels(result, region_map, frozenset()) == result.changed_pixels
