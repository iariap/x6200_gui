#!/usr/bin/env python3
"""The X6200 font pipeline: libre candidate TTFs -> LVGL 8 C families.

Every committed `src/fonts/*.c` is generated, not hand-edited. This is the script
that does it, the way the arial family documented in its own header
(`tools/font_gen.py`) and the roboto/dejavu/sony families (`lv_font_conv`)
were produced before it:

    tools/fonts/fonts.py fetch      # download the pinned sources into .cache/
    tools/fonts/fonts.py plan       # per-candidate size map against Sony's metrics
    tools/fonts/fonts.py generate   # write src/fonts/<family>_<slot>.c
    tools/fonts/fonts.py verify     # check what landed against the targets

Sub-commands:

* `fetch` downloads every candidate and the icon font named in `candidates.json`
  into `tools/fonts/.cache/` (gitignored: the repository keeps the generated C and
  the licence trace, never the font binaries) and verifies each sha256 against the
  manifest. A hash mismatch is a hard failure: the generated families are only
  reproducible if the input bytes are the ones the manifest pins.
* `plan` computes, per candidate and per slot, the pixel size whose rendered
  metrics are closest to the ones the family it replaces has. The targets are in
  `sony-targets.json`, captured from the proprietary family's committed C before
  the migration (see the README): the metric that decides is `line_height` (the
  value LVGL lays a label out with), tie-broken by the sum of the ASCII advance
  widths (how wide a string renders). Matching the ink size instead of the nominal
  size is what keeps the layout from overflowing: the old `sony_N` renders its
  glyphs much smaller than its name suggests (Sony's own line height is ~0.85 of
  the nominal size).
* `generate` runs `lv_font_conv` (the tool every committed family came from, pinned
  to the same version) once per slot, with the icon font merged on the icon slots.
  The ROM's own family slot is generated as `sony_<slot>.c` so every `&sony_N` in
  `src/` keeps working; the candidate families are generated as
  `<c_name>_<slot>.c`.
* `verify` re-reads the generated files and reports, per slot, the line height they
  actually got next to the target: the size map is a plan, the generated file is
  the fact.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0
from PIL import ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
MANIFEST_PATH = HERE / "candidates.json"
TARGETS_PATH = HERE / "sony-targets.json"
CACHE = HERE / ".cache"
SRC_FONTS = ROOT / "src" / "fonts"

LV_FONT_CONV_VERSION = "1.5.3"
ASCII = "".join(chr(code) for code in range(32, 127))

# The size range the metric probe covers: below 5 px the raster is degenerate,
# above 70 px nothing in this GUI renders.
PROBE_FROM, PROBE_TO = 5, 70

# The comment block the stock family carries around its declaration: with LVGL 8
# lv_font_conv emits `const lv_font_t`, and fonts_apply() copies an alternate
# family into the struct (see src/fonts/font_select.c). A const struct lands in
# .rodata and that write killed the GUI, so the ROM's own slot is forced
# non-const. This is the same edit the committed family carried by hand.
STOCK_FORCE = """#if 0   /* forzado: {symbol} tiene que quedar en RAM; fonts_apply() copia la fuente
         * alternativa dentro de la struct (con LVGL 8 salia const, en .rodata, y
         * la escritura mataba la GUI). Lo emite tools/fonts/fonts.py */
const lv_font_t {symbol} = {{
#else
lv_font_t {symbol} = {{
#endif"""


class Fail(Exception):
    """A pipeline step that cannot continue."""


def load_manifest() -> dict:
    return json.loads(MANIFEST_PATH.read_text())


def candidate_by_name(manifest: dict, name: str) -> dict:
    for candidate in manifest["candidates"]:
        if candidate["name"] == name:
            return candidate
    raise Fail(f"unknown candidate {name!r} (see tools/fonts/candidates.json)")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    print(f"  fetching {url}")
    with urllib.request.urlopen(url, timeout=120) as response:
        destination.write_bytes(response.read())


# --- fetch ------------------------------------------------------------------


def fetch_source(entry: dict, name: str) -> Path:
    """Downloads one manifest entry into the cache and returns the TTF path."""
    target = CACHE / entry["file"]
    if not target.is_file():
        # An archive stays in the cache (as `<name>.archive`): the licence text of
        # those families lives inside it, and `fetch --licenses` reads it from here
        # instead of downloading the release again.
        raw = CACHE / (f"{name}.archive" if entry.get("archive_member") else f"{name}.download")
        download(entry["url"], raw)
        if entry.get("archive_member"):
            with zipfile.ZipFile(raw) as archive:
                target.write_bytes(archive.read(entry["archive_member"]))
        else:
            raw.rename(target)
    got = sha256(target)
    if got != entry["sha256"]:
        raise Fail(
            f"{name}: sha256 mismatch for {target}\n  expected {entry['sha256']}\n  got      {got}"
        )
    return target


def cmd_fetch(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    CACHE.mkdir(parents=True, exist_ok=True)

    print("sources:")
    for entry in [manifest["icon_font"], *manifest["candidates"]]:
        path = fetch_source(entry, entry["name"])
        print(f"  ok {entry['name']:16} {entry['sha256'][:12]}… {path.name}")

    # Runnable lv_font_conv, pinned to the version every committed family was
    # generated with. Installed into the cache (never the repository) so the
    # per-size invocations do not pay npx's resolution on each call.
    module = CACHE / "npm" / "node_modules" / "lv_font_conv" / "lv_font_conv.js"
    if not module.is_file():
        npm = shutil.which("npm")
        if npm is None:
            raise Fail("npm is needed once to install lv_font_conv into tools/fonts/.cache")
        print(f"installing lv_font_conv@{LV_FONT_CONV_VERSION} into {CACHE / 'npm'}")
        subprocess.run(
            [npm, "install", "--silent", f"lv_font_conv@{LV_FONT_CONV_VERSION}",
             "--prefix", str(CACHE / "npm")],
            check=True,
        )

    if args.licenses:
        (SRC_FONTS / "licenses").mkdir(parents=True, exist_ok=True)
        base = manifest["google_fonts_commit"]
        for entry in [manifest["icon_font"], *manifest["candidates"]]:
            rel = entry.get("license_file")
            member = entry.get("license_member")
            if not rel and not member:
                continue
            out = SRC_FONTS / "licenses" / f"{entry['name']}.txt"
            if rel:
                download(f"https://raw.githubusercontent.com/google/fonts/{base}/{rel}", out)
            else:
                archive_path = CACHE / f"{entry['name']}.archive"
                if not archive_path.is_file():
                    # Cached before the archive itself was kept: get it again rather
                    # than ship a family without its licence text.
                    download(entry["url"], archive_path)
                with zipfile.ZipFile(archive_path) as archive:
                    out.write_bytes(archive.read(member))
        print("licence texts written to src/fonts/licenses/")
    return 0


# --- metrics ----------------------------------------------------------------


def lv_font_conv_path() -> Path:
    module = CACHE / "npm" / "node_modules" / "lv_font_conv" / "lv_font_conv.js"
    if not module.is_file():
        raise Fail("lv_font_conv is not installed: run `tools/fonts/fonts.py fetch` first")
    return module


def lvfc(spec: dict) -> str:
    """Runs tools/fonts/lvfc.js (one lv_font_conv process for the whole job)."""
    spec_path = CACHE / "lvfc-spec.json"
    spec_path.write_text(json.dumps(spec))
    module_dir = lv_font_conv_path().parent
    result = subprocess.run(
        ["node", str(HERE / "lvfc.js"), str(module_dir), str(spec_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise Fail(f"lv_font_conv failed: {result.stderr.strip()}")
    return result.stdout


def font_entries(ttf: Path, icons: bool) -> list[dict]:
    entries = [{"path": ttf.name, "source": str(kern_source(ttf)), "symbols": ASCII}]
    if icons:
        icon = load_manifest()["icon_font"]
        codes = ",".join(str(code) for code in load_manifest()["icon_codepoints"])
        entries.append({"path": icon["file"], "source": str(CACHE / icon["file"]), "ranges": [codes]})
    return entries


# --- kerning: GPOS -> the legacy `kern` table -------------------------------


# `lv_font_conv` 1.5.3 reads kerning through the `getKerningValue()` of the
# opentype.js 1.3.4 it bundles, and that reader has two paths:
#
#   1. GPOS: the lookups the `kern` feature of the *default script* (`DFLT`, else
#      `latn`) points at, **only** the ones whose lookup type is 2 (pair
#      adjustment). `Position.init()` stores them in `defaultKerningTables`.
#   2. the legacy `kern` table (`font.kerningPairs`), reached only when
#      `defaultKerningTables` is *falsy*.
#
# The second path is dead for every font that has a GPOS table: `getLookupTables()`
# returns `[]` when no type-2 lookup matches, and an empty array is truthy, so
# `getKerningValue()` returns `undefined` for every pair instead of falling back.
# A font whose kerning is emitted in *extension* lookups (type 9 wrapping type 2,
# what recent releases of these families do) therefore renders with no kerning at
# all, and no `kern` table it carries would be read either (measured: Fira Sans,
# Lato, Open Sans, Source Sans 3 and JetBrains Mono -> 0 pairs; Roboto, Archivo,
# PT Sans and Ubuntu, whose kerning is in direct type-2 lookups -> non-zero).
#
# So the pipeline hands the reader a converted copy of the font: the GPOS pair
# kerning written as the legacy `kern` table (format 0, the pair table the tool's
# writer reads -- `lib/font/table_kern.js`) and the GPOS table dropped, because
# its mere presence is what keeps the fallback unreachable. Only the fonts on the
# dead path are converted: a family the reader already kerns is left untouched, so
# its generated C stays byte-identical.

KERN_CACHE = CACHE / "kern"

# `path -> converted pair count` (0 = the font was left untouched), filled by
# kern_source() so cmd_generate can annotate what happened.
_KERNED: dict[str, int] = {}


def kerned_pairs(ttf: Path) -> int:
    """The pairs kern_source() converted for `ttf` (0 = untouched)."""
    return _KERNED.get(str(ttf), 0)


def _kern_lookups(gpos: object) -> tuple[list[int], list[int]]:
    """`(direct, extension)` lookup indexes of the `kern` feature the reader uses.

    `direct` is what the reader follows (type 2); `extension` is the same kerning
    hidden behind a type-9 extension lookup, the one it cannot follow. Both are
    empty when the font has no `kern` feature for the default script, which is the
    case in which the reader gets no kerning from GPOS either.
    """
    scripts = getattr(getattr(gpos, "ScriptList", None), "ScriptRecord", None)
    features = getattr(getattr(gpos, "FeatureList", None), "FeatureRecord", None)
    lookups = getattr(getattr(gpos, "LookupList", None), "Lookup", None)
    if not scripts or features is None or lookups is None:
        return [], []
    tags = {record.ScriptTag for record in scripts}
    script = "DFLT" if "DFLT" in tags else "latn" if "latn" in tags else None
    if script is None:
        return [], []
    record = next(item for item in scripts if item.ScriptTag == script)
    langsys = record.Script.DefaultLangSys
    if langsys is None:
        return [], []
    indexes = list(langsys.FeatureIndex)
    if langsys.ReqFeatureIndex != 0xFFFF:
        indexes.append(langsys.ReqFeatureIndex)
    direct: list[int] = []
    extension: list[int] = []
    for index in indexes:
        record = features[index]
        if record.FeatureTag != "kern":
            continue
        for lookup_index in record.Feature.LookupListIndex:
            lookup = lookups[lookup_index]
            if lookup.LookupType == 2:
                direct.append(lookup_index)
            elif lookup.LookupType == 9 and any(
                sub.ExtensionLookupType == 2 for sub in lookup.SubTable
            ):
                extension.append(lookup_index)
    return direct, extension


def _pair_values(sub: object, glyphs: set[str]):
    """`((left, right), xAdvance)` of one PairPos subtable, ASCII glyphs only.

    Format 1 lists the pairs explicitly; format 2 lists them as classes, so the
    right side is looked up in the class definition (class 0 is a real class: a
    non-zero entry there kerns every glyph of that class, and `getKerningValue()`
    applies it, so it is not skipped here either). A subtable whose value format
    carries no `XAdvance` (bit 3) kerns nothing horizontally.
    """
    if getattr(sub, "Format", None) == 1:
        # `strict=False`: a font whose PairSetCount is shorter than its Coverage
        # (invalid, but seen in the wild) must not take the whole pipeline down.
        for left, pair_set in zip(sub.Coverage.glyphs, sub.PairSet, strict=False):
            if left not in glyphs:
                continue
            for pair in pair_set.PairValueRecord:
                right = pair.SecondGlyph
                if right not in glyphs:
                    continue
                value = getattr(getattr(pair, "Value1", None), "XAdvance", 0)
                if value:
                    yield (left, right), value
    elif getattr(sub, "Format", None) == 2:
        if not getattr(sub, "ValueFormat1", 0) & 0x0004:
            return
        left_classes = sub.ClassDef1.classDefs if sub.ClassDef1 is not None else {}
        right_classes = sub.ClassDef2.classDefs if sub.ClassDef2 is not None else {}
        for left in sub.Coverage.glyphs:
            if left not in glyphs:
                continue
            records = sub.Class1Record[left_classes.get(left, 0)].Class2Record
            for right in glyphs:
                rc = right_classes.get(right, 0)
                if rc >= len(records):
                    continue
                value = getattr(getattr(records[rc], "Value1", None), "XAdvance", 0)
                if value:
                    yield (left, right), value


def kern_pairs(font: TTFont) -> dict[tuple[str, str], int]:
    """The pair kerning of `font`'s GPOS, `{(left, right): xAdvance}` in font units.

    ASCII glyphs only: they are the glyph set the pipeline ships, and lv_font_conv
    reads a pair only when both glyphs are in the subset anyway. On a duplicate
    pair the first subtable wins, which is what `getKerningValue()` does (it
    returns at the first hit instead of adding the tables up).
    """
    if "GPOS" not in font:
        return {}
    direct, extension = _kern_lookups(font["GPOS"].table)
    cmap = font.getBestCmap()
    glyphs = {cmap[code] for code in range(32, 127) if code in cmap}
    pairs: dict[tuple[str, str], int] = {}
    lookups = font["GPOS"].table.LookupList.Lookup
    for index in direct + extension:
        for sub in lookups[index].SubTable:
            subtable = sub.ExtSubTable if lookups[index].LookupType == 9 else sub
            for key, value in _pair_values(subtable, glyphs):
                pairs.setdefault(key, value)
    return pairs


def kern_source(ttf: Path) -> Path:
    """The file to hand the reader so `ttf`'s kerning reaches the generated C.

    Returns `ttf` itself when there is nothing to convert: a font with no pair
    kerning in GPOS, or one whose `kern` feature already lists a direct type-2
    lookup (the path the reader does follow -- converting it would change nothing
    and would rewrite a file that is currently reproducible byte for byte).
    Otherwise it writes `.cache/kern/<name>`: the same font with the kerning in a
    legacy `kern` table and no GPOS, and returns that path. `recalcTimestamp` is
    off, so the converted file is a function of the input (`fetch`'s sha256) and
    the run stays reproducible.
    """
    font = TTFont(ttf, recalcTimestamp=False)
    direct, extension = _kern_lookups(font["GPOS"].table) if "GPOS" in font else ([], [])
    if direct or not extension:
        _KERNED[str(ttf)] = 0
        return ttf
    pairs = kern_pairs(font)
    assert pairs, f"{ttf.name}: the kerning lookups yielded no ASCII pair"
    kern = newTable("kern")
    kern.version = 0
    subtable = KernTable_format_0()
    subtable.version = 0
    subtable.coverage = 1
    subtable.format = 0
    subtable.kernTable = pairs
    kern.kernTables = [subtable]
    font["kern"] = kern
    del font["GPOS"]
    target = KERN_CACHE / ttf.name
    target.parent.mkdir(parents=True, exist_ok=True)
    font.save(target)
    _KERNED[str(ttf)] = len(pairs)
    return target


_PROBES: dict[tuple[str, bool, str], dict[int, tuple[int, int]]] = {}


def probe_metrics(ttf: Path, icons: bool, symbol: str) -> dict[int, tuple[int, int]]:
    """`{size: (line_height, ink of `symbol`)}` as lv_font_conv renders the subset.

    `line_height` is what LVGL lays a one-line label out with, and it is not a
    property of the font but of the rendered glyph set (lib/collect_font_data.js:
    `ascent - descent` over the subset's ink), so it has to come from the tool that
    writes the file. Approximating it from the font tables (hhea, Pillow's
    getmetrics) is off by up to 3 px on some of these families, which is the
    difference between fitting a cell and clipping it. The second number is the ink
    height of one glyph, which is the metric the segmented digit families match (a
    segment digit is nearly as tall as its line height; the proportional families
    draw much smaller ink than their line height).
    """
    key = (str(ttf), icons, symbol)
    if key not in _PROBES:
        output = lvfc(
            {
                "mode": "probe",
                "fonts": font_entries(ttf, icons),
                "from": PROBE_FROM,
                "to": PROBE_TO,
                "probe_symbol": symbol,
                "probe_output": "probe.c",
            }
        )
        _PROBES[key] = {
            int(parts[0]): (int(parts[1]), int(parts[3]))
            for parts in (line.split() for line in output.splitlines())
        }
    return _PROBES[key]


def advance_sums(ttf: Path) -> dict[int, int]:
    """`{size: sum of the ASCII advances}` (px), the width half of the metric.

    Advances come from the same FreeType library lv_font_conv uses (Pillow's
    `getlength`); only the integer line height needs the exact tool probe.
    """
    sums: dict[int, int] = {}
    for size in range(PROBE_FROM, PROBE_TO + 1):
        font = ImageFont.truetype(str(ttf), size)
        sums[size] = round(sum(font.getlength(chr(code)) for code in range(32, 127)))
    return sums


def sony_targets_from_source() -> dict[str, dict[str, int]]:
    """Reads the metrics out of the committed `sony_*.c` (the pre-migration files)."""
    targets: dict[str, dict[str, int]] = {}
    for path in sorted(SRC_FONTS.glob("sony_*.c")):
        slot = int(re.search(r"sony_(\d+)", path.name).group(1))
        text = path.read_text()
        descriptor = re.search(r"glyph_dsc\[\] = \{(.*?)\n\};", text, re.S)
        if descriptor is None:
            raise Fail(f"{path.name}: no glyph_dsc")
        # glyph id 1 is ASCII 32, so the slice is the printable range.
        advances = [int(value) for value in re.findall(r"\.adv_w = (\d+)", descriptor.group(1))]
        targets[str(slot)] = {
            "line_height": int(re.search(r"\.line_height = (\d+)", text).group(1)),
            "base_line": int(re.search(r"\.base_line = (-?\d+)", text).group(1)),
            # adv_w is 1/16 px (LVGL): the sum over the ASCII range is how wide a
            # string renders, the second half of "closest to the family it replaces".
            "advance_sum": round(sum(advances[1:96]) / 16),
        }
    if not targets:
        raise Fail(f"no sony_*.c under {SRC_FONTS}")
    return targets


def load_targets() -> dict[str, dict[str, int]]:
    if not TARGETS_PATH.is_file():
        raise Fail(f"{TARGETS_PATH} is missing (see the README: captured before the migration)")
    return json.loads(TARGETS_PATH.read_text())


def write_targets() -> int:
    targets = sony_targets_from_source()
    TARGETS_PATH.write_text(json.dumps(targets, indent=2, sort_keys=True) + "\n")
    print(f"wrote {TARGETS_PATH.relative_to(ROOT)} from the committed src/fonts/sony_*.c")
    for slot, values in targets.items():
        print(f"  slot {slot:>2}: line_height {values['line_height']:>2} advance_sum {values['advance_sum']}")
    return 0


# --- plan -------------------------------------------------------------------


# The glyph the ink probe reads: the digit of a segmented family, a capital of a
# proportional one (their ink/lines differ by design).
PROBE_SYMBOL = {"line_height": "A", "ink": "8"}


def pick_size(
    metrics: dict[int, tuple[int, int]],
    advances: dict[int, int],
    target: dict[str, int],
    match: str,
) -> tuple[int, int, int, int]:
    """The size whose metrics are closest to `target`, and what it renders.

    Primary criterion: the smallest distance on the metric the family has to
    respect — `line_height` (the value LVGL fits a one-line label with, so the one
    that can clip) or, for the segmented digits, the ink of the digit (a segment
    digit is nearly as tall as its line height, so matching the line height would
    overflow the frequency box). Tie-break, both modes: the smallest relative
    |advance_sum - target|, the width that can overflow a fixed cell. Deterministic:
    same TTF, same size.
    """
    wanted = target["line_height"] if match == "line_height" else target["cap_height"]
    best: tuple[tuple[int, float], int] | None = None
    for size, (line_height, ink) in metrics.items():
        got = line_height if match == "line_height" else ink
        ratio = abs(advances[size] / target["advance_sum"] - 1)
        key = (abs(got - wanted), ratio)
        if best is None or key < best[0]:
            best = (key, size)
    assert best is not None
    size = best[1]
    line_height, ink = metrics[size]
    return size, line_height, ink, advances[size]


def plan_for(candidate: dict, icons: bool) -> dict[int, dict[str, int | float]]:
    """The size map for one family. `icons` is whether the family merges the icon
    font: the merged subset has a different ink extent, so the icon slots of the
    stock family size differently from the ASCII-only alternates."""
    manifest = load_manifest()
    targets = load_targets()
    ttf = CACHE / candidate["file"]
    if not ttf.is_file():
        raise Fail(f"{candidate['name']}: {ttf} is not cached (run `fonts.py fetch`)")
    match = candidate.get("match", "line_height")
    symbol = PROBE_SYMBOL[match]
    plain = probe_metrics(ttf, icons=False, symbol=symbol)
    with_icons = probe_metrics(ttf, icons=True, symbol=symbol)
    advances = advance_sums(ttf)
    slots = [int(slot) for slot in manifest["segmented_slots"]] if candidate.get("segmented") \
        else [int(slot) for slot in targets]
    icon_slots = set(manifest["icon_slots"]) if icons else set()
    rows: dict[int, dict[str, int | float]] = {}
    for slot in slots:
        target = targets[str(slot)]
        size, line_height, ink, advance = pick_size(
            with_icons if slot in icon_slots else plain, advances, target, match
        )
        rows[slot] = {
            "size": size,
            "line_height": line_height,
            "ink": ink,
            "target_line_height": target["line_height"],
            "target_cap_height": target["cap_height"],
            "match": match,
            "advance_ratio": round(advance / target["advance_sum"], 3),
        }
    return rows


def cmd_plan(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    names = args.candidate or [entry["name"] for entry in manifest["candidates"]]
    header = "| slot | " + " | ".join(f"{candidate_by_name(manifest, n)['c_name']}" for n in names) + " |"
    print(header)
    print("|---" * (len(names) + 1) + "|")
    plans = {
        name: plan_for(candidate_by_name(manifest, name), icons=False) for name in names
    }
    slots = sorted(next(iter(plans.values())))
    for slot in slots:
        cells = []
        for name in names:
            row = plans[name][slot]
            mark = "" if row["line_height"] == row["target_line_height"] else "!"
            cells.append(f"{row['size']}{mark} ({row['advance_ratio']:.2f})")
        print(f"| {slot} | " + " | ".join(cells) + " |")
    print("\ncells: `size (advance ratio)`; `!` marks a line height that could not be hit exactly")
    return 0


# --- generate ---------------------------------------------------------------


def digit_order(manifest: dict) -> list[dict]:
    """The segmented (digit) families, in FONT_DIGIT_* order (index 0 is "none")."""
    return [entry for entry in manifest["candidates"] if entry.get("segmented")]


def family_order(manifest: dict) -> list[dict]:
    """The families the ROM compiles, in FONT_* order.

    `FONT_STOCK` (the ROM's own `sony_*.c`) first, then Roboto and DejaVu (the two
    the settings dropdown has always offered, so an existing `params.font` value
    keeps pointing at the same family), then the rest of the manifest. The order is
    data, not prose: it is what `font_families.h` and the dropdown are built from.
    """
    stock = candidate_by_name(manifest, manifest["stock"]) if manifest["stock"] else None
    candidates = [
        entry
        for entry in manifest["candidates"]
        if entry is not stock and not entry.get("segmented")
    ]
    first = ["roboto", "dejavusans"]
    head = [entry for name in first for entry in candidates if entry["name"] == name]
    tail = [entry for entry in candidates if entry["name"] not in first]
    return ([stock] if stock else []) + head + tail


def write_header(manifest: dict) -> None:
    """Writes the generated `src/fonts/font_families.{h,c}`: the family list, as code.

    The settings dropdown's item order, the `params.font` value, the theme JSON's
    `font` id and the compiled `<c_name>_<slot>` symbols are four views of one
    list. Hand-writing them is what made "Roboto" apply DejaVu once (src/main.c
    documents the off-by-one), so the list is generated: a new candidate cannot
    land compiled-but-unnamed, and a name cannot be offered without a family.
    """
    entries = family_order(manifest)
    sizes = manifest["alternate_slots"]
    stock_sizes = manifest["stock_slots"]  # only the tiny slots the alternates do not have

    def pointers(prefix: str, slots: list[int]) -> str:
        return ", ".join(f"&{prefix}_{size}" for size in slots)

    lines = [
        "/* Generated by tools/fonts/fonts.py - do not edit by hand.",
        " *",
        " * The families the ROM can draw with. FONT_STOCK is the ROM's own slot",
        " * (src/fonts/sony_*.c): since issue #9 / decision 1 it is generated from a",
        " * libre font instead of the proprietary Sony one, and it is the default. Every",
        " * other entry is a candidate family from src/fonts/ (see tools/fonts/README.md",
        " * and docs/THIRD_PARTY.md).",
        " *",
        " * SPDX-License-Identifier: LGPL-2.1-or-later */",
        "",
        "#pragma once",
        "",
        '#include "lvgl/lvgl.h"',
        "",
        "/* The slots the alternate families carry (the stock family has the tiny sizes",
        " * the VISION scale walks down to as well). */",
        "#define FONT_SIZES(X) " + " ".join(f"X({size})" for size in sizes),
        "#define FONT_SIZES_N " + str(len(sizes)),
        "",
        "/* The stock family is the one fonts_apply() copies over, so it is not const.",
        " * Declared over every stock slot (the alternate families carry the 17 of",
        " * FONT_SIZES; the stock family also has the three tiny sizes font_down()",
        " * walks down to). `struct _lv_font_t` (what lv_font_t is a typedef of) keeps",
        " * this header includable where LVGL is stubbed (the host tests). */",
        "#define FONT_DECLARE_STOCK(n) extern struct _lv_font_t sony_##n;",
        "#define FONT_STOCK_SLOTS(X) " + " ".join(f"X({slot})" for slot in stock_sizes),
        "FONT_STOCK_SLOTS(FONT_DECLARE_STOCK)",
        "",
    ]
    for entry in entries[1:]:
        macro = f"FONT_DECLARE_{entry['c_name'].upper()}(n)"
        lines.append(
            f"#define {macro} extern const struct _lv_font_t {entry['c_name']}_##n;"
        )
    lines.append("")
    for entry in entries[1:]:
        lines.append(f"FONT_SIZES(FONT_DECLARE_{entry['c_name'].upper()})")
    lines += [
        "",
        "enum {",
        "    FONT_STOCK = 0,   /* the ROM's own family, src/fonts/sony_*.c */",
    ]
    for index, entry in enumerate(entries[1:], start=1):
        lines.append(f"    FONT_{entry['c_name'].upper()} = {index},   /* {entry['display']} */")
    lines += [
        "    FONT_COUNT",
        "};",
        "",
        "/* Index == the FONT_* value == the `params.font` value == the dropdown row.",
        " * `font_ids` is what a theme JSON writes in its `font` key. */",
        "extern const char *const font_ids[FONT_COUNT];",
        "extern const char *const font_names[FONT_COUNT];",
        "",
        "/* Row 0 is the stock slot itself (fonts_apply() never copies over it, but the",
        " * table keeps one row per FONT_* value so no lookup can be off by one). */",
        "extern struct _lv_font_t *const font_stock[FONT_SIZES_N];",
        "extern const struct _lv_font_t *const font_families[FONT_COUNT][FONT_SIZES_N];",
        "",
    ]
    (SRC_FONTS / "font_families.h").write_text("\n".join(lines))

    source = [
        "/* Generated by tools/fonts/fonts.py - do not edit by hand (font_families.h).",
        " *",
        " * SPDX-License-Identifier: LGPL-2.1-or-later */",
        "",
        '#include "font_families.h"',
        "",
        # Same 17 slots as the alternates: the copy in fonts_apply() is indexed by
        # slot, and the three extra tiny sizes (8/10/12) exist only in the stock
        # family (the VISION scale walks down to them).
        f"lv_font_t *const font_stock[FONT_SIZES_N] = {{ {pointers('sony', sizes)} }};",
        "",
        "const lv_font_t *const font_families[FONT_COUNT][FONT_SIZES_N] = {",
        f"    {{ {pointers('sony', sizes)} }},",
    ]
    for entry in entries[1:]:
        source.append(f"    /* FONT_{entry['c_name'].upper()}: {entry['display']} */")
        source.append(f"    {{ {pointers(entry['c_name'], sizes)} }},")
    source += [
        "};",
        "",
        "const char *const font_ids[FONT_COUNT] = {",
        '    "stock",',
    ]
    for entry in entries[1:]:
        source.append(f'    "{entry["c_name"]}",')
    source += [
        "};",
        "",
        "const char *const font_names[FONT_COUNT] = {",
        f'    "{entries[0]["display"]}",   /* FONT_STOCK: the ROM\'s own family */',
    ]
    for entry in entries[1:]:
        source.append(f'    "{entry["display"]}",')
    source += ["};", ""]

    digits = digit_order(manifest)
    slots = manifest["segmented_slots"]
    lines += [
        "/* The segmented-digit families (tools/fonts/README.md): seven-segment LCD",
        " * faces for the frequency row and the clock, never for the letters. They are",
        " * compiled for the slots of `segmented_slots` only and each of them falls back",
        " * to the stock family of the same slot for every glyph it does not carry. */",
        "#define FONT_SEGMENTED_SLOTS(X) " + " ".join(f"X({slot})" for slot in slots),
        "",
    ]
    for entry in digits:
        macro = f"FONT_DECLARE_{entry['c_name'].upper()}(n)"
        lines.append(
            f"#define {macro} extern const struct _lv_font_t {entry['c_name']}_##n;"
        )
    lines.append("")
    for entry in digits:
        lines.append(f"FONT_SEGMENTED_SLOTS(FONT_DECLARE_{entry['c_name'].upper()})")
    lines += [
        "",
        "enum {",
        "    FONT_DIGIT_NONE = 0,   /* the digit panels draw with the default family */",
    ]
    for index, entry in enumerate(digits, start=1):
        lines.append(f"    FONT_DIGIT_{entry['c_name'].upper()} = {index},   /* {entry['display']} */")
    lines += [
        "    FONT_DIGIT_COUNT",
        "};",
        "",
        "extern const char *const font_digit_ids[FONT_DIGIT_COUNT];",
        "extern const char *const font_digit_names[FONT_DIGIT_COUNT];",
        "",
        "/* Indexed by FONT_SIZES_N (the alternate slots): a slot the family does not",
        " * carry points at the stock slot, so `font_digit()` is a no-op there. */",
        "extern const struct _lv_font_t *const font_digit_families"
        "[FONT_DIGIT_COUNT][FONT_SIZES_N];",
        "",
    ]
    (SRC_FONTS / "font_families.h").write_text("\n".join(lines))

    def digit_row(entry: dict | None) -> str:
        return ", ".join(
            f"&{entry['c_name']}_{slot}" if entry and slot in slots else f"&sony_{slot}"
            for slot in sizes
        )

    source += [
        "const char *const font_digit_ids[FONT_DIGIT_COUNT] = {",
        '    "none",',
    ]
    for entry in digits:
        source.append(f'    "{entry["c_name"]}",')
    source += [
        "};",
        "",
        "const char *const font_digit_names[FONT_DIGIT_COUNT] = {",
        '    "Default",',
    ]
    for entry in digits:
        source.append(f'    "{entry["display"]}",')
    source += [
        "};",
        "",
        "const lv_font_t *const font_digit_families[FONT_DIGIT_COUNT][FONT_SIZES_N] = {",
        f"    {{ {digit_row(None)} }},",
    ]
    for entry in digits:
        source.append(f"    /* FONT_DIGIT_{entry['c_name'].upper()}: {entry['display']} */")
        source.append(f"    {{ {digit_row(entry)} }},")
    source += ["};", ""]
    (SRC_FONTS / "font_families.c").write_text("\n".join(source))

    # The families by name, for the harness inspector's JSON (`text_font`): the
    # inspector used to carry its own copy of the stock slot list, which would drift
    # from this one the moment a family is added.
    names = [
        "/* Generated by tools/fonts/fonts.py - do not edit by hand.",
        " *",
        " * Every font symbol by name, for the harness inspector (ui_inspector.c): one",
        " * `static` table, included by the single translation unit that needs it.",
        " *",
        " * SPDX-License-Identifier: LGPL-2.1-or-later */",
        "",
        "/* Brings the declarations of every symbol below (and, through it, the",
        " * FONT_SIZES/FONT_SEGMENTED_SLOTS slot lists). Needs lvgl.h first: the",
        " * includer provides it (ui_inspector.c). */",
        '#include "font_families.h"',
        "",
        "static const struct {",
        "    const lv_font_t *font;",
        "    const char      *name;",
        "} FONT_NAMES[] = {",
    ]
    for slot in stock_sizes:
        names.append(f'    {{ &sony_{slot}, "sony_{slot}" }},')
    for entry in entries[1:]:
        for slot in sizes:
            names.append(f'    {{ &{entry["c_name"]}_{slot}, "{entry["c_name"]}_{slot}" }},')
    for entry in digits:
        for slot in slots:
            names.append(f'    {{ &{entry["c_name"]}_{slot}, "{entry["c_name"]}_{slot}" }},')
    names += ["};", ""]
    (SRC_FONTS / "font_names.h").write_text("\n".join(names))
    print(f"wrote font_families.h/.c and font_names.h: {len(entries)} families, {len(digits)} digits")


def cmd_generate(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    stock_name = args.stock or manifest["stock"]
    names = args.candidate or [entry["name"] for entry in manifest["candidates"]]
    if stock_name and stock_name not in names:
        names.append(stock_name)

    stock_entry = candidate_by_name(manifest, stock_name) if stock_name else None

    for name in names:
        entry = candidate_by_name(manifest, name)
        is_stock = stock_entry is not None and entry["name"] == stock_entry["name"]
        segmented = bool(entry.get("segmented"))
        icon_slots = {int(slot) for slot in manifest["icon_slots"]} if is_stock and not segmented else set()
        plan = plan_for(entry, icons=bool(icon_slots))
        prefix = "sony" if is_stock else entry["c_name"]
        slots = sorted(plan)

        plain = font_entries(CACHE / entry["file"], icons=False)
        with_icons = font_entries(CACHE / entry["file"], icons=True)

        # One spec per (icon set, LVGL fallback) group: the merged icon set changes
        # the rendered subset, and a segmented digit family declares the stock
        # family of the same slot as its fallback (it is used as a style over two
        # labels, so every glyph it lacks -- the letters of the voltage line, the
        # icons -- has to come from the default family).
        specs: dict[tuple[bool, str | None], list[tuple[int, dict]]] = {}
        for slot in slots:
            job = {
                "size": plan[slot]["size"],
                "output": str(SRC_FONTS / f"{prefix}_{slot}.c"),
            }
            fallback = f"sony_{slot}" if segmented else None
            specs.setdefault((slot in icon_slots, fallback), []).append((slot, job))

        for (use_icons, fallback), group in specs.items():
            spec = {
                "mode": "generate",
                "fonts": with_icons if use_icons else plain,
                "jobs": [job for _slot, job in group],
            }
            if fallback:
                spec["lv_fallback"] = fallback
            lvfc(spec)

        print(f"{entry['display']}: {'stock (sony_*)' if is_stock else entry['c_name']}")
        kerned = kerned_pairs(CACHE / entry["file"])
        if kerned:
            print(f"  kerning {kerned} GPOS pair(s) -> legacy `kern` table (see kern_source)")
        for slot in slots:
            row = plan[slot]
            note = f"ink {row['ink']} target {row['target_cap_height']}" if segmented else \
                f"line_height {row['line_height']}"
            print(
                f"  {prefix}_{slot}.c".ljust(26)
                + f"size {row['size']:>2} ({note})"
            )
            annotate(SRC_FONTS / f"{prefix}_{slot}.c", entry, is_stock, slot in icon_slots, kerned)
    write_header(manifest)
    return 0


def annotate(path: Path, entry: dict, stock: bool, icons: bool, kerned: int = 0) -> None:
    """Documents the real source of the generated file in the file itself."""
    lines = [
        f" * Source: {entry['display']} ({entry['license']}) {entry['url']}",
        f" * sha256 {entry['sha256']}  ·  generated by tools/fonts/fonts.py (lv_font_conv"
        f" {LV_FONT_CONV_VERSION}), do not edit by hand",
    ]
    if kerned:
        lines.append(
            f" * Kerning: {kerned} GPOS pair(s) of the source font converted to the legacy"
            " `kern` table (tools/fonts/fonts.py, kern_source): the reader of"
            f" lv_font_conv {LV_FONT_CONV_VERSION} only sees GPOS kerning in direct"
            " pair-adjustment lookups, not in the extension lookups this family emits"
        )
    if icons:
        icon = load_manifest()["icon_font"]
        lines.append(
            f" * Icons: {icon['display']} ({icon['license']}) sha256 {icon['sha256'][:16]}…"
            " merged on the icon slots"
        )
    if stock:
        lines.insert(
            0,
            " * The ROM's own family slot (src/ uses `&sony_N` everywhere); the migration"
            " from the proprietary Sony family (issue #9, decision 1) put this libre font"
            " here.",
        )
    text = path.read_text()
    marker = " ******************************************************************************/\n"
    if marker not in text:
        raise Fail(f"{path.name}: no header terminator to annotate")
    text = text.replace(marker, "\n".join(lines) + "\n" + marker, 1)

    if stock:
        symbol = path.stem
        block = (
            f"#if LVGL_VERSION_MAJOR >= 8\n"
            f"const lv_font_t {symbol} = {{\n"
            f"#else\n"
            f"lv_font_t {symbol} = {{\n"
            f"#endif"
        )
        if text.count(block) != 1:
            raise Fail(f"{path.name}: expected exactly one public-font declaration block")
        text = text.replace(block, STOCK_FORCE.format(symbol=symbol), 1)
    path.write_text(text)


# --- verify -----------------------------------------------------------------


def cmd_verify(args: argparse.Namespace) -> int:
    """Checks the generated tree: every family complete, and the icon slots.

    Reports, per family, the worst line-height residual against the target (the
    size grid is integral and these families' line heights step coarser than
    Sony's, so a residual of 1 is normal on the tiny slots). Fails on a missing
    file, a bad slot set, an icon slot without the icon cmap, or a residual above
    2 px (which would mean the plan is not matching at all).
    """
    manifest = load_manifest()
    targets = load_targets()
    stock = candidate_by_name(manifest, manifest["stock"]) if manifest["stock"] else None
    segmented_slots = [int(slot) for slot in manifest["segmented_slots"]]
    icon_codes = [int(code) for code in manifest["icon_codepoints"]]
    wanted = [("sony", manifest["stock_slots"], stock)]
    wanted += [
        (entry["c_name"], manifest["alternate_slots"], None)
        for entry in manifest["candidates"]
        if entry is not stock and not entry.get("segmented")
    ]
    wanted += [
        (entry["c_name"], manifest["segmented_slots"], None)
        for entry in manifest["candidates"]
        if entry.get("segmented")
    ]

    failures = 0
    for prefix, slots, entry in wanted:
        if entry:
            label = f"{prefix} (stock: {entry['display']})"
        elif prefix in [item["c_name"] for item in manifest["candidates"] if item.get("segmented")]:
            label = f"{prefix} (digit family)"
        else:
            label = prefix
        files = {size: SRC_FONTS / f"{prefix}_{size}.c" for size in slots}
        missing = [path.name for path in files.values() if not path.is_file()]
        if missing:
            print(f"FAIL {label}: missing {', '.join(missing)}")
            failures += 1
            continue

        # A digit family matches the ink of the digit, not the line height (see
        # plan_for): comparing the wrong metric would report a 10 px residual.
        match = "cap_height" if prefix in [
            item["c_name"] for item in manifest["candidates"] if item.get("segmented")
        ] else "line_height"

        worst = 0
        off_by_one: list[int] = []
        for size, path in files.items():
            text = path.read_text()
            if match == "cap_height":
                descriptor = text.split("glyph_dsc[] = {", 1)[-1]
                boxes = [int(value) for value in re.findall(r"\.box_h = (\d+)", descriptor)]
                line_height = boxes[1 + (ord("8") - 0x20)]
            else:
                line_height = int(re.search(r"\.line_height = (\d+)", text).group(1))
            target = targets.get(str(size), {}).get(match, line_height)
            delta = abs(line_height - target)
            if delta == 1:
                off_by_one.append(size)
            worst = max(worst, delta)
            icons_here = "Icons:" in text
            if size in manifest["icon_slots"] and prefix == "sony" and not icons_here:
                print(f"FAIL {path.name}: the icon slot carries no icon glyphs")
                failures += 1
            if size not in manifest["icon_slots"] and icons_here:
                print(f"FAIL {path.name}: icons on a non-icon slot")
                failures += 1
            if icons_here:
                # The ASCII cmap is FORMAT0 (list_length 0) and the icon one is the
                # sparse list; the glyph count is the sparse entry, not the first.
                entries = [int(value) for value in re.findall(r"\.list_length = (\d+)", text)]
                if max(entries, default=0) != len(icon_codes):
                    print(f"FAIL {path.name}: the icon cmap does not carry {len(icon_codes)} glyphs")
                    failures += 1
        note = f", {len(off_by_one)} slot(s) off by 1: {off_by_one}" if off_by_one else ""
        print(f"ok   {label}: {len(files)} files, worst {match} delta {worst}{note}")
        if worst > 2:
            print(f"FAIL {label}: the plan does not match (delta {worst} px)")
            failures += 1
    return 1 if failures else 0


# --- entry point ------------------------------------------------------------


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    subparsers = parser.add_subparsers(dest="command", required=True)

    fetch_parser = subparsers.add_parser("fetch", help="download the pinned sources")
    fetch_parser.add_argument(
        "--licenses", action="store_true", help="also refresh src/fonts/licenses/"
    )
    fetch_parser.set_defaults(function=cmd_fetch)

    targets_parser = subparsers.add_parser(
        "targets", help="capture the metrics of the committed sony_*.c (pre-migration)"
    )
    targets_parser.set_defaults(function=lambda _args: write_targets())

    plan_parser = subparsers.add_parser("plan", help="print the per-candidate size map")
    plan_parser.add_argument("--candidate", action="append")
    plan_parser.set_defaults(function=cmd_plan)

    generate_parser = subparsers.add_parser("generate", help="write the C families")
    generate_parser.add_argument("--candidate", action="append")
    generate_parser.add_argument("--stock", help="candidate generated into sony_*.c")
    generate_parser.set_defaults(function=cmd_generate)

    verify_parser = subparsers.add_parser("verify", help="check the generated files")
    verify_parser.add_argument("--candidate", action="append")
    verify_parser.set_defaults(function=cmd_verify)

    args = parser.parse_args(argv[1:])
    try:
        return int(args.function(args))
    except Fail as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
