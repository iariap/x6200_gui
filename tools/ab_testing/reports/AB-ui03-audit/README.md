# UI-03 audit evidence (issue #8)

The output of the UI-03 audit, kept next to the committed e2e reports so the
triage in `docs/UI_KNOWN_ISSUES.md` can be checked without rebuilding anything.

Everything here comes from revision `7b66686`, the parent of the commit that adds
this directory (a commit cannot carry evidence of its own SHA); the JSONs carry
that revision in their `revision` field. To regenerate, from `tools/ab_testing/`:

```sh
uv run x6200-ab audit --revision 7b66686 --suite visual --json > reports/AB-ui03-audit/audit.json
uv run x6200-ab contracts --revision 7b66686 --json          > reports/AB-ui03-audit/contracts.json
uv run x6200-ab themes --json                                > reports/AB-ui03-audit/themes.json
uv run x6200-ab themes                                       > reports/AB-ui03-audit/themes.txt
```

The two captures are rendered by the harness the determinism gate builds, from
the repository root:

```sh
REV=$(git rev-parse HEAD)
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$PWD:/src" \
    -w /src/tools/ui_harness/build x6200-uiharness -lc \
    "cmake .. -DHARNESS_REVISION=$REV >/dev/null && make -j\"\$(nproc)\" && \
     for n in main-tema-vision main-tema-outdoor-negative; do \
       ./ui_harness --scenario /src/tools/ui_harness/fixtures/\$n.scn \
         --png out/ui03-audit/\$n.png --json out/ui03-audit/\$n.json --inspect; done"
```

Verdicts of this run:

* **audit PASS** — 16 scenarios, 1350 inspected nodes, 0 structural failures. The
  four findings of UI-011/UI-012/UI-013 are listed as dated waivers (`waived until
  2027-03-31`); `UI-010..UI-013` show up as `unverified` in the detector table
  because their detectors are contract rules, not one of the two pixel detectors.
* **contracts PASS** — 163 checks, 0 failures, `region_min_content:frecuencia` on
  `main-otro-tema` as the only UNCHANGED DEFECT (UI-010).
* **themes PASS** — 3 shipped files, 27 pairs, 0 failures.

`main-tema-vision.png` is what UI-011 and UI-012 look like: the power clock and
the WiFi icon come out as empty boxes (`main-tema-vision.json` reports
`missing_glyphs` `["U+F240","U+F1E6"]` on the clock label and `["U+F1EB"]` on the
WiFi one) and `VFO-A` overflows its 56x26 cell (`text_overflow`). The PNG is
byte-identical to the one `tests/check_harness_determinism.sh` hashes:

```text
main-tema-vision.png          d59e7cf8601160f2188ade852486803cbbc2e97bab22352390c117ed1917d871
main-tema-outdoor-negative    31ea5cccb8c076e693ad3af419d7a2362357295fd842e65bc7a0eb058bcbe72d (not kept)
```

`main-tema-outdoor-negative.json` is the UI-013 evidence, and it is a measurement,
not a reading: the `theme` block separates the name that was rendered
(`"requested": "Outdoor-Negative"`) from the one the firmware's own parameter store
holds (`"params_name": "Outdoor-Negativ"`, `params_max_chars` 15). Its PNG is not
kept on purpose — it shows the screen the harness drew, while the defect is that
the firmware could never have drawn it.

The silent fallback that closes UI-013 is one probe away, and the truncated name
is exactly what the firmware would hand to `theme_init()`. Inside the harness
container, with the same build:

```sh
cd /src/tools/ui_harness/build
sed 's/^theme .*/theme Outdoor-Negativ/' ../fixtures/main-tema-outdoor-negative.scn > /tmp/probe.scn
./ui_harness --scenario /tmp/probe.scn --png /tmp/probe.png
# stderr: Theme Outdoor-Negativ not found, using IU1WXY
```
