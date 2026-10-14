"""Pixel diff between two captures: area, boxes, per-region, evidence images.

Pillow loads the PNGs and NumPy does the arithmetic. A pixel counts as changed
when the summed absolute RGB difference exceeds a tolerance (so a single
invisible bit flip is not a "regression"). Changed pixels are grouped into
bounding boxes with a flood fill, mapped onto the semantic regions, and drawn
into two evidence images: `diff.png` (context + red changes) and `overlay.png`
(A blended with B, red boxes). Incompatible framebuffer sizes are reported as a
dimension mismatch instead of a crash.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
from numpy.typing import NDArray
from PIL import Image, ImageDraw

from .capture import Capture
from .regions import Region, RegionMap

RgbArray = NDArray[np.int16]
BoolArray = NDArray[np.bool_]

# Above this many changed pixels the flood fill is skipped and one overall box
# is reported: connected components are O(N) in Python and a full-screen change
# is not worth seconds of CPU for a report that already says "everything moved".
_MAX_COMPONENT_PIXELS = 150_000


@dataclass(frozen=True)
class BoundingBox:
    """A rectangle of changed pixels, with how many it contains."""

    x0: int
    y0: int
    x1: int
    y1: int
    pixels: int

    @property
    def width(self) -> int:
        return self.x1 - self.x0

    @property
    def height(self) -> int:
        return self.y1 - self.y0

    @property
    def box(self) -> tuple[int, int, int, int]:
        return (self.x0, self.y0, self.x1, self.y1)

    def as_dict(self) -> dict[str, Any]:
        return {
            "x0": self.x0,
            "y0": self.y0,
            "x1": self.x1,
            "y1": self.y1,
            "width": self.width,
            "height": self.height,
            "pixels": self.pixels,
        }


@dataclass(frozen=True)
class RegionDiff:
    """How much of one semantic region changed."""

    region: Region
    changed: int
    total: int
    fraction: float

    def as_dict(self) -> dict[str, Any]:
        return {
            "region": self.region.name,
            "changed_pixels": self.changed,
            "total_pixels": self.total,
            "fraction": round(self.fraction, 6),
        }


@dataclass(frozen=True)
class DiffResult:
    """The full outcome of comparing one scenario between A and B."""

    scenario: str
    identical: bool
    changed_pixels: int
    total_pixels: int
    changed_fraction: float
    boxes: tuple[BoundingBox, ...]
    regions: tuple[RegionDiff, ...]
    dimension_mismatch: tuple[tuple[int, int], tuple[int, int]] | None
    diff_png: Path | None
    overlay_png: Path | None
    changed_mask: BoolArray | None = None

    def boxes_dicts(self) -> list[dict[str, Any]]:
        return [box.as_dict() for box in self.boxes]

    def regions_dicts(self) -> list[dict[str, Any]]:
        return [region.as_dict() for region in self.regions]

    def changed_region_names(self) -> list[str]:
        return [region.region.name for region in self.regions if region.changed > 0]


def load_rgb(path: Path) -> RgbArray:
    """Load a PNG as an `(H, W, 3)` int16 array (int16 avoids uint underflow)."""
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.int16)


def changed_mask(a: RgbArray, b: RgbArray, tolerance: int) -> BoolArray:
    """Boolean mask of pixels whose summed |ΔRGB| exceeds `tolerance`."""
    return np.abs(a - b).sum(axis=2) > tolerance


def region_content_fraction(array: RgbArray, region: Region, tolerance: int) -> float:
    """Fraction of a region's pixels that deviate from its median colour.

    A blank/uniform region scores ~0; a region with text or graphics scores
    clearly above 0. Used both by the quality contracts ("this region is not
    empty") and by the known-issue detectors ("this region is not showing
    something it should not").
    """
    patch = array[region.y : region.y + region.height, region.x : region.x + region.width]
    flat = patch.reshape(-1, 3)
    if flat.size == 0:
        return 0.0
    median = np.median(flat, axis=0)
    deviation = np.abs(flat - median).sum(axis=1)
    return float(np.count_nonzero(deviation > tolerance) / flat.shape[0])


def bounding_boxes(mask: BoolArray, min_pixels: int) -> tuple[BoundingBox, ...]:
    """Group changed pixels into bounding boxes (largest first)."""
    ys, xs = np.nonzero(mask)
    if ys.size == 0:
        return ()
    if ys.size > _MAX_COMPONENT_PIXELS:
        return (
            BoundingBox(
                int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1, int(ys.size)
            ),
        )
    pending = set(zip(ys.tolist(), xs.tolist(), strict=True))
    boxes: list[BoundingBox] = []
    while pending:
        start = pending.pop()
        stack = [start]
        min_x = max_x = start[1]
        min_y = max_y = start[0]
        count = 0
        while stack:
            y, x = stack.pop()
            count += 1
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)
            for neighbour in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if neighbour in pending:
                    pending.remove(neighbour)
                    stack.append(neighbour)
        if count >= min_pixels:
            boxes.append(BoundingBox(min_x, min_y, max_x + 1, max_y + 1, count))
    boxes.sort(key=lambda box: box.pixels, reverse=True)
    return tuple(boxes)


def _region_diffs(mask: BoolArray, region_map: RegionMap) -> tuple[RegionDiff, ...]:
    diffs: list[RegionDiff] = []
    for region in region_map.regions.values():
        sub = mask[region.y : region.y + region.height, region.x : region.x + region.width]
        total = int(sub.size)
        changed = int(sub.sum())
        diffs.append(
            RegionDiff(
                region=region,
                changed=changed,
                total=total,
                fraction=(changed / total) if total else 0.0,
            )
        )
    return tuple(diffs)


def _write_diff(a: RgbArray, mask: BoolArray, path: Path) -> None:
    grey = a.mean(axis=2) * 0.35
    canvas = np.repeat(grey[:, :, np.newaxis], 3, axis=2).astype(np.uint8)
    canvas[mask] = (230, 40, 40)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(canvas).save(path)


def _write_overlay(a: RgbArray, b: RgbArray, boxes: tuple[BoundingBox, ...], path: Path) -> None:
    blend = ((a.astype(np.int32) + b.astype(np.int32)) // 2).astype(np.uint8)
    image = Image.fromarray(blend)
    draw = ImageDraw.Draw(image)
    for box in boxes:
        draw.rectangle(
            [box.x0, box.y0, max(box.x0, box.x1 - 1), max(box.y0, box.y1 - 1)],
            outline=(230, 40, 40),
            width=2,
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def compute_diff(
    a: Capture,
    b: Capture,
    region_map: RegionMap,
    *,
    pixel_tolerance: int,
    min_box_pixels: int,
    diff_png: Path | None = None,
    overlay_png: Path | None = None,
) -> DiffResult:
    """Compare two captures of the same scenario and write the evidence images."""
    scenario = a.name
    a_array = load_rgb(a.png)
    b_array = load_rgb(b.png)
    if a_array.shape != b_array.shape:
        return DiffResult(
            scenario=scenario,
            identical=False,
            changed_pixels=0,
            total_pixels=0,
            changed_fraction=0.0,
            boxes=(),
            regions=(),
            dimension_mismatch=(_shape(a_array), _shape(b_array)),
            diff_png=None,
            overlay_png=None,
        )
    mask = changed_mask(a_array, b_array, pixel_tolerance)
    changed = int(mask.sum())
    total = int(mask.size)
    boxes = bounding_boxes(mask, min_box_pixels)
    written_diff: Path | None = None
    written_overlay: Path | None = None
    if changed > 0:
        if diff_png is not None:
            _write_diff(a_array, mask, diff_png)
            written_diff = diff_png
        if overlay_png is not None:
            _write_overlay(a_array, b_array, boxes, overlay_png)
            written_overlay = overlay_png
    return DiffResult(
        scenario=scenario,
        identical=changed == 0,
        changed_pixels=changed,
        total_pixels=total,
        changed_fraction=(changed / total) if total else 0.0,
        boxes=boxes,
        regions=_region_diffs(mask, region_map),
        dimension_mismatch=None,
        diff_png=written_diff,
        overlay_png=written_overlay,
        changed_mask=mask,
    )


def unexplained_changed_pixels(
    diff: DiffResult, region_map: RegionMap, allowed_regions: frozenset[str]
) -> int:
    """Changed pixels that fall outside every allowed (explained) region."""
    if diff.changed_mask is None:
        return 0
    explained = np.zeros_like(diff.changed_mask)
    for name in allowed_regions:
        region = region_map.regions.get(name)
        if region is None:
            continue
        explained[region.y : region.y + region.height, region.x : region.x + region.width] = True
    return int(np.count_nonzero(diff.changed_mask & ~explained))


def _shape(array: RgbArray) -> tuple[int, int]:
    return (int(array.shape[1]), int(array.shape[0]))
