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


def test_themes_json_lists_the_full_inventory(capsys: pytest.CaptureFixture) -> None:
    """The static command reports every palette, not only the shipped files."""
    repo = Path(__file__).resolve().parents[3]
    code = main(["themes", "--repo", str(repo), "--json"])
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "PASS"
    names = [theme["theme"] for theme in payload["themes"]]
    assert names == [
        "IU1WXY",
        "Legacy",
        "Outdoor",
        "Classic",
        "Aurora",
        "Cathode",
        "Redshift",
        "Sprint",
        "Sunlit",
        "Trail",
    ]
    assert "unsupported" not in payload
    assert payload["pairs_checked"] == len(names) * len(payload["pairs"]) == 90
    assert all(theme["ok"] for theme in payload["themes"])


def test_themes_text_reports_the_palettes(capsys: pytest.CaptureFixture) -> None:
    repo = Path(__file__).resolve().parents[3]
    code = main(["themes", "--repo", str(repo)])
    assert code == 0
    out = capsys.readouterr().out
    assert "10 palette(s) (4 base + 6 file(s))" in out
    assert "90 pair(s) checked, 0 failure(s)" in out
    assert "Aurora" in out
    assert "Outdoor-Negative" not in out
    assert "VISION" not in out
    assert "Holo" not in out
    assert "UNSUPPORTED" not in out


def test_themes_reports_the_nontext_objects_and_the_exempt_roles(
    capsys: pytest.CaptureFixture,
) -> None:
    repo = Path(__file__).resolve().parents[3]
    code = main(["themes", "--repo", str(repo), "--json"])
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["nontext"]["objects"] == 15
    assert payload["nontext"]["failures"] == []
    assert payload["nontext"]["missing"] == []
    assert {exempt["role"] for exempt in payload["exempt"]} == {
        "btn_off_text",
        "text_mute",
        "text_faint",
        "icon_off",
        "switch_off",
        "off",
        "track",
    }


def test_a_declared_nontext_object_without_a_render_fails(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """A coverage hole is not a pass: the owner's order is measured, not declared."""
    repo = Path(__file__).resolve().parents[3]
    code = main(["themes", "--repo", str(repo), "--renders-dir", str(tmp_path), "--json"])
    assert code == 1
    payload = json.loads(capsys.readouterr().out)
    assert payload["status"] == "FAIL"
    assert payload["nontext"]["missing"], "a missing render has to be reported"


def test_themes_can_skip_the_nontext_check(capsys: pytest.CaptureFixture) -> None:
    repo = Path(__file__).resolve().parents[3]
    code = main(["themes", "--repo", str(repo), "--no-nontext", "--json"])
    assert code == 0
    payload = json.loads(capsys.readouterr().out)
    assert payload["nontext"]["objects"] == 0
