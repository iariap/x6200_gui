"""Render benchmark statistics and the relative regression verdict.

The C harness measures one frame's real render cost with `CLOCK_MONOTONIC`
(see `tools/ui_harness/src/bench_metrics.h`) and writes the per-frame timings.
This module turns those raw samples into p50/p95/p99, an effective FPS and a
dispersion figure, and compares a baseline (A) against a candidate (B) with
alternating rounds.

Everything here is an **x86 host proxy**: it detects relative regressions
between two revisions, it does not claim the Cortex-A7 frame budget nor the
radio's battery life. A measurement whose dispersion is too high is reported
INCONCLUSIVE instead of being trusted.
"""

from __future__ import annotations

import math
import statistics
from dataclasses import dataclass
from typing import Any

from .capture import FrameSample, MemorySample
from .quality_config import BenchmarkBudget
from .status import Status, combine


def percentile(values: list[float], p: float) -> float:
    """Linear-interpolated percentile (numpy's default method)."""
    if not values:
        raise ValueError("percentile of an empty sample")
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = (p / 100.0) * (len(ordered) - 1)
    low = math.floor(rank)
    high = math.ceil(rank)
    if low == high:
        return ordered[int(rank)]
    fraction = rank - low
    return ordered[low] * (1.0 - fraction) + ordered[high] * fraction


def pct_delta(a_value: float, b_value: float) -> float:
    """B relative to A in percent; A == 0 is only a change when B is not."""
    if a_value == 0.0:
        return 0.0 if b_value == 0.0 else 100.0
    return (b_value - a_value) * 100.0 / a_value


@dataclass(frozen=True)
class ScenarioStats:
    """Aggregated render cost of one scenario for one side."""

    scenario: str
    frames: int
    render_p50_ms: float
    render_p95_ms: float
    render_p99_ms: float
    render_mean_ms: float
    render_stdev_ms: float
    dispersion_cv_pct: float
    fps: float
    flush_ms_mean: float
    draw_ms_mean: float
    flushes_mean: float
    redrawn_area_mean: float
    unique_redrawn_mean: float
    memory: MemorySample | None

    def as_dict(self) -> dict[str, Any]:
        return {
            "scenario": self.scenario,
            "frames": self.frames,
            "render_p50_ms": round(self.render_p50_ms, 4),
            "render_p95_ms": round(self.render_p95_ms, 4),
            "render_p99_ms": round(self.render_p99_ms, 4),
            "render_mean_ms": round(self.render_mean_ms, 4),
            "render_stdev_ms": round(self.render_stdev_ms, 4),
            "dispersion_cv_pct": round(self.dispersion_cv_pct, 3),
            "fps": round(self.fps, 2),
            "flush_ms_mean": round(self.flush_ms_mean, 4),
            "draw_ms_mean": round(self.draw_ms_mean, 4),
            "flushes_mean": round(self.flushes_mean, 3),
            "redrawn_area_mean": round(self.redrawn_area_mean, 1),
            "unique_redrawn_mean": round(self.unique_redrawn_mean, 1),
            "memory": _memory_dict(self.memory),
        }


def _memory_dict(memory: MemorySample | None) -> dict[str, int] | None:
    if memory is None:
        return None
    return {
        "rss_bytes": memory.rss_bytes,
        "pss_bytes": memory.pss_bytes,
        "peak_rss_bytes": memory.peak_rss_bytes,
        "peak_vm_bytes": memory.peak_vm_bytes,
    }


def stats_from_frames(
    scenario: str,
    frames: tuple[FrameSample, ...],
    memory: MemorySample | None = None,
) -> ScenarioStats:
    """Aggregate the frames of one scenario; FPS comes from the median frame."""
    if not frames:
        raise ValueError(f"no frames to aggregate for {scenario}")
    render = [frame.render_ms for frame in frames]
    mean = statistics.fmean(render)
    stdev = statistics.pstdev(render) if len(render) > 1 else 0.0
    p50 = percentile(render, 50)
    return ScenarioStats(
        scenario=scenario,
        frames=len(frames),
        render_p50_ms=p50,
        render_p95_ms=percentile(render, 95),
        render_p99_ms=percentile(render, 99),
        render_mean_ms=mean,
        render_stdev_ms=stdev,
        dispersion_cv_pct=(stdev / mean * 100.0) if mean > 0 else 0.0,
        fps=(1000.0 / p50) if p50 > 0 else 0.0,
        flush_ms_mean=statistics.fmean(frame.flush_ms for frame in frames),
        draw_ms_mean=statistics.fmean(frame.draw_ms for frame in frames),
        flushes_mean=statistics.fmean(frame.flushes for frame in frames),
        redrawn_area_mean=statistics.fmean(frame.redrawn_area_px for frame in frames),
        unique_redrawn_mean=statistics.fmean(frame.unique_redrawn_px for frame in frames),
        memory=memory,
    )


@dataclass(frozen=True)
class ScenarioComparison:
    """A vs B for one scenario, with the budget verdict and its reasons."""

    scenario: str
    a: ScenarioStats
    b: ScenarioStats
    p95_delta_pct: float
    flushes_delta_pct: float
    redrawn_area_delta_pct: float
    status: Status
    reasons: tuple[str, ...]

    def as_dict(self) -> dict[str, Any]:
        return {
            "scenario": self.scenario,
            "status": self.status.value,
            "p95_delta_pct": round(self.p95_delta_pct, 3),
            "flushes_delta_pct": round(self.flushes_delta_pct, 3),
            "redrawn_area_delta_pct": round(self.redrawn_area_delta_pct, 3),
            "reasons": list(self.reasons),
            "a": self.a.as_dict(),
            "b": self.b.as_dict(),
        }


@dataclass(frozen=True)
class BenchmarkVerdict:
    """The verdict of a benchmark comparison: FAIL / PASS / INCONCLUSIVE."""

    status: Status
    budget: BenchmarkBudget
    comparisons: tuple[ScenarioComparison, ...]

    def as_dict(self) -> dict[str, Any]:
        return {
            "status": self.status.value,
            "exit_code": self.status.exit_code,
            "budget": {
                "warmup_frames": self.budget.warmup_frames,
                "repeats": self.budget.repeats,
                "rounds": self.budget.rounds,
                "min_frames": self.budget.min_frames,
                "max_dispersion_cv_pct": self.budget.max_dispersion_cv_pct,
                "max_p95_delta_pct": self.budget.max_p95_delta_pct,
                "max_flushes_delta_pct": self.budget.max_flushes_delta_pct,
                "max_redrawn_area_delta_pct": self.budget.max_redrawn_area_delta_pct,
            },
            "comparisons": [comparison.as_dict() for comparison in self.comparisons],
        }


def compare_scenario(
    a_stats: ScenarioStats, b_stats: ScenarioStats, budget: BenchmarkBudget
) -> ScenarioComparison:
    """Compare B against A with the run's fixed budgets.

    Too few frames or excessive dispersion is INCONCLUSIVE: the honest answer
    is "not measurable", never a PASS on noise.
    """
    reasons: list[str] = []
    p95_delta = pct_delta(a_stats.render_p95_ms, b_stats.render_p95_ms)
    flushes_delta = pct_delta(a_stats.flushes_mean, b_stats.flushes_mean)
    area_delta = pct_delta(a_stats.redrawn_area_mean, b_stats.redrawn_area_mean)

    if a_stats.frames < budget.min_frames or b_stats.frames < budget.min_frames:
        reasons.append(
            f"not enough frames (A={a_stats.frames}, B={b_stats.frames}, "
            f"minimum {budget.min_frames})"
        )
    if a_stats.dispersion_cv_pct > budget.max_dispersion_cv_pct:
        reasons.append(
            f"A dispersion {a_stats.dispersion_cv_pct:.1f}% > {budget.max_dispersion_cv_pct:.1f}%"
        )
    if b_stats.dispersion_cv_pct > budget.max_dispersion_cv_pct:
        reasons.append(
            f"B dispersion {b_stats.dispersion_cv_pct:.1f}% > {budget.max_dispersion_cv_pct:.1f}%"
        )
    if reasons:
        return ScenarioComparison(
            scenario=a_stats.scenario,
            a=a_stats,
            b=b_stats,
            p95_delta_pct=p95_delta,
            flushes_delta_pct=flushes_delta,
            redrawn_area_delta_pct=area_delta,
            status=Status.INCONCLUSIVE,
            reasons=tuple(reasons),
        )

    problems: list[str] = []
    if p95_delta > budget.max_p95_delta_pct:
        problems.append(
            f"p95 render {p95_delta:+.1f}% (budget {budget.max_p95_delta_pct:.1f}%): "
            f"{a_stats.render_p95_ms:.3f} -> {b_stats.render_p95_ms:.3f} ms"
        )
    if flushes_delta > budget.max_flushes_delta_pct:
        problems.append(
            f"flushes {flushes_delta:+.1f}% (budget {budget.max_flushes_delta_pct:.1f}%): "
            f"{a_stats.flushes_mean:.1f} -> {b_stats.flushes_mean:.1f}"
        )
    if area_delta > budget.max_redrawn_area_delta_pct:
        problems.append(
            f"redrawn area {area_delta:+.1f}% "
            f"(budget {budget.max_redrawn_area_delta_pct:.1f}%): "
            f"{a_stats.redrawn_area_mean:.0f} -> {b_stats.redrawn_area_mean:.0f} px"
        )
    return ScenarioComparison(
        scenario=a_stats.scenario,
        a=a_stats,
        b=b_stats,
        p95_delta_pct=p95_delta,
        flushes_delta_pct=flushes_delta,
        redrawn_area_delta_pct=area_delta,
        status=Status.FAIL if problems else Status.PASS,
        reasons=tuple(problems),
    )


def compare_benchmark(
    a_stats: dict[str, ScenarioStats],
    b_stats: dict[str, ScenarioStats],
    budget: BenchmarkBudget,
) -> BenchmarkVerdict:
    """Compare every shared scenario and combine the verdict (worst wins)."""
    shared = sorted(set(a_stats) & set(b_stats))
    if not shared:
        return BenchmarkVerdict(status=Status.INCONCLUSIVE, budget=budget, comparisons=())
    comparisons = tuple(compare_scenario(a_stats[name], b_stats[name], budget) for name in shared)
    return BenchmarkVerdict(
        status=combine([comparison.status for comparison in comparisons]),
        budget=budget,
        comparisons=comparisons,
    )


def measure_verdict(stats: dict[str, ScenarioStats], budget: BenchmarkBudget) -> Status:
    """Verdict of a single-revision measurement: PASS, or INCONCLUSIVE on noise."""
    statuses: list[Status] = []
    for scenario in stats.values():
        noisy = (
            scenario.frames < budget.min_frames
            or scenario.dispersion_cv_pct > budget.max_dispersion_cv_pct
        )
        statuses.append(Status.INCONCLUSIVE if noisy else Status.PASS)
    return combine(statuses)
