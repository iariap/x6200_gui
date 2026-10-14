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
    """One contract checked against one capture."""

    rule: str
    fixture: str
    ok: bool
    detail: str
    path: str | None = None

    def as_dict(self) -> dict[str, Any]:
        return {
            "rule": self.rule,
            "fixture": self.fixture,
            "ok": self.ok,
            "detail": self.detail,
            "path": self.path,
        }
