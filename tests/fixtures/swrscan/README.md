# SWR Scan multipass — ready-to-use mock fixtures

Issue: https://github.com/iariap/x6200_gui/issues/125

**These are complete, deterministic sample files, not specifications or scripts that an agent must run to create data.** Run tests without X6200 hardware, amplifier, or RF transmissions.

## Files

- `swr_mock_samples.csv`: **1,428 measured-looking samples**, 51 frequencies for each of 3 or 5 passes, 8 scenarios; alternating frequency direction.
- `swr_expected_aggregates.csv`: **408 exact point-by-point expected aggregates**. Includes median SWR and median absolute deviation (MAD), in decimal SWR and integer tenths.
- `swr_scenarios.json`: scenario definitions, physical ground truth, expected minima and per-pass minima; read it for acceptance tests.
- `swr_mock_delayed_flow.csv`: **459 command/packet events**, 3 passes, with a stale reading after every frequency change and later valid reading. Injectable through a virtual clock.

## Scenarios

| ID | Span | Passes | Purpose |
| --- | --- | ---: | --- |
| `single_valley_3pass` | 100 kHz | 3 | Known 7.100 MHz dip and +/-0.1 SWR measurement noise |
| `outliers_5pass` | 100 kHz | 5 | Wrong low sample at point 6/pass 1 (SWR 1.0), wrong high at point 25/pass 3 (SWR 4.6) |
| `moving_5pass` | 100 kHz | 5 | Resonance moves 7.092 -> 7.108 MHz: report instability |
| `double_valley_3pass` | 100 kHz | 3 | Two real valleys; do not force one global parabola |
| `flat_3pass` | 100 kHz | 3 | All medians equal: no single credible resonance |
| `narrow_zoom_3pass` | 50 kHz | 3 | Sharp 7.103 MHz resonance resolvable at 1 kHz |
| `medium_zoom_3pass` | 200 kHz | 3 | Fixed 51 points at 4 kHz spacing |
| `wide_zoom_3pass` | 500 kHz | 3 | Identical sharp dip to narrow zoom, missed at 10 kHz. MOCK ONLY: frequencies extend outside 40m TX allocation |

## How to test

1. Load `swr_mock_samples.csv`. Within each `scenario`, group by `point_index` **not acquisition order**, because odd passes run backward. The frequencies for the same index must match.
2. Compute the median of `raw_swr_tenths` (integer 0.1 units) and median absolute deviation. Compare **every point** against `swr_expected_aggregates.csv`.
3. Cross-check minima and truth in `swr_scenarios.json`. A small median is NOT sufficient for a unique resonance in flat/moving cases.
4. Replay `swr_mock_delayed_flow.csv` through the fake hardware backend and a virtual clock. `set_frequency` is a fake tuning command; `swr_packet` offers SWR + timestamp + virtual TX status. **DO NOT pass `oracle_source_freq_hz` or `oracle_stale` to production acquisition code.** These are hidden oracle columns for tests.
5. For delayed-flow tests, assert stale packets are not misattributed to the newly requested frequency. A *test* waiting policy of 100 ms is illustrative only; real hardware settling behavior remains `requires-radio`.
6. Render the real SWR Scan UI in ASF/QEMU with these samples after each pass (1/3, 2/3, 3/3) and after final stop/freeze; test white/black themes, aborts and restarts.

## Hardware & correctness

- The X6200 reports scalar SWR (`pack->vswr`), not a complex S11 sample. Oracle columns are **not** real hardware signals.
- Synthetic dwell times of 150 or 210 ms are **not measured RF timing requirements** and are not an authorization to increase TX duration.
- **No real TX** is permitted in fixtures, CI, QEMU or harness; fake TX flag = 1 models state only.
- Tests also need explicit injection for dropped packets, NaN/Inf, absent TX, timeout and cancellation; provided curves are intentional ground-truth datasets, not exhaustive fault injection.
- A median can reject isolated outliers, but cannot correct frequency association errors, true antenna movement or systematic bias.
- A 500 kHz scan centered at 7.100 MHz extends beyond 40m: real RF should be blocked. Use this fixture only to test zoom/render and safety rejection.

Fixture contents generated offline, numerically checked, and committed to an isolated branch. No changes to production firmware.
