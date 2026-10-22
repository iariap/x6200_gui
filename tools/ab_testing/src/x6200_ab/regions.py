"""Semantic regions of the main screen (frequency, top bar, buttons, …).

A pixel diff says *something* moved; regions say *where* in UI terms, which is
what the report and the known-issue mapping need. The regions are rectangles
over the 800x480 framebuffer defined in `quality/regions.yaml`, so they can be
tuned without touching code. Regions may overlap (the S-meter sits inside the
top bar band); that is intentional and does not change the diff itself.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, TypeGuard

from .configio import read_yaml_mapping
from .errors import ConfigError


@dataclass(frozen=True)
class Region:
    """A named rectangle over the framebuffer."""

    name: str
    x: int
    y: int
    width: int
    height: int
    description: str = ""

    @property
    def box(self) -> tuple[int, int, int, int]:
        """`(x0, y0, x1, y1)` with the far edges exclusive."""
        return (self.x, self.y, self.x + self.width, self.y + self.height)

    def contains(self, bbox: tuple[int, int, int, int]) -> bool:
        """True when `bbox` is fully inside this region."""
        x0, y0, x1, y1 = bbox
        rx0, ry0, rx1, ry1 = self.box
        return rx0 <= x0 and ry0 <= y0 and x1 <= rx1 and y1 <= ry1

    def overlaps(self, bbox: tuple[int, int, int, int]) -> bool:
        x0, y0, x1, y1 = bbox
        rx0, ry0, rx1, ry1 = self.box
        return x0 < rx1 and rx0 < x1 and y0 < ry1 and ry0 < y1

    def as_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "x": self.x,
            "y": self.y,
            "width": self.width,
            "height": self.height,
            "description": self.description,
        }


@dataclass(frozen=True)
class RegionMap:
    """Every semantic region plus the framebuffer size they live in."""

    width: int
    height: int
    regions: dict[str, Region]

    def get(self, name: str) -> Region:
        try:
            return self.regions[name]
        except KeyError as exc:
            raise ConfigError(f"unknown region `{name}`; known: {sorted(self.regions)}") from exc

    def for_box(self, bbox: tuple[int, int, int, int]) -> list[Region]:
        """Regions overlapping `bbox`, most specific (smallest) first."""
        hits = [region for region in self.regions.values() if region.overlaps(bbox)]
        return sorted(hits, key=lambda region: region.width * region.height)

    def covering(self, bbox: tuple[int, int, int, int]) -> Region | None:
        """The smallest region fully containing `bbox`, if any."""
        hits = [region for region in self.regions.values() if region.contains(bbox)]
        if not hits:
            return None
        return min(hits, key=lambda region: region.width * region.height)


def _has_size(framebuffer: object) -> TypeGuard[dict[str, Any]]:
    return isinstance(framebuffer, dict) and "width" in framebuffer and "height" in framebuffer


def load_regions(path: Path) -> RegionMap:
    """Load `quality/regions.yaml`."""
    data = read_yaml_mapping(path)
    framebuffer = data.get("framebuffer")
    if not _has_size(framebuffer):
        raise ConfigError(f"{path}: `framebuffer` must have width and height")
    width = int(framebuffer["width"])
    height = int(framebuffer["height"])
    raw_regions = data.get("regions")
    if not isinstance(raw_regions, list) or not raw_regions:
        raise ConfigError(f"{path}: `regions` must be a non-empty list")
    regions: dict[str, Region] = {}
    for entry in raw_regions:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: every region must be a mapping")
        try:
            name = str(entry["name"])
            rect = entry["rect"]
        except KeyError as exc:
            raise ConfigError(f"{path}: region missing `{exc.args[0]}`") from exc
        if not isinstance(rect, list) or len(rect) != 4:
            raise ConfigError(f"{path}: region `{name}` needs rect [x, y, width, height]")
        x, y, rwidth, rheight = (int(value) for value in rect)
        if x < 0 or y < 0 or rwidth <= 0 or rheight <= 0:
            raise ConfigError(f"{path}: region `{name}` has a non-positive rectangle {rect}")
        if x + rwidth > width or y + rheight > height:
            raise ConfigError(f"{path}: region `{name}` runs past the {width}x{height} framebuffer")
        if name in regions:
            raise ConfigError(f"{path}: duplicate region `{name}`")
        regions[name] = Region(
            name=name,
            x=x,
            y=y,
            width=rwidth,
            height=rheight,
            description=str(entry.get("description", "")),
        )
    return RegionMap(width=width, height=height, regions=regions)
