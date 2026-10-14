"""Typed errors raised by the A/B framework.

Every failure that is part of the normal flow is typed so the CLI can map it to
a BLOCKED verdict with a concrete cause instead of a traceback: a revision that
does not build, a missing Docker image, a scenario the harness rejects, a
malformed YAML contract. Programming errors still bubble up as usual.
"""

from __future__ import annotations


class AbError(Exception):
    """Base class for every framework error that is reported as BLOCKED."""


class DockerUnavailable(AbError):
    """The Docker daemon or the harness image is not available."""


class BuildError(AbError):
    """A revision did not compile; carries the build log tail."""

    def __init__(self, message: str, log: str = "") -> None:
        super().__init__(message)
        self.log = log


class RevisionError(AbError):
    """A git revision could not be resolved, materialised or verified."""


class HarnessError(AbError):
    """The C harness rejected a scenario or produced no output."""


class ConfigError(AbError):
    """A quality file (regions/contracts/budgets/known issues) is malformed."""
