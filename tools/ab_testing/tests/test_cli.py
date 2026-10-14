"""Tests for the CLI surface that does not need Docker."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from x6200_ab.cli import _gate_name, _status_from_value, build_parser, main
from x6200_ab.errors import AbError

REQUIRED_GATES = ("Functional", "Visual Regression", "Quality Contract", "Performance")
ADVISORY_GATES = ("Hardware", "Energy")


def _results(recorded: str, functional: str) -> dict:
    gates = []
    for name in REQUIRED_GATES:
        gates.append(
            {
                "gate": name,
                "status": functional if name == "Functional" else "PASS",
                "required": True,
                "summary": "",
                "evidence": [],
                "details": {},
            }
        )
    for name in ADVISORY_GATES:
        gates.append(
            {
                "gate": name,
                "status": "BLOCKED",
                "required": False,
                "summary": "",
                "evidence": [],
                "details": {},
            }
        )
    return {"experiment_id": "AB-x", "status": recorded, "gates": gates}


def test_parser_exposes_every_subcommand() -> None:
    help_text = build_parser().format_help()
    for command in ("capture", "audit", "contracts", "compare", "benchmark", "report", "evaluate"):
        assert command in help_text


def test_report_without_results_is_blocked(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    code = main(["report", "--experiment-id", "AB-nope", "--reports-dir", str(tmp_path)])
    assert code == 2
    assert "BLOCKED" in capsys.readouterr().err


def test_evaluate_accepts_consistent_results(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    path = tmp_path / "results.json"
    path.write_text(json.dumps(_results("PASS", "PASS")))
    code = main(["evaluate", "--results", str(path), "--json"])
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["consistent_with_recorded"] is True


def test_evaluate_flags_inconsistent_results(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    path = tmp_path / "results.json"
    path.write_text(json.dumps(_results("PASS", "FAIL")))
    code = main(["evaluate", "--results", str(path)])
    assert code == 2
    assert "INCONSISTENT" in capsys.readouterr().out


def test_unknown_status_is_an_error() -> None:
    with pytest.raises(AbError, match="unknown status"):
        _status_from_value("MAYBE")


def test_unknown_gate_is_an_error() -> None:
    with pytest.raises(AbError, match="unknown gate"):
        _gate_name("Nonexistent")
