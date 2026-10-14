# A/B UI report — AB-e2e-reappear

- Date: 2026-10-13
- Profile: `host-ui`
- A (baseline): `92d940529a9252d50b8eb4410611b957213ff6ac` (a)
- B (candidate): `61b44a13f551e3a5feafee6741ba74a2b9b17591` (b)
- Scenarios: `home-usb-20m`

## Verdict: **FAIL** (exit 1)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 1 scenarios captured on both revisions |
| Visual Regression | yes | FAIL | visual regressions vs the baseline |
| Quality Contract | yes | FAIL | 1 contract(s) violated by the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| home-usb-20m | no | 107 | 0.028% | 3 | barra_superior, multiplicador |

## Coverage (declared by the harness)

| Scenario | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|
| home-usb-20m | 14 | 4 | 3 | 4 |

Components declared below REAL (never written as REAL):

- band_info.c=PARTIAL, cfg (catalogue)=SYNTHETIC, cw_tune_ui.c=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL

## Known issues

| Issue | Fixture | Severity | Status | A | B | Classification |
|---|---|---|---|---|---|---|
| UI-001 | home-usb-20m | major | fixed | no | yes | REGRESSION |
| UI-002 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-005 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | main-otro-tema | major | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | unverified | — | — | UNVERIFIED |

7 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

10 contract checks, 1 failure(s).
- FAIL `layout_overlap` [home-usb-20m]: text labels overlap: '\x89EüH\x8bEè\x8b 12.1v\n\uf1e6 13.8v' at 591,0 and 'x1.0' at 762,10 (100% covered)

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
