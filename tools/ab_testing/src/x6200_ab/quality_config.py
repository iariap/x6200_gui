"""Quality contracts, budgets and declared expected changes (C, independent of A).

Two files, both versioned and read before A or B are captured:

* `contracts.yaml` — objective rules the candidate must satisfy on its own
  (minimum content per region, coverage levels that must be declared). They do
  not look at the baseline at all.
* `budgets.yaml` — thresholds fixed *before* running: the pixel tolerance that
  defines a "change", the minimum box size worth reporting, the render-cost
  deltas considered acceptable, and the area above which a visual change is
  called major.

`expected_changes` are the only way a visual change stops being a regression
without being a fix; each one carries a reason, an author and an expiry date,
so an expectation cannot silently become permanent.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Any

from .configio import as_float, as_int, read_yaml_mapping
from .errors import ConfigError


@dataclass(frozen=True)
class VisualBudget:
    """Thresholds for the pixel diff."""

    pixel_tolerance: int
    min_box_pixels: int
    major_change_pct: float


@dataclass(frozen=True)
class PerformanceBudget:
    """Relative render-cost budgets (x86 host proxy, never battery)."""

    max_flushed_pixels_delta_pct: float
    max_flushes_delta_pct: float


@dataclass(frozen=True)
class BenchmarkBudget:
    """Thresholds and sampling plan for the real-clock render benchmark.

    The sampling plan (warm-up, measured frames per run, alternating rounds) is
    part of the budget so a run cannot quietly reduce the sample to make a
    regression disappear.
    """

    warmup_frames: int
    repeats: int
    rounds: int
    min_frames: int
    max_dispersion_cv_pct: float
    max_p95_delta_pct: float
    max_flushes_delta_pct: float
    max_redrawn_area_delta_pct: float


@dataclass(frozen=True)
class Budgets:
    visual: VisualBudget
    performance: PerformanceBudget
    benchmark: BenchmarkBudget


@dataclass(frozen=True)
class ExpectedChange:
    """A visual change that is known and acceptable, until a date."""

    region: str
    reason: str
    author: str
    until: str

    def active(self, today: date) -> bool:
        try:
            expiry = date.fromisoformat(self.until)
        except ValueError as exc:
            raise ConfigError(f"expected change until `{self.until}` is not an ISO date") from exc
        return today <= expiry

    def as_dict(self) -> dict[str, Any]:
        return {
            "region": self.region,
            "reason": self.reason,
            "author": self.author,
            "until": self.until,
        }


@dataclass(frozen=True)
class QualityContracts:
    """Objective rules evaluated on the candidate captures."""

    region_min_content: dict[str, float]
    required_coverage_components: tuple[str, ...]
    coverage_levels: tuple[str, ...]
    expected_changes: tuple[ExpectedChange, ...]
    review_regions: tuple[str, ...] = ()

    def active_expectations(self, today: date) -> tuple[ExpectedChange, ...]:
        return tuple(change for change in self.expected_changes if change.active(today))


def load_budgets(path: Path) -> Budgets:
    data = read_yaml_mapping(path)
    visual = _mapping(data, "visual", path)
    performance = _mapping(data, "performance", path)
    benchmark = _mapping(data, "benchmark", path)
    return Budgets(
        visual=VisualBudget(
            pixel_tolerance=as_int(visual, "pixel_tolerance", path),
            min_box_pixels=as_int(visual, "min_box_pixels", path),
            major_change_pct=as_float(visual, "major_change_pct", path),
        ),
        performance=PerformanceBudget(
            max_flushed_pixels_delta_pct=as_float(
                performance, "max_flushed_pixels_delta_pct", path
            ),
            max_flushes_delta_pct=as_float(performance, "max_flushes_delta_pct", path),
        ),
        benchmark=BenchmarkBudget(
            warmup_frames=as_int(benchmark, "warmup_frames", path),
            repeats=as_int(benchmark, "repeats", path),
            rounds=as_int(benchmark, "rounds", path),
            min_frames=as_int(benchmark, "min_frames", path),
            max_dispersion_cv_pct=as_float(benchmark, "max_dispersion_cv_pct", path),
            max_p95_delta_pct=as_float(benchmark, "max_p95_delta_pct", path),
            max_flushes_delta_pct=as_float(benchmark, "max_flushes_delta_pct", path),
            max_redrawn_area_delta_pct=as_float(benchmark, "max_redrawn_area_delta_pct", path),
        ),
    )


def load_contracts(path: Path, known_regions: set[str]) -> QualityContracts:
    data = read_yaml_mapping(path)
    raw_min = data.get("region_min_content", {})
    if not isinstance(raw_min, dict):
        raise ConfigError(f"{path}: `region_min_content` must be a mapping")
    region_min_content: dict[str, float] = {}
    for name, value in raw_min.items():
        if name not in known_regions:
            raise ConfigError(f"{path}: region_min_content names unknown region `{name}`")
        fraction = float(value)
        if not 0.0 <= fraction <= 1.0:
            raise ConfigError(f"{path}: region_min_content[{name}] must be within [0, 1]")
        region_min_content[str(name)] = fraction
    coverage = data.get("coverage", {})
    if not isinstance(coverage, dict):
        raise ConfigError(f"{path}: `coverage` must be a mapping")
    required = coverage.get("required_components", [])
    levels = coverage.get("allowed_levels", [])
    if not isinstance(required, list) or not isinstance(levels, list):
        raise ConfigError(f"{path}: coverage lists must be sequences")
    expected = _load_expected_changes(data.get("expected_changes", []), path, known_regions)
    review = data.get("review_regions", [])
    if not isinstance(review, list):
        raise ConfigError(f"{path}: `review_regions` must be a list")
    review_regions: list[str] = []
    for name in review:
        if str(name) not in known_regions:
            raise ConfigError(f"{path}: review_regions names unknown region `{name}`")
        review_regions.append(str(name))
    return QualityContracts(
        region_min_content=region_min_content,
        required_coverage_components=tuple(str(item) for item in required),
        coverage_levels=tuple(str(item) for item in levels),
        expected_changes=expected,
        review_regions=tuple(review_regions),
    )


def _load_expected_changes(
    raw: object, path: Path, known_regions: set[str]
) -> tuple[ExpectedChange, ...]:
    if raw is None:
        return ()
    if not isinstance(raw, list):
        raise ConfigError(f"{path}: `expected_changes` must be a list")
    changes: list[ExpectedChange] = []
    for entry in raw:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: every expected change must be a mapping")
        try:
            region = str(entry["region"])
            reason = str(entry["reason"])
            until = str(entry["until"])
            author = str(entry["author"])
        except KeyError as exc:
            raise ConfigError(f"{path}: expected change missing `{exc.args[0]}`") from exc
        if region not in known_regions:
            raise ConfigError(f"{path}: expected change names unknown region `{region}`")
        changes.append(ExpectedChange(region=region, reason=reason, author=author, until=until))
    return tuple(changes)


def _mapping(data: dict[str, Any], key: str, path: Path) -> dict[str, Any]:
    value = data.get(key)
    if not isinstance(value, dict):
        raise ConfigError(f"{path}: `{key}` must be a mapping")
    return value
