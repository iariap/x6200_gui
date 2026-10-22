"""Docker driver for the reproducible harness build image.

`tools/ui_harness/Dockerfile` defines the `x6200-uiharness` image (the ROM
Buildroot image plus the host build dependencies). Everything the framework
compiles or renders happens inside that image, mounted read-write so a revision
worktree can be built there while the experiment's output directory is mounted
separately at `/out`.

The mount layout is deliberately simple and never points at the user's
checkout: `-v <workspace>:/src` and `-v <output>:/out`.
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


@dataclass(frozen=True)
class DockerRunner:
    """Runs shell snippets inside the harness image, as the current user."""

    image: str = HARNESS_IMAGE
    docker: str = "docker"

    def _base_argv(self, workspace: Path, output: Path) -> list[str]:
        return [
            self.docker,
            "run",
            "--rm",
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
