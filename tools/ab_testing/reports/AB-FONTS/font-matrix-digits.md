Baseline (the proprietary family): 43 text nodes, 0 overflow, 0 truncated, 0 missing glyph(s), 0 fallback

| family | score | boxes | ink | width |Δw| | p95 |Δw| | sign Δw | overflow | truncated | missing | cap(20) | cap(30) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| DSEG7 Classic (digits) **<- proposed default** | 0.27 | 0.27 | 0.00 | 0.06 | 0.00 | +0.1% | 0 | 0 | 0 | - | 15/15 |
| DSEG14 Classic (digits) | 0.30 | 0.30 | 0.00 | 0.06 | 0.00 | -0.1% | 0 | 0 | 0 | - | 15/15 |
| DSEG7 Classic Mini (digits) | 6.97 | 0.30 | 6.67 | 0.06 | 0.00 | +0.1% | 0 | 0 | 0 | - | 16/15 |

`score` = `boxes` + `ink`, both measured and in %. `boxes` = 0.65·mean|Δw|/w + 0.35·mean|Δh|/h over every text box of the fixture set, paired label by label against the baseline (0 = the same boxes the proprietary family produced); `ink` = the mean relative distance of the drawn height of `A` at the two slots of the top bar and the clock (column `cap(20)`/`cap(30)` against the baseline's own cap height). `p95|Δw|` is the worst 5% of labels; `sign Δw` says whether the family runs wide (+) or narrow (-). `!` marks a family that adds a legibility defect (overflow, truncation or a missing glyph) the baseline did not have: no score qualifies it. Lower is closer to the family being replaced.
