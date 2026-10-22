# X6200 alternative firmware — LU2DSX build (2026-10-11)

Non-invasive microSD firmware for the **Xiegu X6200**, based on the R1CBU/R2RFE
alternative firmware (`gdyuldin/x6200_gui` + `gdyuldin/AetherX6200Buildroot`) with the
LU2DSX changes. **Nothing is written to the radio**: boot with the card inserted, remove
it to go back to the OEM firmware.

## Asset

| File | Size | MD5 | SHA256 |
|---|---|---|---|
| `x6200_20261011_201456.img` | 571.473.920 B (545 MB) | `4a8264bf48d75a7eec3cdf712963d9d1` | `d944fb9faff77603ec4d5c9c9812c66af31a2e99952ba09356301bdd6c545ea9` |

Built from [`iariap/x6200_gui`](https://github.com/iariap/x6200_gui) branch
`lu2dsx/x6200-rom` at commit `db02829` (the later docs/tests commits do not affect the
image) plus [`iariap/AetherX6200Buildroot`](https://github.com/iariap/AetherX6200Buildroot)
commit `14d2aa1`.

## What's inside

- **GPS and TTS (RHVoice) removed** for power savings: no gpsd, no libgps, no RHVoice
  libraries or voice data (≈ 16 MB less rootfs).
- **About dialog** in Settings (version and author).
- **Adaptive tuning**: tuning step multiplier derived from rotation speed, with an
  on-screen `x%.1f` indicator, gated by the existing *Freq acceleration* setting
  (none = 1x, lite ≤ 10x, strong ≤ 100x).
- **Audio enhancement**: SpeexDSP noise reduction + AF leveler, selectable presets
  (Off / CW weak / SSB voice / AM broadcast / Digital) in Settings.
- **Waterfall battery mode** setting (skips 3 of 4 frames).
- Display refresh 30 → 50 ms (20 fps).
- **Fixes from the ROM review**: the audio enhancement chain now actually processes the
  ~4410-sample capture blocks (before, anything larger than one frame was dropped and the
  feature never ran); the tuning multiplier label hides again; the adaptive step no longer
  overflows; dead `EVENT_ROTARY` path removed; RHVoice/gpsd leftovers purged from the image.

Image verified: 2 partitions (32 MB FAT boot with kernel 6.1.82 + DTB + boot script,
512 MB ext4 rootfs), `e2fsck` clean, every GUI library dependency resolved inside the
rootfs.

## Install

1. Write the `.img` to a microSD card (balenaEtcher, Raspberry Pi Imager or `dd`).
2. Insert the card in the radio and power it on.
3. To go back to the OEM firmware, power off and remove the card.

First boot creates the `DATA` (FAT32) partition on the free space of the card, used for
the ADIF log, settings backup and recordings.

## Read this before flashing

- **Beta, and not yet tested on hardware by this fork's author**: there are no power,
  audio or field measurements. Test any transmission into a dummy load.
- **OEM BASE version matters**: this build sits on the GUI that predates the
  *BASE 1.0.7 flow format* support. If your radio runs OEM base **1.0.7 or newer**,
  spectrum / S-meter / battery may not display correctly — use the upstream `v0.1.2`
  build instead (see [UPSTREAM_REBASE_PLAN](https://github.com/iariap/x6200_gui/blob/lu2dsx/x6200-rom/docs/UPSTREAM_REBASE_PLAN.md)).
- Known gaps inherited from upstream: Bluetooth does not work, WSPR/wfview are not
  implemented, no 60 m. See
  [X6200_FEASIBILITY.md](https://github.com/iariap/x6200_gui/blob/lu2dsx/x6200-rom/docs/X6200_FEASIBILITY.md).

## Documentation

All project documents are in
[`docs/`](https://github.com/iariap/x6200_gui/tree/lu2dsx/x6200-rom/docs): technical
reference (TRD), ROM audit, testing without hardware, X6100 feature comparison and the
upstream rebase plan.

## Credits and license

Original firmware by **Oleg Belousov R1CBU**, alternative port and build system by
**Georgy Dyuldin R2RFE**; LU2DSX changes by **Pablo Iaria LU2DSX**. GUI sources are
LGPL-2.1-or-later; the Buildroot external tree keeps its own license (GPL-2.0).
