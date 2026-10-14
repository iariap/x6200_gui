"""Shared helpers for the unit tests: synthetic captures without Docker."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from numpy.typing import NDArray
from PIL import Image

from x6200_ab.capture import Capture
from x6200_ab.quality_config import (
    BenchmarkBudget,
    Budgets,
    PerformanceBudget,
    QualityContracts,
    VisualBudget,
)
from x6200_ab.revisions import Revision
from x6200_ab.scenarios import Scenario

QUALITY_DIR = Path(__file__).resolve().parents[1] / "quality"
FRAMEBUFFER = (800, 480)
BACKGROUND = (30, 40, 55)
FOREGROUND = (220, 220, 220)

DEFAULT_COVERAGE = [
    {"component": "main_screen.c", "level": "REAL", "note": ""},
    {"component": "theme.c", "level": "REAL", "note": ""},
    {"component": "styles.c", "level": "REAL", "note": ""},
]


def blank_image(width: int = 800, height: int = 480, colour: tuple[int, int, int] = BACKGROUND):
    """A solid framebuffer."""
    return np.full((height, width, 3), colour, dtype=np.uint8)


def draw_box(
    image: NDArray[np.uint8],
    x: int,
    y: int,
    width: int,
    height: int,
    colour: tuple[int, int, int] = FOREGROUND,
):
    """Return a copy of `image` with a filled rectangle."""
    copy = image.copy()
    copy[y : y + height, x : x + width] = colour
    return copy


def revision(label: str = "rev", sha: str | None = None) -> Revision:
    return Revision(label=label, sha=sha or ("a" * 40), worktree=Path("/nonexistent"))


def make_capture(
    directory: Path,
    name: str,
    image: NDArray[np.uint8],
    *,
    rev: Revision | None = None,
    coverage: list[dict[str, str]] | None = None,
    metrics: dict[str, object] | None = None,
) -> Capture:
    """Write a PNG + harness-like JSON and return the resulting `Capture`."""
    rev = rev or revision()
    directory.mkdir(parents=True, exist_ok=True)
    height, width = image.shape[0], image.shape[1]
    png = directory / f"{name}.png"
    Image.fromarray(image).save(png)
    report = {
        "scenario": name,
        "screen": "main",
        "seed": 1,
        "revision": rev.sha,
        "framebuffer": {"width": width, "height": height},
        "virtual_time_ms": 2200,
        "coverage": coverage if coverage is not None else list(DEFAULT_COVERAGE),
        "radio_calls": {},
        "metrics": metrics
        if metrics is not None
        else {
            "flushes": 6,
            "flushed_pixels": width * height,
            "framebuffer_fnv1a": "0x0",
            "png_path": str(png),
            "png_bytes": 0,
        },
    }
    report_path = directory / f"{name}.json"
    report_path.write_text(json.dumps(report))
    return Capture(
        scenario=Scenario(name=name, screen="main", filename=f"{name}.scn", seed=1, raw={}),
        revision=rev,
        png=png,
        report_path=report_path,
        report=report,
        png_sha256=hashlib.sha256(png.read_bytes()).hexdigest(),
    )


def contracts(
    *,
    region_min_content: dict[str, float] | None = None,
    expected_changes: tuple = (),
    review_regions: tuple[str, ...] = (),
    required_components: tuple[str, ...] = ("main_screen.c", "theme.c"),
) -> QualityContracts:
    return QualityContracts(
        region_min_content=region_min_content or {},
        required_coverage_components=required_components,
        coverage_levels=("REAL", "PARTIAL", "SYNTHETIC", "UNSUPPORTED"),
        expected_changes=expected_changes,
        review_regions=review_regions,
    )


def budgets(
    *,
    pixel_tolerance: int = 12,
    min_box_pixels: int = 4,
    max_flushed_pixels_delta_pct: float = 10.0,
    max_flushes_delta_pct: float = 20.0,
    benchmark: BenchmarkBudget | None = None,
) -> Budgets:
    return Budgets(
        visual=VisualBudget(
            pixel_tolerance=pixel_tolerance,
            min_box_pixels=min_box_pixels,
            major_change_pct=1.0,
        ),
        performance=PerformanceBudget(
            max_flushed_pixels_delta_pct=max_flushed_pixels_delta_pct,
            max_flushes_delta_pct=max_flushes_delta_pct,
        ),
        benchmark=benchmark
        if benchmark is not None
        else BenchmarkBudget(
            warmup_frames=3,
            repeats=5,
            rounds=2,
            min_frames=4,
            max_dispersion_cv_pct=25.0,
            max_p95_delta_pct=25.0,
            max_flushes_delta_pct=20.0,
            max_redrawn_area_delta_pct=20.0,
        ),
    )
