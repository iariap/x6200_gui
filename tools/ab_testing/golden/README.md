# Golden renders (the pixels the non-text contract measures)

These PNGs are the `.scn` fixtures rendered by the harness at a fixed revision.
They are the source of truth for the WCAG 1.4.11 non-text check
(`src/x6200_ab/nontext_checks.py`, `quality/nontext.yaml`): the objects it
measures — the S-meter arc over a bitmap face, the waterfall column, the focus
indicator — have ink or surface that is not a theme role the
static check can resolve, so only the pixels answer.

They are committed, so `x6200-ab themes` stays offline and deterministic (no
Docker, no revision). `--renders-dir` points the check at a fresh capture
instead.

## How they were produced

From the revision the check ships with (the branch tip), inside
`tools/ab_testing`:

```bash
uv run x6200-ab capture --revision <sha> --label golden \
    --out "$(git rev-parse --show-toplevel)/tools/ab_testing/golden" \
    --scenario main-foco --scenario main-espectro-agua --scenario main-tema-aurora \
    --scenario main-tema-cathode --scenario main-tema-sprint --scenario main-tema-sunlit \
    --scenario main-tema-sunlit-agua --scenario settings-home --scenario settings-perfil
```

The harness is deterministic (two runs of a fixture produce a byte-identical
PNG; see `tests/check_harness_determinism.sh`), so these files are reproducible.

## 2026-10-15 — regenerated for the font migration (AB-FONTS)

Every render here was captured again at the font migration of issue #9 / decision 1
(the ROM draws with a libre family, `docs/THIRD_PARTY.md`): the objects this check
measures are painted with theme roles and bitmaps, so their contrast does not
depend on the font, and `x6200-ab themes --renders-dir` over the new renders reports
the same ratios as before (the S-meter arc, the waterfall signal, the focus
indicator and the band-plan chips all keep their values). The renders themselves
changed, because the text of every screen did: that is the declared change of
`quality/contracts.yaml` (`expected_changes`, AB-FONTS).

## When they change

A render has to be regenerated when a change moves a measured object: a theme
role the object is painted with, the layout, or the object's geometry. The change
that moves it declares the region in `quality/contracts.yaml`
(`expected_changes`), regenerates the golden PNG, and the non-text check then
measures the new pixels — a contrast that drops below 3:1 fails `x6200-ab themes`
and `tests/test_nontext_checks.py`.

## 2026-10-15 — Sunlit added (issue #67, THEMES v2)

Two renders are new: `main-tema-sunlit` and `main-tema-sunlit-agua`, the
principal and waterfall fixtures of the Sunlit theme. They are the renders the
new non-text objects of `quality/nontext.yaml` measure: the S-meter arc
(`TC(meter_over)` over the theme-independent `top_long.bin` face, which on a
light theme must be a *light* ink) and the theme's own waterfall column (dark
navy signal over a near-white floor). No existing render changed: adding a theme
file does not move any object the other fixtures paint.

## 2026-10-15 — re-baseline of the default-family renders (issue #73)

Seven renders that draw with the **default font family** were stale: they were
last captured at AB-FONTS (`b78b779`/`4e9b5f4` era) and the AB-FONTSE kerning
change moved their glyphs without re-baselining them (the theme files that name
their own family — Classic/RS/Redshift/Sprint/Cathode — were already current).
Regenerated at the #73 tip: `main-espectro-agua`, `main-foco`,
`main-otro-tema`, `main-tema-outdoor-negative`, `main-tema-vision`,
`settings-home` and `settings-perfil`.

The objects `nontext.yaml` measures are painted with theme roles and the fixed
bitmaps (`top_long.bin`, the waterfall palette), so the font move does not change
their contrast: `x6200-ab themes` reports the **same** ratios as before for every
object (the S-meter arc, the waterfall signal, the focus indicator, the band-plan
chips). Only the pixels under them changed. The regeneration is declared with the
region-scoped `expected_changes` of `quality/contracts.yaml` (AB-FONTS/AB-FONTSE
for the font move; `ajustes_tema` for the #73 Theme row below).

`settings-perfil` carries a second, #73-specific move: the Theme row of Ajustes
draws the dropdown's selected option, and with the six-only selector the saved
default (`IU1WXY`, a legacy name) is no longer in the list, so the row falls back
to the first offered option (`Sunlit`). That single cell is declared under the new
region `ajustes_tema`; the #74 default migration removes the fallback.

## 2026-10-15 — `main-tema-aurora.png` (THEMES v2, issue #72)

A **new** render, not a regeneration: the Aurora theme
(`rootfs/usr/share/x6200/themes/Aurora.json`) and its fixture
(`tools/ui_harness/fixtures/main-tema-aurora.scn`, the same radio state as
`home-usb-20m`, the theme the only variable) arrive together with this PNG, so the
non-text contract can measure the two objects that only the pixels answer on this
theme: `s_meter_arc` (Aurora's own `meter_over` over the meter's bitmap face,
**6.23:1**) and `waterfall_signal` (Aurora's own `waterfall` table, **6.57:1**),
both declared in `quality/nontext.yaml`. The other renders in this directory are
untouched by the change.

Captured at the same revision as the rest of the branch, inside
`tools/ab_testing`:

```bash
uv run x6200-ab capture --revision <sha> --label golden \
    --scenario main-tema-aurora --out "$(git rev-parse --show-toplevel)/tools/ab_testing/golden"
```

or, without Docker, with the harness build of `tests/check_harness_determinism.sh`:

```bash
tools/ui_harness/build/ui_harness --scenario tools/ui_harness/fixtures/main-tema-aurora.scn \
    --png tools/ab_testing/golden/main-tema-aurora.png --json /tmp/main-tema-aurora.json
```

## 2026-10-15 — `main-tema-sprint.png` (THEMES v2, issue #69)

A **new** render, not a regeneration: the Sprint theme
(`rootfs/usr/share/x6200/themes/Sprint.json`) and its fixture
(`tools/ui_harness/fixtures/main-tema-sprint.scn`, the same radio state as
`home-usb-20m`, the theme the only variable) arrive together with this PNG, so the
non-text contract can measure the two objects that only the pixels answer on this
theme: `s_meter_arc` (Sprint's own `meter_over` over the meter's bitmap face,
**5.46:1**) and `waterfall_signal` (Sprint's own `waterfall` table, **8.31:1**),
both declared in `quality/nontext.yaml`. The other renders in this directory are
untouched by the change.

Captured at the same revision as the rest of the branch, inside
`tools/ab_testing`:

```bash
uv run x6200-ab capture --revision <sha> --label golden \
    --scenario main-tema-sprint --out "$(git rev-parse --show-toplevel)/tools/ab_testing/golden"
```

or, without Docker, with the harness build of `tests/check_harness_determinism.sh`:

```bash
tools/ui_harness/build/ui_harness --scenario tools/ui_harness/fixtures/main-tema-sprint.scn \
    --png tools/ab_testing/golden/main-tema-sprint.png --json /tmp/main-tema-sprint.json
```

**Nota medida (pre-existente, no de este cambio).** Los renders dorados de las fixtures que
dibujan con la familia por defecto (`main-foco`, `settings-home`, `main-espectro-agua`,
`main-otro-tema`, `main-tema-vision`) **no** coinciden byte a byte con el render de la
revisión de hoy: el kerning de la migración de fuentes (`4e9b5f4`, AB-FONTSE) movió sus
glifos y esos PNG no se regeneraron (los de los temas que nombran Roboto —Classic, RS, Holo,
Material 3— sí coinciden). La medición no-texto de esas fixtures describe, entonces, un
render anterior; regenerarlos es una re-baseline declarada (`expected_changes`) que queda
para la pasada de integración (#73).

## 2026-10-15 — `main-tema-cathode.png` (THEMES v2, issue #70)

A **new** render, not a regeneration: the Cathode theme
(`rootfs/usr/share/x6200/themes/Cathode.json`) and its fixture
(`tools/ui_harness/fixtures/main-tema-cathode.scn`, the same radio state as
`home-usb-20m`, the theme the only variable) arrive together with this PNG, so the
non-text contract can measure the two objects that only the pixels answer on this
theme: `s_meter_arc` (Cathode's own `meter_over` over the meter's bitmap face,
**8.11:1**) and `waterfall_signal` (Cathode's own `waterfall` table, **6.82:1**),
both declared in `quality/nontext.yaml`. The other renders in this directory are
untouched by the change.

Captured at the same revision as the rest of the branch, inside
`tools/ab_testing`:

```bash
uv run x6200-ab capture --revision <sha> --label golden \
    --scenario main-tema-cathode --out "$(git rev-parse --show-toplevel)/tools/ab_testing/golden"
```

or, without Docker, with the harness build of `tests/check_harness_determinism.sh`:

```bash
tools/ui_harness/build/ui_harness --scenario tools/ui_harness/fixtures/main-tema-cathode.scn \
    --png tools/ab_testing/golden/main-tema-cathode.png --json /tmp/main-tema-cathode.json
```

## 2026-10-15 — `main-tema-sunlit-agua.png` regenerada (AB-WFWIDTH)

El ancho de la **línea central del waterfall** pasa a ser una clave de tema
(`waterfall_signal_width`, default 1, clamp 1..8) y los dos temas outdoor la piden en 3
(`rootfs/usr/share/x6200/themes/Sunlit.json`, `Trail.json`). La fixture
`main-tema-sunlit-agua` es la que dibuja esa línea (las demás la llevan apagada vía
`waterfall_center_line`), así que su render se regenera con la línea de 3 px. Los objetos
no-texto que este directorio mide —el arco del S-meter y la columna de señal del
waterfall— **no** tocan la línea del centro, así que sus ratios no cambian;
`main-tema-sunlit` (sin la línea) queda byte-idéntico. El cambio de píxeles lo declara
`quality/contracts.yaml` (`expected_changes`, AB-WFWIDTH).

Capturada con el build de `tests/check_harness_determinism.sh`:

```bash
tools/ui_harness/build/ui_harness --scenario tools/ui_harness/fixtures/main-tema-sunlit-agua.scn \
    --png tools/ab_testing/golden/main-tema-sunlit-agua.png --json /tmp/main-tema-sunlit-agua.json
```
