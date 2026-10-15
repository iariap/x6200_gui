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

Three families of served themes are measured here, so the report is a full
inventory of what the user can pick and not only of what ships as a file:

- the **built-in palettes** of `src/theme.h` (IU1WXY, Legacy, Outdoor, Classic) —
  they are the `base` every file resolves against, the same data the macro holds;
- the **runtime built-ins** `use_builtin()` (`src/theme.c`) derives at init:
  Outdoor-Negative (`negative()` over Outdoor plus its hand-tuned WCAG role
  overrides) and VISION (IU1WXY plus the `vision_colors[]` array). They are not
  data, so they are *derived* from the same C the firmware runs — never guessed
  — and `tests/test_theme_files.py` pins the derivation byte-for-byte against
  `tests/check_wcag.py`, the ROM's own sweep over the nine palettes;
- the **shipped files** of `rootfs/usr/share/x6200/themes` (`<name>.json` or
  `<name>/theme.json`), with the `base` inheritance `src/theme.c` applies.

The ninth theme the menu offers is `Outdoor-Negative`; with the derivation below
every served palette resolves, so the whole inventory is measured (every served
theme x 9 pairs) and nothing is declared UNCOVERED.

The exhaustive sweep over the nine palettes stays in `tests/check_wcag.py`, which
the ROM's own CI runs. This one is the framework-side inventory over what the
files and the C table spell out, so a file that stops defining a role cannot
silently inherit an illegible one, and no served theme is missing from the map.
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

# The two built-ins `use_builtin()` (src/theme.c) derives at init, in the order
# of the `B_*` enum: Outdoor-Negative and VISION. Their palette is not data, so
# `load_runtime_palettes()` builds it from the same C the firmware runs.
DERIVED_PALETTES = ("Outdoor-Negative", "VISION")

# Every built-in palette in the order `builtin_names[]` declares them: the four
# data palettes of THEME_COLORS(X) first, then the two `use_builtin()` derives.
SERVED_BUILTINS = (*BASE_PALETTES, *DERIVED_PALETTES)

# Which of the built-ins `theme_list()` (src/theme.c) offers in Settings. Legacy
# is kept as a palette and as a `base` for shipped files, but the menu skips it,
# so the inventory has to say so instead of counting it as a served choice.
BUILTIN_OFFERED = {
    "IU1WXY": True,
    "Legacy": False,
    "Outdoor": True,
    "Classic": True,
    "Outdoor-Negative": True,
    "VISION": True,
}

# The surfaces of the `images` block (src/theme.c, `image_names[]`): a file may
# paint one with a PNG it ships instead of the role's colour. A pair whose
# background is one of these still measures the role (the value src/theme.c keeps
# when the PNG is missing or unreadable), but the pixels that reach the screen
# come from the binary, so the pair is flagged and the theme's coverage is
# partial — the binary is not statically resolvable.
IMAGE_SURFACES = (
    "bg",
    "btn",
    "btn_active",
    "btn_off",
    "btn_page",
    "bar",
    "strip",
    "dialog",
    "mark",
    "mark_active",
)

RULE_FILE_INVALID = "theme_file_invalid"
RULE_PAIR_PREFIX = "theme_pair:"


_ROLE_LINE = re.compile(
    r"^\s*X\(\s*([a-z_0-9]+)\s*,\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*,"
    r"\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*\)",
    re.MULTILINE,
)
_COLOUR_VALUE = re.compile(r"#?[0-9a-fA-F]{6}")

# The derivation `use_builtin()` applies at init (src/theme.c). Both are read
# from the C so the framework never carries a second copy of the numbers:
_VISION_ENTRY = re.compile(r"\{\s*TC_([a-z_0-9]+)\s*,\s*(0x[0-9a-fA-F]{6})\s*\}")
# Outdoor-Negative keeps a role at its Outdoor value (`colors[TC_x] =
# builtin_colors[B_OUTDOOR][TC_y];`) or pins it to a literal (`= 0xrrggbb;`).
_NEG_FROM_OUTDOOR = re.compile(
    r"colors\[TC_([a-z_0-9]+)\]\s*=\s*builtin_colors\[B_OUTDOOR\]\[TC_([a-z_0-9]+)\]\s*;"
)
_NEG_LITERAL = re.compile(r"colors\[TC_([a-z_0-9]+)\]\s*=\s*(0x[0-9a-fA-F]{6})\s*;")


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
    binary: bool = False
    image: str = ""

    @property
    def detail(self) -> str:
        note = f" [bg painted by {self.image}, ratio is the role's]" if self.binary else ""
        return (
            f"{self.pair.fg} on {self.pair.bg} = {self.ratio:.2f}:1 "
            f"(min {self.pair.min_ratio:.1f}:1) [{self.origin}]{note}"
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
            "binary": self.binary,
            "image": self.image,
            "detail": self.detail,
        }


@dataclass(frozen=True)
class ThemeReport:
    """One served theme: its palette and every pair measured on it."""

    name: str
    path: str
    base: str
    inherited: tuple[str, ...]
    results: tuple[PairResult, ...]
    error: str | None = None
    source: str = "file"
    offered: bool = True
    binary_surfaces: tuple[str, ...] = ()

    @property
    def ok(self) -> bool:
        return self.error is None and all(result.ok for result in self.results)

    @property
    def failures(self) -> tuple[PairResult, ...]:
        return tuple(result for result in self.results if not result.ok)

    @property
    def coverage(self) -> str:
        """`full` when every declared pair is measured, `partial` with binaries.

        A theme whose `images` block replaces a measured surface keeps passing
        (the role is still measured) but its coverage is partial: the painted
        pixel comes from a PNG the static check cannot read.
        """
        if self.error is not None:
            return "error"
        return "partial" if self.binary_surfaces else "full"

    def as_dict(self) -> dict[str, Any]:
        return {
            "theme": self.name,
            "path": self.path,
            "source": self.source,
            "offered": self.offered,
            "base": self.base,
            "inherited_roles": list(self.inherited),
            "binary_surfaces": list(self.binary_surfaces),
            "coverage": self.coverage,
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


def _negative(colour: str) -> str:
    """`negative()` of src/theme.c: lightness inverted, hue and saturation kept.

    Black maps to white and a colour whose highest and lowest channel already
    add up to 255 (the selection yellow, the pure VFO blue) does not move: that
    is the C arithmetic, and the caller keeps those roles at their Outdoor value.
    """
    value = int(colour.lstrip("#"), 16)
    r, g, b = (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF
    delta = 255 - max(r, g, b) - min(r, g, b)
    return hex_colour(((r + delta) & 0xFF) << 16 | ((g + delta) & 0xFF) << 8 | ((b + delta) & 0xFF))


def derive_outdoor_negative(theme_c: Path, outdoor: dict[str, str]) -> dict[str, str]:
    """The Outdoor-Negative palette `use_builtin(B_OUTDOOR_NEG)` builds.

    Every role goes through `negative()`; the roles the C then pins (`sel_text`,
    `btn_active_text`, `sel_border`, `btn_active_border`, `near_bg`, `focus_bg`,
    `vfo`) are read back from the same C — five keep their Outdoor value and two
    are literals — so this is a port, not a second source of truth.
    """
    text = theme_c.read_text(errors="surrogateescape")
    palette = {role: _negative(colour) for role, colour in outdoor.items()}
    from_outdoor = _NEG_FROM_OUTDOOR.findall(text)
    if not from_outdoor:
        raise ConfigError(
            f"{theme_c}: found no `builtin_colors[B_OUTDOOR]` override in use_builtin()"
        )
    for role, source in from_outdoor:
        palette[role] = outdoor[source]
    for role, value in _NEG_LITERAL.findall(text):
        palette[role] = hex_colour(int(value, 16))
    return palette


def derive_vision(theme_c: Path, iu1wxy: dict[str, str]) -> dict[str, str]:
    """The VISION palette `use_builtin(B_VISION)` builds: IU1WXY plus `vision_colors[]`."""
    text = theme_c.read_text(errors="surrogateescape")
    block = re.search(r"vision_colors\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if block is None:
        raise ConfigError(f"{theme_c}: no `vision_colors[]` array")
    overrides = {
        role: hex_colour(int(value, 16)) for role, value in _VISION_ENTRY.findall(block.group(1))
    }
    if not overrides:
        raise ConfigError(f"{theme_c}: `vision_colors[]` has no entries")
    return dict(iu1wxy) | overrides


def load_runtime_palettes(
    theme_c: Path, palettes: dict[str, dict[str, str]]
) -> dict[str, dict[str, str]]:
    """The palettes the user can see: the four data plus the two derives.

    Shipping a palette whose roles are *derived* from C is what closes the
    coverage hole: the static contract used to declare Outdoor-Negative and
    VISION UNCOVERED because re-deriving them looked like a second source of
    truth. Reading the derivation out of `src/theme.c` instead (and pinning it
    against `tests/check_wcag.py`) makes that hole measurable.
    """
    if not theme_c.is_file():
        raise ConfigError(f"missing the theme derivation: {theme_c}")
    extended = dict(palettes)
    extended["Outdoor-Negative"] = derive_outdoor_negative(theme_c, palettes["Outdoor"])
    extended["VISION"] = derive_vision(theme_c, palettes["IU1WXY"])
    return extended


@dataclass(frozen=True)
class Exemption:
    """A colour WCAG 1.4.3 exempts, with the reason it is dimmed on purpose."""

    role: str
    reason: str

    def as_dict(self) -> dict[str, Any]:
        return {"role": self.role, "reason": self.reason}


def load_exemptions(path: Path) -> tuple[Exemption, ...]:
    """The `exempt:` section of `quality/theme_pairs.yaml`.

    Disabled and incidental content is exempt from the contrast minimum (WCAG
    1.4.3): the dimmed button label, the muted text, the off rail. They are not
    pairs in this table (nothing has to reach 4.5:1), but they are not silence
    either: the list is data, and `tests/test_theme_files.py` pins it against the
    roles `tests/check_wcag.py` measures without failing, so the two cannot
    disagree on what is exempt.
    """
    data = read_yaml_mapping(path)
    raw = data.get("exempt", [])
    if not isinstance(raw, list):
        raise ConfigError(f"{path}: `exempt` must be a list")
    exemptions: list[Exemption] = []
    seen: set[str] = set()
    for entry in raw:
        if not isinstance(entry, dict):
            raise ConfigError(f"{path}: every exempt role must be a mapping")
        role = str(entry.get("role", "")).strip()
        reason = str(entry.get("reason", "")).strip()
        if not role:
            raise ConfigError(f"{path}: exempt entry without a `role`")
        if not reason:
            raise ConfigError(f"{path}: exempt role `{role}` needs a `reason`")
        if role in seen:
            raise ConfigError(f"{path}: exempt role `{role}` is listed twice")
        seen.add(role)
        exemptions.append(Exemption(role=role, reason=reason))
    return tuple(exemptions)


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


def served_theme_reports(
    directory: Path,
    theme_h: Path,
    palettes: dict[str, dict[str, str]],
    pairs: tuple[ThemePair, ...],
) -> tuple[ThemeReport, ...]:
    """Every theme the firmware serves, in `builtin_names[]` order then the files.

    The six built-in palettes first (the four data ones of `src/theme.h` plus the
    two `use_builtin()` derives), then the shipped files. Every served palette is
    measured, so there is no UNCOVERED theme left.
    """
    _validate_pairs(pairs, palettes)
    reports = [_measure_builtin(name, palettes[name], pairs, theme_h) for name in SERVED_BUILTINS]
    reports.extend(
        measure_theme_file(path, pairs, palettes) for path in shipped_theme_files(directory)
    )
    return tuple(reports)


def _measure_builtin(
    name: str,
    palette: dict[str, str],
    pairs: tuple[ThemePair, ...],
    theme_h: Path,
) -> ThemeReport:
    """One built-in palette: data from src/theme.h, or derived by `use_builtin()`."""
    origin = (
        "built-in palette src/theme.h THEME_COLORS"
        if name in BASE_PALETTES
        else "derived palette use_builtin() src/theme.c"
    )
    results = tuple(
        _pair_result(
            name,
            pair,
            ratio=contrast_ratio(palette[pair.fg], palette[pair.bg]),
            origin=origin,
        )
        for pair in pairs
    )
    return ThemeReport(
        name=name,
        path=str(theme_h),
        base=name,
        inherited=(),
        results=results,
        source="builtin",
        offered=BUILTIN_OFFERED[name],
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
        base, colours, images = _read_theme_file(path)
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
                f"base `{base}` is not one of {list(palettes)}: src/theme.c resolves a base "
                f"by `builtin_index()`, so a shipped file may only inherit a name the six "
                f"built-ins declare"
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
    results = tuple(_measure(name, palette, colours, base, pair, images) for pair in pairs)
    binary = tuple(sorted(surface for surface in images if surface in known))
    return ThemeReport(
        name=name,
        path=str(path),
        base=base,
        inherited=inherited,
        results=results,
        binary_surfaces=binary,
    )


def _pair_result(
    theme: str,
    pair: ThemePair,
    *,
    ratio: float,
    origin: str,
    image: str = "",
) -> PairResult:
    return PairResult(
        theme=theme,
        pair=pair,
        ratio=ratio,
        ok=ratio >= pair.min_ratio,
        origin=origin,
        binary=bool(image),
        image=image,
    )


def _measure(
    name: str,
    palette: dict[str, str],
    colours: dict[str, str],
    base: str,
    pair: ThemePair,
    images: dict[str, str],
) -> PairResult:
    ratio = contrast_ratio(palette[pair.fg], palette[pair.bg])
    from_base = [role for role in (pair.fg, pair.bg) if role not in colours]
    origin = (
        "both roles set by the file"
        if not from_base
        else f"inherited from base {base}: {', '.join(from_base)}"
    )
    return _pair_result(name, pair, ratio=ratio, origin=origin, image=images.get(pair.bg, ""))


def _read_theme_file(path: Path) -> tuple[str, dict[str, str], dict[str, str]]:
    """`(base, colours the file sets, images it ships)`, with src/theme.c's schema."""
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
    return base, colours, _read_images(data.get("images"), path)


def _read_images(raw: Any, path: Path) -> dict[str, str]:
    """`surface -> png file` of the `images` block, for the surfaces src/theme.c paints.

    Only the names `image_names[]` knows count: an unknown key is ignored by
    src/theme.c too. A malformed entry does not fail the colour check — the image
    never changes a role's value.
    """
    if not isinstance(raw, dict):
        return {}
    images: dict[str, str] = {}
    for surface, entry in raw.items():
        if surface not in IMAGE_SURFACES or not isinstance(entry, dict):
            continue
        file = entry.get("file")
        if isinstance(file, str) and file:
            images[str(surface)] = file
    return images
