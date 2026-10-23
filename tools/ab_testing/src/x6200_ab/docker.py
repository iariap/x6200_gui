"""Docker driver for the reproducible harness build image.

`tools/ui_harness/Dockerfile` defines the `x6200-uiharness` image (the ROM
Buildroot image plus the host build dependencies). Everything the framework
compiles or renders happens inside that image, mounted read-write so a revision
worktree can be built there while the experiment's output directory is mounted
separately at `/out`.

The mount layout is deliberately simple and never points at the user's
checkout: `-v <workspace>:/src` and `-v <output>:/out`.

Every `docker run` also carries resource ceilings (`--pids-limit`, `--memory`
and `--memory-swap`), because without them a runaway build or a lab that forks
inside the container exhausts the *host*, not the container: a single build
reached 83.4 GB and took the desktop down with a kernel OOM, and a lab whose
init script recursed reached 70 GB of slab before it was killed. The limits are
configurable (see the module constants and `DockerRunner`) but the defaults are
the safe ones.
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from . import proc
from .errors import DockerUnavailable

HARNESS_IMAGE = "x6200-uiharness"
# Paths inside the container.
CONTAINER_SRC = Path("/src")
CONTAINER_OUT = Path("/out")

# Safe defaults for the resource ceilings of every `docker run`, overridable per
# machine through the environment (see `DockerRunner._base_argv`).
DEFAULT_PIDS_LIMIT = "512"
DEFAULT_MEMORY = "4g"


def _env_override(name: str, fallback: str) -> str:
    """Return the environment value of `name`, or `fallback` when unset/blank."""
    value = os.environ.get(name, "").strip()
    return value or fallback


@dataclass(frozen=True)
class DockerRunner:
    """Runs shell snippets inside the harness image, as the current user.

    `pids_limit` and `memory` are the resource ceilings passed to `docker run`.
    Left as `None` they are read from the environment at argv time:
    `X6200_AB_PIDS_LIMIT` (default 512) and `X6200_AB_DOCKER_MEMORY`
    (default 4g, also used as `--memory-swap` so the container never falls back
    to swap).
    """

    image: str = HARNESS_IMAGE
    docker: str = "docker"
    pids_limit: str | None = None
    memory: str | None = None

    def _base_argv(self, workspace: Path, output: Path) -> list[str]:
        pids_limit = self.pids_limit or _env_override("X6200_AB_PIDS_LIMIT", DEFAULT_PIDS_LIMIT)
        memory = self.memory or _env_override("X6200_AB_DOCKER_MEMORY", DEFAULT_MEMORY)
        return [
            self.docker,
            "run",
            "--rm",
            "--pids-limit",
            pids_limit,
            "--memory",
            memory,
            "--memory-swap",
            memory,
            "--entrypoint",
            "bash",
            "--user",
            f"{os.getuid()}:{os.getgid()}",
            "-v",
            f"{workspace}:{CONTAINER_SRC}",
            "-v",
            f"{output}:{CONTAINER_OUT}",
            "-w",
            str(CONTAINER_SRC),
            self.image,
        ]

    def check(self) -> None:
        """Raise `DockerUnavailable` when Docker or the image is missing."""
        if not proc.has_program(self.docker):
            raise DockerUnavailable(f"`{self.docker}` is not on PATH")
        result = proc.run([self.docker, "image", "inspect", self.image])
        if not result.ok:
            raise DockerUnavailable(
                f"Docker image `{self.image}` is not available; build it with "
                f"`docker build -t {self.image} tools/ui_harness`"
            )

    def run_script(
        self,
        workspace: Path,
        output: Path,
        script: str,
        *,
        timeout: float | None = None,
        log_path: Path | None = None,
    ) -> proc.CommandResult:
        """Run `script` with `bash -lc` inside the image.

        `workspace` is mounted at `/src` and `output` at `/out`.
        """
        output.mkdir(parents=True, exist_ok=True)
        argv = [*self._base_argv(workspace, output), "-lc", script]
        return proc.run(argv, timeout=timeout, log_path=log_path)
