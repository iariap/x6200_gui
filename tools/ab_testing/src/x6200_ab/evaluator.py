"""Turn the measured data of a comparison into gates and one verdict.

This is the phase that knows nothing about Docker or git: it receives the
captures of A and B, their diffs, the known-issue observations and the contract
findings, runs the six gates and combines the *required* ones into a single
`Status`. An improvement (a known issue that A had and B fixes) is recorded
explicitly so the report can say so instead of calling it "no change".
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from typing import Any

from .capture import Capture
from .contracts import ContractFinding
from .known_issues import (
    IssueClassification,
    IssueObservation,
    KnownIssue,
    classify,
    evaluate_detector,
)
from .quality_config import Budgets, QualityContracts
from .quality_gates import (
    GateProfile,
    GateResult,
    energy_gate,
    functional_gate,
    hardware_gate,
    overall,
    performance_gate,
    quality_contract_gate,
    visual_regression_gate,
)
from .regions import RegionMap
from .revisions import Revision
from .status import Status
from .visual_diff import DiffResult

INTRODUCED = frozenset({IssueClassification.REGRESSION, IssueClassification.NEW_DEFECT})


@dataclass(frozen=True)
class ComparisonData:
    """Everything two revisions produced, before any judgement is applied."""

    revision_a: Revision
    revision_b: Revision
    scenario_names: tuple[str, ...]
    a_captures: dict[str, Capture]
    b_captures: dict[str, Capture]
    diffs: tuple[DiffResult, ...]
    issues: tuple[IssueObservation, ...]
    contract_findings: tuple[ContractFinding, ...]


@dataclass(frozen=True)
class Evaluation:
    """The verdict of one comparison, with every gate's evidence."""

    experiment_id: str
    date: str
    profile: str
    revision_a: Revision
    revision_b: Revision
    scenario_names: tuple[str, ...]
    diffs: tuple[DiffResult, ...]
    issues: tuple[IssueObservation, ...]
    contract_findings: tuple[ContractFinding, ...]
    gates: tuple[GateResult, ...]
    status: Status
    improvement: bool
    captures: dict[str, list[dict[str, Any]]]

    @property
    def failed_gates(self) -> list[GateResult]:
        return [gate for gate in self.gates if gate.status is Status.FAIL]

    @property
    def blocked_gates(self) -> list[GateResult]:
        return [gate for gate in self.gates if gate.status is Status.BLOCKED]

    def as_dict(self) -> dict[str, Any]:
        return {
            "experiment_id": self.experiment_id,
            "date": self.date,
            "profile": self.profile,
            "status": self.status.value,
            "exit_code": self.status.exit_code,
            "improvement": self.improvement,
            "revision_a": {"label": self.revision_a.label, "sha": self.revision_a.sha},
            "revision_b": {"label": self.revision_b.label, "sha": self.revision_b.sha},
            "scenarios": list(self.scenario_names),
            "captures": self.captures,
            "gates": [gate.as_dict() for gate in self.gates],
            "known_issues": [issue.as_dict() for issue in self.issues],
            "contracts": [finding.as_dict() for finding in self.contract_findings],
            "diffs": [
                {
                    "scenario": diff.scenario,
                    "identical": diff.identical,
                    "changed_pixels": diff.changed_pixels,
                    "changed_fraction": round(diff.changed_fraction, 6),
                    "boxes": diff.boxes_dicts(),
                    "regions": diff.regions_dicts(),
                    "dimension_mismatch": diff.dimension_mismatch,
                }
                for diff in self.diffs
            ],
        }


def observe_issues(
    known_issues: tuple[KnownIssue, ...],
    a_captures: dict[str, Capture],
    b_captures: dict[str, Capture],
    region_map: RegionMap,
) -> tuple[IssueObservation, ...]:
    """Run every detector on A and B and classify the issue."""
    observations: list[IssueObservation] = []
    for issue in known_issues:
        capture_a = a_captures.get(issue.fixture)
        capture_b = b_captures.get(issue.fixture)
        if issue.detector is None or capture_a is None or capture_b is None:
            classification, detail = classify(issue, None, None)
            observations.append(IssueObservation(issue, None, None, classification, detail))
            continue
        outcome_a = evaluate_detector(issue.detector, capture_a, region_map)
        outcome_b = evaluate_detector(issue.detector, capture_b, region_map)
        classification, detail = classify(issue, outcome_a.reproduced, outcome_b.reproduced)
        observations.append(
            IssueObservation(
                issue=issue,
                reproduced_a=outcome_a.reproduced,
                reproduced_b=outcome_b.reproduced,
                classification=classification,
                detail=f"{detail}; A: {outcome_a.detail}; B: {outcome_b.detail}",
            )
        )
    return tuple(observations)


def evaluate(
    data: ComparisonData,
    *,
    region_map: RegionMap,
    contracts: QualityContracts,
    budgets: Budgets,
    profile: GateProfile,
    experiment_id: str,
    today: date | None = None,
) -> Evaluation:
    """Run the six gates and combine the required ones into the verdict."""
    day = today or date.today()
    introduced = [issue for issue in data.issues if issue.classification in INTRODUCED]
    allowed_regions = _allowed_regions(data, contracts, day)
    gates = [
        functional_gate(data.a_captures, data.b_captures, list(data.scenario_names)),
        visual_regression_gate(
            list(data.diffs),
            region_map,
            allowed_regions=allowed_regions,
            introduced_issues=introduced,
            review_regions=frozenset(contracts.review_regions),
        ),
        quality_contract_gate(list(data.contract_findings)),
        performance_gate(
            data.a_captures,
            data.b_captures,
            max_flushed_pixels_delta_pct=budgets.performance.max_flushed_pixels_delta_pct,
            max_flushes_delta_pct=budgets.performance.max_flushes_delta_pct,
        ),
        hardware_gate(),
        energy_gate(),
    ]
    applied = [
        GateResult(
            name=gate.name,
            status=gate.status,
            summary=gate.summary,
            required=profile.is_required(gate.name),
            evidence=gate.evidence,
            details=gate.details,
        )
        for gate in gates
    ]
    status = overall(profile, applied)
    improvement = any(issue.classification is IssueClassification.FIXED for issue in data.issues)
    captures = {
        "a": [
            capture.summary() for capture in sorted(data.a_captures.values(), key=lambda c: c.name)
        ],
        "b": [
            capture.summary() for capture in sorted(data.b_captures.values(), key=lambda c: c.name)
        ],
    }
    return Evaluation(
        experiment_id=experiment_id,
        date=day.isoformat(),
        profile=profile.name,
        revision_a=data.revision_a,
        revision_b=data.revision_b,
        scenario_names=data.scenario_names,
        diffs=data.diffs,
        issues=data.issues,
        contract_findings=data.contract_findings,
        gates=tuple(applied),
        status=status,
        improvement=improvement,
        captures=captures,
    )


def _allowed_regions(
    data: ComparisonData, contracts: QualityContracts, day: date
) -> dict[str, frozenset[str]]:
    """Per fixture, the regions whose changes are explained (not regressions)."""
    allowed: dict[str, set[str]] = {}
    for issue in data.issues:
        if issue.classification is IssueClassification.FIXED and issue.issue.region is not None:
            allowed.setdefault(issue.issue.fixture, set()).add(issue.issue.region)
    for change in contracts.active_expectations(day):
        for name in data.scenario_names:
            allowed.setdefault(name, set()).add(change.region)
    return {name: frozenset(regions) for name, regions in allowed.items()}
