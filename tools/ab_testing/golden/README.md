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

## When they change

A render has to be regenerated when a change moves a measured object: a theme
role the object is painted with, the layout, or the object's geometry. The change
that moves it declares the region in `quality/contracts.yaml`
(`expected_changes`), regenerates the golden PNG, and the non-text check then
measures the new pixels — a contrast that drops below 3:1 fails `x6200-ab themes`
and `tests/test_nontext_checks.py`.
