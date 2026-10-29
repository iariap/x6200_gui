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
from .inspection import inspection_of, iter_tree
from .known_issues import KnownIssue, baseline_issue_for
from .layout_checks import run_layout_checks
from .quality_config import QualityContracts
from .regions import Region, RegionMap
from .theme_checks import run_theme_checks
from .typography_checks import run_typography_checks
from .visual_diff import load_rgb, region_content_fraction

__all__ = ["ContractFinding", "run_contracts"]


def run_contracts(
    captures: list[Capture],
    region_map: RegionMap,
    contracts: QualityContracts,
    known_issues: tuple[KnownIssue, ...] = (),
    today: date | None = None,
) -> list[ContractFinding]:
    """Evaluate every contract for every capture, in a stable order.

    `known_issues` is the registry of recorded defects: a violation it already
    documents as an open, evidenced baseline defect is reported as an UNCHANGED
    DEFECT (still visible, no longer a new failure). Every other violation stays
    a failure, so a regression the registry does not record is not hidden.
    """
    findings: list[ContractFinding] = []
    day = today or date.today()
    for capture in captures:
        findings.append(_framebuffer(capture, region_map))
        findings.extend(_region_content(capture, region_map, contracts))
        findings.append(_coverage_levels(capture, contracts))
        findings.append(_coverage_required(capture, contracts))
        findings.extend(run_layout_checks(capture, contracts.layout))
        findings.extend(run_typography_checks(capture, contracts.typography))
        findings.extend(run_theme_checks(capture))
    waived = apply_waivers(findings, contracts, day)
    return mark_baseline_defects(waived, known_issues, day)


def mark_baseline_defects(
    findings: list[ContractFinding],
    known_issues: tuple[KnownIssue, ...],
    today: date,
) -> list[ContractFinding]:
    """Tag failures already recorded as an open known issue (UNCHANGED DEFECT).

    The finding keeps `ok=False` — it is a real defect — but carries the issue id
    in `baseline_issue`, which is what the gate reads to not fail on it. Nothing
    is dropped: a finding that no issue explains comes back untouched.
    """
    if not known_issues:
        return findings
    result: list[ContractFinding] = []
    for finding in findings:
        if finding.ok:
            result.append(finding)
            continue
        issue = baseline_issue_for(finding.rule, finding.fixture, known_issues, today)
        if issue is None:
            result.append(finding)
            continue
        result.append(
            ContractFinding(
                rule=finding.rule,
                fixture=finding.fixture,
                ok=False,
                detail=f"{finding.detail} — unchanged defect {issue.id}: {issue.title}",
                path=finding.path,
                baseline_issue=issue.id,
            )
        )
    return result


def run_structural_checks(
    captures: list[Capture], contracts: QualityContracts
) -> list[ContractFinding]:
    """The rules that read the harness' own metadata, with waivers applied.

    Object-tree geometry and typography, plus the theme the capture rendered:
    everything the harness reports about itself, never the pixels. The audit
    reports this side of the contracts on its own, without the pixel-region rules
    that need a rendered PNG to be interpreted.
    """
    findings: list[ContractFinding] = []
    for capture in captures:
        findings.extend(run_layout_checks(capture, contracts.layout))
        findings.extend(run_typography_checks(capture, contracts.typography))
        findings.extend(run_theme_checks(capture))
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
    # The shared app window is opaque and sits on top of the screen: while a capture
    # shows it, the pixels it covers are not visible, so each region's content is
    # measured over its VISIBLE part. The contract's intent does not change ("is the
    # component being drawn"): a blank waterfall still scores ~0 on the visible part
    # (negative control in `tests/test_contracts.py`).
    modal = _visible_modal_surface(capture, contracts.modal_surface)
    findings: list[ContractFinding] = []
    for name, minimum in sorted(contracts.region_min_content.items()):
        region = region_map.get(name)
        # Only the regions the contract declares (`visible_under_modal`) are measured
        # over their visible part: the window is a flat plate, and where it covers a
        # band the pixels left are the ones the user sees.
        exclude = modal if modal is not None and name in contracts.modal_visible_regions else None
        if exclude is not None and not _has_visible_row(region, exclude):
            # The window leaves no complete row of the region on screen: either it
            # covers all of it or it only leaves the screen margin around its box
            # (the frequency row: 796 of 800 columns). Those pixels cannot answer
            # "is it drawn" -they are the window's own content and the margin-, so
            # the region is declared, never failed and never counted as content of
            # the window on top.
            findings.append(
                ContractFinding(
                    rule=f"region_min_content:{name}",
                    fixture=capture.name,
                    ok=True,
                    detail=(
                        "region fully covered by the modal window (no row is visible): not measured"
                    ),
                )
            )
            continue
        # A fixed tolerance: the contract asks "is there content", not "did it
        # change", so a coarse threshold avoids counting compression noise.
        measured = region_content_fraction(array, region, 24, exclude)
        findings.append(
            ContractFinding(
                rule=f"region_min_content:{name}",
                fixture=capture.name,
                ok=measured >= minimum,
                detail=f"content {measured:.4f} >= {minimum:.4f}",
            )
        )
    return findings


def _has_visible_row(region: Region, exclude: tuple[int, int, int, int]) -> bool:
    """Whether at least one whole row of `region` is outside the window `exclude`.

    The regions of `region_min_content` are horizontal bands of the screen, so the
    band is on screen only if one of its rows is: a row the window covers (even
    partially) does not show the component. A window that leaves just the margin
    around its box -the shared dialog is 796 px wide on an 800 px screen- leaves
    no row: what is left at the edges is the margin, not the band.
    """
    x0, y0, x1, y1 = region.box
    ex0, ey0, ex1, ey1 = exclude

    if not (ex0 < x1 and ex1 > x0):
        return True  # the window does not overlap the region horizontally
    # The window is a rectangle: with the horizontal overlap established, a row is
    # covered exactly when it falls inside the window's vertical span.
    return y0 < ey0 or y1 > ey1


def _visible_modal_surface(
    capture: Capture, rect: tuple[int, int, int, int] | None
) -> tuple[int, int, int, int] | None:
    """The modal window's rectangle when this capture shows it, else `None`.

    `dialog_init()` creates one window per app with `dialog_style`'s geometry
    (796x348 at x2/y66, `src/styles.h`): in the `--inspect` tree it is the object
    whose VISIBLE box is exactly that rectangle and which paints a surface (opaque
    background or the theme's image). Without an inspection block nothing is
    assumed: the region is measured whole, exactly as before.
    """
    if rect is None:
        return None
    inspection = inspection_of(capture)
    if inspection is None:
        return None
    for node, _ancestors, _path in iter_tree(inspection.nodes):
        box = node.visible
        if node.hidden or box is None:
            continue
        if (box.x, box.y, box.w, box.h) == rect and (node.bg_opa >= 250 or node.bg_image):
            return rect
    return None


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
