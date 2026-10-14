"""The six registered gates and the verdict each one produces.

The plan asks for the gates to stay separate, so a run reports Functional,
Visual Regression, Quality Contract, Performance, Hardware and Energy
independently. Hardware and Energy have no instrumentation on the host and are
therefore BLOCKED — never PASS — which is the honest answer, not a failure to
hide. A `GateProfile` decides which gates are *required* for the overall verdict
(Hardware and Energy are advisory: they are reported, not silently dropped).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any

from .capture import Capture
from .contracts import ContractFinding
from .known_issues import IssueObservation
from .regions import RegionMap
from .status import Status, combine
from .visual_diff import DiffResult, unexplained_changed_pixels


class GateName(StrEnum):
    """Stable gate identifiers; the value is what the report shows."""

    FUNCTIONAL = "Functional"
    VISUAL_REGRESSION = "Visual Regression"
    QUALITY_CONTRACT = "Quality Contract"
    PERFORMANCE = "Performance"
    HARDWARE = "Hardware"
    ENERGY = "Energy"


ALL_GATES: tuple[GateName, ...] = tuple(GateName)


@dataclass(frozen=True)
class GateResult:
    """One gate's verdict with its evidence."""

    name: GateName
    status: Status
    summary: str
    required: bool = True
    evidence: tuple[str, ...] = ()
    details: dict[str, Any] = field(default_factory=dict)

    def as_dict(self) -> dict[str, Any]:
        return {
            "gate": self.name.value,
            "status": self.status.value,
            "required": self.required,
            "summary": self.summary,
            "evidence": list(self.evidence),
            "details": self.details,
        }


@dataclass(frozen=True)
class GateProfile:
    """Which gates decide the overall verdict for a given run scope."""

    name: str
    required: frozenset[GateName]

    @staticmethod
    def host_ui() -> GateProfile:
        """Default profile: the four gates the host can actually measure."""
        return GateProfile(
            name="host-ui",
            required=frozenset(
                {
                    GateName.FUNCTIONAL,
                    GateName.VISUAL_REGRESSION,
                    GateName.QUALITY_CONTRACT,
                    GateName.PERFORMANCE,
                }
            ),
        )

    def is_required(self, gate: GateName) -> bool:
        return gate in self.required

    @staticmethod
    def full() -> GateProfile:
        """Every gate required, including the ones BLOCKED on the host."""
        return GateProfile(name="full", required=frozenset(ALL_GATES))


def functional_gate(
    a_captures: dict[str, Capture],
    b_captures: dict[str, Capture],
    expected_scenarios: list[str],
) -> GateResult:
    """Did the harness run for every scenario on both revisions?"""
    problems: list[str] = []
    for name in expected_scenarios:
        if name not in a_captures:
            problems.append(f"A missing scenario {name}")
        if name not in b_captures:
            problems.append(f"B missing scenario {name}")
    if problems:
        return GateResult(
            name=GateName.FUNCTIONAL,
            status=Status.FAIL,
            summary="the harness did not produce every capture",
            evidence=tuple(problems),
        )
    revisions_a = {capture.reported_revision for capture in a_captures.values()}
    revisions_b = {capture.reported_revision for capture in b_captures.values()}
    return GateResult(
        name=GateName.FUNCTIONAL,
        status=Status.PASS,
        summary=f"{len(expected_scenarios)} scenarios captured on both revisions",
        details={"revision_a": sorted(revisions_a), "revision_b": sorted(revisions_b)},
    )


def visual_regression_gate(
    diffs: list[DiffResult],
    region_map: RegionMap,
    *,
    allowed_regions: dict[str, frozenset[str]],
    introduced_issues: list[IssueObservation],
    review_regions: frozenset[str] = frozenset(),
) -> GateResult:
    """Unexplained pixel changes and newly introduced defects fail the gate.

    A change confined to a `review_regions` region is not failed: it becomes
    REVIEW REQUIRED, because "different there" may be an aesthetic change that
    only a human can judge.
    """
    problems: list[str] = []
    evidence: list[str] = []
    explained_changes: list[str] = []
    review: list[str] = []
    for diff in diffs:
        if diff.dimension_mismatch is not None:
            a_shape, b_shape = diff.dimension_mismatch
            problems.append(f"{diff.scenario}: framebuffer {a_shape} vs {b_shape}")
            continue
        if diff.identical:
            continue
        allowed = allowed_regions.get(diff.scenario, frozenset())
        unexplained = unexplained_changed_pixels(diff, region_map, allowed)
        if unexplained > 0:
            review_only = unexplained_changed_pixels(diff, region_map, allowed | review_regions)
            if review_only == 0:
                review.append(
                    f"{diff.scenario}: {unexplained} changed pixels confined to "
                    f"{sorted(review_regions)} need a human look"
                )
            else:
                problems.append(
                    f"{diff.scenario}: {review_only} unexplained changed pixels "
                    f"({diff.changed_fraction * 100:.2f}% of the screen)"
                )
                if diff.diff_png is not None:
                    evidence.append(str(diff.diff_png))
                if diff.overlay_png is not None:
                    evidence.append(str(diff.overlay_png))
        elif allowed:
            explained_changes.append(
                f"{diff.scenario}: {diff.changed_pixels} changed pixels, all inside "
                f"{sorted(allowed)}"
            )
        else:
            problems.append(
                f"{diff.scenario}: {diff.changed_pixels} changed pixels outside any "
                f"declared region ({diff.changed_fraction * 100:.2f}% of the screen)"
            )
        evidence.extend(
            f"{diff.scenario} box {box['x0']},{box['y0']}-{box['x1']},{box['y1']} "
            f"({box['pixels']}px)"
            for box in diff.boxes_dicts()[:5]
        )
    for observation in introduced_issues:
        problems.append(f"{observation.issue.id}: {observation.classification.value}")
    if problems:
        return GateResult(
            name=GateName.VISUAL_REGRESSION,
            status=Status.FAIL,
            summary="visual regressions vs the baseline",
            evidence=tuple(evidence),
            details={"problems": problems},
        )
    if review:
        return GateResult(
            name=GateName.VISUAL_REGRESSION,
            status=Status.REVIEW_REQUIRED,
            summary="visual changes that need a human decision",
            evidence=tuple(evidence),
            details={"review": review},
        )
    if explained_changes:
        return GateResult(
            name=GateName.VISUAL_REGRESSION,
            status=Status.PASS,
            summary="no unexplained visual regressions",
            details={"explained_changes": explained_changes},
        )
    return GateResult(
        name=GateName.VISUAL_REGRESSION,
        status=Status.PASS,
        summary="captures are pixel-identical to the baseline",
    )


def quality_contract_gate(findings: list[ContractFinding]) -> GateResult:
    """The candidate must satisfy the objective contracts on its own."""
    failures = [finding for finding in findings if not finding.ok]
    if failures:
        return GateResult(
            name=GateName.QUALITY_CONTRACT,
            status=Status.FAIL,
            summary=f"{len(failures)} contract(s) violated by the candidate",
            evidence=tuple(f"{f.rule} [{f.fixture}]: {f.detail}" for f in failures),
        )
    return GateResult(
        name=GateName.QUALITY_CONTRACT,
        status=Status.PASS,
        summary=f"{len(findings)} contract checks passed on the candidate",
    )


def performance_gate(
    a_captures: dict[str, Capture],
    b_captures: dict[str, Capture],
    *,
    max_flushed_pixels_delta_pct: float,
    max_flushes_delta_pct: float,
) -> GateResult:
    """Relative render-cost change between A and B (x86 proxy, not battery)."""
    shared = sorted(set(a_captures) & set(b_captures))
    if not shared:
        return GateResult(
            name=GateName.PERFORMANCE,
            status=Status.INCONCLUSIVE,
            summary="no scenario was captured on both revisions",
        )
    problems: list[str] = []
    measured: list[dict[str, Any]] = []
    for name in shared:
        a = a_captures[name]
        b = b_captures[name]
        pixels_delta = _pct_delta(a.flushed_pixels(), b.flushed_pixels())
        flushes_delta = _pct_delta(a.flushes(), b.flushes())
        measured.append(
            {
                "scenario": name,
                "flushed_pixels_a": a.flushed_pixels(),
                "flushed_pixels_b": b.flushed_pixels(),
                "flushed_pixels_delta_pct": round(pixels_delta, 3),
                "flushes_a": a.flushes(),
                "flushes_b": b.flushes(),
                "flushes_delta_pct": round(flushes_delta, 3),
            }
        )
        if pixels_delta > max_flushed_pixels_delta_pct:
            problems.append(
                f"{name}: flushed_pixels {pixels_delta:+.1f}% "
                f"(budget {max_flushed_pixels_delta_pct:.1f}%)"
            )
        if flushes_delta > max_flushes_delta_pct:
            problems.append(
                f"{name}: flushes {flushes_delta:+.1f}% (budget {max_flushes_delta_pct:.1f}%)"
            )
    status = Status.FAIL if problems else Status.PASS
    return GateResult(
        name=GateName.PERFORMANCE,
        status=status,
        summary=(
            "render-cost proxy within budget" if not problems else "render-cost proxy over budget"
        ),
        evidence=tuple(problems),
        details={
            "note": "x86 render counters under virtual time; not a battery measurement",
            "measured": measured,
        },
    )


def hardware_gate() -> GateResult:
    """No physical radio on the host: nothing to measure, so BLOCKED."""
    return GateResult(
        name=GateName.HARDWARE,
        status=Status.BLOCKED,
        summary="no physical X6200 attached; hardware behaviour is not measured",
        required=False,
        details={
            "cause": "the framework runs on the host and never in the ROM",
            "needed": "an X6200 unit with the capture/instrumentation flow of milestone 7",
        },
    )


def energy_gate() -> GateResult:
    """No power meter: BLOCKED, and CPU% is never converted to watts."""
    return GateResult(
        name=GateName.ENERGY,
        status=Status.BLOCKED,
        summary="no power measurement; energy evidence must be physical",
        required=False,
        details={
            "cause": "render counters are not joules",
            "needed": "physical energy measurement (milestone 7)",
        },
    )


def overall(profile: GateProfile, gates: list[GateResult]) -> Status:
    """Combine the required gates; advisory gates never change the verdict."""
    required_statuses = [gate.status for gate in gates if profile.is_required(gate.name)]
    return combine(required_statuses)


def _pct_delta(a_value: int, b_value: int) -> float:
    if a_value == 0:
        return 0.0 if b_value == 0 else 100.0
    return (b_value - a_value) * 100.0 / a_value
