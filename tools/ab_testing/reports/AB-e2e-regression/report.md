# A/B UI report — AB-e2e-regression

- Date: 2026-10-13
- Profile: `host-ui`
- A (baseline): `ec1b0a58ec30270ece2a6b490ae41ed341133755` (a)
- B (candidate): `a7569b11d303df166157db9bfe4ae99d8ecbe55c` (b)
- Scenarios: `home-usb-20m`

## Verdict: **FAIL** (exit 1)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 1 scenarios captured on both revisions |
| Visual Regression | yes | FAIL | visual regressions vs the baseline |
| Quality Contract | yes | FAIL | 15 contract(s) violated by the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| home-usb-20m | no | 48267 | 12.570% | 5 | botones |

## Coverage (declared by the harness)

| Scenario | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|
| home-usb-20m | 14 | 4 | 3 | 4 |

Components declared below REAL (never written as REAL):

- band_info.c=PARTIAL, cfg (catalogue)=SYNTHETIC, cw_tune_ui.c=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL

## Known issues

| Issue | Fixture | Severity | Status | A | B | Classification |
|---|---|---|---|---|---|---|
| UI-001 | home-usb-20m | major | fixed | no | no | NOT_REPRODUCED |
| UI-009 | home-bateria-externa | major | fixed | — | — | UNVERIFIED |
| UI-002 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-005 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | main-otro-tema | major | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | unverified | — | — | UNVERIFIED |

8 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

24 contract checks, 15 failure(s).
- FAIL `layout_clipped` [home-usb-20m]: lv_obj at 0,448 160x62 cut to 160x32 (lost 0x30)
- FAIL `layout_touch_target` [home-usb-20m]: lv_obj at 0,448 160x62 is 160x32, below the 44px touch target
- FAIL `layout_clipped` [home-usb-20m]: lv_label at 26,467 108x25 cut to 108x13 (lost 0x12)
- FAIL `layout_clipped` [home-usb-20m]: lv_obj at 160,448 160x62 cut to 160x32 (lost 0x30)
- FAIL `layout_touch_target` [home-usb-20m]: lv_obj at 160,448 160x62 is 160x32, below the 44px touch target
- FAIL `layout_clipped` [home-usb-20m]: lv_label at 193,454 95x50 cut to 95x26 (lost 0x24)
- FAIL `layout_clipped` [home-usb-20m]: lv_obj at 320,448 160x62 cut to 160x32 (lost 0x30)
- FAIL `layout_touch_target` [home-usb-20m]: lv_obj at 320,448 160x62 is 160x32, below the 44px touch target
- FAIL `layout_clipped` [home-usb-20m]: lv_label at 349,454 103x50 cut to 103x26 (lost 0x24)
- FAIL `layout_clipped` [home-usb-20m]: lv_obj at 480,448 160x62 cut to 160x32 (lost 0x30)
- FAIL `layout_touch_target` [home-usb-20m]: lv_obj at 480,448 160x62 is 160x32, below the 44px touch target
- FAIL `layout_clipped` [home-usb-20m]: lv_label at 510,454 101x50 cut to 101x26 (lost 0x24)
- FAIL `layout_clipped` [home-usb-20m]: lv_obj at 640,448 160x62 cut to 160x32 (lost 0x30)
- FAIL `layout_touch_target` [home-usb-20m]: lv_obj at 640,448 160x62 is 160x32, below the 44px touch target
- FAIL `layout_clipped` [home-usb-20m]: lv_label at 660,454 120x50 cut to 120x26 (lost 0x24)

## Evidence

- Screenshots: `screenshots/`
- Diffs: `diffs/`
- Logs: `logs/`
- Docker image: `x6200-uiharness`

### Gates without instrumentation (BLOCKED, never PASS)

- **Hardware**: no physical X6200 attached; hardware behaviour is not measured
  - cause: the framework runs on the host and never in the ROM
  - needed: an X6200 unit with the capture/instrumentation flow of milestone 7
- **Energy**: no power measurement; energy evidence must be physical
  - cause: render counters are not joules
  - needed: physical energy measurement (milestone 7)
