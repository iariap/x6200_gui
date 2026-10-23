#!/usr/bin/env python3
"""analyze-bench.py — #198: turn the two x6200-ab JSONs into compact evidence.

Writes capturas/benchmark-per-scenario.csv (199 rows, the measurement) and
capturas/aa-comparisons.csv (A/A per scenario), plus a JSON summary with the
distribution and the verdict counts. Percentiles and the verdict come from
x6200_ab.performance, the JSON is the tool's own output: nothing is recomputed
with a different formula.
"""
from __future__ import annotations

import csv
import json
import statistics
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUN1 = ROOT / "logs" / "step1-benchmark" / "benchmark-host.json"
RUN_AA = ROOT / "logs" / "step2-aa" / "benchmark-aa.json"
OUT = ROOT / "capturas"
OUT.mkdir(exist_ok=True)


def load(path: Path) -> dict:
    return json.loads(path.read_text())


def main() -> None:
    run1 = load(RUN1)
    ms = run1["measurements"]
    keys = [
        "scenario", "frames", "render_p50_ms", "render_p95_ms", "render_p99_ms",
        "render_mean_ms", "render_stdev_ms", "dispersion_cv_pct", "fps", "flush_ms_mean",
        "draw_ms_mean", "flushes_mean", "redrawn_area_mean", "unique_redrawn_mean",
    ]
    with (OUT / "benchmark-per-scenario.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys + ["rss_bytes", "pss_bytes", "peak_vm_bytes"])
        w.writeheader()
        for m in ms:
            row = {k: m.get(k) for k in keys}
            mem = m.get("memory") or {}
            row.update({k: mem.get(k) for k in ("rss_bytes", "pss_bytes", "peak_vm_bytes")})
            w.writerow(row)

    summary = {
        "status": run1["status"],
        "exit_code": run1["exit_code"],
        "scenarios": len(ms),
        "frames_per_scenario": sorted({m["frames"] for m in ms}),
        "total_frames": sum(m["frames"] for m in ms),
        "unstable_scenarios": [
            {"scenario": m["scenario"], "dispersion_cv_pct": m["dispersion_cv_pct"]}
            for m in ms
            if m["dispersion_cv_pct"] > run1["budget"]["max_dispersion_cv_pct"]
            or m["frames"] < run1["budget"]["min_frames"]
        ],
        "distributions": {},
    }
    for key in ("render_p50_ms", "render_p95_ms", "render_p99_ms", "fps", "dispersion_cv_pct"):
        vals = [m[key] for m in ms]
        summary["distributions"][key] = {
            "min": round(min(vals), 4), "p50": round(statistics.median(vals), 4),
            "p95": round(sorted(vals)[int(0.95 * (len(vals) - 1))], 4), "max": round(max(vals), 4),
        }
    summary["slowest_p95"] = [
        {"scenario": m["scenario"], "p95_ms": m["render_p95_ms"]}
        for m in sorted(ms, key=lambda x: -x["render_p95_ms"])[:5]
    ]
    summary["rss_bytes_range"] = [
        min((m.get("memory") or {}).get("rss_bytes", 0) for m in ms),
        max((m.get("memory") or {}).get("rss_bytes", 0) for m in ms),
    ]

    if RUN_AA.exists():
        aa = load(RUN_AA)
        rows = []
        counts = Counter()
        for item in aa["comparisons"]:
            counts[item["status"]] += 1
            rows.append({
                "scenario": item["scenario"],
                "status": item["status"],
                "p95_delta_pct": item["p95_delta_pct"],
                "flushes_delta_pct": item["flushes_delta_pct"],
                "redrawn_area_delta_pct": item["redrawn_area_delta_pct"],
                "host_noise": item.get("host_noise"),
                "reasons": " | ".join(item.get("reasons", [])),
                "a_p95_ms": item["a"]["render_p95_ms"],
                "b_p95_ms": item["b"]["render_p95_ms"],
            })
        rows.sort(key=lambda r: -abs(r["p95_delta_pct"]))
        with (OUT / "aa-comparisons.csv").open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        summary["aa"] = {
            "status": aa["status"],
            "exit_code": aa["exit_code"],
            "counts": dict(counts),
            "p95_delta_pct": {
                "min": round(min(r["p95_delta_pct"] for r in rows), 4),
                "p50": round(statistics.median([r["p95_delta_pct"] for r in rows]), 4),
                "max": round(max(r["p95_delta_pct"] for r in rows), 4),
            },
            "proxies_unchanged": sum(
                1 for r in rows if r["flushes_delta_pct"] == 0 and r["redrawn_area_delta_pct"] == 0
            ),
            "worst": rows[:10],
        }
    (OUT / "benchmark-summary.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
