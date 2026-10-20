"""Tests for the scenario parser and suite."""

from __future__ import annotations

from pathlib import Path
from typing import cast

import pytest

from helpers import revision
from x6200_ab.capture import Capturer
from x6200_ab.docker import DockerRunner
from x6200_ab.errors import HarnessError
from x6200_ab.scenarios import (
    Scenario,
    ScenarioSuite,
    parse_scenario,
    split_by_fixtures_dir,
)

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


def test_split_reports_the_fixture_b_does_not_ship(tmp_path: Path) -> None:
    """A fixture of A that B no longer ships is reported, not fatal.

    The comparison suite is A's; a change may remove a screen (and its fixture) in B.
    That scenario cannot be rendered on B, so it is left out of the run and named,
    instead of aborting the whole comparison on a missing file.
    """
    a_dir = tmp_path / "a"
    b_dir = tmp_path / "b"
    a_dir.mkdir()
    b_dir.mkdir()
    _write(a_dir, VALID, "keep.scn")
    _write(a_dir, VALID.replace("name demo", "name gone"), "gone.scn")
    _write(b_dir, VALID, "keep.scn")

    kept, removed = split_by_fixtures_dir(ScenarioSuite.from_fixtures_dir(a_dir), b_dir)

    assert kept.names == ["demo"]
    assert removed == ("gone",)


def test_split_rejects_a_suite_with_nothing_left(tmp_path: Path) -> None:
    a_dir = tmp_path / "a"
    b_dir = tmp_path / "b"
    a_dir.mkdir()
    b_dir.mkdir()
    _write(a_dir, VALID, "only.scn")

    with pytest.raises(HarnessError, match="none of the 1 scenario"):
        split_by_fixtures_dir(ScenarioSuite.from_fixtures_dir(a_dir), b_dir)


def test_capture_rejects_a_fixture_the_revision_does_not_ship(tmp_path: Path) -> None:
    """`Capturer` names the missing fixture instead of dying on `read_text()`."""
    scenario = Scenario(name="gone", screen="main", filename="gone.scn", seed=None, raw={})
    capturer = Capturer(runner=cast(DockerRunner, object()))

    with pytest.raises(HarnessError, match="is not in revision"):
        capturer.capture(
            revision("b", "b" * 40),
            scenario,
            tmp_path / "captures",
            tmp_path / "logs",
        )


@pytest.mark.skipif(not HARNESS_FIXTURES.is_dir(), reason="harness fixtures not present")
def test_repo_fixtures_are_all_parsable() -> None:
    suite = ScenarioSuite.from_fixtures_dir(HARNESS_FIXTURES)
    assert suite.names == [
        "app-bands",
        "app-close-home",
        "apps-menu-1",
        "apps-menu-2",
        "apps-menu-3",
        "apps-menu-4",
        "apps-menu-5",
        "apps-menu-6",
        "autotune",
        "autotune-found",
        "autotune-progress",
        "bench-render",
        "bt-dialog",
        "bt-dialog-tema-aurora",
        "bt-dialog-tema-cathode",
        "bt-dialog-tema-redshift",
        "bt-dialog-tema-sprint",
        "bt-dialog-tema-sunlit",
        "bt-dialog-tema-trail",
        "cw-autotune-off",
        "cw-autotune-on",
        "cw-tune-default",
        "cw-tune-ssb",
        "cw-tune-tema-sunlit",
        "cw-tune-tuner",
        # Editor DFL (issue #173, PR 2): el dialogo real, USB-D/LSB-D, los seis
        # temas, TX deshabilitado, interaccion (+/arrastre/rechazo) y 30 ciclos.
        "dfl-editor-cycles",
        "dfl-editor-interaction",
        "dfl-editor-lsb-d",
        "dfl-editor-tx",
        "dfl-editor-usb-d",
        "dfl-editor-usb-d-tema-aurora",
        "dfl-editor-usb-d-tema-cathode",
        "dfl-editor-usb-d-tema-redshift",
        "dfl-editor-usb-d-tema-sprint",
        "dfl-editor-usb-d-tema-sunlit",
        "dfl-editor-usb-d-tema-trail",
        # DFL (issue #173, PR 1): banda pasante RX a escala, USB-D/LSB-D, los seis
        # temas y dos spans distintos.
        "dfl-lsb-d",
        "dfl-usb-d",
        "dfl-usb-d-span100k",
        "dfl-usb-d-span50k",
        "dfl-usb-d-tema-cathode",
        "dfl-usb-d-tema-redshift",
        "dfl-usb-d-tema-sprint",
        "dfl-usb-d-tema-sunlit",
        "dfl-usb-d-tema-trail",
        "home-bateria-externa",
        "home-cb",
        "home-cb-a",
        "home-cb-cathode",
        "home-cb-d",
        "home-cb-offchannel",
        "home-frecuencia-extrema",
        "home-usb-20m",
        "main-bateria-baja",
        "main-espectro-agua",
        "main-foco",
        "main-frecuencia-dseg",
        "main-natural",
        "main-natural-eco",
        "main-tema-aurora",
        "main-tema-aurora-agua",
        "main-tema-aurora-eco",
        "main-tema-aurora-tx",
        "main-tema-cathode",
        "main-tema-cathode-agua",
        "main-tema-cathode-eco",
        "main-tema-cathode-tx",
        "main-tema-redshift",
        "main-tema-redshift-agua",
        "main-tema-redshift-eco",
        "main-tema-redshift-tx",
        "main-tema-sprint",
        "main-tema-sprint-agua",
        "main-tema-sprint-eco",
        "main-tema-sprint-tx",
        "main-tema-sunlit",
        "main-tema-sunlit-agua",
        "main-tema-sunlit-eco",
        "main-tema-sunlit-tx",
        "main-tema-trail",
        "main-tema-trail-agua",
        "main-tema-trail-eco",
        "main-tema-trail-tx",
        "main-tx-simulado",
        "settings-audio",
        "settings-bands",
        "settings-home",
        "settings-perfil",
        "settings-tema-aurora",
        "settings-tema-cathode",
        "settings-tema-redshift",
        "settings-tema-sprint",
        "settings-tema-sunlit",
        "settings-tema-trail",
        "settings-wfbat",
        "swrscan-3pass",
        "swrscan-3pass-p1",
        "swrscan-3pass-p2",
        "swrscan-3pass-p3",
        "swrscan-3pass-stop",
        "swrscan-3pass-tema-aurora",
        "swrscan-3pass-tema-cathode",
        "swrscan-5pass-moving",
        "swrscan-5pass-outliers",
        "swrscan-double-valley",
        "swrscan-flat",
        "swrscan-narrow-zoom",
        "swrscan-owner-40m",
        "swrscan-owner-40m-domo",
        "swrscan-owner-40m-sweep",
        "swrscan-quick",
        "swrscan-wide-zoom",
    ]
    # Every fixture builds one of the screens the harness knows; the theme
    # fixtures still build `main`, only the focus, settings, app-menu, BT,
    # app-close and SWR Scan ones do not.
    assert all(
        scenario.screen
        in {
            "main",
            "main-foco",
            "settings-audio",
            "settings-home",
            "settings-perfil",
            "settings-wfbat",
            "settings-bands",
            "app-bands",
            "apps-menu",
            "autotune",
            "autotune-progress",
            "autotune-found",
            "bt-dialog",
            "app-close-home",
            "swrscan",
            "cw-autotune-off",
            "cw-autotune-on",
            "cw-tune-default",
            "cw-tune-ssb",
            "cw-tune-tuner",
            # Editor DFL (issue #173, PR 2)
            "dfl-editor",
            "dfl-editor-tx",
            "dfl-editor-interaction",
            "dfl-editor-cycles",
        }
        for scenario in suite.scenarios
    )
