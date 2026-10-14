# A/B UI report — AB-e2e-fix

- Date: 2026-10-13
- Profile: `host-ui`
- A (baseline): `261ae08c7b6dd48c7d33327623d9775b20cf583f` (a)
- B (candidate): `ec1b0a58ec30270ece2a6b490ae41ed341133755` (b)
- Scenarios: `home-usb-20m`

## Verdict: **PASS (IMPROVEMENT: a known issue present in A is fixed in B)** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 1 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 9 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| home-usb-20m | no | 107 | 0.028% | 3 | barra_superior, multiplicador, reloj_potencia |

## Coverage (declared by the harness)

| Scenario | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|
| home-usb-20m | 14 | 4 | 3 | 4 |

Components declared below REAL (never written as REAL):

- band_info.c=PARTIAL, cfg (catalogue)=SYNTHETIC, cw_tune_ui.c=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL

## Known issues

| Issue | Fixture | Severity | Status | A | B | Classification |
|---|---|---|---|---|---|---|
| UI-001 | home-usb-20m | major | fixed | yes | no | FIXED |
| UI-009 | home-bateria-externa | major | fixed | — | — | UNVERIFIED |
| UI-010 | main-otro-tema | critical | verified | — | — | UNVERIFIED |
| UI-002 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-005 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | main-otro-tema | major | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | unverified | — | — | UNVERIFIED |

9 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

9 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
