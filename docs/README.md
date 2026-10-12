# X6200 ROM documentation (LU2DSX)

Documentation for the X6200 alternative firmware built on the R1CBU/R2RFE lineage
(`gdyuldin/x6200_gui` + `gdyuldin/AetherX6200Buildroot`).

Most of these documents are written in Spanish; the technical reference is in English.
They are mirrored from the project workspace, where the firmware is built.

## Documents

| Document | Language | What it covers |
|---|---|---|
| [TRD.md](TRD.md) | EN | Technical reference: GPS/TTS removal, display, waterfall, About dialog, power budget, adaptive tuning, and the 2026-10-11 revision (§17) |
| [ROM_REVIEW.md](ROM_REVIEW.md) | ES | Audit of the built SD images: integrity, what each build really contains, documentation vs reality, and the fixes applied |
| [TESTING.md](TESTING.md) | ES | How to test the firmware without the radio: native unit tests, target-ISA tests with `qemu-arm-static`, dependency closure checks, QEMU and LVGL-simulator options |
| [X6200_FEASIBILITY.md](X6200_FEASIBILITY.md) | ES | Can the X6200 do what the X6100 firmware does? Platform, kernel, control library and base-firmware comparison |
| [X6100_FEATURE_GAP.md](X6100_FEATURE_GAP.md) | ES | Feature-by-feature comparison against IU1WXY's X6100 firmware (digital modes, DX cluster, Field Ops, themes, BT/wfview, ...) |
| [UPSTREAM_REBASE_PLAN.md](UPSTREAM_REBASE_PLAN.md) | ES | Upstream status and versions, how to rebase this fork, and how to mitigate the pending risks |
| [AUDIO_ENHANCEMENT_GUIDE.md](AUDIO_ENHANCEMENT_GUIDE.md) | EN | Audio enhancement module (notch API, SpeexDSP NR, AF leveler, presets) and its current status |
| [AUDIO_ENHANCEMENT_IMPLEMENTATION_PLAN.md](AUDIO_ENHANCEMENT_IMPLEMENTATION_PLAN.md) | EN | Original implementation plan (partially aspirational; see the guide for the current state) |
| [RELEASE_NOTES_2026-10-11.md](RELEASE_NOTES_2026-10-11.md) | EN | What the first published SD image contains, hashes, install steps and caveats |
| [FT8_UDP.md](FT8_UDP.md) | ES | WSJT-X compatible UDP broadcaster: config file, GridTracker/JTAlert usage, implementation notes and tests |
| [NEW_FEATURES.md](NEW_FEATURES.md) | ES | Running log of the features ported from the X6100 firmware: status, usage and tests |
| [UI_HARNESS.md](UI_HARNESS.md) | ES | Rendering the real GUI on a PC without the radio: the X6100 harness, why it is not drop-in, and the adaptation plan |
| [X6100_REPO_INDEX.md](X6100_REPO_INDEX.md) | ES | Catalogue of everything in IU1WXY's X6100 repo and what each piece is for in this port |
| [RESOURCE_USAGE.md](RESOURCE_USAGE.md) | ES | Measured RAM/CPU cost of the Python web configurator and the 128 MB CMA pool reserved for an unused GPU |

## Downloads

Prebuilt SD-card images are published as GitHub Releases (the image is ~545 MB, over
GitHub's 100 MB file limit, so it is not committed to the repository):

| Release | Image | Size | SHA256 |
|---|---|---|---|
| [lu2dsx-2026-10-11](https://github.com/iariap/x6200_gui/releases/tag/lu2dsx-2026-10-11) (beta) | `x6200_20261011_201456.img` | 545 MB | `d944fb9faff77603ec4d5c9c9812c66af31a2e99952ba09356301bdd6c545ea9` |

Read [RELEASE_NOTES_2026-10-11.md](RELEASE_NOTES_2026-10-11.md) first: it lists what is
included, how to write the card, and the OEM base-firmware version caveat.

## Project layout

The firmware is built from two repositories plus generated images:

```
x6200_gui/                     this repository (LVGL GUI, GitHub: iariap/x6200_gui)
AetherX6200Buildroot/          buildroot external tree (docs/UPSTREAM_REBASE_PLAN.md §3)
docs/                          these documents
images/                        generated SD-card images (sdcard.img, rootfs.tar)
```

The SD-card image itself is **not** in this repository (it is ~545 MB and GitHub's
file limit is 100 MB): see [Downloads](#downloads) above or build it with the
instructions in [UPSTREAM_REBASE_PLAN.md](UPSTREAM_REBASE_PLAN.md) and the build skill.

## Building

```bash
git clone https://github.com/iariap/AetherX6200Buildroot
git clone https://github.com/iariap/x6200_gui
# see the build skill: Docker + buildroot, GUI package rebuilt with the local source
```

The GUI package is built from the local source using Buildroot's
`X6200_GUI_OVERRIDE_SRCDIR` mechanism; the build skill documents the exact Docker
commands.

## Keeping these in sync

These files are mirrored from the build workspace, where the firmware project lives
(the workspace also holds `AetherX6200Buildroot/` and the generated `images/`). After
editing any document there, publish it with:

```bash
cp docs/*.md x6200_gui/docs/
git -C x6200_gui add docs
git -C x6200_gui commit -m "docs: update"
git -C x6200_gui push fork lu2dsx/x6200-rom
```

## License

The GUI and these documents follow the project license (LGPL-2.1-or-later for the GUI
sources; the original R1CBU firmware and the Buildroot external tree keep their own
licenses). See `LICENSE` in this repository.
