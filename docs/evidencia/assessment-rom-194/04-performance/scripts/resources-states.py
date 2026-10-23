#!/usr/bin/env python3
"""resources-states.py — #198: one CSV row per resource window with the state the
socket actually reported before/after the 60 s window."""
from __future__ import annotations

import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CAP = ROOT / "capturas" / "resources"
OUT = CAP / "states.csv"


def state(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except Exception:
        return {}


rows = []
for before in sorted(CAP.glob("*-r[0-9]-state-before.json")):
    stem = before.name.replace("-state-before.json", "")
    after = CAP / f"{stem}-state-after.json"
    b, a = state(before), state(after)
    rows.append({
        "window": stem,
        "state_requested": stem.rsplit("-r", 1)[0],
        "app_before": b.get("app", ""),
        "app_after": a.get("app", ""),
        "ax_before": json.dumps(b.get("ax", {}), separators=(",", ":")),
        "ax_after": json.dumps(a.get("ax", {}), separators=(",", ":")),
        "freq_before": b.get("freq"),
        "freq_after": a.get("freq"),
        "mode_before": b.get("mode"),
        "mode_after": a.get("mode"),
        "filter_before": b.get("filter"),
        "filter_after": a.get("filter"),
        "tx_before": b.get("tx"),
        "tx_after": a.get("tx"),
    })

with OUT.open("w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ["window"])
    w.writeheader()
    w.writerows(rows)
print(f"wrote {OUT} ({len(rows)} windows)")
