"""Build one revision's C harness inside the reproducible Docker image.

Each revision is compiled in its own worktree, so the produced `ui_harness`
binary always belongs to that exact commit; there is no shared binary cache and
nothing is copied between revisions. A revision that does not compile raises
`BuildError`, which the CLI turns into a BLOCKED verdict with the log tail.
The revision SHA is embedded with `-DHARNESS_REVISION` because git cannot see
the linked worktree from inside the container (the mount hides `.git`).
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .docker import DockerRunner
from .errors import BuildError
from .revisions import Revision

BUILD_SCRIPT = """\
set -euo pipefail
cd tools/ui_harness
mkdir -p build
cd build
cmake .. -DHARNESS_REVISION={sha} >/dev/null
# Cap the parallel compiler count: -j"$(nproc)" on a many-core host opens one
# compiler per core and their concurrent peaks are what drove the host into OOM.
JOBS="${X6200_AB_JOBS:-$(nproc)}"
if [ "$JOBS" -gt 8 ]; then JOBS=8; fi
make -j"$JOBS"
"""


@dataclass(frozen=True)
class HarnessBuilder:
    """Compiles the harness for a revision using the Docker image."""

    runner: DockerRunner
    logs_dir: Path

    def build(self, revision: Revision) -> Path:
        """Build `revision` and return the path to its `ui_harness` binary."""
        log_path = self.logs_dir / f"build-{revision.label}-{revision.short_sha}.log"
        result = self.runner.run_script(
            revision.worktree,
            self.logs_dir,
            BUILD_SCRIPT.format(sha=revision.sha),
            timeout=1800,
            log_path=log_path,
        )
        binary = revision.binary_path
        if not result.ok:
            raise BuildError(
                f"revision {revision.short_sha} did not build (exit {result.returncode})",
                log=result.tail(60),
            )
        if not binary.is_file():
            raise BuildError(
                f"revision {revision.short_sha} built without producing {binary}",
                log=result.tail(40),
            )
        return binary
