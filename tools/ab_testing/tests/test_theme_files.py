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
    ThemeReport,
    UnsupportedTheme,
    check_shipped_themes,
    ensure_unsupported_are_not_measured,
    load_builtin_palettes,
    load_theme_pairs,
    load_unsupported_themes,
    measure_theme_file,
    served_theme_reports,
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


# --- the served inventory: built-ins + files, and what stays unsupported ------


def _served(palettes: dict[str, dict[str, str]]):
    pairs = load_theme_pairs(QUALITY_DIR / "theme_pairs.yaml")
    return served_theme_reports(SHIPPED_DIR, THEME_H, palettes, pairs), pairs


def test_the_served_inventory_is_the_builtins_then_the_files(
    palettes: dict[str, dict[str, str]],
) -> None:
    """The inventory has no silent hole: the four data palettes come first."""
    reports, pairs = _served(palettes)
    assert [report.name for report in reports] == [
        "IU1WXY",
        "Legacy",
        "Outdoor",
        "Classic",
        "Holo",
        "Material 3",
        "RS",
    ]
    builtins = [report for report in reports if report.source == "builtin"]
    assert len(builtins) == 4
    assert all(report.inherited == () for report in builtins)
    assert all(report.base == report.name for report in builtins)
    assert all(len(report.results) == len(pairs) for report in reports)


def test_every_served_theme_passes_every_declared_pair(
    palettes: dict[str, dict[str, str]],
) -> None:
    """The extended coverage finds no sub-WCAG pair: 7 themes, 63 measurements."""
    reports, _ = _served(palettes)
    checked = sum(len(report.results) for report in reports)
    assert checked == 63
    assert all(report.ok for report in reports)
    assert all(report.error is None for report in reports)


def test_builtin_report_ratios_agree_with_the_c_table(
    palettes: dict[str, dict[str, str]],
) -> None:
    """The numbers here have to match tests/check_wcag.py on the same palettes."""
    reports, _ = _served(palettes)
    by_name = {report.name: report for report in reports}
    iu = _ratio(by_name["IU1WXY"], "text", "dialog")
    classic = _ratio(by_name["Classic"], "control_border", "dialog")
    outdoor = _ratio(by_name["Outdoor"], "text", "dialog")
    assert iu == pytest.approx(6.37, abs=0.01)
    assert classic == pytest.approx(3.12, abs=0.01)
    assert outdoor == pytest.approx(21.00, abs=0.01)


def test_legacy_is_measured_but_not_offered(palettes: dict[str, dict[str, str]]) -> None:
    """`theme_list()` skips Legacy, so the inventory has to say it is not a choice."""
    reports, _ = _served(palettes)
    by_name = {report.name: report for report in reports}
    assert by_name["Legacy"].offered is False
    assert all(by_name[name].offered for name in ("IU1WXY", "Outdoor", "Classic"))


def test_rs_declares_the_binary_surfaces_it_paints(
    palettes: dict[str, dict[str, str]],
) -> None:
    """The PNG backgrounds are not statically resolvable, so the theme is partial."""
    reports, _ = _served(palettes)
    rs = next(report for report in reports if report.name == "RS")
    assert "dialog" in rs.binary_surfaces
    assert "panel" not in rs.binary_surfaces
    assert rs.coverage == "partial"
    dialog = next(
        result for result in rs.results if (result.pair.fg, result.pair.bg) == ("text", "dialog")
    )
    assert dialog.binary is True
    assert dialog.image == "dialog.png"
    panel = next(
        result for result in rs.results if (result.pair.fg, result.pair.bg) == ("text", "panel")
    )
    assert panel.binary is False
    assert panel.image == ""
    # The two roles are still the file's: the flag does not change the verdict.
    assert dialog.ok is True
    assert "both roles set by the file" in dialog.origin


def test_the_runtime_builtins_are_declared_unsupported() -> None:
    """Outdoor-Negative and VISION are made by C at runtime; declared, not guessed."""
    unsupported = load_unsupported_themes(QUALITY_DIR / "theme_pairs.yaml")
    assert [theme.name for theme in unsupported] == ["Outdoor-Negative", "VISION"]
    assert all(theme.source == "builtin" for theme in unsupported)
    assert all("use_builtin" in theme.reason for theme in unsupported)
    # Declared unsupported is not measured: no theme appears in both lists.
    assert not {theme.name for theme in unsupported} & {"IU1WXY", "Outdoor", "RS"}


def test_the_unsupported_themes_do_not_collide_with_the_measured_ones() -> None:
    palettes = load_builtin_palettes(THEME_H)
    reports, _ = _served(palettes)
    unsupported = load_unsupported_themes(QUALITY_DIR / "theme_pairs.yaml")
    ensure_unsupported_are_not_measured(unsupported, reports, "theme_pairs.yaml")


def test_a_theme_both_measured_and_unsupported_is_refused(
    palettes: dict[str, dict[str, str]],
) -> None:
    reports, _ = _served(palettes)
    unsupported = load_unsupported_themes(QUALITY_DIR / "theme_pairs.yaml")
    clash = UnsupportedTheme(name="RS", source="builtin", reason="x")
    with pytest.raises(ConfigError, match="measured and declared unsupported"):
        ensure_unsupported_are_not_measured((*unsupported, clash), reports, "theme_pairs.yaml")


def test_an_unsupported_theme_needs_a_reason(tmp_path: Path) -> None:
    path = tmp_path / "pairs.yaml"
    path.write_text("pairs: []\nunsupported:\n  - theme: VISION\n")
    with pytest.raises(ConfigError, match="needs a `reason`"):
        load_unsupported_themes(path)


def test_an_unsupported_theme_is_listed_once(tmp_path: Path) -> None:
    path = tmp_path / "pairs.yaml"
    path.write_text(
        "pairs: []\n"
        "unsupported:\n"
        "  - theme: VISION\n    reason: a\n"
        "  - theme: VISION\n    reason: b\n"
    )
    with pytest.raises(ConfigError, match="listed twice"):
        load_unsupported_themes(path)


def _ratio(report: ThemeReport, fg: str, bg: str) -> float:
    return next(
        result.ratio for result in report.results if (result.pair.fg, result.pair.bg) == (fg, bg)
    )
