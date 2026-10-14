"""Known issues: recorded defects, their detectors and their A→B classification.

An issue is only trusted when the harness *reproduces* it: a detector runs on a
capture and says whether the defect is present. Issues with no detector yet
(the seeds from the repository history) are kept as `unverified` and never fail
a run. Given the detector's answer on A and on B, the issue is classified as
FIXED, UNCHANGED DEFECT, REGRESSION, NEW DEFECT or NOT REPRODUCED.

Exclusions are scoped and dated — a fixture, a region and an expiry date with a
reason — so a waiver cannot become permanent or global by accident.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import date
from enum import StrEnum
from pathlib import Path
from typing import Any

from .capture import Capture
from .configio import read_yaml_mapping
from .errors import ConfigError, HarnessError
from .inspection import inspection_of, iter_tree
from .regions import RegionMap
from .visual_diff import load_rgb, region_content_fraction

DETECTOR_KINDS = frozenset({"region_content_above", "leading_glyph_range"})
STATUSES = frozenset({"verified", "fixed", "unverified", "new"})
SEVERITIES = frozenset({"critical", "major", "minor", "info"})

# The contract rules that are scoped to one semantic region carry the region in
# their name (`region_min_content:frecuencia`); that is the region a baseline
# waiver is matched against.
REGION_RULE_PREFIX = "region_min_content:"


def contract_rule_region(rule: str) -> str | None:
    """The region a contract rule is scoped to, when it names one."""
    if rule.startswith(REGION_RULE_PREFIX):
        return rule[len(REGION_RULE_PREFIX) :]
    return None


class IssueClassification(StrEnum):
    """How one known issue behaves from A to B."""

    NOT_REPRODUCED = "NOT_REPRODUCED"
    FIXED = "FIXED"
    UNCHANGED_DEFECT = "UNCHANGED DEFECT"
    REGRESSION = "REGRESSION"
    NEW_DEFECT = "NEW DEFECT"
    UNVERIFIED = "UNVERIFIED"


@dataclass(frozen=True)
class Detector:
    """A deterministic check over one capture.

    Two kinds:

    * `region_content_above`: the region must stay near-uniform; content there
      means the element that should be hidden is being drawn;
    * `leading_glyph_range`: the label whose text matches `text_pattern` (the
      power clock is the only label with a voltage line) must start with a code
      point inside `glyph_range`. The harness's structural inspection carries
      that text, so the check is exact and needs no OCR.
    """

    kind: str
    region: str
    pixel_tolerance: int = 12
    max_content: float = 0.02
    text_pattern: str = ""
    glyph_range: tuple[int, int] = (0, 0)


@dataclass(frozen=True)
class Exclusion:
    """A scoped, dated waiver for a known issue.

    The scope is always fixture + region; when `rule` is set the waiver is
    additionally pinned to one contract rule (a contract finding whose rule,
    fixture and region match). There is no global form: a waiver without a rule
    never matches a contract finding.
    """

    fixture: str
    region: str
    until: str
    reason: str
    rule: str | None = None

    def active(self, fixture: str, today: date) -> bool:
        if self.fixture != fixture:
            return False
        try:
            return today <= date.fromisoformat(self.until)
        except ValueError as exc:
            raise ConfigError(f"exclusion until `{self.until}` is not an ISO date") from exc

    def covers(self, rule: str, fixture: str, region: str | None, today: date) -> bool:
        """True when the waiver covers one failing region contract.

        Matching is exact and scoped: fixture + region + rule must all agree and
        the waiver must still be within its expiry. A finding with no region
        (non-region rule) is never covered, so a waiver cannot go global.
        """
        if region is None or self.rule != rule or self.region != region:
            return False
        return self.active(fixture, today)

    def as_dict(self) -> dict[str, Any]:
        return {
            "fixture": self.fixture,
            "region": self.region,
            "until": self.until,
            "reason": self.reason,
            "rule": self.rule,
        }


@dataclass(frozen=True)
class KnownIssue:
    """One recorded defect."""

    id: str
    title: str
    severity: str
    fixture: str
    region: str | None
    status: str
    note: str
    evidence: tuple[str, ...]
    detector: Detector | None
    exclusions: tuple[Exclusion, ...]

    @property
    def is_open_defect(self) -> bool:
        """An evidenced defect that is present now (open, not fixed, not a guess).

        `verified` means the harness reproduces it today and `unverified` means
        it is only a recorded hypothesis; only an evidenced `verified` issue is
        an open baseline defect and can explain a failing contract.
        """
        return self.status == "verified" and bool(self.evidence)

    def as_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "title": self.title,
            "severity": self.severity,
            "fixture": self.fixture,
            "region": self.region,
            "status": self.status,
            "note": self.note,
            "evidence": list(self.evidence),
            "detector": None
            if self.detector is None
            else {
                "kind": self.detector.kind,
                "region": self.detector.region,
                "pixel_tolerance": self.detector.pixel_tolerance,
                "max_content": self.detector.max_content,
                "text_pattern": self.detector.text_pattern,
                "glyph_range": list(self.detector.glyph_range),
            },
            "exclusions": [exclusion.as_dict() for exclusion in self.exclusions],
        }


@dataclass(frozen=True)
class DetectorOutcome:
    """What a detector measured on one capture."""

    reproduced: bool
    measured: float
    detail: str


@dataclass(frozen=True)
class IssueObservation:
    """A known issue evaluated against A and B."""

    issue: KnownIssue
    reproduced_a: bool | None
    reproduced_b: bool | None
    classification: IssueClassification
    detail: str

    def as_dict(self) -> dict[str, Any]:
        return {
            "id": self.issue.id,
            "title": self.issue.title,
            "severity": self.issue.severity,
            "fixture": self.issue.fixture,
            "region": self.issue.region,
            "status": self.issue.status,
            "reproduced_a": self.reproduced_a,
            "reproduced_b": self.reproduced_b,
            "classification": self.classification.value,
            "detail": self.detail,
        }


def load_known_issues(path: Path, known_regions: set[str]) -> tuple[KnownIssue, ...]:
    """Load `quality/known_issues.yaml`."""
    data = read_yaml_mapping(path)
    raw = data.get("issues")
    if not isinstance(raw, list):
        raise ConfigError(f"{path}: `issues` must be a list")
    issues: list[KnownIssue] = []
    seen: set[str] = set()
    for entry in raw:
        issue = _parse_issue(entry, path, known_regions)
        if issue.id in seen:
            raise ConfigError(f"{path}: duplicate issue id `{issue.id}`")
        seen.add(issue.id)
        issues.append(issue)
    return tuple(issues)


def _parse_issue(entry: object, path: Path, known_regions: set[str]) -> KnownIssue:
    if not isinstance(entry, dict):
        raise ConfigError(f"{path}: every issue must be a mapping")
    required = ("id", "title", "severity", "fixture", "status")
    for key in required:
        if key not in entry:
            raise ConfigError(f"{path}: issue missing `{key}`")
    issue_id = str(entry["id"])
    severity = str(entry["severity"])
    if severity not in SEVERITIES:
        raise ConfigError(f"{path}: issue {issue_id} has unknown severity `{severity}`")
    status = str(entry["status"])
    if status not in STATUSES:
        raise ConfigError(f"{path}: issue {issue_id} has unknown status `{status}`")
    detector = _parse_detector(entry.get("detector"), issue_id, path, known_regions)
    region_value = entry.get("region")
    region = str(region_value) if region_value is not None else None
    if detector is None:
        # An issue with no detector is a recorded hypothesis: it lives outside
        # any main-screen region for now, so a region is optional.
        if region is not None and region not in known_regions:
            raise ConfigError(f"{path}: issue {issue_id} names unknown region `{region}`")
    elif region is None:
        raise ConfigError(f"{path}: issue {issue_id} has a detector but no `region`")
    exclusions = _parse_exclusions(entry.get("exclusions", []), issue_id, path, known_regions)
    evidence = entry.get("evidence", [])
    if not isinstance(evidence, list):
        raise ConfigError(f"{path}: issue {issue_id} `evidence` must be a list")
    return KnownIssue(
        id=issue_id,
        title=str(entry["title"]),
        severity=severity,
        fixture=str(entry["fixture"]),
        region=region,
        status=status,
        note=str(entry.get("note", "")),
        evidence=tuple(str(item) for item in evidence),
        detector=detector,
        exclusions=exclusions,
    )


def _parse_detector(
    raw: object, issue_id: str, path: Path, known_regions: set[str]
) -> Detector | None:
    if raw is None:
        return None
    if not isinstance(raw, dict):
        raise ConfigError(f"{path}: issue {issue_id} `detector` must be a mapping")
    kind = str(raw.get("kind", ""))
    if kind not in DETECTOR_KINDS:
        raise ConfigError(f"{path}: issue {issue_id} has unknown detector kind `{kind}`")
    region = str(raw.get("region", ""))
    if region not in known_regions:
        raise ConfigError(f"{path}: issue {issue_id} detector names unknown region `{region}`")
    if kind == "leading_glyph_range":
        return _parse_glyph_detector(raw, issue_id, path, region)
    return Detector(
        kind=kind,
        region=region,
        pixel_tolerance=int(raw.get("pixel_tolerance", 12)),
        max_content=float(raw.get("max_content", 0.02)),
    )


def _parse_glyph_detector(raw: dict[str, Any], issue_id: str, path: Path, region: str) -> Detector:
    pattern = str(raw.get("text_pattern", ""))
    if not pattern:
        raise ConfigError(f"{path}: issue {issue_id} `leading_glyph_range` needs `text_pattern`")
    try:
        re.compile(pattern)
    except re.error as exc:
        raise ConfigError(f"{path}: issue {issue_id} invalid `text_pattern`: {exc}") from exc
    raw_range = raw.get("glyph_range")
    if (
        not isinstance(raw_range, list)
        or len(raw_range) != 2
        or not all(isinstance(item, int) for item in raw_range)
    ):
        raise ConfigError(
            f"{path}: issue {issue_id} `leading_glyph_range` needs `glyph_range: [start, end]`"
        )
    start, end = (int(raw_range[0]), int(raw_range[1]))
    if start > end:
        raise ConfigError(f"{path}: issue {issue_id} `glyph_range` start > end")
    return Detector(
        kind="leading_glyph_range",
        region=region,
        text_pattern=pattern,
        glyph_range=(start, end),
    )


def _parse_exclusions(
    raw: object, issue_id: str, path: Path, known_regions: set[str]
) -> tuple[Exclusion, ...]:
    if raw is None:
        return ()
    if not isinstance(raw, list):
        raise ConfigError(f"{path}: issue {issue_id} `exclusions` must be a list")
    exclusions: list[Exclusion] = []
    for entry in raw:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: issue {issue_id} exclusion must be a mapping")
        for key in ("fixture", "region", "until", "reason"):
            if key not in entry:
                raise ConfigError(f"{path}: issue {issue_id} exclusion missing `{key}`")
        region = str(entry["region"])
        if region not in known_regions:
            raise ConfigError(f"{path}: issue {issue_id} exclusion names unknown region `{region}`")
        rule_value = entry.get("rule")
        rule = str(rule_value) if rule_value is not None else None
        if rule is not None:
            _validate_exclusion_rule(rule, region, issue_id, path)
        exclusions.append(
            Exclusion(
                fixture=str(entry["fixture"]),
                region=region,
                until=str(entry["until"]),
                reason=str(entry["reason"]),
                rule=rule,
            )
        )
    return tuple(exclusions)


def _validate_exclusion_rule(rule: str, region: str, issue_id: str, path: Path) -> None:
    """A waiver may only name a region-scoped contract rule about its own region.

    Anything else would either never match (a silent no-op that looks like a
    guard) or waive a rule about a different region. Both are refused here.
    """
    rule_region = contract_rule_region(rule)
    if rule_region is None:
        raise ConfigError(
            f"{path}: issue {issue_id} exclusion rule `{rule}` is not region-scoped; "
            f"only `{REGION_RULE_PREFIX}<region>` rules can be excluded"
        )
    if rule_region != region:
        raise ConfigError(
            f"{path}: issue {issue_id} exclusion rule `{rule}` is scoped to region "
            f"`{rule_region}`, not `{region}`"
        )


def evaluate_detector(
    detector: Detector, capture: Capture, region_map: RegionMap
) -> DetectorOutcome:
    """Run one detector on one capture and report whether the defect shows."""
    if detector.kind == "leading_glyph_range":
        return _evaluate_leading_glyph(detector, capture)
    region = region_map.get(detector.region)
    array = load_rgb(capture.png)
    measured = region_content_fraction(array, region, detector.pixel_tolerance)
    reproduced = measured > detector.max_content
    detail = (
        f"region `{detector.region}` content {measured:.4f} "
        f"{'>' if reproduced else '<='} {detector.max_content:.4f}"
    )
    return DetectorOutcome(reproduced=reproduced, measured=measured, detail=detail)


def _evaluate_leading_glyph(detector: Detector, capture: Capture) -> DetectorOutcome:
    """The label matching `text_pattern` must start with a glyph inside `glyph_range`.

    The power clock is the only label whose first line ends in a voltage
    (`... 12.1v\\n`), so its text is the one to check. `measured` is the leading
    code point as a number so the report shows it; `reproduced` is True when that
    code point falls outside the range (the defect: ASCII memory instead of an
    icon glyph). The text comes from the harness' structural inspection, never
    from OCR.
    """
    inspection = inspection_of(capture)
    if inspection is None:
        raise HarnessError(
            f"detector `leading_glyph_range` needs the structural inspection: "
            f"{capture.name} was captured without `--inspect`"
        )
    pattern = re.compile(detector.text_pattern)
    start, end = detector.glyph_range
    for node, _ancestors, _path in iter_tree(inspection.nodes):
        text = node.text
        if not text or pattern.search(text) is None:
            continue
        leading = ord(text[0])
        inside = start <= leading <= end
        return DetectorOutcome(
            reproduced=not inside,
            measured=float(leading),
            detail=(
                f"label {text!r} starts with U+{leading:04X}, "
                f"{'inside' if inside else 'outside'} U+{start:04X}..U+{end:04X}"
            ),
        )
    return DetectorOutcome(
        reproduced=False,
        measured=-1.0,
        detail=f"no label matched {detector.text_pattern!r}",
    )


def classify(
    issue: KnownIssue, reproduced_a: bool | None, reproduced_b: bool | None
) -> tuple[IssueClassification, str]:
    """Classify an issue from its reproduction on A and B."""
    if reproduced_a is None or reproduced_b is None:
        return IssueClassification.UNVERIFIED, "no detector: only recorded from the repository"
    if reproduced_a and not reproduced_b:
        return IssueClassification.FIXED, "reproduced on A, absent on B"
    if reproduced_a and reproduced_b:
        return IssueClassification.UNCHANGED_DEFECT, "reproduced on both A and B"
    if not reproduced_a and reproduced_b:
        if issue.status == "fixed":
            return IssueClassification.REGRESSION, "a previously fixed defect came back on B"
        return IssueClassification.NEW_DEFECT, "present only on B"
    return IssueClassification.NOT_REPRODUCED, "absent on both A and B"


def baseline_issue_for(
    rule: str, fixture: str, issues: tuple[KnownIssue, ...], today: date
) -> KnownIssue | None:
    """The open, founded known issue that explains a failing contract rule.

    The match is exact and scoped: the issue must be an open baseline defect for
    the same fixture, its region must be the region the rule names, and it must
    carry an active waiver that pins that same rule. Any mismatch (a fixed or
    unverified issue, another fixture, another region) leaves the finding as a
    real failure, so the gate still catches a violation the registry does not
    record.
    """
    region = contract_rule_region(rule)
    if region is None:
        return None
    for issue in issues:
        if not issue.is_open_defect or issue.fixture != fixture or issue.region != region:
            continue
        if any(waiver.covers(rule, fixture, region, today) for waiver in issue.exclusions):
            return issue
    return None
