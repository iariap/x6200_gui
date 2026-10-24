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
from datetime import date
from enum import StrEnum
from typing import Any

from .capture import Capture
from .contracts import ContractFinding
from .known_issues import IssueObservation
from .quality_config import (
    NO_PERFORMANCE_EXCEPTIONS,
    ExpectedChange,
    PerformanceExceptions,
)
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


@dataclass(frozen=True)
class FixtureAllowance:
    """What makes ONE fixture's pixel changes acceptable (never a global set).

    `regions` is what `unexplained_changed_pixels` reads; `changes` and `fixes`
    are the provenance the report prints: the dated entries that declare those
    regions for this fixture and the known issues the candidate fixes on it. A
    region allowed for another fixture never lands here (#296).
    """

    regions: frozenset[str] = frozenset()
    changes: tuple[ExpectedChange, ...] = ()
    fixes: tuple[str, ...] = ()


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
    allowances: dict[str, FixtureAllowance],
    introduced_issues: list[IssueObservation],
    review_regions: frozenset[str] = frozenset(),
) -> GateResult:
    """Unexplained pixel changes and newly introduced defects fail the gate.

    A change confined to a `review_regions` region is not failed: it becomes
    REVIEW REQUIRED, because "different there" may be an aesthetic change that
    only a human can judge.

    `allowances` is per fixture: the regions a change may land in without being
    a regression are only the ones this fixture's own declarations name, and the
    report prints which entry (reason, author, expiry) explained them.
    """
    problems: list[str] = []
    evidence: list[str] = []
    explained_changes: list[str] = []
    explained_by: list[dict[str, Any]] = []
    review: list[str] = []
    for diff in diffs:
        if diff.dimension_mismatch is not None:
            a_shape, b_shape = diff.dimension_mismatch
            problems.append(f"{diff.scenario}: framebuffer {a_shape} vs {b_shape}")
            continue
        if diff.identical:
            continue
        allowance = allowances.get(diff.scenario, FixtureAllowance())
        allowed = allowance.regions
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
            explaining = _explaining(diff, allowance)
            explained_changes.append(_explained_change(diff, explaining))
            explained_by.append(_explained_record(diff, explaining))
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
            details={"explained_changes": explained_changes, "explained_by": explained_by},
        )
    return GateResult(
        name=GateName.VISUAL_REGRESSION,
        status=Status.PASS,
        summary="captures are pixel-identical to the baseline",
    )


def _explaining(diff: DiffResult, allowance: FixtureAllowance) -> FixtureAllowance:
    """The part of the allowance the diff actually lands on.

    An entry whose region is allowed but did not move does not explain this
    change: the report names the declarations that cover the changed pixels, not
    every declaration the fixture has.
    """
    changed = set(diff.changed_region_names())
    regions = frozenset(allowance.regions & changed)
    if not regions:
        return allowance
    return FixtureAllowance(
        regions=regions,
        changes=tuple(change for change in allowance.changes if change.region in changed),
        fixes=allowance.fixes,
    )


def _explained_change(diff: DiffResult, allowance: FixtureAllowance) -> str:
    """The human line of an explained change, with the entry that explained it."""
    declared = [
        f"`{change.region}` declared by `{change.author}` until {change.until}: "
        f"{one_line(change.reason)}"
        for change in allowance.changes
    ]
    if allowance.fixes:
        declared.append(f"the fix of {', '.join(allowance.fixes)}")
    return (
        f"{diff.scenario}: {diff.changed_pixels} changed pixels, all inside "
        f"{sorted(allowance.regions)} — {'; '.join(declared)}"
    )


def _explained_record(diff: DiffResult, allowance: FixtureAllowance) -> dict[str, Any]:
    """The machine-readable provenance of one explained change."""
    return {
        "scenario": diff.scenario,
        "changed_pixels": diff.changed_pixels,
        "regions": sorted(allowance.regions),
        "changes": [change.as_dict() for change in allowance.changes],
        "fixes": list(allowance.fixes),
    }


def one_line(text: str) -> str:
    """A `reason` collapsed to one line.

    The gate and the report print the same entry text, so both collapse it the
    same way: a YAML folded scalar arrives with newlines and the tables take one
    line per entry.
    """
    return " ".join(text.split())


def quality_contract_gate(findings: list[ContractFinding]) -> GateResult:
    """The candidate must satisfy the objective contracts on its own.

    A failure the known-issue registry already records as an open baseline
    defect is an UNCHANGED DEFECT: reported prominently, but not a new failure,
    so it does not fail the gate. Any other violation does — the registry only
    covers the exact (rule, fixture, region) it declares.
    """
    failures = [
        finding for finding in findings if not finding.ok and finding.baseline_issue is None
    ]
    unchanged = [finding for finding in findings if finding.unchanged_defect]
    unchanged_evidence = tuple(
        f"UNCHANGED DEFECT {finding.rule} [{finding.fixture}] "
        f"({finding.baseline_issue}): {finding.detail}"
        for finding in unchanged
    )
    if failures:
        return GateResult(
            name=GateName.QUALITY_CONTRACT,
            status=Status.FAIL,
            summary=(
                f"{len(failures)} contract(s) violated by the candidate"
                + (
                    f"; {len(unchanged)} unchanged baseline defect(s) also recorded"
                    if unchanged
                    else ""
                )
            ),
            evidence=tuple(f"{f.rule} [{f.fixture}]: {f.detail}" for f in failures)
            + unchanged_evidence,
            details={"unchanged_defects": [finding.as_dict() for finding in unchanged]},
        )
    if unchanged:
        return GateResult(
            name=GateName.QUALITY_CONTRACT,
            status=Status.PASS,
            summary=(
                f"{len(findings) - len(unchanged)} contract checks passed; "
                f"{len(unchanged)} unchanged baseline defect(s) recorded, not blocking"
            ),
            evidence=unchanged_evidence,
            details={"unchanged_defects": [finding.as_dict() for finding in unchanged]},
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
    exceptions: PerformanceExceptions = NO_PERFORMANCE_EXCEPTIONS,
    today: date | None = None,
) -> GateResult:
    """Relative render-cost change between A and B (x86 proxy, not battery).

    The global budgets of `budgets.yaml` are fixed before the run and are never
    relaxed. A scenario the dated registry lists — while the entry is current and
    every breached proxy is inside its ceilings — passes *by exception*: the
    breach stays in the evidence with the reason and the expiry, it is not
    hidden. An expired entry fails closed, a delta above the ceilings fails, and
    a scenario with no entry keeps the global budget exactly as before.
    """
    day = today or date.today()
    shared = sorted(set(a_captures) & set(b_captures))
    if not shared:
        return GateResult(
            name=GateName.PERFORMANCE,
            status=Status.INCONCLUSIVE,
            summary="no scenario was captured on both revisions",
        )
    problems: list[str] = []
    evidence: list[str] = []
    excused: list[dict[str, Any]] = []
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
        breaches: list[str] = []
        if pixels_delta > max_flushed_pixels_delta_pct:
            breaches.append(
                f"{name}: flushed_pixels {pixels_delta:+.1f}% "
                f"(budget {max_flushed_pixels_delta_pct:.1f}%)"
            )
        if flushes_delta > max_flushes_delta_pct:
            breaches.append(
                f"{name}: flushes {flushes_delta:+.1f}% (budget {max_flushes_delta_pct:.1f}%)"
            )
        if not breaches:
            continue
        waiver = exceptions.waiver(name, pixels_delta, flushes_delta, day)
        if waiver is not None:
            evidence.append(
                f"{'; '.join(breaches)} — accepted deviation for {name} "
                f"(expires {waiver.expires}): {waiver.reason}"
            )
            excused.append(
                {
                    "scenario": name,
                    "flushed_pixels_delta_pct": round(pixels_delta, 3),
                    "flushes_delta_pct": round(flushes_delta, 3),
                    "ceilings": waiver.ceilings.as_dict(),
                    "reason": waiver.reason,
                    "author": waiver.author,
                    "expires": waiver.expires,
                }
            )
            continue
        problems.extend(breaches)
        covering = exceptions.covering(name)
        if covering is not None and not covering.active(day):
            problems.append(f"{name}: the dated exception expired {covering.expires} — fail-closed")
        elif covering is not None:
            ceilings = covering.ceilings
            problems.append(
                f"{name}: over the exception ceilings (flushes ≤ "
                f"{ceilings.flushes_delta_pct:.1f}%, flushed_pixels ≤ "
                f"{ceilings.flushed_pixels_delta_pct:.1f}%)"
            )
    status = Status.FAIL if problems else Status.PASS
    if problems:
        summary = "render-cost proxy over budget"
    elif excused:
        summary = (
            "render-cost proxy within budget; "
            f"{len(excused)} scenario(s) accepted by a dated exception"
        )
    else:
        summary = "render-cost proxy within budget"
    return GateResult(
        name=GateName.PERFORMANCE,
        status=status,
        summary=summary,
        evidence=tuple(problems) + tuple(evidence),
        details={
            "note": "x86 render counters under virtual time; not a battery measurement",
            "budget": {
                "flushed_pixels_delta_pct": max_flushed_pixels_delta_pct,
                "flushes_delta_pct": max_flushes_delta_pct,
            },
            "measured": measured,
            "by_exception": excused,
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
