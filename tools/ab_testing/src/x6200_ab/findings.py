"""The shared result type of every quality contract.

It lives in its own module so the pixel-based contracts (`contracts.py`) and the
structural ones (`layout_checks.py`, `typography_checks.py`) can produce the same
type without importing each other.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class ContractFinding:
    """One contract checked against one capture.

    A failing finding whose `baseline_issue` is set is an UNCHANGED DEFECT: the
    registry already records that defect as open, so the gate reports it
    (prominently) but does not fail on it. The failure is never dropped: `ok`
    stays False, only its severity for the verdict changes.
    """

    rule: str
    fixture: str
    ok: bool
    detail: str
    path: str | None = None
    baseline_issue: str | None = None

    @property
    def unchanged_defect(self) -> bool:
        """True when a real failure is explained by an open baseline issue."""
        return not self.ok and self.baseline_issue is not None

    def as_dict(self) -> dict[str, Any]:
        return {
            "rule": self.rule,
            "fixture": self.fixture,
            "ok": self.ok,
            "detail": self.detail,
            "path": self.path,
            "baseline_issue": self.baseline_issue,
        }
