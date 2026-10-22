"""The five verdicts of the evaluator and their process exit codes.

`docs/AB_TESTING_PLAN.md` fixes the vocabulary; the exit codes are what CI
reads. They are deliberately stable: 0 PASS, 1 FAIL, 2 BLOCKED, 3 INCONCLUSIVE,
4 REVIEW REQUIRED.
"""

from __future__ import annotations

from enum import StrEnum


class Status(StrEnum):
    """Result of a single gate or of the whole comparison."""

    PASS = "PASS"
    FAIL = "FAIL"
    BLOCKED = "BLOCKED"
    INCONCLUSIVE = "INCONCLUSIVE"
    REVIEW_REQUIRED = "REVIEW REQUIRED"

    @property
    def exit_code(self) -> int:
        return _EXIT_CODES[self]


_EXIT_CODES: dict[Status, int] = {
    Status.PASS: 0,
    Status.FAIL: 1,
    Status.BLOCKED: 2,
    Status.INCONCLUSIVE: 3,
    Status.REVIEW_REQUIRED: 4,
}

# Worst-first: the first status present among the *required* gates wins. BLOCKED
# outranks INCONCLUSIVE because "we could not measure" is weaker evidence than
# "we measured and the sample is not conclusive". REVIEW REQUIRED outranks
# INCONCLUSIVE too: a human decision is more urgent than more data.
_SEVERITY: dict[Status, int] = {
    Status.PASS: 0,
    Status.REVIEW_REQUIRED: 1,
    Status.INCONCLUSIVE: 2,
    Status.BLOCKED: 3,
    Status.FAIL: 4,
}


def combine(statuses: list[Status]) -> Status:
    """Return the most severe status in `statuses` (PASS when empty)."""
    if not statuses:
        return Status.PASS
    return max(statuses, key=lambda s: _SEVERITY[s])
