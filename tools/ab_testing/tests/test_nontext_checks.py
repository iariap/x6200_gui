"""Tests for the WCAG 1.4.11 non-text contrast over rendered pixels.

The measurement is over the committed renders (`tools/ab_testing/golden`), but
the geometry and the percentile logic are pinned with *synthetic* images first:
an object the algorithm cannot see would otherwise pass by accident, and the two
polarities (bright ink on a dark surface, dark ink on a bright one) have to be
exercised without depending on a theme's colours.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest
from numpy.typing import NDArray
from PIL import Image

from x6200_ab.errors import ConfigError
from x6200_ab.nontext_checks import (
    NonTextObject,
    load_nontext_objects,
    measure_nontext,
    run_nontext_checks,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
QUALITY_DIR = Path(__file__).resolve().parents[1] / "quality"
GOLDEN_DIR = REPO_ROOT / "tools" / "ab_testing" / "golden"

FRAMEBUFFER = (800, 480)


def _canvas(fill: tuple[int, int, int]) -> NDArray[np.uint8]:
    canvas = np.zeros((FRAMEBUFFER[1], FRAMEBUFFER[0], 3), dtype=np.uint8)
    canvas[:, :] = fill
    return canvas


def _paint(
    canvas: NDArray[np.uint8], rect: tuple[int, int, int, int], colour: tuple[int, int, int]
) -> None:
    x, y, width, height = rect
    canvas[y : y + height, x : x + width] = colour


def _object(
    *,
    object_rect: tuple[int, int, int, int] = (200, 200, 20, 20),
    background_rect: tuple[int, int, int, int] = (300, 200, 40, 40),
    min_ratio: float = 3.0,
    name: str = "synthetic",
) -> NonTextObject:
    return NonTextObject(
        name=name,
        fixture="synthetic",
        theme="synthetic",
        object_rect=object_rect,
        background_rect=background_rect,
        min_ratio=min_ratio,
        foreground_percentile=90.0,
        background_percentile=10.0,
        where="tests",
    )


def _write_png(path: Path, canvas: NDArray[np.uint8]) -> Path:
    Image.fromarray(canvas, mode="RGB").save(path)
    return path


def test_a_bright_object_on_a_dark_surface_passes() -> None:
    canvas = _canvas((0, 0, 0))
    _paint(canvas, (200, 200, 20, 20), (255, 255, 255))
    result = measure_nontext(canvas, _object())
    assert result.ok is True
    assert result.ratio == pytest.approx(21.0, abs=0.1)


def test_a_dark_object_on_a_bright_surface_passes() -> None:
    """The polarity is discovered, not declared: Outdoor's waterfall is dark ink."""
    canvas = _canvas((255, 255, 255))
    _paint(canvas, (200, 200, 20, 20), (0, 0, 0))
    result = measure_nontext(canvas, _object())
    assert result.ok is True
    assert result.ratio == pytest.approx(21.0, abs=0.1)
    assert "object darker" in result.detail


def test_two_mid_greys_fail() -> None:
    canvas = _canvas((128, 128, 128))
    _paint(canvas, (200, 200, 20, 20), (119, 119, 119))
    result = measure_nontext(canvas, _object())
    assert result.ok is False
    assert result.ratio == pytest.approx(1.13, abs=0.05)


def test_an_object_region_that_is_all_surface_cannot_pass_by_accident() -> None:
    """A rectangle that misses the object measures surface against surface."""
    canvas = _canvas((60, 60, 60))
    _paint(canvas, (200, 200, 20, 20), (255, 255, 255))  # the object is elsewhere
    result = measure_nontext(canvas, _object(object_rect=(400, 400, 20, 20)))
    assert result.ok is False
    assert result.ratio == pytest.approx(1.0, abs=0.02)


def test_a_single_antialiased_pixel_does_not_decide() -> None:
    """The object is grey with one white speck; the percentile still sees the grey."""
    canvas = _canvas((0, 0, 0))
    _paint(canvas, (200, 200, 20, 20), (128, 128, 128))
    _paint(canvas, (200, 200, 1, 1), (255, 255, 255))
    result = measure_nontext(canvas, _object())
    # #808080 on black is 5.3:1; the speck would have been 21:1.
    assert result.ratio == pytest.approx(5.32, abs=0.1)


def test_a_rect_that_runs_past_the_render_is_refused() -> None:
    canvas = _canvas((0, 0, 0))
    with pytest.raises(ConfigError, match="runs past"):
        measure_nontext(canvas, _object(object_rect=(790, 400, 40, 40)))


def test_run_checks_reports_a_missing_render(tmp_path: Path) -> None:
    objects = (_object(name="a"),)
    results, missing = run_nontext_checks({}, objects)
    assert results == ()
    assert len(missing) == 1
    assert missing[0].fixture == "synthetic"
    assert missing[0].objects == ("a",)
    # A file that is declared but absent is still missing.
    results, missing = run_nontext_checks({"synthetic": tmp_path / "nope.png"}, objects)
    assert results == ()
    assert missing[0].detail.startswith("no render")


def test_run_checks_measures_every_object_of_a_fixture(tmp_path: Path) -> None:
    canvas = _canvas((0, 0, 0))
    _paint(canvas, (200, 200, 20, 20), (255, 255, 255))
    path = _write_png(tmp_path / "synthetic.png", canvas)
    objects = (_object(name="a"), _object(name="b"))
    results, missing = run_nontext_checks({"synthetic": path}, objects)
    assert missing == ()
    assert [result.obj.name for result in results] == ["a", "b"]
    assert all(result.ok for result in results)


# --- the declared table ------------------------------------------------------


def test_the_committed_table_is_well_formed() -> None:
    objects = load_nontext_objects(QUALITY_DIR / "nontext.yaml")
    assert objects
    width, height = FRAMEBUFFER
    for obj in objects:
        assert obj.min_ratio == 3.0, f"{obj.name} is not on the 1.4.11 threshold"
        assert obj.where.strip(), f"{obj.name} does not say where it is drawn"
        for rect in (obj.object_rect, obj.background_rect):
            x, y, w, h = rect
            assert x + w <= width and y + h <= height, f"{obj.name} rect {rect} is off screen"
    # Nine themes get the S-meter and the waterfall; the focus and the strip
    # markers are single-fixture objects.
    names = {obj.name for obj in objects}
    assert {"s_meter_arc", "waterfall_signal", "focus_indicator"} <= names
    assert {"strip_marker_cw", "strip_marker_dig"} <= names


def test_the_committed_golden_renders_pass_the_declared_objects() -> None:
    """The real 1.4.11 verdict: every declared object on the committed renders."""
    objects = load_nontext_objects(QUALITY_DIR / "nontext.yaml")
    renders = {path.stem: path for path in sorted(GOLDEN_DIR.glob("*.png"))}
    results, missing = run_nontext_checks(renders, objects)
    assert missing == (), f"declared objects without a golden render: {missing}"
    assert len(results) == len(objects)
    failures = [result.detail for result in results if not result.ok]
    assert failures == [], "non-text objects below 3:1: " + "; ".join(failures)


def test_the_golden_renders_cover_every_declared_fixture() -> None:
    objects = load_nontext_objects(QUALITY_DIR / "nontext.yaml")
    renders = {path.stem for path in GOLDEN_DIR.glob("*.png")}
    assert {obj.fixture for obj in objects} <= renders


# --- the loader --------------------------------------------------------------


def test_the_loader_refuses_a_bad_table(tmp_path: Path) -> None:
    def write(text: str) -> Path:
        path = tmp_path / "nontext.yaml"
        path.write_text(text)
        return path

    header = "foreground_percentile: 90\nbackground_percentile: 10\nobjects:\n"
    with pytest.raises(ConfigError, match="`objects` must be a non-empty list"):
        load_nontext_objects(write("objects: []\n"))
    with pytest.raises(ConfigError, match="does not say where"):
        load_nontext_objects(
            write(
                header + "  - name: a\n    fixture: f\n    object: [1, 1, 1, 1]\n"
                "    background: [2, 2, 1, 1]\n    min_ratio: 3.0\n"
            )
        )
    with pytest.raises(ConfigError, match=r"needs .object."):
        load_nontext_objects(
            write(
                header + "  - name: a\n    fixture: f\n    background: [2, 2, 1, 1]\n"
                '    min_ratio: 3.0\n    where: "x"\n'
            )
        )
    with pytest.raises(ConfigError, match="non-positive"):
        load_nontext_objects(
            write(
                header + "  - name: a\n    fixture: f\n    object: [1, 1, 0, 1]\n"
                "    background: [2, 2, 1, 1]\n"
                '    min_ratio: 3.0\n    where: "x"\n'
            )
        )
    with pytest.raises(ConfigError, match="listed twice"):
        load_nontext_objects(
            write(
                header + "  - name: a\n    fixture: f\n    object: [1, 1, 2, 2]\n"
                '    background: [4, 4, 2, 2]\n    min_ratio: 3.0\n    where: "x"\n'
                + "  - name: a\n    fixture: f\n    object: [1, 1, 2, 2]\n"
                '    background: [4, 4, 2, 2]\n    min_ratio: 3.0\n    where: "x"\n'
            )
        )
