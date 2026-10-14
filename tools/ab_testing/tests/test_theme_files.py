"""Tests for the static contrast contract over the theme JSON files.

The pairs and the numbers here are cross-checked against `tests/check_wcag.py`
(the ROM's own sweep over the nine palettes): the two implementations agree on
every shared pair, so a disagreement in the future is a real signal.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from x6200_ab.errors import ConfigError
from x6200_ab.theme_files import (
    SHIPPED_THEMES_SUBDIR,
    ThemePair,
    check_shipped_themes,
    load_builtin_palettes,
    load_theme_pairs,
    measure_theme_file,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
THEME_H = REPO_ROOT / "src" / "theme.h"
SHIPPED_DIR = REPO_ROOT.joinpath(*SHIPPED_THEMES_SUBDIR)
QUALITY_DIR = Path(__file__).resolve().parents[1] / "quality"

# The pair UI-007 recorded as broken: the switch/slider rail against the dialog.
RAIL_ON_DIALOG = ThemePair(
    fg="control_border",
    bg="dialog",
    min_ratio=3.0,
    where="styles.c styles_switch_init/styles_slider_init",
)


@pytest.fixture(scope="module")
def palettes() -> dict[str, dict[str, str]]:
    return load_builtin_palettes(THEME_H)


def _write_theme(directory: Path, name: str, body: dict) -> Path:
    path = directory / name / "theme.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(body, indent=2) + "\n")
    return path


def _rs_without_control_border() -> dict:
    """The RS file as it was before hallazgo 28: the rail inherited, not set."""
    body = json.loads((SHIPPED_DIR / "RS" / "theme.json").read_text())
    del body["colors"]["control_border"]
    return body


def test_builtin_palettes_come_from_the_role_table(palettes: dict[str, dict[str, str]]) -> None:
    assert list(palettes) == ["IU1WXY", "Legacy", "Outdoor", "Classic"]
    assert len(palettes["IU1WXY"]) == 128
    assert all(len(roles) == 128 for roles in palettes.values())
    # Spot checks against src/theme.h: the macro is the only source of these.
    assert palettes["IU1WXY"]["bg"] == "#27313a"
    assert palettes["Classic"]["control_border"] == "#626262"
    assert palettes["Outdoor"]["dialog"] == "#ffffff"


def test_a_theme_that_stops_defining_the_rail_reproduces_the_2_58_defect(
    tmp_path: Path, palettes: dict[str, dict[str, str]]
) -> None:
    """The deliberate regression of UI-007: the file inherits an illegible rail.

    `measure_theme_file` has to resolve the base to see it: the value is not in
    the file, it comes from the Classic palette in src/theme.h.
    """
    path = _write_theme(tmp_path, "RS", _rs_without_control_border())
    report = measure_theme_file(path, (RAIL_ON_DIALOG,), palettes)
    assert report.ok is False
    assert report.base == "Classic"
    assert "control_border" in report.inherited
    failure = report.failures[0]
    assert failure.ratio == pytest.approx(2.58, abs=0.01)
    assert failure.origin == "inherited from base Classic: control_border"
    assert "control_border on dialog = 2.58:1 (min 3.0:1)" in failure.detail


def test_a_theme_that_defines_the_rail_passes(
    tmp_path: Path, palettes: dict[str, dict[str, str]]
) -> None:
    body = _rs_without_control_border()
    body["colors"]["control_border"] = "#6e6e6e"
    report = measure_theme_file(_write_theme(tmp_path, "RS", body), (RAIL_ON_DIALOG,), palettes)
    assert report.ok is True
    assert report.results[0].origin == "both roles set by the file"


def test_the_shipped_themes_pass_every_declared_pair(
    palettes: dict[str, dict[str, str]],
) -> None:
    pairs = load_theme_pairs(QUALITY_DIR / "theme_pairs.yaml")
    reports = check_shipped_themes(SHIPPED_DIR, pairs, palettes)
    assert [report.name for report in reports] == ["Holo", "Material 3", "RS"]
    assert all(report.ok for report in reports)
    assert all(report.error is None for report in reports)
    assert all(len(report.results) == len(pairs) for report in reports)
    # RS sets the rail it used to inherit (hallazgo 28); the check has to see the
    # file's own value, and it has to agree with tests/check_wcag.py on it.
    rs = next(report for report in reports if report.name == "RS")
    rail = next(result for result in rs.results if result.pair.rule == RAIL_ON_DIALOG.rule)
    assert "control_border" not in rs.inherited
    assert rail.ratio == pytest.approx(3.08, abs=0.01)
    assert rail.origin == "both roles set by the file"


def test_an_unknown_role_is_refused(tmp_path: Path, palettes: dict[str, dict[str, str]]) -> None:
    """src/theme.c ignores a colour the C table does not define; so do we."""
    path = _write_theme(tmp_path, "Roto", {"base": "Classic", "colors": {"nope": "#ffffff"}})
    report = measure_theme_file(path, (RAIL_ON_DIALOG,), palettes)
    assert report.ok is False
    assert report.error is not None
    assert "unknown role" in report.error


def test_a_derived_base_is_refused(tmp_path: Path, palettes: dict[str, dict[str, str]]) -> None:
    path = _write_theme(tmp_path, "Vision", {"base": "VISION"})
    report = measure_theme_file(path, (RAIL_ON_DIALOG,), palettes)
    assert report.error is not None
    assert "derived built-in" in report.error


def test_bad_json_colour_and_shape_are_refused(
    tmp_path: Path, palettes: dict[str, dict[str, str]]
) -> None:
    broken = tmp_path / "Broken" / "theme.json"
    broken.parent.mkdir(parents=True)
    broken.write_text("{ not json")
    report = measure_theme_file(broken, (RAIL_ON_DIALOG,), palettes)
    assert report.error is not None
    assert "not valid JSON" in report.error

    bad_colour = _write_theme(tmp_path, "Bad", {"colors": {"bg": "white"}})
    report = measure_theme_file(bad_colour, (RAIL_ON_DIALOG,), palettes)
    assert report.error is not None
    assert "is not #rrggbb" in report.error

    bad_colors = _write_theme(tmp_path, "Badcolors", {"colors": ["#ffffff"]})
    report = measure_theme_file(bad_colors, (RAIL_ON_DIALOG,), palettes)
    assert report.error is not None
    assert "`colors` must be an object" in report.error


def test_a_pair_that_names_an_unknown_role_is_refused(
    tmp_path: Path, palettes: dict[str, dict[str, str]]
) -> None:
    """A typo in the pairs table must be a config error, not a KeyError."""
    pairs = (ThemePair(fg="textt", bg="dialog", min_ratio=4.5),)
    with pytest.raises(ConfigError, match="unknown theme role `textt`"):
        check_shipped_themes(tmp_path, pairs, palettes)


def test_the_repo_pairs_are_well_formed() -> None:
    pairs = load_theme_pairs(QUALITY_DIR / "theme_pairs.yaml")
    assert pairs
    for pair in pairs:
        assert pair.fg != pair.bg
        assert pair.min_ratio in {4.5, 3.0}
        assert pair.where.strip(), f"{pair.rule} does not say where it is drawn"


def test_a_pair_that_compares_a_role_with_itself_is_refused(tmp_path: Path) -> None:
    path = tmp_path / "pairs.yaml"
    path.write_text("pairs:\n  - fg: text\n    bg: text\n    min_ratio: 4.5\n")
    with pytest.raises(ConfigError, match="compares a role with itself"):
        load_theme_pairs(path)


def test_a_pair_without_a_threshold_is_refused(tmp_path: Path) -> None:
    path = tmp_path / "pairs.yaml"
    path.write_text("pairs:\n  - fg: text\n    bg: dialog\n")
    with pytest.raises(ConfigError, match="missing `min_ratio`"):
        load_theme_pairs(path)


def test_a_pair_with_a_non_numeric_threshold_is_refused(tmp_path: Path) -> None:
    path = tmp_path / "pairs.yaml"
    path.write_text("pairs:\n  - fg: text\n    bg: dialog\n    min_ratio: alto\n")
    with pytest.raises(ConfigError, match="`min_ratio` must be a number"):
        load_theme_pairs(path)
