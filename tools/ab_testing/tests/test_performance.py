"""Unit tests for the render-benchmark statistics and verdict (no Docker)."""

from __future__ import annotations

import pytest

from x6200_ab.capture import FrameSample, MemorySample
from x6200_ab.performance import (
    BenchmarkVerdict,
    ScenarioStats,
    compare_benchmark,
    compare_scenario,
    measure_verdict,
    pct_delta,
    percentile,
    stats_from_frames,
)
from x6200_ab.quality_config import BenchmarkBudget
from x6200_ab.status import Status


def _frames(values: list[float]) -> tuple[FrameSample, ...]:
    return tuple(
        FrameSample(
            render_ms=value,
            flush_ms=value * 0.2,
            draw_ms=value * 0.8,
            flushes=1,
            redrawn_area_px=384000,
            unique_redrawn_px=384000,
        )
        for value in values
    )


BUDGET = BenchmarkBudget(
    warmup_frames=3,
    repeats=5,
    rounds=2,
    min_frames=4,
    max_dispersion_cv_pct=25.0,
    max_p95_delta_pct=25.0,
    max_flushes_delta_pct=20.0,
    max_redrawn_area_delta_pct=20.0,
)


def test_percentile_interpolates() -> None:
    assert percentile([1.0, 2.0, 3.0, 4.0, 5.0], 50) == 3.0
    assert percentile([1.0, 2.0, 3.0, 4.0, 5.0], 95) == pytest.approx(4.8)
    assert percentile([1.0, 2.0, 3.0, 4.0, 5.0], 99) == pytest.approx(4.96)
    assert percentile([7.0], 95) == 7.0
    with pytest.raises(ValueError, match="empty sample"):
        percentile([], 50)


def test_pct_delta() -> None:
    assert pct_delta(4.0, 4.0) == 0.0
    assert pct_delta(4.0, 8.0) == pytest.approx(100.0)
    assert pct_delta(0.0, 0.0) == 0.0
    assert pct_delta(0.0, 5.0) == 100.0


def test_stats_from_frames_reports_percentiles_fps_and_dispersion() -> None:
    memory = MemorySample(rss_bytes=10, pss_bytes=8, peak_rss_bytes=12, peak_vm_bytes=80)
    stats = stats_from_frames("s", _frames([1.0, 2.0, 3.0, 4.0, 5.0]), memory)
    assert stats.frames == 5
    assert stats.render_p50_ms == 3.0
    assert stats.render_p95_ms == pytest.approx(4.8)
    assert stats.render_p99_ms == pytest.approx(4.96)
    assert stats.fps == pytest.approx(1000.0 / 3.0)
    # The dispersion is the robust (MAD) CV: median 3, MAD 1 -> 1.4826/3 = 49.42%,
    # not the stdev/mean of the same sample (47.14%). The stdev is still reported.
    assert stats.dispersion_cv_pct == pytest.approx(49.42, abs=0.01)
    assert stats.render_stdev_ms == pytest.approx(1.4142, abs=0.0001)
    assert stats.memory is memory


def test_a_few_host_spikes_are_not_dispersion() -> None:
    """One preemption frame must not read as `the host is too noisy to trust`.

    Measured on this host: a quiet 20-frame run reads ~1.5%, and one 10 ms frame
    among twenty (3.3 ms median) pushed the stdev CV to ~38% while it does not
    move any p95 (1 frame in 20 is below the 95th percentile, so it cannot change
    a p95 comparison). The robust CV stays low there, and still flags a sample a
    non-trivial fraction of which is outliers — what does move a p95.
    """
    quiet = [3.3] * 19 + [10.0]
    spiked = stats_from_frames("s", _frames(quiet))
    assert spiked.dispersion_cv_pct < BUDGET.max_dispersion_cv_pct
    assert measure_verdict({"s": spiked}, BUDGET) is Status.PASS

    every_other = stats_from_frames("s", _frames([3.0, 10.0] * 3))
    assert every_other.dispersion_cv_pct > BUDGET.max_dispersion_cv_pct
    assert measure_verdict({"s": every_other}, BUDGET) is Status.INCONCLUSIVE


def test_stats_without_frames_is_an_error() -> None:
    with pytest.raises(ValueError, match="no frames"):
        stats_from_frames("s", ())


def test_scenario_stats_as_dict_is_serialisable() -> None:
    stats = stats_from_frames("s", _frames([3.0, 3.1, 3.2, 3.3, 3.4]))
    payload = stats.as_dict()
    assert payload["render_p50_ms"] == pytest.approx(3.2)
    assert payload["memory"] is None


def test_compare_identical_sides_pass() -> None:
    a = stats_from_frames("s", _frames([4.0, 4.1, 4.2, 3.9, 4.0, 4.1]))
    b = stats_from_frames("s", _frames([4.0, 4.1, 4.2, 3.9, 4.0, 4.1]))
    comparison = compare_scenario(a, b, BUDGET)
    assert comparison.status is Status.PASS
    assert comparison.reasons == ()


def test_compare_detects_a_render_regression() -> None:
    a = stats_from_frames("s", _frames([4.0, 4.1, 4.2, 3.9, 4.0, 4.1]))
    b = stats_from_frames("s", _frames([12.0, 12.1, 12.2, 11.9, 12.0, 12.1]))
    comparison = compare_scenario(a, b, BUDGET)
    assert comparison.status is Status.FAIL
    assert comparison.p95_delta_pct > 100
    assert any("p95 render" in reason for reason in comparison.reasons)


def test_compare_flags_flush_regression() -> None:
    a = stats_from_frames("s", _frames([4.0, 4.1, 4.2, 3.9, 4.0, 4.1]))
    b_frames = tuple(
        FrameSample(
            render_ms=sample.render_ms,
            flush_ms=sample.flush_ms,
            draw_ms=sample.draw_ms,
            flushes=3,
            redrawn_area_px=sample.redrawn_area_px,
            unique_redrawn_px=sample.unique_redrawn_px,
        )
        for sample in _frames([4.0, 4.1, 4.2, 3.9, 4.0, 4.1])
    )
    b = stats_from_frames("s", b_frames)
    comparison = compare_scenario(a, b, BUDGET)
    assert comparison.status is Status.FAIL
    assert any("flushes" in reason for reason in comparison.reasons)


def test_noisy_side_is_inconclusive_not_pass() -> None:
    a = stats_from_frames("s", _frames([1.0, 10.0, 1.0, 10.0, 1.0, 10.0]))
    b = stats_from_frames("s", _frames([1.0, 10.0, 1.0, 10.0, 1.0, 10.0]))
    comparison = compare_scenario(a, b, BUDGET)
    assert comparison.status is Status.INCONCLUSIVE
    assert any("dispersion" in reason for reason in comparison.reasons)


def test_too_few_frames_is_inconclusive() -> None:
    a = stats_from_frames("s", _frames([4.0, 4.1]))
    b = stats_from_frames("s", _frames([4.0, 4.1]))
    comparison = compare_scenario(a, b, BUDGET)
    assert comparison.status is Status.INCONCLUSIVE
    assert any("not enough frames" in reason for reason in comparison.reasons)


def test_compare_benchmark_combines_worst_scenario() -> None:
    a = {
        "good": stats_from_frames("good", _frames([4.0, 4.1, 4.2, 3.9])),
        "bad": stats_from_frames("bad", _frames([4.0, 4.1, 4.2, 3.9])),
    }
    b = {
        "good": stats_from_frames("good", _frames([4.0, 4.1, 4.2, 3.9])),
        "bad": stats_from_frames("bad", _frames([12.0, 12.1, 12.2, 11.9])),
    }
    verdict = compare_benchmark(a, b, BUDGET)
    assert isinstance(verdict, BenchmarkVerdict)
    assert verdict.status is Status.FAIL
    statuses = {comparison.scenario: comparison.status for comparison in verdict.comparisons}
    assert statuses == {"good": Status.PASS, "bad": Status.FAIL}


def test_compare_benchmark_without_shared_scenarios_is_inconclusive() -> None:
    a = {"only-a": stats_from_frames("only-a", _frames([4.0, 4.1, 4.2, 3.9]))}
    b = {"only-b": stats_from_frames("only-b", _frames([4.0, 4.1, 4.2, 3.9]))}
    verdict = compare_benchmark(a, b, BUDGET)
    assert verdict.status is Status.INCONCLUSIVE
    assert verdict.comparisons == ()


def test_measure_verdict_flags_noise() -> None:
    stable = {"s": stats_from_frames("s", _frames([4.0, 4.05, 4.1, 3.95, 4.0]))}
    noisy = {"s": stats_from_frames("s", _frames([1.0, 10.0, 1.0, 10.0]))}
    assert measure_verdict(stable, BUDGET) is Status.PASS
    assert measure_verdict(noisy, BUDGET) is Status.INCONCLUSIVE


def test_scenario_stats_type_is_used() -> None:
    # Keeps the import honest: ScenarioStats is the aggregation unit.
    stats: ScenarioStats = stats_from_frames("s", _frames([4.0, 4.1]))
    assert isinstance(stats, ScenarioStats)
