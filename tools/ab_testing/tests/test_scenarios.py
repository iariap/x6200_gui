"""Tests for the scenario parser and suite."""

from __future__ import annotations

from pathlib import Path

import pytest

from x6200_ab.errors import HarnessError
from x6200_ab.scenarios import ScenarioSuite, parse_scenario

REPO_ROOT = Path(__file__).resolve().parents[3]
HARNESS_FIXTURES = REPO_ROOT / "tools" / "ui_harness" / "fixtures"

VALID = """\
# a comment
name demo
screen main
seed 7
frequency_hz 14074000
mode usb
"""


def _write(tmp_path: Path, text: str, name: str = "demo.scn") -> Path:
    path = tmp_path / name
    path.write_text(text)
    return path


def test_parse_valid(tmp_path: Path) -> None:
    scenario = parse_scenario(_write(tmp_path, VALID))
    assert scenario.name == "demo"
    assert scenario.screen == "main"
    assert scenario.seed == 7
    assert scenario.filename == "demo.scn"
    assert scenario.raw["mode"] == "usb"


def test_unknown_key_is_rejected(tmp_path: Path) -> None:
    with pytest.raises(HarnessError, match="unknown scenario key"):
        parse_scenario(_write(tmp_path, VALID + "not_a_key 1\n"))


def test_missing_name_is_rejected(tmp_path: Path) -> None:
    with pytest.raises(HarnessError, match="missing required key `name`"):
        parse_scenario(_write(tmp_path, "screen main\n"))


def test_bad_seed_is_rejected(tmp_path: Path) -> None:
    with pytest.raises(HarnessError, match="seed is not an integer"):
        parse_scenario(_write(tmp_path, VALID.replace("seed 7", "seed seven")))


def test_suite_subset_and_unknown_name(tmp_path: Path) -> None:
    _write(tmp_path, VALID, "a.scn")
    _write(tmp_path, VALID.replace("name demo", "name other"), "b.scn")
    suite = ScenarioSuite.from_fixtures_dir(tmp_path, ["other"])
    assert suite.names == ["other"]
    with pytest.raises(HarnessError, match="unknown scenario"):
        ScenarioSuite.from_fixtures_dir(tmp_path, ["nope"])


def test_empty_suite_is_rejected(tmp_path: Path) -> None:
    with pytest.raises(HarnessError, match="no scenarios found"):
        ScenarioSuite.from_fixtures_dir(tmp_path)


@pytest.mark.skipif(not HARNESS_FIXTURES.is_dir(), reason="harness fixtures not present")
def test_repo_fixtures_are_all_parsable() -> None:
    suite = ScenarioSuite.from_fixtures_dir(HARNESS_FIXTURES)
    assert suite.names == [
        "app-bands",
        "bench-render",
        "home-bateria-externa",
        "home-frecuencia-extrema",
        "home-usb-20m",
        "main-bateria-baja",
        "main-espectro-agua",
        "main-foco",
        "main-natural",
        "main-natural-eco",
        "main-otro-tema",
        "main-tema-classic",
        "main-tema-holo",
        "main-tema-material-3",
        "main-tema-outdoor-negative",
        "main-tema-rs",
        "main-tema-vision",
        "main-tema-vision-cwr",
        "main-tema-vision-usbd",
        "main-tx-simulado",
        "settings-bands",
        "settings-home",
        "settings-wfbat",
    ]
    # Every fixture builds one of the screens the harness knows; the theme
    # fixtures still build `main`, only the focus, settings and app-menu ones do
    # not.
    assert all(
        scenario.screen
        in {
            "main",
            "main-foco",
            "settings-home",
            "settings-wfbat",
            "settings-bands",
            "app-bands",
        }
        for scenario in suite.scenarios
    )
