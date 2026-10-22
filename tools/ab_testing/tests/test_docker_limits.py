"""The `docker run` resource ceilings and the capped in-container build.

A `docker run` with no ceiling can exhaust the *host* (a build peaked at 83.4 GB
and triggered a kernel OOM; a recursive lab reached 70 GB of slab). These tests
pin the flags so a future edit cannot silently drop them, and pin the env
overrides so the ceilings stay configurable without touching the code.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from x6200_ab.builds import BUILD_SCRIPT
from x6200_ab.docker import DockerRunner


def _argv(runner: DockerRunner | None = None) -> list[str]:
    runner = runner or DockerRunner()
    return runner._base_argv(Path("/ws"), Path("/out"))


def test_base_argv_carries_safe_default_ceilings(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("X6200_AB_PIDS_LIMIT", raising=False)
    monkeypatch.delenv("X6200_AB_DOCKER_MEMORY", raising=False)
    argv = _argv()
    assert argv[argv.index("--pids-limit") + 1] == "512"
    assert argv[argv.index("--memory") + 1] == "4g"
    assert argv[argv.index("--memory-swap") + 1] == "4g"


def test_base_argv_reads_the_environment(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("X6200_AB_PIDS_LIMIT", "1024")
    monkeypatch.setenv("X6200_AB_DOCKER_MEMORY", "8g")
    argv = _argv()
    assert argv[argv.index("--pids-limit") + 1] == "1024"
    assert argv[argv.index("--memory") + 1] == "8g"
    assert argv[argv.index("--memory-swap") + 1] == "8g"


def test_base_argv_explicit_fields_win_over_the_environment(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("X6200_AB_PIDS_LIMIT", "1024")
    monkeypatch.setenv("X6200_AB_DOCKER_MEMORY", "8g")
    argv = _argv(DockerRunner(pids_limit="256", memory="2g"))
    assert argv[argv.index("--pids-limit") + 1] == "256"
    assert argv[argv.index("--memory") + 1] == "2g"
    assert argv[argv.index("--memory-swap") + 1] == "2g"


def test_base_argv_keeps_the_mount_layout() -> None:
    argv = _argv()
    assert "/ws:/src" in argv
    assert "/out:/out" in argv


def test_build_script_caps_the_parallelism() -> None:
    assert 'JOBS="${X6200_AB_JOBS:-$(nproc)}"' in BUILD_SCRIPT
    assert 'make -j"$JOBS"' in BUILD_SCRIPT
    assert 'make -j"$(nproc)"' not in BUILD_SCRIPT
