"""Tests for the static contrast contract over the theme JSON files.

The pairs and the numbers here are cross-checked against `tests/check_wcag.py`
(the ROM's own sweep over the served palettes): the two implementations agree on
every shared pair, so a disagreement in the future is a real signal.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from x6200_ab.errors import ConfigError
from x6200_ab.theme_files import (
    DERIVED_PALETTES,
    SERVED_BUILTINS,
    SHIPPED_THEMES_SUBDIR,
    ThemePair,
    ThemeReport,
    check_shipped_themes,
    derive_outdoor_negative,
    derive_vision,
    load_builtin_palettes,
    load_exemptions,
    load_runtime_palettes,
    load_theme_pairs,
    measure_theme_file,
    served_theme_reports,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
THEME_H = REPO_ROOT / "src" / "theme.h"
THEME_C = REPO_ROOT / "src" / "theme.c"
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


@pytest.fixture(scope="module")
def runtime_palettes() -> dict[str, dict[str, str]]:
    return load_runtime_palettes(THEME_C, load_builtin_palettes(THEME_H))


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
    assert len(palettes["IU1WXY"]) == 122
    assert all(len(roles) == 122 for roles in palettes.values())
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
    assert [report.name for report in reports] == ["Holo", "Material 3", "Redshift", "RS"]
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


def test_a_derived_base_resolves(
    tmp_path: Path, runtime_palettes: dict[str, dict[str, str]]
) -> None:
    """A shipped file may inherit a `use_builtin()` base, as `builtin_index()` allows."""
    path = _write_theme(tmp_path, "Vision", {"base": "VISION"})
    report = measure_theme_file(path, (RAIL_ON_DIALOG,), runtime_palettes)
    assert report.error is None
    assert report.base == "VISION"
    assert all(result.ok for result in report.results)


def test_an_unknown_base_is_refused(tmp_path: Path, palettes: dict[str, dict[str, str]]) -> None:
    path = _write_theme(tmp_path, "Raro", {"base": "NoExiste"})
    report = measure_theme_file(path, (RAIL_ON_DIALOG,), palettes)
    assert report.error is not None
    assert "not one of" in report.error


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
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """The inventory has no silent hole: every served theme, built-ins first."""
    reports, pairs = _served(runtime_palettes)
    assert [report.name for report in reports] == [
        "IU1WXY",
        "Legacy",
        "Outdoor",
        "Classic",
        "Outdoor-Negative",
        "VISION",
        "Holo",
        "Material 3",
        "Redshift",
        "RS",
    ]
    builtins = [report for report in reports if report.source == "builtin"]
    assert len(builtins) == 6
    assert all(report.inherited == () for report in builtins)
    assert all(report.base == report.name for report in builtins)
    assert all(len(report.results) == len(pairs) for report in reports)


def test_every_served_theme_passes_every_declared_pair(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """The full coverage finds no sub-WCAG pair: 10 themes, 90 measurements."""
    reports, _ = _served(runtime_palettes)
    checked = sum(len(report.results) for report in reports)
    assert checked == 90
    assert all(report.ok for report in reports)
    assert all(report.error is None for report in reports)


def test_builtin_report_ratios_agree_with_the_c_table(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """The numbers here have to match tests/check_wcag.py on the same palettes."""
    reports, _ = _served(runtime_palettes)
    by_name = {report.name: report for report in reports}
    iu = _ratio(by_name["IU1WXY"], "text", "dialog")
    classic = _ratio(by_name["Classic"], "control_border", "dialog")
    outdoor = _ratio(by_name["Outdoor"], "text", "dialog")
    assert iu == pytest.approx(6.37, abs=0.01)
    assert classic == pytest.approx(3.12, abs=0.01)
    assert outdoor == pytest.approx(21.00, abs=0.01)


def test_legacy_is_measured_but_not_offered(runtime_palettes: dict[str, dict[str, str]]) -> None:
    """`theme_list()` skips Legacy, so the inventory has to say it is not a choice."""
    reports, _ = _served(runtime_palettes)
    by_name = {report.name: report for report in reports}
    assert by_name["Legacy"].offered is False
    assert all(by_name[name].offered for name in ("IU1WXY", "Outdoor", "Classic"))


def test_rs_declares_the_binary_surfaces_it_paints(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """The PNG backgrounds are not statically resolvable, so the theme is partial."""
    reports, _ = _served(runtime_palettes)
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


def test_derived_builtins_are_measured_like_the_data_ones(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """Outdoor-Negative and VISION are no longer UNCOVERED: they carry all 9 pairs."""
    reports, _ = _served(runtime_palettes)
    by_name = {report.name: report for report in reports}
    for name in DERIVED_PALETTES:
        report = by_name[name]
        assert report.source == "builtin"
        assert report.error is None
        assert len(report.results) == 9
        assert all(result.ok for result in report.results)
        assert all("use_builtin()" in result.origin for result in report.results)


def test_derived_palettes_never_invent_a_role(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """The derivation is a port: Outdoor-Negative and VISION keep the 122 C roles."""
    data_roles = set(load_builtin_palettes(THEME_H)["Outdoor"])
    assert set(runtime_palettes["Outdoor-Negative"]) == data_roles
    assert set(runtime_palettes["VISION"]) == data_roles


def test_served_builtins_are_the_c_enum_order() -> None:
    """SERVED_BUILTINS mirrors `builtin_names[]` (src/theme.c): data then derives."""
    assert SERVED_BUILTINS == (
        "IU1WXY",
        "Legacy",
        "Outdoor",
        "Classic",
        "Outdoor-Negative",
        "VISION",
    )
    assert DERIVED_PALETTES == ("Outdoor-Negative", "VISION")


def test_outdoor_negative_is_negative_over_outdoor() -> None:
    """Spot checks against src/theme.c: white <-> black, and the pinned yellow/blue."""
    outdoor = load_builtin_palettes(THEME_H)["Outdoor"]
    negative = derive_outdoor_negative(THEME_C, outdoor)
    # negative(#ffffff) == #000000 and the two hand-tuned literals of use_builtin().
    assert negative["dialog"] == "#000000"
    assert negative["focus_bg"] == "#877100"
    assert negative["vfo"] == "#4574ff"
    # The selection yellow stays at its Outdoor value (its channels add to 255).
    for role in ("sel_text", "btn_active_text", "sel_border", "btn_active_border", "near_bg"):
        assert negative[role] == outdoor[role]


def test_vision_is_iu1wxy_plus_vision_colors() -> None:
    iu1wxy = load_builtin_palettes(THEME_H)["IU1WXY"]
    vision = derive_vision(THEME_C, iu1wxy)
    # Every role the array does not override keeps its IU1WXY value.
    assert vision["text"] == "#ffffff"
    assert vision["bg"] == "#1c1f24"
    assert vision["meter_bar"] == "#5dcaa5"


def _load_wcag_sweep() -> tuple[Any, dict[str, dict[str, str]]]:
    """tests/check_wcag.py, loaded from the ROM's test tree, plus its palettes."""
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "check_wcag_parity", REPO_ROOT / "tests" / "check_wcag.py"
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    colores = module.leer_theme_h()
    paletas, _shapes = module.armar_paletas(colores)
    sweep = {
        theme: {role: f"#{value & 0xFFFFFF:06x}" for role, value in paleta.items()}
        for theme, paleta in paletas.items()
    }
    return module, sweep


def test_the_derivation_matches_the_rom_sweep_byte_for_byte(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """Parity with tests/check_wcag.py: the same C, two implementations agree.

    `check_wcag.py` is the ROM's own sweep; if the port and it ever disagree on a
    role of any of the served palettes, one of them is wrong. This is the
    self-test the spec asks for: it pins the derivation, it does not trust it.
    """
    _module, sweep = _load_wcag_sweep()
    for theme in (*("IU1WXY", "Legacy", "Outdoor", "Classic"), *DERIVED_PALETTES):
        theirs = sweep[theme]
        mine = runtime_palettes[theme]
        # check_wcag injects its literals table (LVGL colours, hand-written hex);
        # the parity is over the roles src/theme.h defines.
        assert set(mine) <= set(theirs), f"{theme}: a role of the C table is missing in the sweep"
        drift = {role: (mine[role], theirs[role]) for role in mine if mine[role] != theirs[role]}
        assert drift == {}, f"{theme}: roles disagree with check_wcag.py: {drift}"


def test_the_declared_pairs_match_the_rom_sweep_ratio_by_ratio(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """Every framework pair's ratio equals the ROM sweep's on the same palette."""
    module, sweep = _load_wcag_sweep()
    pairs = load_theme_pairs(QUALITY_DIR / "theme_pairs.yaml")
    reports, _ = _served(runtime_palettes)
    by_name = {report.name: report for report in reports}
    for theme in (*("IU1WXY", "Legacy", "Outdoor", "Classic"), *DERIVED_PALETTES):
        paleta = sweep[theme]
        for pair in pairs:
            expected = module.contraste(
                int(paleta[pair.fg].lstrip("#"), 16), int(paleta[pair.bg].lstrip("#"), 16)
            )
            assert _ratio(by_name[theme], pair.fg, pair.bg) == pytest.approx(expected, abs=1e-9)


def _ratio(report: ThemeReport, fg: str, bg: str) -> float:
    return next(
        result.ratio for result in report.results if (result.pair.fg, result.pair.bg) == (fg, bg)
    )


# --- the WCAG 1.4.3 exemptions ----------------------------------------------


def test_the_exempt_roles_are_the_ones_the_rom_sweep_exempts(
    runtime_palettes: dict[str, dict[str, str]],
) -> None:
    """The framework and tests/check_wcag.py cannot disagree on what is exempt."""
    module, _sweep = _load_wcag_sweep()
    declared = {exemption.role for exemption in load_exemptions(QUALITY_DIR / "theme_pairs.yaml")}
    assert declared == set(module.EXENTOS)
    # Every exempt role is a role the C table actually has, not a typo.
    assert declared <= set(runtime_palettes["IU1WXY"])


def test_every_exempt_role_is_drawn_somewhere_in_the_rom_sweep() -> None:
    """An exemption has to cover a role the sweep measures (not dead data)."""
    module, _sweep = _load_wcag_sweep()
    measured = {fg for fg, _bg, _threshold, _where in module.PARES_EXENTOS}
    declared = {exemption.role for exemption in load_exemptions(QUALITY_DIR / "theme_pairs.yaml")}
    assert declared <= measured, f"exempt but never drawn: {sorted(declared - measured)}"


def test_an_exempt_role_needs_a_reason_and_is_listed_once(tmp_path: Path) -> None:
    path = tmp_path / "pairs.yaml"
    path.write_text("pairs: []\nexempt:\n  - role: off\n")
    with pytest.raises(ConfigError, match="needs a `reason`"):
        load_exemptions(path)
    path.write_text(
        "pairs: []\nexempt:\n  - role: off\n    reason: a\n  - role: off\n    reason: b\n"
    )
    with pytest.raises(ConfigError, match="listed twice"):
        load_exemptions(path)
