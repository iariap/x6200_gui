"""Quality contracts: objective rules checked on the candidate (B) alone.

Contracts are independent of the baseline — they are the "C" of the plan. They
answer questions a diff cannot: is the framebuffer the size the layout expects,
is every component on screen instead of blank, is the coverage table honest and
complete, and — from the harness' structural inspection — does the layout hold
(text fits, nothing clipped or stacked, controls big enough) with legible
colours and real glyphs. A diff of zero is not proof of quality; these rules are.
"""

from __future__ import annotations

from datetime import date

from .capture import Capture
from .findings import ContractFinding
from .layout_checks import run_layout_checks
from .quality_config import QualityContracts
from .regions import RegionMap
from .typography_checks import run_typography_checks
from .visual_diff import load_rgb, region_content_fraction

__all__ = ["ContractFinding", "run_contracts"]


def run_contracts(
    captures: list[Capture], region_map: RegionMap, contracts: QualityContracts
) -> list[ContractFinding]:
    """Evaluate every contract for every capture, in a stable order."""
    findings: list[ContractFinding] = []
    today = date.today()
    for capture in captures:
        findings.append(_framebuffer(capture, region_map))
        findings.extend(_region_content(capture, region_map, contracts))
        findings.append(_coverage_levels(capture, contracts))
        findings.append(_coverage_required(capture, contracts))
        findings.extend(run_layout_checks(capture, contracts.layout))
        findings.extend(run_typography_checks(capture, contracts.typography))
    return apply_waivers(findings, contracts, today)


def run_structural_checks(
    captures: list[Capture], contracts: QualityContracts
) -> list[ContractFinding]:
    """Only the layout + typography checks, with waivers applied.

    The audit reports this side of the contracts on its own, without the
    pixel-region rules that need a rendered PNG to be interpreted.
    """
    findings: list[ContractFinding] = []
    for capture in captures:
        findings.extend(run_layout_checks(capture, contracts.layout))
        findings.extend(run_typography_checks(capture, contracts.typography))
    return apply_waivers(findings, contracts, date.today())


def apply_waivers(
    findings: list[ContractFinding], contracts: QualityContracts, today: date
) -> list[ContractFinding]:
    """Turn a failing finding covered by an active exception into a recorded waiver.

    The finding stays in the report, marked `ok` with the reason and the expiry,
    so an exception is visible instead of silently dropping evidence.
    """
    result: list[ContractFinding] = []
    for finding in findings:
        if finding.ok:
            result.append(finding)
            continue
        waiver = contracts.waiver(finding.rule, finding.fixture, finding.path, today)
        if waiver is None:
            result.append(finding)
            continue
        result.append(
            ContractFinding(
                rule=finding.rule,
                fixture=finding.fixture,
                ok=True,
                detail=(
                    f"waived until {waiver.until} by {waiver.author}: {waiver.reason} "
                    f"({finding.detail})"
                ),
                path=finding.path,
            )
        )
    return result


def _framebuffer(capture: Capture, region_map: RegionMap) -> ContractFinding:
    ok = capture.width == region_map.width and capture.height == region_map.height
    expected = f"{region_map.width}x{region_map.height}"
    return ContractFinding(
        rule="framebuffer_size",
        fixture=capture.name,
        ok=ok,
        detail=f"{capture.width}x{capture.height} (expected {expected})",
    )


def _region_content(
    capture: Capture, region_map: RegionMap, contracts: QualityContracts
) -> list[ContractFinding]:
    if not contracts.region_min_content:
        return []
    array = load_rgb(capture.png)
    findings: list[ContractFinding] = []
    for name, minimum in sorted(contracts.region_min_content.items()):
        region = region_map.get(name)
        # A fixed tolerance: the contract asks "is there content", not "did it
        # change", so a coarse threshold avoids counting compression noise.
        measured = region_content_fraction(array, region, 24)
        findings.append(
            ContractFinding(
                rule=f"region_min_content:{name}",
                fixture=capture.name,
                ok=measured >= minimum,
                detail=f"content {measured:.4f} >= {minimum:.4f}",
            )
        )
    return findings


def _coverage_levels(capture: Capture, contracts: QualityContracts) -> ContractFinding:
    allowed = set(contracts.coverage_levels)
    if not allowed:
        return ContractFinding("coverage_levels", capture.name, True, "no level constraint")
    bad = [
        f"{entry.get('component')}={entry.get('level')}"
        for entry in capture.coverage
        if entry.get("level") not in allowed
    ]
    return ContractFinding(
        rule="coverage_levels",
        fixture=capture.name,
        ok=not bad,
        detail="all levels allowed" if not bad else f"unexpected levels: {', '.join(bad)}",
    )


def _coverage_required(capture: Capture, contracts: QualityContracts) -> ContractFinding:
    if not contracts.required_coverage_components:
        return ContractFinding("coverage_required", capture.name, True, "no required components")
    present = {entry.get("component") for entry in capture.coverage}
    missing = [name for name in contracts.required_coverage_components if name not in present]
    return ContractFinding(
        rule="coverage_required",
        fixture=capture.name,
        ok=not missing,
        detail="all required components declared" if not missing else f"missing: {missing}",
    )
