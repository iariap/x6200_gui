"""Static contrast over the theme JSON files the ROM ships (no render needed).

The rendered `typography_contrast` contract only speaks about the screens the
harness can open. Settings is not one of them (`dialog_settings.c` drags
`dialog_field.c` and dozens of dependencies — UI-004), and its own pair — the
switch/slider rail `TC(control_border)` against the dialog and the panel — is
exactly the one UI-007 recorded as broken: an RS theme that did not define
`control_border` inherited one from its `base` and drew 2.58:1.

This module reads the files the image installs
(`rootfs/usr/share/x6200/themes/<name>.json` or `<name>/theme.json`), resolves
their `base` inheritance the way `src/theme.c` does (`use_builtin()` plus the
file's `colors`) and measures the pairs `quality/theme_pairs.yaml` declares: the
ones that live on the screen the harness cannot reach yet. Everything is offline
— files in, findings out; no Docker, no radio, no pixels.

The exhaustive sweep over the nine palettes (including the C tables and the
`negative()`/VISION derivations) stays in `tests/check_wcag.py`, which the ROM's
own CI runs. This one is the framework-side contract over the shipped files, so
a file that stops defining a role cannot silently inherit an illegible one.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .configio import as_float, read_yaml_mapping
from .contrast import contrast_ratio, hex_colour
from .errors import ConfigError

# Where the image installs the theme files (the harness points
# THEMES_DIR_SHIPPED here; src/theme.c defaults to the same path on the radio).
SHIPPED_THEMES_SUBDIR = ("rootfs", "usr", "share", "x6200", "themes")

# The four palettes of `THEME_COLORS(X)` in src/theme.h, in the order
# `builtin_names[]` uses in src/theme.c: IU1WXY, Legacy, Outdoor, Classic. A
# shipped file's `base` names one of them, and every role the file does not set
# comes from there. The two derived built-ins (Outdoor-Negative, VISION) are made
# at runtime by `use_builtin()` and are refused here on purpose: guessing their
# values would hide exactly the inheritance this check exists to catch.
BASE_PALETTES = ("IU1WXY", "Legacy", "Outdoor", "Classic")

RULE_FILE_INVALID = "theme_file_invalid"
RULE_PAIR_PREFIX = "theme_pair:"

_ROLE_LINE = re.compile(
    r"^\s*X\(\s*([a-z_0-9]+)\s*,\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*,"
    r"\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*\)",
    re.MULTILINE,
)
_COLOUR_VALUE = re.compile(r"#?[0-9a-fA-F]{6}")


@dataclass(frozen=True)
class ThemePair:
    """One text-on-background (or non-text) pair the UI draws."""

    fg: str
    bg: str
    min_ratio: float
    where: str = ""

    @property
    def rule(self) -> str:
        return f"{RULE_PAIR_PREFIX}{self.fg}_on_{self.bg}"

    def as_dict(self) -> dict[str, Any]:
        return {"fg": self.fg, "bg": self.bg, "min_ratio": self.min_ratio, "where": self.where}


@dataclass(frozen=True)
class PairResult:
    """One pair measured on one theme."""

    theme: str
    pair: ThemePair
    ratio: float
    ok: bool
    origin: str

    @property
    def detail(self) -> str:
        return (
            f"{self.pair.fg} on {self.pair.bg} = {self.ratio:.2f}:1 "
            f"(min {self.pair.min_ratio:.1f}:1) [{self.origin}]"
        )

    def as_dict(self) -> dict[str, Any]:
        return {
            "theme": self.theme,
            "rule": self.pair.rule,
            "fg": self.pair.fg,
            "bg": self.pair.bg,
            "ratio": round(self.ratio, 2),
            "min_ratio": self.pair.min_ratio,
            "ok": self.ok,
            "origin": self.origin,
            "where": self.pair.where,
            "detail": self.detail,
        }


@dataclass(frozen=True)
class ThemeReport:
    """One shipped theme file: its palette and every pair measured on it."""

    name: str
    path: str
    base: str
    inherited: tuple[str, ...]
    results: tuple[PairResult, ...]
    error: str | None = None

    @property
    def ok(self) -> bool:
        return self.error is None and all(result.ok for result in self.results)

    @property
    def failures(self) -> tuple[PairResult, ...]:
        return tuple(result for result in self.results if not result.ok)

    def as_dict(self) -> dict[str, Any]:
        return {
            "theme": self.name,
            "path": self.path,
            "base": self.base,
            "inherited_roles": list(self.inherited),
            "pairs_checked": len(self.results),
            "ok": self.ok,
            "error": self.error,
            "results": [result.as_dict() for result in self.results],
        }


def load_builtin_palettes(theme_h: Path) -> dict[str, dict[str, str]]:
    """The four built-in palettes of src/theme.h, as `#rrggbb` per role.

    The macro is what the firmware's enum is generated from, so a role added
    there (or a value changed) is picked up here without touching this module;
    what this parser cannot guess is the palette order, which is
    `builtin_names[]`'s in src/theme.c.
    """
    if not theme_h.is_file():
        raise ConfigError(f"missing the theme role table: {theme_h}")
    body = _macro_body(theme_h.read_text(errors="surrogateescape"))
    palettes: dict[str, dict[str, str]] = {name: {} for name in BASE_PALETTES}
    for match in _ROLE_LINE.finditer(body):
        role = match.group(1)
        for index, name in enumerate(BASE_PALETTES):
            palettes[name][role] = hex_colour(int(match.group(index + 2), 16))
    counts = {name: len(roles) for name, roles in palettes.items()}
    if not counts[BASE_PALETTES[0]]:
        raise ConfigError(f"{theme_h}: found no role in THEME_COLORS(X)")
    if len(set(counts.values())) != 1:
        raise ConfigError(f"{theme_h}: THEME_COLORS(X) is not rectangular: {counts}")
    return palettes


def _macro_body(text: str) -> str:
    """The `#define THEME_COLORS(X)` block (backslash-continued lines)."""
    lines = text.splitlines()
    start = next(
        (i for i, line in enumerate(lines) if line.startswith("#define THEME_COLORS(X)")), None
    )
    if start is None:
        raise ConfigError("src/theme.h: no THEME_COLORS(X) macro")
    body: list[str] = []
    for line in lines[start:]:
        body.append(line)
        if not line.rstrip().endswith("\\"):
            break
    return "\n".join(body)


def load_theme_pairs(path: Path) -> tuple[ThemePair, ...]:
    """The declared pairs from `quality/theme_pairs.yaml`."""
    data = read_yaml_mapping(path)
    raw = data.get("pairs")
    if not isinstance(raw, list) or not raw:
        raise ConfigError(f"{path}: `pairs` must be a non-empty list")
    pairs: list[ThemePair] = []
    for entry in raw:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: every pair must be a mapping")
        for key in ("fg", "bg", "min_ratio"):
            if key not in entry:
                raise ConfigError(f"{path}: pair missing `{key}`")
        fg, bg = str(entry["fg"]), str(entry["bg"])
        if fg == bg:
            raise ConfigError(f"{path}: pair `{fg}` compares a role with itself")
        pairs.append(
            ThemePair(
                fg=fg,
                bg=bg,
                min_ratio=as_float(entry, "min_ratio", path),
                where=str(entry.get("where", "")),
            )
        )
    return tuple(pairs)


def shipped_theme_files(directory: Path) -> tuple[Path, ...]:
    """`<name>.json` and `<name>/theme.json`, as `src/theme.c` looks for them."""
    if not directory.is_dir():
        raise ConfigError(f"missing the shipped themes directory: {directory}")
    return tuple(sorted(directory.glob("*.json")) + sorted(directory.glob("*/theme.json")))


def check_shipped_themes(
    directory: Path,
    pairs: tuple[ThemePair, ...],
    palettes: dict[str, dict[str, str]],
) -> tuple[ThemeReport, ...]:
    """Every shipped theme file measured against every declared pair."""
    _validate_pairs(pairs, palettes)
    return tuple(
        measure_theme_file(path, pairs, palettes) for path in shipped_theme_files(directory)
    )


def _validate_pairs(pairs: tuple[ThemePair, ...], palettes: dict[str, dict[str, str]]) -> None:
    """A pair may only name roles the C table has: `palette[role]` would be a typo."""
    known = set(palettes[BASE_PALETTES[0]])
    for pair in pairs:
        for role in (pair.fg, pair.bg):
            if role not in known:
                raise ConfigError(
                    f"pair `{pair.rule}` names unknown theme role `{role}`; "
                    f"src/theme.h defines {len(known)} roles"
                )


def measure_theme_file(
    path: Path,
    pairs: tuple[ThemePair, ...],
    palettes: dict[str, dict[str, str]],
) -> ThemeReport:
    """Resolve one theme file and measure every declared pair on it.

    A file that cannot be resolved (bad JSON, an unknown role, a `base` this
    check cannot derive) is reported as an error of its own: a theme the user can
    pick that the checker cannot read is not a pass.
    """
    name = path.stem if path.name != "theme.json" else path.parent.name
    try:
        base, colours = _read_theme_file(path)
    except ConfigError as exc:
        return ThemeReport(
            name=name, path=str(path), base="", inherited=(), results=(), error=str(exc)
        )
    if base not in palettes:
        return ThemeReport(
            name=name,
            path=str(path),
            base=base,
            inherited=(),
            results=(),
            error=(
                f"base `{base}` is not one of {list(BASE_PALETTES)}: this check resolves "
                f"the four palettes of src/theme.h, and a derived built-in has to be "
                f"spelled out here before a shipped file can use it"
            ),
        )
    known = set(palettes[BASE_PALETTES[0]])
    unknown = sorted(set(colours) - known)
    if unknown:
        return ThemeReport(
            name=name,
            path=str(path),
            base=base,
            inherited=(),
            results=(),
            error=(
                f"unknown role(s) {unknown}: src/theme.c ignores a colour the C table "
                f"does not define, so the theme silently keeps the base value"
            ),
        )

    palette = dict(palettes[base]) | colours
    inherited = tuple(role for role in sorted(known) if role not in colours)
    results = tuple(_measure(name, palette, colours, base, pair) for pair in pairs)
    return ThemeReport(name=name, path=str(path), base=base, inherited=inherited, results=results)


def _measure(
    name: str,
    palette: dict[str, str],
    colours: dict[str, str],
    base: str,
    pair: ThemePair,
) -> PairResult:
    ratio = contrast_ratio(palette[pair.fg], palette[pair.bg])
    from_base = [role for role in (pair.fg, pair.bg) if role not in colours]
    origin = (
        "both roles set by the file"
        if not from_base
        else f"inherited from base {base}: {', '.join(from_base)}"
    )
    return PairResult(theme=name, pair=pair, ratio=ratio, ok=ratio >= pair.min_ratio, origin=origin)


def _read_theme_file(path: Path) -> tuple[str, dict[str, str]]:
    """`(base, colours the file sets)`, with the schema `src/theme.c` accepts."""
    try:
        data = json.loads(path.read_text(errors="surrogateescape"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ConfigError(f"{path}: not valid JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ConfigError(f"{path}: top level must be an object")
    base = str(data.get("base", "IU1WXY"))
    raw_colours = data.get("colors", {})
    if not isinstance(raw_colours, dict):
        raise ConfigError(f"{path}: `colors` must be an object")
    colours: dict[str, str] = {}
    for role, value in raw_colours.items():
        if not isinstance(value, str) or not _COLOUR_VALUE.fullmatch(value):
            raise ConfigError(f"{path}: colour `{role}` is not #rrggbb: {value!r}")
        colours[str(role)] = "#" + value.lstrip("#").lower()
    return base, colours
