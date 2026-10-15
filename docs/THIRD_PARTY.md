# Third-party fonts of the X6200 GUI

Everything the GUI draws text or icons with, where it comes from and under which
licence. The `.c` families in `src/fonts/` are **generated**: `tools/fonts/fonts.py`
turns the pinned sources of this table into LVGL 8 C (`tools/fonts/README.md`), and
the font binaries themselves are **not** vendored — the repository keeps the
generated code, the licence texts (`src/fonts/licenses/`) and this trace.

Until issue #9 / decision 1 the GUI drew with two families whose binaries could not
be redistributed or regenerated from this repository: **SonySketchEF** (the ROM's
own slot, `src/fonts/sony_*.c`) and **Arial Bold** (`src/fonts/arial_*.c`, generated
from `arialbd.ttf` out of a Windows install). Both are gone:

* the ROM's own slot (`sony_*.c`, the name the whole `src/` tree keeps using) is
  generated from a libre family;
* the `arial_*` family was deleted (its source is not redistributable; the licence
  text of the old Roboto family stays in `src/fonts/LICENSE-Roboto-Apache-2.0.txt`
  as the provenance of the previous artifacts).

## The pinned sources

Generation is reproducible from this table: every URL names an exact revision
(Google Fonts is pinned by commit `94301472b4d809299b4775bc6502592b83dd6934`, the
other two by release tag), and `tools/fonts/fonts.py fetch` refuses to continue on
a hash mismatch.

| Font | File | Licence | Upstream | sha256 |
|---|---|---|---|---|
| FontAwesome Free 6 Solid | `fa-solid-900.ttf` | OFL-1.1 | FortAwesome/Font-Awesome | `af19d135d3a935b3ebfbd80320716ffe1202052c5f68dc2c5f1abc57005ac605` |
| Inter | `Inter[opsz,wght].ttf` | OFL-1.1 | Google Fonts / rsms/inter | `29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031` |
| Roboto | `Roboto[wdth,wght].ttf` | OFL-1.1 | Google Fonts / googlefonts/roboto | `d7598e12c5dbef095ff8272cfc55da0250bd07fbdecbac8a530b9b277872a134` |
| Roboto Condensed | `RobotoCondensed[wght].ttf` | OFL-1.1 | Google Fonts / googlefonts/roboto | `dace262afcee68a5276f200d8026c57221735c0118ab5fda8c2c0d3dc409a8d0` |
| Noto Sans | `NotoSans[wdth,wght].ttf` | OFL-1.1 | Google Fonts / notofonts/latin-greek-cyrillic | `bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d` |
| IBM Plex Sans | `IBMPlexSans[wdth,wght].ttf` | OFL-1.1 | Google Fonts / IBM/plex | `3b031aa4216174205bd8471f88a49b91f093169e9e87bd5262242bc5967fe2e3` |
| Fira Sans | `FiraSans-Regular.ttf` | OFL-1.1 | Google Fonts / bBoxType/Fira | `c29556a2719bf613ef3d5e070e40d903a8965d9c081beca1375dc1e6e0f93c23` |
| Open Sans | `OpenSans[wdth,wght].ttf` | OFL-1.1 | Google Fonts / googlefonts/opensans | `36643644f318a812aab2d2ed3bb98f8cf0872527f835fe9398d95fe6b9adb878` |
| Source Sans 3 | `SourceSans3[wght].ttf` | OFL-1.1 | Google Fonts / adobe-fonts/source-sans | `042fe2cc0b933e328410d7acbd0aa6a1873dca5aef81875f4bc214b08825c7b9` |
| PT Sans | `PT_Sans-Web-Regular.ttf` | OFL-1.1 | Google Fonts / para-type-foundation | `9cc831490532009bae2b3ce0d39c62adfc889060beb421593bfd9d2396d0f10a` |
| Lato | `Lato-Regular.ttf` | OFL-1.1 | Google Fonts / googlefonts/lato | `d636e4683231f931eda222d588e944d082bfd3bdba02f928bee461c0f185b251` |
| DejaVu Sans | `DejaVuSans.ttf` | Bitstream Vera / DejaVu | dejavu-fonts 2.37 | `7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954` |
| Archivo | `Archivo[wdth,wght].ttf` | OFL-1.1 | Google Fonts / Omnibus-Type/Archivo | `0e094a7d3c7c4c25cf1310c4b30014f1dae9332220b1c2c88f4fa996f0b05053` |
| Archivo Narrow | `ArchivoNarrow[wght].ttf` | OFL-1.1 | Google Fonts / Omnibus-Type/Archivo | `adbe027f625c8393ae0f6e174e32e233dda485bc3eda5153ce428275394ef97f` |
| Ubuntu | `Ubuntu-Regular.ttf` | UFL-1.0 | Google Fonts / canonical/ubuntu-font-family | `3128df86a31805618436d0ae5651ba4285d0c9de0a39057d025f64ee33bceb64` |
| JetBrains Mono | `JetBrainsMono[wght].ttf` | OFL-1.1 | Google Fonts / JetBrains/JetBrainsMono | `48715a42ec242c21e9f02692891e147d022299a52e48d5e413e1a942193ffeda` |
| DSEG7 Classic | `DSEG7Classic-Regular.ttf` | OFL-1.1 | keshikan/DSEG v0.46 (`fonts-DSEG_v046.zip`) | `e3270928ced68082f32d3b62877a6741426116335ac0832919326341a8b5cfaf` |
| DSEG7 Classic Mini | `DSEG7ClassicMini-Regular.ttf` | OFL-1.1 | keshikan/DSEG v0.46 (`fonts-DSEG_v046.zip`) | `1a9103b69043dabcb592ded1c02cc26c88ebe0a12c55c50589cd1a4f7b053fc7` |
| DSEG14 Classic | `DSEG14Classic-Regular.ttf` | OFL-1.1 | keshikan/DSEG v0.46 (`fonts-DSEG_v046.zip`) | `0106818dd2e2c9ac9c6b5b7edbbe193df367397d48a18377a58090b00580d4e6` |

The licence text of every family is committed next to the generated code
(`src/fonts/licenses/<name>.txt`, taken from the same pinned revision as the font).
The two families that were already in the tree keep their own files:

* `src/fonts/LICENSE-DejaVu-Bitstream-Vera.txt` — DejaVu Sans (the DejaVu licence:
  Bitstream Vera plus the DejaVu changes, free to use and redistribute);
* `src/fonts/LICENSE-Roboto-Apache-2.0.txt` — the **old** Roboto family's licence.
  The pinned Roboto of this table is OFL-1.1 (Google relicensed it), and the
  generated `roboto_*.c` come from it; the Apache text stays as the provenance of
  the artifacts that were in the tree before.

## Icons

The eight PUA code points the ROM's `sony_20`/`sony_30` carried
(`0xE55B` plug-circle-bolt, `0xF1E6` plug, `0xF1EB` wifi, `0xF240`-`0xF244`
battery, `0xF078` chevron) are **FontAwesome Free 6 Solid** code points, which is
where the proprietary `SonySketchEF_plus_big.woff` had taken them from. They are
merged into the stock family's icon slots from `fa-solid-900.ttf` (OFL-1.1 for the
font, CC BY 4.0 for the icons), so the migration does not lose a glyph.

`0xF078` (`LV_SYMBOL_DOWN`, the chevron LVGL's dropdown draws) was **not** in the
old range — it is the missing glyph the owner reported from the radio's log in
issue #9 (`lv_draw_letter: glyph dsc. not found for U+F078`). The stock family now
carries it. The fixture set does not open a dropdown, so no `missing_glyph`
contract changed with it; the difference is measurable in the generated file (the
sparse cmap of `sony_20.c`/`sony_30.c` goes from 8 to 9 code points) and on the
radio.

`0xF293` (bluetooth) is *not* included: `src/info.c` draws the BT icon with
`lv_line` since the port, so nothing asks the font chain for it.

## The segmented digit families

Three more families are **not** whole-family choices: DSEG7 Classic (the
seven-segment LCD face — the "Casio watch" panel), DSEG7 Classic Mini (the same
face with a tighter colon/dot) and DSEG14 Classic (fourteen segments, so the
letters keep a readable shape) are digit-panel faces. A segmented face has no
lowercase and its letters are segment approximations, so the ROM only uses one for
the labels that are pure digits — the frequency row's own labels and the clock
(`src/styles.c` `freq_style`/`freq_main_style`, `src/clock.c`).

They are compiled for those two slots only (`segmented_slots` in
`tools/fonts/candidates.json`) and each of them declares the stock family of the
same slot as its LVGL `fallback`, so any glyph the segmented face does not carry
(the letters of the voltage line, the icons) is drawn by the default family instead
of as a placeholder box. They are selected with a theme's `font_digits` key
(`FONT_DIGIT_*` in `src/fonts/font_families.h`, `font_digit()` in
`src/fonts/font_select.c`) or with the harness' `--digits`, and the sample screen is
`tools/ui_harness/fixtures/main-frecuencia-dseg.scn`.

The licence text of all three is the same file of the release
(`src/fonts/licenses/dseg7.txt`, `dseg7mini.txt`, `dseg14.txt`; SIL OFL 1.1,
`Copyright (c) 2017, keshikan`, with the reserved font name "DSEG").

## What ships in the ROM

`src/fonts/font_families.h` (generated) lists the families that are compiled in and
selectable: the ROM's own slot (the proposed default, **Fira Sans** — see
`tools/fonts/README.md` for the score and `docs/fonts-spec.md` for the matrix) plus
every other candidate, one `.c` per slot each (~600 KB of `.rodata` per family).
A family the ROM does not need to carry is pruned by deleting its `<c_name>_*.c`
files and its entry in `tools/fonts/candidates.json` and re-running
`tools/fonts/fonts.py generate`: the enum, the symbol declarations, the family table
and the settings list all come from that one manifest, so a pruned family cannot
leave a dangling name behind.
