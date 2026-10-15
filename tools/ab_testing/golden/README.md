# Golden renders (the pixels the non-text contract measures)

These PNGs are the `.scn` fixtures rendered by the harness at a fixed revision.
They are the source of truth for the WCAG 1.4.11 non-text check
(`src/x6200_ab/nontext_checks.py`, `quality/nontext.yaml`): the objects it
measures — the S-meter arc over a bitmap face, the waterfall column, the focus
indicator, the band-plan chips — have ink or surface that is not a theme role the
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
    --scenario main-foco --scenario main-espectro-agua --scenario main-otro-tema \
    --scenario main-tema-classic --scenario main-tema-holo \
    --scenario main-tema-material-3 --scenario main-tema-outdoor-negative \
    --scenario main-tema-vision --scenario main-tema-rs --scenario settings-home
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

