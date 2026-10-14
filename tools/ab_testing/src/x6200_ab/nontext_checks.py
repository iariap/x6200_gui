"""WCAG 1.4.11 over the pixels the harness renders: non-text contrast.

The static contract (`theme_files.py`) reads roles; the typography contract reads
the live LVGL object tree. Neither can answer 1.4.11 for an object whose *both*
the ink and the surface are painted by code the framework cannot introspect: the
S-meter arc is drawn by hand over a bitmap face (`images/top_long.bin`, a binary
the static check only knows as a filename), the waterfall column comes from the
DSP palette, the band-plan chips carry hand-picked literals. For those the PNG
the harness produces *is* the source of truth, so the measurement is over pixels.

What is measured, and how (never a guess):

* every object is a **declared, fixed rectangle** in `quality/nontext.yaml`, from
  the code that draws it (`where`), the same idiom the `region_*` contracts use;
* the object's colour is the `foreground_percentile` pixel of its rectangle
  (luminance-sorted) and the surface's is the `background_percentile` pixel of
  the background rectangle — so a single anti-aliased pixel cannot decide, and a
  region that is mostly surface cannot be mistaken for ink;
* the object may be lighter *or* darker than its surface (Outdoor's waterfall is
  dark ink on a white face, Classic's is bright ink on black), so both pairings
  are tried and the contrast reported is the one the user can actually see
  (the object against the surface is distinguishable in exactly one direction);
* the threshold is 3.0:1 (WCAG 1.4.11, non-text contrast).

The rendered PNGs are committed under `tools/ab_testing/golden/`, so the check is
offline and deterministic like the rest of `x6200-ab themes`; `--renders-dir`
points it at a fresh capture instead.
"""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
from numpy.typing import NDArray
from PIL import Image

from .configio import as_float, read_yaml_mapping
from .contrast import contrast_ratio, luminance
from .errors import ConfigError

RULE_NONTEXT_PREFIX = "nontext:"

# The pixel coordinates are [x, y, width, height] over the 800x480 framebuffer.
Rect = tuple[int, int, int, int]


@dataclass(frozen=True)
class NonTextObject:
    """One informative graphic, its surface, and where in the code it is drawn."""

    name: str
    fixture: str
    theme: str
    object_rect: Rect
    background_rect: Rect
    min_ratio: float
    foreground_percentile: float
    background_percentile: float
    where: str = ""

    @property
    def rule(self) -> str:
        return f"{RULE_NONTEXT_PREFIX}{self.name}"

    def as_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "fixture": self.fixture,
            "theme": self.theme,
            "object": list(self.object_rect),
            "background": list(self.background_rect),
            "min_ratio": self.min_ratio,
            "foreground_percentile": self.foreground_percentile,
            "background_percentile": self.background_percentile,
            "where": self.where,
        }


@dataclass(frozen=True)
class NonTextResult:
    """One object measured on one fixture."""

    obj: NonTextObject
    ratio: float
    foreground: str
    background: str
    ok: bool

    @property
    def detail(self) -> str:
        polarity = (
            "object lighter"
            if luminance(self.foreground) >= luminance(self.background)
            else "object darker"
        )
        return (
            f"{self.obj.name} on {self.obj.fixture} = {self.ratio:.2f}:1 "
            f"(min {self.obj.min_ratio:.1f}:1) [{polarity}; {self.foreground} vs "
            f"{self.background}]"
        )

    def as_dict(self) -> dict[str, Any]:
        return {
            "name": self.obj.name,
            "fixture": self.obj.fixture,
            "theme": self.obj.theme,
            "rule": self.obj.rule,
            "ratio": round(self.ratio, 2),
            "min_ratio": self.obj.min_ratio,
            "ok": self.ok,
            "foreground": self.foreground,
            "background": self.background,
            "where": self.obj.where,
            "detail": self.detail,
        }


@dataclass(frozen=True)
class MissingRender:
    """A declared object whose fixture has no committed render to measure."""

    fixture: str
    objects: tuple[str, ...]
    detail: str

    def as_dict(self) -> dict[str, Any]:
        return {"fixture": self.fixture, "objects": list(self.objects), "detail": self.detail}


def load_nontext_objects(path: Path) -> tuple[NonTextObject, ...]:
    """The declared objects of `quality/nontext.yaml`."""
    data = read_yaml_mapping(path)
    default_fg = as_float(data, "foreground_percentile", path, default=90.0)
    default_bg = as_float(data, "background_percentile", path, default=10.0)
    raw = data.get("objects")
    if not isinstance(raw, list) or not raw:
        raise ConfigError(f"{path}: `objects` must be a non-empty list")
    objects: list[NonTextObject] = []
    seen: set[tuple[str, str]] = set()
    for entry in raw:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: every object must be a mapping")
        name = _required_str(entry, "name", path)
        fixture = _required_str(entry, "fixture", path)
        where = str(entry.get("where", "")).strip()
        if not where:
            raise ConfigError(f"{path}: object `{name}` does not say where it is drawn")
        if (fixture, name) in seen:
            raise ConfigError(f"{path}: object `{name}` is listed twice for `{fixture}`")
        seen.add((fixture, name))
        objects.append(
            NonTextObject(
                name=name,
                fixture=fixture,
                theme=str(entry.get("theme", "")),
                object_rect=_rect(entry, "object", path, name),
                background_rect=_rect(entry, "background", path, name),
                min_ratio=as_float(entry, "min_ratio", path),
                foreground_percentile=as_float(
                    entry, "foreground_percentile", path, default=default_fg
                ),
                background_percentile=as_float(
                    entry, "background_percentile", path, default=default_bg
                ),
                where=where,
            )
        )
    return tuple(objects)


def _required_str(entry: dict[str, Any], key: str, path: Path) -> str:
    value = entry.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{path}: object missing `{key}`")
    return value.strip()


def _rect(entry: dict[str, Any], key: str, path: Path, name: str) -> Rect:
    value = entry.get(key)
    if not isinstance(value, list) or len(value) != 4:
        raise ConfigError(f"{path}: object `{name}` needs `{key}: [x, y, width, height]`")
    try:
        x, y, width, height = (int(channel) for channel in value)
    except (TypeError, ValueError) as exc:
        raise ConfigError(f"{path}: object `{name}` has a non-integer `{key}`: {value!r}") from exc
    if x < 0 or y < 0 or width <= 0 or height <= 0:
        raise ConfigError(f"{path}: object `{name}` has a non-positive `{key}`: {value!r}")
    return (x, y, width, height)


def _crop(image: NDArray[np.uint8], rect: Rect) -> NDArray[np.uint8]:
    x, y, width, height = rect
    if x + width > image.shape[1] or y + height > image.shape[0]:
        raise ConfigError(f"rect {rect} runs past the {image.shape[1]}x{image.shape[0]} render")
    return image[y : y + height, x : x + width].reshape(-1, 3)


def _luminance(pixels: NDArray[np.uint8]) -> NDArray[np.float64]:
    """WCAG relative luminance of every pixel, vectorised."""
    channels = pixels.astype(np.float64) / 255.0
    linear = np.where(channels <= 0.03928, channels / 12.92, ((channels + 0.055) / 1.055) ** 2.4)
    return 0.2126 * linear[:, 0] + 0.7152 * linear[:, 1] + 0.0722 * linear[:, 2]


def _percentile_pixel(pixels: NDArray[np.uint8], percentile: float) -> tuple[int, int, int]:
    """The pixel at `percentile` of the region, sorted by luminance (0 = darkest)."""
    order = np.argsort(_luminance(pixels), kind="stable")
    index = round(percentile / 100.0 * (order.size - 1))
    pixel = pixels[order[index]]
    return int(pixel[0]), int(pixel[1]), int(pixel[2])


def _hex(pixel: tuple[int, int, int]) -> str:
    return "#{:02x}{:02x}{:02x}".format(*pixel)


def measure_nontext(image: NDArray[np.uint8], obj: NonTextObject) -> NonTextResult:
    """Measure one declared object against its surface, over the rendered pixels."""
    object_pixels = _crop(image, obj.object_rect)
    background_pixels = _crop(image, obj.background_rect)
    ink = _percentile_pixel(object_pixels, obj.foreground_percentile)
    far_ink = _percentile_pixel(object_pixels, 100.0 - obj.foreground_percentile)
    surface = _percentile_pixel(background_pixels, obj.background_percentile)
    far_surface = _percentile_pixel(background_pixels, 100.0 - obj.background_percentile)
    # The object is either lighter than its surface or darker; the user sees the
    # pairing that separates them, so measure both and keep that one.
    contrast = max(
        contrast_ratio(_hex(ink), _hex(far_surface)),
        contrast_ratio(_hex(far_ink), _hex(surface)),
    )
    foreground, background = (
        (_hex(ink), _hex(far_surface))
        if contrast_ratio(_hex(ink), _hex(far_surface))
        >= contrast_ratio(_hex(far_ink), _hex(surface))
        else (_hex(far_ink), _hex(surface))
    )
    return NonTextResult(
        obj=obj,
        ratio=contrast,
        foreground=foreground,
        background=background,
        ok=contrast >= obj.min_ratio,
    )


def run_nontext_checks(
    renders: Mapping[str, Path],
    objects: tuple[NonTextObject, ...],
) -> tuple[tuple[NonTextResult, ...], tuple[MissingRender, ...]]:
    """Measure every declared object whose fixture has a render.

    The renders are keyed by fixture name (the `.scn` `name`, which is what the
    capture writes). A declared fixture with no render is reported as missing, not
    silently skipped: a coverage hole has to be visible.
    """
    by_fixture: dict[str, list[NonTextObject]] = {}
    for obj in objects:
        by_fixture.setdefault(obj.fixture, []).append(obj)

    results: list[NonTextResult] = []
    missing: list[MissingRender] = []
    for fixture, fixture_objects in by_fixture.items():
        path = renders.get(fixture)
        if path is None or not path.is_file():
            missing.append(
                MissingRender(
                    fixture=fixture,
                    objects=tuple(obj.name for obj in fixture_objects),
                    detail=f"no render for fixture `{fixture}` in the renders directory",
                )
            )
            continue
        image = _load_png(path)
        results.extend(measure_nontext(image, obj) for obj in fixture_objects)
    return tuple(results), tuple(missing)


def _load_png(path: Path) -> NDArray[np.uint8]:
    try:
        with Image.open(path) as handle:
            return np.asarray(handle.convert("RGB"), dtype=np.uint8)
    except (OSError, ValueError) as exc:
        raise ConfigError(f"{path}: not a readable PNG: {exc}") from exc
