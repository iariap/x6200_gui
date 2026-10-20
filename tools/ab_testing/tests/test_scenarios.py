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
        # Editor DFL inline (issue #179): restablecer, la rueda principal
        # (rotary0) y las selecciones BW/HIGH de los seis temas, sobre el panel
        # nativo (dfl_inline.c).
        "dfl-editor-reset",
        "dfl-editor-rotary0",
        "dfl-editor-sel-bw-tema-aurora",
        "dfl-editor-sel-bw-tema-cathode",
        "dfl-editor-sel-bw-tema-redshift",
        "dfl-editor-sel-bw-tema-sprint",
        "dfl-editor-sel-bw-tema-sunlit",
        "dfl-editor-sel-bw-tema-trail",
        "dfl-editor-sel-high-tema-aurora",
        "dfl-editor-sel-high-tema-cathode",
        "dfl-editor-sel-high-tema-redshift",
        "dfl-editor-sel-high-tema-sprint",
        "dfl-editor-sel-high-tema-sunlit",
        "dfl-editor-sel-high-tema-trail",
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
        # Espejo USB/LSB del editor DFL inline (issue #182).
        "dfl-mirror-lsb",
        "dfl-mirror-usb",
        # Pagina DFL normal (issue #179) en los seis temas.
        "dfl-page-tema-aurora",
        "dfl-page-tema-cathode",
        "dfl-page-tema-redshift",
        "dfl-page-tema-sprint",
        "dfl-page-tema-sunlit",
        "dfl-page-tema-trail",
        # Presets de filtro del DFL (issue #183): las seis paginas A (Favoritos),
        # B (Ajustar), C (Grafico), D (Mas), E (Confirmar) y F (Elegir origen) en
        # los seis temas, los estados activo/Personalizado/DIGI y las fixtures de
        # interaccion (aplicar, guardar, reasignar, navegacion, TX, 30 ciclos).
        "dfl-presets-activo",
        "dfl-presets-adj-tema-aurora",
        "dfl-presets-adj-tema-cathode",
        "dfl-presets-adj-tema-redshift",
        "dfl-presets-adj-tema-sprint",
        "dfl-presets-adj-tema-sunlit",
        "dfl-presets-adj-tema-trail",
        "dfl-presets-apply",
        "dfl-presets-assign-cancel",
        "dfl-presets-assign-ok",
        "dfl-presets-assign-tema-aurora",
        "dfl-presets-assign-tema-cathode",
        "dfl-presets-assign-tema-redshift",
        "dfl-presets-assign-tema-sprint",
        "dfl-presets-assign-tema-sunlit",
        "dfl-presets-assign-tema-trail",
        "dfl-presets-confirm-tema-aurora",
        "dfl-presets-confirm-tema-cathode",
        "dfl-presets-confirm-tema-redshift",
        "dfl-presets-confirm-tema-sprint",
        "dfl-presets-confirm-tema-sunlit",
        "dfl-presets-confirm-tema-trail",
        "dfl-presets-custom",
        "dfl-presets-cycles",
        "dfl-presets-digi",
        "dfl-presets-fav-tema-aurora",
        "dfl-presets-fav-tema-cathode",
        "dfl-presets-fav-tema-redshift",
        "dfl-presets-fav-tema-sprint",
        "dfl-presets-fav-tema-sunlit",
        "dfl-presets-fav-tema-trail",
        "dfl-presets-graph-tema-aurora",
        "dfl-presets-graph-tema-cathode",
        "dfl-presets-graph-tema-redshift",
        "dfl-presets-graph-tema-sprint",
        "dfl-presets-graph-tema-sunlit",
        "dfl-presets-graph-tema-trail",
        "dfl-presets-mode-change",
        "dfl-presets-more-tema-aurora",
        "dfl-presets-more-tema-cathode",
        "dfl-presets-more-tema-redshift",
        "dfl-presets-more-tema-sprint",
        "dfl-presets-more-tema-sunlit",
        "dfl-presets-more-tema-trail",
        "dfl-presets-nav",
        "dfl-presets-nfm",
        "dfl-presets-rotary0",
        "dfl-presets-save",
        "dfl-presets-save-cancel",
        "dfl-presets-save-fail",
        "dfl-presets-touch",
        "dfl-presets-touch-hold",
        "dfl-presets-tx",
        "dfl-presets-vol",
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
        # Paginas de Ajustes del esquema unico de la grilla (issue #175): la
        # pantalla generica `settings-grupo` + su grupo, y las seis variantes de
        # tema de la pagina Operation.
        "settings-grupo-actions",
        "settings-grupo-display",
        "settings-grupo-info",
        "settings-grupo-operation",
        "settings-grupo-operation-tema-aurora",
        "settings-grupo-operation-tema-cathode",
        "settings-grupo-operation-tema-redshift",
        "settings-grupo-operation-tema-sprint",
        "settings-grupo-operation-tema-sunlit",
        "settings-grupo-operation-tema-trail",
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
            # Paginas de Ajustes del esquema unico de la grilla (issue #175).
            "settings-grupo",
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
            # Editor DFL inline (issue #179): restablecer, la rueda principal y
            # las selecciones BW/HIGH; y la pagina DFL normal.
            "dfl-editor-reset",
            "dfl-editor-rotary0",
            "dfl-editor-sel-bw",
            "dfl-editor-sel-high",
            # Espejo USB/LSB del editor inline (issue #182).
            "dfl-editor-mirror",
            "dfl-page",
            # Presets de filtro del DFL (issue #183): A..F.
            "dfl-presets-adj",
            "dfl-presets-activo",
            "dfl-presets-apply",
            "dfl-presets-assign",
            "dfl-presets-assign-cancel",
            "dfl-presets-assign-ok",
            "dfl-presets-confirm",
            "dfl-presets-custom",
            "dfl-presets-cycles",
            "dfl-presets-digi",
            "dfl-presets-fav",
            "dfl-presets-graph",
            "dfl-presets-mode-change",
            "dfl-presets-more",
            "dfl-presets-nav",
            "dfl-presets-nfm",
            "dfl-presets-rotary0",
            "dfl-presets-save",
            "dfl-presets-save-cancel",
            "dfl-presets-save-fail",
            "dfl-presets-touch",
            "dfl-presets-touch-hold",
            "dfl-presets-tx",
            "dfl-presets-vol",
        }
        for scenario in suite.scenarios
    )
