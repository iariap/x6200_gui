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
class LayoutRules:
    """Geometry rules over the harness' structural inspection."""

    min_touch_target_px: int
    clip_tolerance_px: int


@dataclass(frozen=True)
class TypographyRules:
    """Text-legibility rules over the structural inspection.

    `allow_fallback_glyphs` is true for this firmware on purpose: the alternate
    fonts are ASCII-only and the icon glyphs are meant to come from the Sony
    fallback. A glyph missing from *every* font (`missing_glyphs`, a placeholder
    rectangle on screen) is a defect regardless.
    """

    min_contrast_ratio: float
    large_text_min_contrast_ratio: float
    large_text_px: int
    allow_fallback_glyphs: bool


@dataclass(frozen=True)
class RuleException:
    """A scoped, dated waiver for one structural finding.

    Never global and never permanent: it names the rule and the fixture (and
    optionally one node path) and carries a reason, an author and an expiry date.
    """

    rule: str
    fixture: str
    reason: str
    author: str
    until: str
    path: str | None = None

    def active(self, rule: str, fixture: str, path: str | None, today: date) -> bool:
        if self.rule != rule or self.fixture != fixture:
            return False
        if self.path is not None and self.path != path:
            return False
        try:
            return today <= date.fromisoformat(self.until)
        except ValueError as exc:
            raise ConfigError(f"exception until `{self.until}` is not an ISO date") from exc

    def as_dict(self) -> dict[str, Any]:
        return {
            "rule": self.rule,
            "fixture": self.fixture,
            "path": self.path,
            "reason": self.reason,
            "author": self.author,
            "until": self.until,
        }


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
    layout: LayoutRules = LayoutRules(min_touch_target_px=44, clip_tolerance_px=0)
    typography: TypographyRules = TypographyRules(
        min_contrast_ratio=4.5,
        large_text_min_contrast_ratio=3.0,
        large_text_px=24,
        allow_fallback_glyphs=True,
    )
    exceptions: tuple[RuleException, ...] = ()

    def active_expectations(self, today: date) -> tuple[ExpectedChange, ...]:
        return tuple(change for change in self.expected_changes if change.active(today))

    def waiver(
        self, rule: str, fixture: str, path: str | None, today: date
    ) -> RuleException | None:
        """The active exception that covers a finding, if any."""
        for exception in self.exceptions:
            if exception.active(rule, fixture, path, today):
                return exception
        return None


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
    layout = _load_layout(data.get("layout", {}), path)
    typography = _load_typography(data.get("typography", {}), path)
    exceptions = _load_exceptions(data.get("exceptions", []), path)
    return QualityContracts(
        region_min_content=region_min_content,
        required_coverage_components=tuple(str(item) for item in required),
        coverage_levels=tuple(str(item) for item in levels),
        expected_changes=expected,
        review_regions=tuple(review_regions),
        layout=layout,
        typography=typography,
        exceptions=exceptions,
    )


def _load_layout(raw: object, path: Path) -> LayoutRules:
    if not isinstance(raw, dict):
        raise ConfigError(f"{path}: `layout` must be a mapping")
    return LayoutRules(
        min_touch_target_px=as_int(raw, "min_touch_target_px", path, default=44),
        clip_tolerance_px=as_int(raw, "clip_tolerance_px", path, default=0),
    )


def _load_typography(raw: object, path: Path) -> TypographyRules:
    if not isinstance(raw, dict):
        raise ConfigError(f"{path}: `typography` must be a mapping")
    allow_fallback = raw.get("allow_fallback_glyphs", True)
    if not isinstance(allow_fallback, bool):
        raise ConfigError(f"{path}: `allow_fallback_glyphs` must be a boolean")
    return TypographyRules(
        min_contrast_ratio=as_float(raw, "min_contrast_ratio", path, default=4.5),
        large_text_min_contrast_ratio=as_float(
            raw, "large_text_min_contrast_ratio", path, default=3.0
        ),
        large_text_px=as_int(raw, "large_text_px", path, default=24),
        allow_fallback_glyphs=allow_fallback,
    )


def _load_exceptions(raw: object, path: Path) -> tuple[RuleException, ...]:
    if raw is None:
        return ()
    if not isinstance(raw, list):
        raise ConfigError(f"{path}: `exceptions` must be a list")
    exceptions: list[RuleException] = []
    for entry in raw:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: every exception must be a mapping")
        for key in ("rule", "fixture", "reason", "author", "until"):
            if key not in entry:
                raise ConfigError(f"{path}: exception missing `{key}`")
        path_value = entry.get("path")
        exceptions.append(
            RuleException(
                rule=str(entry["rule"]),
                fixture=str(entry["fixture"]),
                reason=str(entry["reason"]),
                author=str(entry["author"]),
                until=str(entry["until"]),
                path=str(path_value) if path_value is not None else None,
            )
        )
    return tuple(exceptions)


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
