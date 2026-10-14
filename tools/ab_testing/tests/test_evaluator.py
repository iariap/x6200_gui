"""Tests for the six gates, the evaluator and every verdict."""

from __future__ import annotations

from pathlib import Path

import pytest

from helpers import blank_image, budgets, contracts, draw_box, make_capture, revision
from x6200_ab.contracts import ContractFinding
from x6200_ab.evaluator import ComparisonData, evaluate, observe_issues
from x6200_ab.known_issues import Detector, KnownIssue
from x6200_ab.quality_gates import (
    ALL_GATES,
    GateName,
    GateProfile,
    GateResult,
    overall,
    performance_gate,
    visual_regression_gate,
)
from x6200_ab.regions import RegionMap
from x6200_ab.status import Status, combine
from x6200_ab.visual_diff import DiffResult, compute_diff


def _multiplier_issue() -> KnownIssue:
    return KnownIssue(
        id="UI-001",
        title="multiplier label visible",
        severity="major",
        fixture="home",
        region="multiplicador",
        status="fixed",
        note="",
        evidence=(),
        detector=Detector(
            kind="region_content_above",
            region="multiplicador",
            pixel_tolerance=24,
            max_content=0.05,
        ),
        exclusions=(),
    )


def _data(
    tmp_path: Path,
    region_map: RegionMap,
    a_image,
    b_image,
    *,
    issues: tuple = (),
    findings: tuple = (),
    scenarios: tuple[str, ...] = ("home",),
) -> ComparisonData:
    a = make_capture(tmp_path / "a", "home", a_image, rev=revision("a", "a" * 40))
    b = make_capture(tmp_path / "b", "home", b_image, rev=revision("b", "b" * 40))
    diff = compute_diff(
        a,
        b,
        region_map,
        pixel_tolerance=12,
        min_box_pixels=4,
        diff_png=tmp_path / "diffs" / "home-diff.png",
        overlay_png=tmp_path / "diffs" / "home-overlay.png",
    )
    return ComparisonData(
        revision_a=a.revision,
        revision_b=b.revision,
        scenario_names=scenarios,
        a_captures={"home": a},
        b_captures={"home": b},
        diffs=(diff,),
        issues=issues,
        contract_findings=findings,
    )


def _evaluate(tmp_path: Path, region_map: RegionMap, data: ComparisonData, **kwargs):
    return evaluate(
        data,
        region_map=region_map,
        contracts=kwargs.pop("contracts_override", contracts()),
        budgets=budgets(),
        profile=kwargs.pop("profile", GateProfile.host_ui()),
        experiment_id="AB-test",
        **kwargs,
    )


def test_identical_is_pass_with_blocked_advisory_gates(
    tmp_path: Path, region_map: RegionMap
) -> None:
    image = draw_box(blank_image(), 0, 418, 800, 62)
    data = _data(tmp_path, region_map, image, image)
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.status is Status.PASS
    assert evaluation.status.exit_code == 0
    assert {gate.name for gate in evaluation.blocked_gates} == {
        GateName.HARDWARE,
        GateName.ENERGY,
    }
    visual = next(g for g in evaluation.gates if g.name is GateName.VISUAL_REGRESSION)
    assert visual.status is Status.PASS
    payload = evaluation.as_dict()
    assert payload["captures"]["a"][0]["scenario"] == "home"
    assert payload["captures"]["a"][0]["coverage"][0]["component"] == "main_screen.c"


def test_unexplained_change_fails(tmp_path: Path, region_map: RegionMap) -> None:
    a_image = blank_image()
    b_image = draw_box(a_image, 300, 430, 60, 30)
    data = _data(tmp_path, region_map, a_image, b_image)
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.status is Status.FAIL
    assert evaluation.status.exit_code == 1
    visual = next(g for g in evaluation.gates if g.name is GateName.VISUAL_REGRESSION)
    assert visual.status is Status.FAIL
    assert any("box" in item for item in visual.evidence)
    assert any(path.endswith("home-diff.png") for path in visual.evidence)


def test_fixed_known_issue_is_an_improvement(tmp_path: Path, region_map: RegionMap) -> None:
    defective = draw_box(blank_image(), 762, 12, 28, 8)
    clean = blank_image()
    # A is the defective revision, B fixes it (it is the clean one)
    a = make_capture(tmp_path / "a", "home", defective, rev=revision("a", "a" * 40))
    b = make_capture(tmp_path / "b", "home", clean, rev=revision("b", "b" * 40))
    issues = observe_issues((_multiplier_issue(),), {"home": a}, {"home": b}, region_map)
    assert issues[0].classification.value == "FIXED"
    diff = compute_diff(a, b, region_map, pixel_tolerance=12, min_box_pixels=4)
    data = ComparisonData(
        revision_a=a.revision,
        revision_b=b.revision,
        scenario_names=("home",),
        a_captures={"home": a},
        b_captures={"home": b},
        diffs=(diff,),
        issues=issues,
        contract_findings=(),
    )
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.improvement is True
    assert evaluation.status is Status.PASS
    assert evaluation.status.exit_code == 0


def test_change_in_review_region_needs_a_human(tmp_path: Path, region_map: RegionMap) -> None:
    a_image = blank_image()
    b_image = draw_box(a_image, 300, 430, 60, 30)
    data = _data(tmp_path, region_map, a_image, b_image)
    evaluation = _evaluate(
        tmp_path,
        region_map,
        data,
        contracts_override=contracts(review_regions=("botones",)),
    )
    assert evaluation.status is Status.REVIEW_REQUIRED
    assert evaluation.status.exit_code == 4


def test_full_profile_blocks_on_hardware(tmp_path: Path, region_map: RegionMap) -> None:
    image = blank_image()
    data = _data(tmp_path, region_map, image, image)
    evaluation = _evaluate(tmp_path, region_map, data, profile=GateProfile.full())
    assert evaluation.status is Status.BLOCKED
    assert evaluation.status.exit_code == 2


def test_dimension_mismatch_fails(tmp_path: Path, region_map: RegionMap) -> None:
    data = _data(tmp_path, region_map, blank_image(800, 480), blank_image(640, 480))
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.status is Status.FAIL


def test_contract_failure_fails_the_run(tmp_path: Path, region_map: RegionMap) -> None:
    image = blank_image()
    finding = ContractFinding(rule="coverage_required", fixture="home", ok=False, detail="missing")
    data = _data(tmp_path, region_map, image, image, findings=(finding,))
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.status is Status.FAIL
    contract_gate = next(g for g in evaluation.gates if g.name is GateName.QUALITY_CONTRACT)
    assert contract_gate.status is Status.FAIL


def test_unchanged_baseline_defect_does_not_fail_the_run(
    tmp_path: Path, region_map: RegionMap
) -> None:
    """A failure the registry already records as open is reported, not blocking."""
    finding = ContractFinding(
        rule="region_min_content:frecuencia",
        fixture="home",
        ok=False,
        detail="content 0.0000 >= 0.0200",
        baseline_issue="UI-010",
    )
    data = _data(tmp_path, region_map, blank_image(), blank_image(), findings=(finding,))
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.status is Status.PASS
    contract_gate = next(g for g in evaluation.gates if g.name is GateName.QUALITY_CONTRACT)
    assert contract_gate.status is Status.PASS
    assert contract_gate.details["unchanged_defects"][0]["baseline_issue"] == "UI-010"
    assert any("UNCHANGED DEFECT" in item for item in contract_gate.evidence)


def test_regression_next_to_a_baseline_defect_still_fails(
    tmp_path: Path, region_map: RegionMap
) -> None:
    baseline = ContractFinding(
        rule="region_min_content:frecuencia",
        fixture="home",
        ok=False,
        detail="known",
        baseline_issue="UI-010",
    )
    regression = ContractFinding(
        rule="region_min_content:waterfall", fixture="home", ok=False, detail="new violation"
    )
    data = _data(
        tmp_path, region_map, blank_image(), blank_image(), findings=(baseline, regression)
    )
    evaluation = _evaluate(tmp_path, region_map, data)
    assert evaluation.status is Status.FAIL
    contract_gate = next(g for g in evaluation.gates if g.name is GateName.QUALITY_CONTRACT)
    assert contract_gate.status is Status.FAIL
    assert any("waterfall" in item for item in contract_gate.evidence)
    assert contract_gate.details["unchanged_defects"][0]["rule"] == "region_min_content:frecuencia"


# --- gate-level unit tests ---------------------------------------------------


def test_performance_gate_inconclusive_without_shared_scenarios(
    tmp_path: Path, region_map: RegionMap
) -> None:
    a = make_capture(tmp_path / "a", "home", blank_image(), rev=revision("a", "a" * 40))
    gate = performance_gate(
        {"home": a}, {}, max_flushed_pixels_delta_pct=10.0, max_flushes_delta_pct=20.0
    )
    assert gate.status is Status.INCONCLUSIVE


def test_combine_prefers_the_worst_status() -> None:
    assert combine([]) is Status.PASS
    assert combine([Status.PASS, Status.INCONCLUSIVE]) is Status.INCONCLUSIVE
    assert combine([Status.INCONCLUSIVE, Status.BLOCKED]) is Status.BLOCKED
    assert combine([Status.BLOCKED, Status.FAIL]) is Status.FAIL


def test_overall_ignores_advisory_gates() -> None:
    profile = GateProfile.host_ui()
    gates = [
        GateResult(GateName.FUNCTIONAL, Status.PASS, "", required=True),
        GateResult(GateName.HARDWARE, Status.BLOCKED, "", required=False),
        GateResult(GateName.ENERGY, Status.BLOCKED, "", required=False),
    ]
    assert overall(profile, gates) is Status.PASS
    assert set(ALL_GATES) == {gate.name for gate in gates} | {
        GateName.VISUAL_REGRESSION,
        GateName.QUALITY_CONTRACT,
        GateName.PERFORMANCE,
    }


def test_visual_gate_reports_introduced_issue(region_map: RegionMap) -> None:
    from x6200_ab.known_issues import IssueClassification, IssueObservation

    observation = IssueObservation(
        issue=_multiplier_issue(),
        reproduced_a=False,
        reproduced_b=True,
        classification=IssueClassification.REGRESSION,
        detail="",
    )
    gate = visual_regression_gate(
        [],
        region_map,
        allowed_regions={},
        introduced_issues=[observation],
        review_regions=frozenset(),
    )
    assert gate.status is Status.FAIL


def test_diff_result_with_dimension_mismatch_fails(region_map: RegionMap) -> None:
    # a DiffResult with no boxes (dimension mismatch) must not crash the gate
    diff = DiffResult(
        scenario="home",
        identical=False,
        changed_pixels=0,
        total_pixels=0,
        changed_fraction=0.0,
        boxes=(),
        regions=(),
        dimension_mismatch=((800, 480), (640, 480)),
        diff_png=None,
        overlay_png=None,
    )
    gate = visual_regression_gate(
        [diff],
        region_map,
        allowed_regions={},
        introduced_issues=[],
        review_regions=frozenset(),
    )
    assert gate.status is Status.FAIL


@pytest.mark.parametrize("status", list(Status))
def test_exit_codes_are_stable(status: Status) -> None:
    expected = {
        Status.PASS: 0,
        Status.FAIL: 1,
        Status.BLOCKED: 2,
        Status.INCONCLUSIVE: 3,
        Status.REVIEW_REQUIRED: 4,
    }
    assert status.exit_code == expected[status]
