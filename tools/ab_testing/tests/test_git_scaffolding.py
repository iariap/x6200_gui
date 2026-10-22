"""The ephemeral-revision helper must commit on a runner with no git identity.

The CI runner (and any bare environment) has no `user.name`/`user.email`, so the
throwaway commit `_ephemeral` makes has to carry its own scaffolding identity.
This reproduces that condition: global and system git config are disabled, so a
commit without `-c user.name/user.email` would fail with "Author identity
unknown" exactly as it did in CI.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from test_e2e import _ephemeral, _git

SCAFFOLDING_AUTHOR = "ab-test-ephemeral <ab-test-ephemeral@example.invalid>"


def _seed_repo(repo: Path) -> str:
    """Init a repo and return the SHA of an empty base commit."""
    assert _git("-C", str(repo), "init", "-q").returncode == 0
    completed = _git(
        "-C",
        str(repo),
        "-c",
        "user.name=seed",
        "-c",
        "user.email=seed@example.invalid",
        "commit",
        "--allow-empty",
        "-m",
        "base",
    )
    assert completed.returncode == 0, completed.stderr
    return _git("-C", str(repo), "rev-parse", "HEAD").stdout.strip()


def _touch_probe(worktree: Path) -> None:
    """A one-line change so the ephemeral commit has something to record."""
    (worktree / "probe.txt").write_text("ephemeral\n")


def test_ephemeral_commits_without_a_configured_identity(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    repo = tmp_path / "repo"
    repo.mkdir()
    base = _seed_repo(repo)

    # A bare runner: no global and no system git configuration at all.
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", os.devnull)
    monkeypatch.setenv("GIT_CONFIG_SYSTEM", os.devnull)

    sha = _ephemeral(repo, base, "regression", _touch_probe)
    assert sha and sha != base
    author = _git("-C", str(repo), "log", "-1", "--format=%an <%ae>", sha).stdout.strip()
    assert author == SCAFFOLDING_AUTHOR
