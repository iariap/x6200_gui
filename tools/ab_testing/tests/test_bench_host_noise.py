"""The bench gate must not FAIL on the runner's own noise.

The `UI Performance` CI job failed five times on 2026-10-15 with the same
signature: one scenario at random had its p95 over budget while **both**
deterministic proxies (`flushes_mean`, `redrawn_area_mean`) sat at exactly
0.0%. The render workload is a deterministic pixel-count under the harness'
virtual time, so a change of the code moves those counters; host preemption
(CI and host alike) does not. The gate therefore has to read "p95 over budget
with both proxies unchanged" as INCONCLUSIVE (host noise) instead of FAIL, and
keep FAILing as soon as any proxy moves — which is what a real regression does.

The fixtures in `tests/fixtures/benchci_today.json` are the verbatim A/B stats
of that day's four job runs (each scenario's `a`/`b` block copied from the job's
`benchmark.json` artifact), so the classification is pinned on the real payloads
that failed, not on a hand-rolled sample.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from x6200_ab.capture import MemorySample
from x6200_ab.performance import (
    HOST_NOISE_MARKER,
    ScenarioStats,
    compare_benchmark,
    compare_scenario,
)
from x6200_ab.quality_config import BenchmarkBudget
from x6200_ab.status import Status

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "benchci_today.json"

BUDGET = BenchmarkBudget(
    warmup_frames=3,
    repeats=5,
    rounds=4,
    min_frames=12,
    max_dispersion_cv_pct=25.0,
    max_p95_delta_pct=25.0,
    max_flushes_delta_pct=20.0,
    max_redrawn_area_delta_pct=20.0,
)


def _load_cases() -> list[dict[str, Any]]:
    return json.loads(FIXTURES.read_text())["cases"]


def _memory(raw: Any) -> MemorySample | None:
    if not isinstance(raw, dict):
        return None
    return MemorySample(
        rss_bytes=int(raw["rss_bytes"]),
        pss_bytes=int(raw["pss_bytes"]),
        peak_rss_bytes=int(raw["peak_rss_bytes"]),
        peak_vm_bytes=int(raw["peak_vm_bytes"]),
    )


def _stats(payload: dict[str, Any]) -> ScenarioStats:
    """Rebuild the aggregation unit from one side of a real comparison payload."""
    return ScenarioStats(
        scenario=str(payload["scenario"]),
        frames=int(payload["frames"]),
        render_p50_ms=float(payload["render_p50_ms"]),
        render_p95_ms=float(payload["render_p95_ms"]),
        render_p99_ms=float(payload["render_p99_ms"]),
        render_mean_ms=float(payload["render_mean_ms"]),
        render_stdev_ms=float(payload["render_stdev_ms"]),
        dispersion_cv_pct=float(payload["dispersion_cv_pct"]),
        fps=float(payload["fps"]),
        flush_ms_mean=float(payload["flush_ms_mean"]),
        draw_ms_mean=float(payload["draw_ms_mean"]),
        flushes_mean=float(payload["flushes_mean"]),
        redrawn_area_mean=float(payload["redrawn_area_mean"]),
        unique_redrawn_mean=float(payload["unique_redrawn_mean"]),
        memory=_memory(payload.get("memory")),
    )


def _compare(case: dict[str, Any]) -> tuple[Status, bool]:
    a = _stats(case["a"])
    b = _stats(case["b"])
    comparison = compare_scenario(a, b, BUDGET)
    return comparison.status, comparison.host_noise


def _perturbed(case: dict[str, Any], **changes: float) -> dict[str, Any]:
    b = dict(case["b"])
    b.update(changes)
    return {"a": case["a"], "b": b}


def _case_id(case: dict[str, Any]) -> str:
    return f"{case['source_run']}-{case['scenario']}"


HOST_NOISE_CASES = [c for c in _load_cases() if c["expected"] == "INCONCLUSIVE"]
PASS_CASES = [c for c in _load_cases() if c["expected"] == "PASS"]


def test_the_fixture_carries_todays_real_signature() -> None:
    """Guard the premise: every "expected INCONCLUSIVE" case is that signature."""
    assert HOST_NOISE_CASES, "the fixture lost the real failing cases"
    for case in HOST_NOISE_CASES:
        assert case["real_status"] == "FAIL"
        assert float(case["reported_flushes_delta_pct"]) == 0.0
        assert float(case["reported_redrawn_area_delta_pct"]) == 0.0
        assert float(case["reported_p95_delta_pct"]) > BUDGET.max_p95_delta_pct


@pytest.mark.parametrize("case", HOST_NOISE_CASES, ids=_case_id)
def test_only_p95_with_zeroed_proxies_is_host_noise_not_fail(case: dict[str, Any]) -> None:
    status, host_noise = _compare(case)
    assert status is Status.INCONCLUSIVE
    assert host_noise is True


@pytest.mark.parametrize("case", PASS_CASES, ids=_case_id)
def test_a_pure_pass_stays_pass(case: dict[str, Any]) -> None:
    status, host_noise = _compare(case)
    assert status is Status.PASS
    assert host_noise is False


def test_a_moved_flush_proxy_keeps_failing() -> None:
    """The deterministic proxy is the bar: a real regression moves it.

    The reproduction of issue #9's "repaint three times per frame" `len(k)*lock`
    regression triples the per-frame redraw, so the flush count and the redrawn
    area both grow (see `test_e2e.py::_raise_bench_cost`). Here the p95 already
    breaches and the flush count moves off 0.0%: that is a real change, FAIL.
    """
    case = HOST_NOISE_CASES[0]
    status, host_noise = _compare(_perturbed(case, flushes_mean=3.0))
    assert status is Status.FAIL
    assert host_noise is False


def test_a_moved_area_proxy_keeps_failing() -> None:
    case = HOST_NOISE_CASES[0]
    status, host_noise = _compare(_perturbed(case, redrawn_area_mean=768000.0))
    assert status is Status.FAIL
    assert host_noise is False


def test_a_proxy_that_moves_down_still_fails() -> None:
    """Any proxy change is a code change, not host noise — even a decrease."""
    case = HOST_NOISE_CASES[0]
    status, host_noise = _compare(_perturbed(case, flushes_mean=2.0))
    assert status is Status.FAIL
    assert host_noise is False


def test_the_verdict_combines_real_fail_over_host_noise() -> None:
    """One real regression anywhere keeps the experiment FAIL, not INCONCLUSIVE."""
    noise = HOST_NOISE_CASES[0]
    real = _perturbed(noise, flushes_mean=3.0)
    verdict = compare_benchmark(
        {"noisy": _stats(noise["a"]), "real": _stats(noise["a"])},
        {"noisy": _stats(noise["b"]), "real": _stats(real["b"])},
        BUDGET,
    )
    assert verdict.status is Status.FAIL


def test_host_noise_marker_is_stable() -> None:
    """The CI report step greps the reasons for this exact marker."""
    assert HOST_NOISE_MARKER == "host noise"


# The `test_only_p95_with_zeroed_proxies_...` cases above are the TDD red: on the
# pre-fix code every one of them reads FAIL, exactly as CI reported on 2026-10-15.
