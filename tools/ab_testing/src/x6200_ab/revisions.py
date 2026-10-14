"""Revision manager: one isolated git worktree per exact commit SHA.

The plan requires A and B to be *exact* commits, built in isolation. This
module turns a rev (SHA, branch or tag) into a `Revision` with its own git
worktree under a scratch directory, verifies that the checked-out HEAD really
is the requested SHA (fail-closed), and materialises the `lvgl` submodule
without ever touching the user's own worktrees.

Submodule handling: `git worktree add` does not check submodules out, and
cloning lvgl per revision is slow. The pinned `lvgl` tree is materialised once
into a cache keyed by the submodule's gitlink SHA and copied into each worktree,
so a revision always builds against the exact LVGL commit its tree records.
"""

from __future__ import annotations

import shutil
from dataclasses import dataclass, field
from pathlib import Path

from . import proc
from .errors import RevisionError

SUBMODULES = ("lvgl",)


def find_repo(start: Path) -> Path:
    """Walk upwards from `start` to the first directory holding a `.git`."""
    for candidate in (start, *start.parents):
        if (candidate / ".git").exists():
            return candidate
    raise RevisionError(f"no git repository found at or above {start}")


@dataclass(frozen=True)
class Revision:
    """A commit checked out in its own worktree."""

    label: str
    sha: str
    worktree: Path

    @property
    def short_sha(self) -> str:
        return self.sha[:12]

    @property
    def harness_dir(self) -> Path:
        return self.worktree / "tools" / "ui_harness"

    @property
    def fixtures_dir(self) -> Path:
        return self.harness_dir / "fixtures"

    @property
    def binary_path(self) -> Path:
        return self.harness_dir / "build" / "ui_harness"


@dataclass
class RevisionManager:
    """Creates, caches and removes the revision worktrees of one experiment."""

    repo: Path
    workspace: Path
    cache_dir: Path | None = None
    _by_sha: dict[str, Revision] = field(default_factory=dict, init=False)
    _created: list[Path] = field(default_factory=list, init=False)

    def __post_init__(self) -> None:
        self._revisions_dir.mkdir(parents=True, exist_ok=True)
        if self.cache_dir is not None:
            self.cache_dir.mkdir(parents=True, exist_ok=True)

    @property
    def _revisions_dir(self) -> Path:
        return self.workspace / "revisions"

    def resolve(self, ref: str) -> str:
        """Resolve a rev to a full commit SHA (peeling tags/annotations)."""
        result = proc.run(
            ["git", "-C", str(self.repo), "rev-parse", "--verify", f"{ref}^{{commit}}"]
        )
        if not result.ok:
            raise RevisionError(f"cannot resolve revision `{ref}`: {result.tail(5)}")
        return result.stdout.strip()

    def materialize(self, ref: str, label: str) -> Revision:
        """Return a revision for `ref`, creating its worktree on first use."""
        sha = self.resolve(ref)
        existing = self._by_sha.get(sha)
        if existing is not None:
            return existing
        self._ensure_sha_exists(sha)
        worktree = self._revisions_dir / f"{label}-{sha[:12]}"
        if not worktree.exists():
            result = proc.run(
                ["git", "-C", str(self.repo), "worktree", "add", "--detach", str(worktree), sha],
                log_path=self.workspace / "logs" / f"worktree-{label}-{sha[:12]}.log",
            )
            if not result.ok:
                raise RevisionError(f"git worktree add failed for {sha[:12]}: {result.tail(5)}")
            self._created.append(worktree)
        self._materialize_submodules(worktree)
        head = proc.run(["git", "-C", str(worktree), "rev-parse", "HEAD"]).stdout.strip()
        if head != sha:
            raise RevisionError(
                f"worktree {worktree} is at {head[:12]} but {sha[:12]} was requested"
            )
        revision = Revision(label=label, sha=sha, worktree=worktree)
        self._by_sha[sha] = revision
        return revision

    def _ensure_sha_exists(self, sha: str) -> None:
        if not proc.run(["git", "-C", str(self.repo), "cat-file", "-e", f"{sha}^{{commit}}"]).ok:
            raise RevisionError(f"commit {sha} is not present in {self.repo}")

    def _materialize_submodules(self, worktree: Path) -> None:
        for name in SUBMODULES:
            target = worktree / name
            if (target / "CMakeLists.txt").exists():
                continue
            pinned = self._submodule_sha(worktree, name)
            cached = None if self.cache_dir is None else self.cache_dir / f"{name}-{pinned}"
            if cached is not None and (cached / "CMakeLists.txt").exists():
                shutil.copytree(cached, target, symlinks=True, dirs_exist_ok=True)
                continue
            result = proc.run(
                ["git", "-C", str(worktree), "submodule", "update", "--init", name],
                timeout=900,
                log_path=self.workspace / "logs" / f"submodule-{name}-{pinned[:12]}.log",
            )
            if not result.ok:
                raise RevisionError(
                    f"could not initialise submodule `{name}` ({pinned[:12]}): {result.tail(5)}"
                )
            if cached is not None:
                if cached.exists():
                    shutil.rmtree(cached)
                shutil.copytree(target, cached, symlinks=True)

    def _submodule_sha(self, worktree: Path, name: str) -> str:
        result = proc.run(["git", "-C", str(worktree), "ls-files", "-s", "--", name])
        for line in result.stdout.splitlines():
            fields = line.split()
            if len(fields) >= 2 and fields[0] == "160000":
                return fields[1]
        raise RevisionError(f"`{name}` is not a submodule of {worktree}")

    def cleanup(self) -> list[Path]:
        """Remove the worktrees this manager created; never touches others."""
        removed: list[Path] = []
        for worktree in self._created:
            proc.run(["git", "-C", str(self.repo), "worktree", "remove", "--force", str(worktree)])
            shutil.rmtree(worktree, ignore_errors=True)
            removed.append(worktree)
        self._created.clear()
        self._by_sha.clear()
        proc.run(["git", "-C", str(self.repo), "worktree", "prune"])
        return removed
