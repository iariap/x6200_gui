"""Thin wrapper around `subprocess` with typed results and captured logs.

Every external command the framework runs (git, docker) goes through here so
that stdout/stderr are always captured, the log is available for the report and
a missing binary becomes a typed error instead of a traceback.
"""

from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CommandResult:
    """Outcome of a finished command, with its full captured output."""

    argv: list[str]
    returncode: int
    stdout: str
    stderr: str

    @property
    def ok(self) -> bool:
        return self.returncode == 0

    @property
    def combined(self) -> str:
        return self.stdout + self.stderr

    def tail(self, lines: int = 40) -> str:
        """Last `lines` lines of the combined output (for BLOCKED causes)."""
        text = self.combined.strip().splitlines()
        return "\n".join(text[-lines:])


class CommandNotFound(Exception):
    """The executable is not on PATH."""

    def __init__(self, program: str) -> None:
        super().__init__(f"executable not found on PATH: {program}")
        self.program = program


def has_program(program: str) -> bool:
    """True when `program` can be executed from PATH."""
    return shutil.which(program) is not None


def run(
    argv: list[str],
    *,
    cwd: Path | None = None,
    timeout: float | None = None,
    log_path: Path | None = None,
) -> CommandResult:
    """Run `argv`, capturing stdout/stderr.

    Raises `CommandNotFound` when the program is missing. The caller decides
    what a non-zero exit code means. When `log_path` is given the combined
    output is written there (so the report can link to it).
    """
    program = argv[0]
    if not has_program(program):
        raise CommandNotFound(program)
    try:
        proc = subprocess.run(
            argv,
            cwd=str(cwd) if cwd is not None else None,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        # Surface a timeout as a non-zero result rather than an exception so the
        # caller can report BLOCKED with the cause.
        out = exc.stdout if isinstance(exc.stdout, str) else ""
        err = exc.stderr if isinstance(exc.stderr, str) else ""
        result = CommandResult(argv, 124, out, err + f"\ntimeout after {timeout}s\n")
        if log_path is not None:
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text(result.combined)
        return result
    result = CommandResult(argv, proc.returncode, proc.stdout, proc.stderr)
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(result.combined)
    return result
