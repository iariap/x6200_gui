Baseline (the proprietary family): 522 text nodes, 0 overflow, 0 truncated, 0 missing glyph(s), 6 fallback

| family | score | boxes | ink | width |Δw| | p95 |Δw| | sign Δw | overflow | truncated | missing | cap(20) | cap(30) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| stock (Fira Sans) **<- proposed default** | 20.26 | 14.70 | 5.56 | 21.43 | 34.52 | -21.4% | 0 | 0 | 0 | 10/9 | 15/15 |
| Inter | 26.11 | 11.67 | 14.44 | 15.90 | 30.85 | -15.9% | 0 | 0 | 0 | 11/9 | 16/15 |
| PT Sans | 27.98 | 13.54 | 14.44 | 19.03 | 28.57 | -19.0% | 0 | 0 | 0 | 11/9 | 16/15 |
| Roboto | 28.12 | 13.68 | 14.44 | 19.79 | 28.57 | -19.8% | 0 | 0 | 0 | 11/9 | 16/15 |
| Source Sans 3 | 29.02 | 14.58 | 14.44 | 20.77 | 33.33 | -20.8% | 0 | 0 | 0 | 11/9 | 16/15 |
| IBM Plex Sans | 29.52 | 6.19 | 23.33 | 8.74 | 19.05 | -8.0% | 0 | 0 | 0 | 12/9 | 17/15 |
| JetBrains Mono | 30.04 | 6.71 | 23.33 | 10.05 | 23.81 | +0.3% | 0 | 0 | 0 | 12/9 | 17/15 |
| DejaVu Sans | 31.85 | 5.18 | 26.67 | 6.96 | 19.05 | -4.6% | 0 | 0 | 0 | 12/9 | 18/15 |
| Ubuntu | 31.96 | 8.63 | 23.33 | 12.62 | 23.81 | -12.1% | 0 | 0 | 0 | 12/9 | 17/15 |
| Roboto Condensed | 33.39 | 18.95 | 14.44 | 27.90 | 38.10 | -27.9% | 0 | 0 | 0 | 11/9 | 16/15 |
| Noto Sans | 34.91 | 8.24 | 26.67 | 12.06 | 23.81 | -10.6% | 0 | 0 | 0 | 12/9 | 18/15 |
| Open Sans | 35.04 | 8.37 | 26.67 | 12.26 | 23.81 | -10.9% | 0 | 0 | 0 | 12/9 | 18/15 |
| Archivo Narrow | 48.93 | 13.37 | 35.56 | 20.31 | 28.57 | -20.3% | 0 | 0 | 0 | 13/9 | 19/15 |
| Archivo ! | 25.57 | 4.46 | 21.11 | 6.18 | 19.05 | -3.7% | 2 | 0 | 0 | 11/9 | 18/15 |
| Lato ! | 40.11 | 7.89 | 32.22 | 11.95 | 23.81 | -10.3% | 1 | 0 | 0 | 13/9 | 18/15 |

`score` = `boxes` + `ink`, both measured and in %. `boxes` = 0.65·mean|Δw|/w + 0.35·mean|Δh|/h over every text box of the fixture set, paired label by label against the baseline (0 = the same boxes the proprietary family produced); `ink` = the mean relative distance of the drawn height of `A` at the two slots of the top bar and the clock (column `cap(20)`/`cap(30)` against the baseline's own cap height). `p95|Δw|` is the worst 5% of labels; `sign Δw` says whether the family runs wide (+) or narrow (-). `!` marks a family that adds a legibility defect (overflow, truncation or a missing glyph) the baseline did not have: no score qualifies it. Lower is closer to the family being replaced.
