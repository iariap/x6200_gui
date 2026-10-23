# `tools/fonts` — the font pipeline and the candidate matrix

The font families of `src/fonts/` are **generated**, not hand-edited. This
directory is the pipeline: it downloads the pinned libre sources
(`candidates.json`), turns them into LVGL 8 C with the same tool the previous
families came from (`lv_font_conv`), renders the whole candidate matrix against the
family they replace, and generates the family list the ROM and the Ajustes window
read.

Spec and decision trail: [`docs/fonts-spec.md`](../../docs/fonts-spec.md) (issue #9
"font quality and glyph coverage", decision 1 "migrate to libre fonts", decision 8
"the natural bench runs at the ROM's 50 ms"), licences in
[`docs/THIRD_PARTY.md`](../../docs/THIRD_PARTY.md).

## Commands

```sh
tools/fonts/fonts.py fetch --licenses   # pinned TTFs into .cache/ (gitignored) + src/fonts/licenses/
tools/fonts/fonts.py plan               # the per-candidate size map against Sony's metrics
tools/fonts/fonts.py generate           # write every family into src/fonts/ + font_families.h/.c
tools/fonts/fonts.py verify             # check what landed (completeness, icons, target line heights)

tools/fonts/matrix.py capture --out /tmp/fontmatrix          # render every family (Docker harness)
tools/fonts/matrix.py report  --baseline /tmp/fontbase --captures /tmp/fontmatrix
tools/fonts/matrix.py gallery --captures /tmp/fontmatrix --out gallery.png
```

`fetch` is the only step that needs the network; it verifies every sha256 against
the manifest and fails on a mismatch. `generate` and `verify` are offline. The
script runs on the host `python3`: `plan` and `generate` need `Pillow`, and
`generate` needs `fontTools` for the kerning conversion (`pip install fonttools`).

## The conversion pipeline

Each family is one C file per slot, generated exactly like the families that were
already in the tree (their `Opts:` header is the record):

```text
lv_font_conv --bpp 4 --size <S> --no-compress \
    --font <candidate>.ttf --symbols ' !"#…~' \
    [--font fa-solid-900.ttf --range 58715,61560,61926,61931,62016-62020] \
    --format lvgl -o sony_<slot>.c
```

* `tools/fonts/lvfc.js` drives `lv_font_conv`'s library in one process for the
  whole family (~50 s for 15 families × 17–20 slots instead of one node process per
  size), pinned to `lv_font_conv@1.5.3` installed in `.cache/npm/`.
* The **stock family** (the ROM's own slot, `sony_*.c`, named after the proprietary
  font it replaces because all of `src/` draws with `&sony_N`) also merges the icon
  font on the two slots that carried an icon range (`sony_20`, `sony_30`), and its
  declaration is emitted non-`const` with the `#if 0` block `fonts_apply()` needs
  (`src/fonts/font_select.c`).
* The **alternate families** are ASCII-only, like the Roboto/DejaVu/Arial families
  that were there before: their icon glyphs resolve through the stock family's
  `fallback` (the contracts allow that: `allow_fallback_glyphs: true`).
* Every generated file carries its source, its sha256 and the tool version in its
  header, so a `.c` that does not match `candidates.json` is visible by inspection.
* **Kerning**: the reader of `lv_font_conv` (`opentype.js` 1.3.4: `Position.init()`
  + `Font.getKerningValue()`) collects the pair kerning of GPOS only from the
  lookups of **type 2** that the `kern` feature of the default script (`DFLT`,
  else `latn`) lists, and it never reaches the legacy `kern` table while the font
  has a GPOS table (with no type-2 lookup the list is empty, and an empty array is
  truthy, so it reports "no kerning" instead of looking further). A family that
  emits its kerning in **extension** lookups (type 9 wrapping type 2) therefore
  renders with no kerning at all, and no `kern` table it carried would be read
  either. That is the case of Fira Sans (the stock family), IBM Plex Sans, Source
  Sans 3 and Lato in this manifest; Roboto, Archivo, PT Sans, DejaVu Sans and
  Ubuntu list direct type-2 lookups (their kerning was already reaching the C),
  Inter and Noto Sans list both, and Open Sans, JetBrains Mono and the DSEG
  families carry no pair kerning.
  `fonts.py` therefore hands the reader a converted copy of the fonts on that dead
  path (`.cache/kern/<name>`, `kern_source()`): the GPOS pair kerning written as
  the legacy `kern` table (format 0, the pair table `lib/font/table_kern.js`
  reads) with the GPOS table dropped. The conversion is a no-op for every other
  family -- their generated files stay byte-identical -- and the converted file is
  a function of the pinned TTF (`recalcTimestamp` off), so `generate` is
  reproducible. The measurement of what the added kerning moves is in
  `tools/ab_testing/reports/AB-FONTSE-KERN` (region `frecuencia` and friends).

### The size map: matching the family it replaces

A family is not generated at its "natural" size. `sony_N` renders much smaller
than `N` suggests (the stock family's own line height is ~0.85 × the nominal size),
so a libre font at the same nominal size overflows the fixed cells of the top bar
and the frequency row. `plan` therefore picks, per slot, the pixel size whose
rendered metrics are closest to the metrics the committed `sony_*.c` had
(`sony-targets.json`, captured from the pre-migration files):

1. primary: the smallest `|line_height − target|`. `line_height` is what LVGL lays
   a one-line label out with, so it is the value that can clip a label. It is not a
   property of the font but of the rendered glyph set, so it is **probed with the
   tool that writes the file** (`lvfc.js probe`, every size from 5 to 70 px);
   approximating it from the font tables (Pillow's `getmetrics`) is off by up to
   3 px on these families, which is the difference between fitting a cell and
   clipping it.
2. tie-break: the smallest relative `|Σ advances − target|`, i.e. how wide a string
   renders (the value that can overflow a fixed cell).

The result is an integral size per slot; these families' line heights step coarser
than Sony's, so a residual of 1 px is normal (and reported by `verify`). Slots 8/10/12
are only reachable through `font_down()` (the VISION scale walking down) and are
matched the same way.

## The matrix

`matrix.py capture` renders one fixture per family with `--font <id>` (one build
carries every family, see the harness' `--font`), and `report` compares each
family against a baseline render of the tree **before** the migration — the same
fixtures, the same harness, the proprietary family:

* `boxes` = `0.65·mean|Δw|/w + 0.35·mean|Δh|/h` over **every text box** of the
  fixture set, paired label by label (`lv_txt_get_size` as the inspector reports
  it). Pairing is per node position: both trees are the same object tree with a
  different font, so the n-th text node is the same widget.
* `ink` = the mean relative distance of the drawn height of `A` at the two slots
  the top bar and the clock use, against the baseline's own cap height there.
  Matching the *line height* does not match the *ink*: every libre family here
  draws bigger letters than Sony did at the same line height.
* `score` = `boxes + ink`, both measured and in %. A family that adds a legibility
  defect the baseline did not have (an overflowing or truncated label, a missing
  glyph) is marked `!` and no score qualifies it: two candidates are disqualified
  that way (Archivo, Lato — they overflow a label).
* `gallery` is the human half: a contact sheet of the candidates over the same
  render, for the owner's eye.

## The default, and the one-line flip

The owner's decision 1 was "migrate", so the ROM's own slot is generated from a
libre family and is the default. The proposed default is the best-scoring candidate
of the matrix; the manifest names it in one place:

```json
"stock": "firasans"        // tools/fonts/candidates.json
```

Changing it is one line plus `tools/fonts/fonts.py generate` (the winner goes into
`sony_*.c`, the previous winner becomes a selectable alternate). Because the winner
is the ROM's own slot, no C code changes: `src/` and `src/fonts/font_select.c` only
ever name `&sony_N`.

The **other** families are selectable at runtime, with no regeneration and no code
change: a theme's `font` key names one by id (`font_ids[]` in the generated
`font_families.h`), and the Ajustes window lists every compiled family in its
"Font" dropdown — its item list is built from the same generated `font_names[]`, so
the row that is picked *is* the `FONT_*` value (the hand-written parallel list is
gone; see the off-by-one `src/main.c` documented).

## The segmented digit families

The three DSEG candidates (`dseg7`, `dseg7mini`, `dseg14`, `segmented: true` in the
manifest) are the exception to the rules above, on purpose:

* they are **not** whole-family choices (a segmented face has no lowercase and its
  letters are approximations), so they are not in `font_families[]` but in
  `font_digit_families[]`, and the ROM only uses one for the labels that are pure
  digits — the frequency row and the clock;
* they are generated for `segmented_slots` only (the slots the digit labels use:
  the frequency row's `sony_30`/`sony_38` and the clock's `sony_44` since issue
  #285), not for the whole size range;
* they are generated with `match: ink`, not `line_height`: a segment digit is nearly
  as tall as its line height, so matching the line height (what the proportional
  families match) would overflow the 36 px frequency box. The probe reads the ink of
  `8` for them and of `A` for the proportional families;
* they carry the stock family of the same slot as their LVGL `fallback`
  (`--lv-fallback sony_<slot>`), so the letters of the voltage line and the icons
  come from the default family instead of a placeholder box.

Sampling and measurement use the digit screen `main-frecuencia-dseg` (the main
screen with `digits dseg7`) instead of the eight whole-family fixtures:

```sh
tools/fonts/matrix.py capture --digits --scenario main-frecuencia-dseg --out /tmp/fontmatrix
# and the same fixture with the panels off, as the baseline to score against:
docker run … ./ui_harness --scenario ../fixtures/main-frecuencia-dseg.scn --digits none …
tools/fonts/matrix.py report --digits --scenario main-frecuencia-dseg \
    --baseline <the none render> --captures /tmp/fontmatrix
```

## Adding a candidate

1. Add its entry to `candidates.json` (display name, `c_name` prefix, pinned URL,
   license, `license_file` when it is in the Google Fonts repository).
2. `tools/fonts/fonts.py fetch --licenses` (verifies the sha256, writes the licence
   text), then `tools/fonts/fonts.py generate` and `verify`.
3. `tools/fonts/matrix.py capture --font <c_name>` + `report` to score it, and
   `docs/THIRD_PARTY.md` for the licence trace.

Nothing else: the enum, the `extern` declarations, the family table, the dropdown
list and the theme ids are all generated from the manifest.
