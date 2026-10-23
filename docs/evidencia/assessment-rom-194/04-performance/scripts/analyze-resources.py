#!/usr/bin/env python3
"""analyze-resources.py — #198: aggregate the /proc windows and `get` RTT CSVs.

Reads capturas/resources/<state>-r<rep>.csv and -rtt.csv produced by
scripts/run-resources.sh (res-sampler.c / rtt-sampler.c) and writes
capturas/resources-summary.csv + capturas/resources-summary.json.

CPU% is 100 * (delta(utime+stime)/CLK_TCK) / delta_t: percentage of ONE host
core, not of the Cortex-A7. RSS/PSS come from the QEMU process' /proc inside the
container (emulator + guest mappings), never "native RSS". /proc/PID/io is the
kernel's cached-accounting view: it does not measure SD wear or energy.
"""
from __future__ import annotations

import csv
import json
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CAP = ROOT / "capturas" / "resources"
OUT_CSV = ROOT / "capturas" / "resources-summary.csv"
OUT_JSON = ROOT / "capturas" / "resources-summary.json"


def pct(values: list[float], p: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = (p / 100.0) * (len(ordered) - 1)
    lo, hi = int(rank), min(int(rank) + 1, len(ordered) - 1)
    frac = rank - lo
    return ordered[lo] * (1 - frac) + ordered[hi] * frac


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def window_metrics(path: Path) -> dict:
    rows = read_rows(path)
    if len(rows) < 2:
        return {"samples": len(rows)}
    first, last = rows[0], rows[-1]
    dt = (int(last["ts_mono_ms"]) - int(first["ts_mono_ms"])) / 1000.0
    clk = int(first["clk_tck"]) or 100
    dticks = (int(last["utime_ticks"]) + int(last["stime_ticks"])) - (
        int(first["utime_ticks"]) + int(first["stime_ticks"])
    )
    cpu = 100.0 * (dticks / clk) / dt if dt > 0 else None
    main_ticks = sum(int(r.get("main_utime_ticks") or 0) + int(r.get("main_stime_ticks") or 0) for r in rows[1:])
    other_ticks = sum(int(r.get("other_utime_ticks") or 0) + int(r.get("other_stime_ticks") or 0) for r in rows[1:])
    main_cpu = 100.0 * (main_ticks / clk) / dt if dt > 0 else None
    other_cpu = 100.0 * (other_ticks / clk) / dt if dt > 0 else None
    top = {}
    for r in rows[1:]:
        tid = r.get("top_tid")
        if tid and tid != "-1":
            top[tid] = max(top.get(tid, 0), int(r.get("top_ticks") or 0))
    top_tid = max(top, key=top.get) if top else None
    top_cpu = 100.0 * (top[top_tid] / clk) / (int(rows[1]["ts_mono_ms"]) - int(rows[0]["ts_mono_ms"])) * 1000.0 if top_tid and dt > 0 else None
    ctxt = (int(last["vol_ctxt"]) + int(last["nonvol_ctxt"])) - (
        int(first["vol_ctxt"]) + int(first["nonvol_ctxt"])
    )
    io = {k: int(last[k]) - int(first[k]) for k in ("rchar", "wchar", "syscr", "syscw", "read_bytes", "write_bytes")}
    rss = [int(r["rss_kb"]) for r in rows if int(r["rss_kb"]) >= 0]
    pss = [int(r["pss_kb"]) for r in rows if int(r["pss_kb"]) >= 0]
    return {
        "samples": len(rows),
        "duration_s": round(dt, 3),
        "cpu_pct_one_core": round(cpu, 3) if cpu is not None else None,
        "main_cpu_pct": round(main_cpu, 3) if main_cpu is not None else None,
        "other_cpu_pct": round(other_cpu, 3) if other_cpu is not None else None,
        "top_tid": top_tid,
        "top_cpu_pct": round(top_cpu, 3) if top_cpu is not None else None,
        "tasks_max": max(int(r.get("tasks") or 0) for r in rows),
        "ctxt_switches_per_s": round(ctxt / dt, 2) if dt > 0 else None,
        "rss_kb_median": statistics.median(rss) if rss else None,
        "rss_kb_max": max(rss) if rss else None,
        "pss_kb_median": statistics.median(pss) if pss else None,
        "pss_kb_max": max(pss) if pss else None,
        "fds_min": min(int(r["fds"]) for r in rows),
        "fds_max": max(int(r["fds"]) for r in rows),
        "threads_min": min(int(r["threads"]) for r in rows),
        "threads_max": max(int(r["threads"]) for r in rows),
        "io_per_s": {k: round(v / dt, 1) if dt > 0 else None for k, v in io.items()},
        "log_bytes_per_min": round((int(last["log_bytes"]) - int(first["log_bytes"])) / dt * 60, 1)
        if int(first["log_bytes"]) >= 0 and dt > 0
        else None,
        "gui_log_bytes_per_min": round(
            (int(last["gui_log_bytes"]) - int(first["gui_log_bytes"])) / dt * 60, 1
        )
        if int(first["gui_log_bytes"]) >= 0 and dt > 0
        else None,
    }


def rtt_metrics(path: Path) -> dict:
    rows = read_rows(path)
    ok = [int(r["rtt_us"]) for r in rows if r["ok"] == "1"]
    bad = len(rows) - len(ok)
    return {
        "rtt_n": len(ok),
        "rtt_errors": bad,
        "rtt_p50_us": pct(ok, 50),
        "rtt_p95_us": pct(ok, 95),
        "rtt_p99_us": pct(ok, 99),
        "rtt_max_us": max(ok) if ok else None,
    }


def main() -> None:
    rows = []
    payload = {}
    for path in sorted(CAP.glob("*-r[0-9].csv")):
        stem = path.stem
        state, rep = stem.rsplit("-r", 1)
        metrics = window_metrics(path)
        rtt_path = path.with_name(f"{stem}-rtt.csv")
        if rtt_path.exists():
            metrics.update(rtt_metrics(rtt_path))
        payload[f"{state}#{rep}"] = metrics
        row = {"state": state, "rep": int(rep), **metrics}
        rows.append(row)

    # Aggregate the three repetitions per state (median of the per-rep values).
    agg = {}
    for state in sorted({r["state"] for r in rows}):
        reps = [r for r in rows if r["state"] == state]
        aggregate = {"state": state, "reps": len(reps)}
        for key in (
            "cpu_pct_one_core", "main_cpu_pct", "other_cpu_pct", "top_cpu_pct",
            "ctxt_switches_per_s", "rss_kb_median", "pss_kb_median",
            "fds_max", "threads_max", "log_bytes_per_min", "rtt_p50_us", "rtt_p95_us",
            "rtt_p99_us", "rtt_n",
        ):
            vals = [r[key] for r in reps if r.get(key) is not None]
            aggregate[key] = statistics.median(vals) if vals else None
        agg[state] = aggregate

    OUT_JSON.write_text(json.dumps({"windows": payload, "states": agg}, indent=2))
    keys = [
        "state", "rep", "samples", "duration_s", "cpu_pct_one_core", "main_cpu_pct",
        "other_cpu_pct", "top_tid", "top_cpu_pct", "tasks_max", "ctxt_switches_per_s",
        "rss_kb_median", "rss_kb_max", "pss_kb_median", "pss_kb_max", "fds_min", "fds_max",
        "threads_min", "threads_max", "log_bytes_per_min", "gui_log_bytes_per_min",
        "rtt_n", "rtt_errors", "rtt_p50_us", "rtt_p95_us", "rtt_p99_us", "rtt_max_us",
    ]
    with OUT_CSV.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys, extrasaction="ignore")
        w.writeheader()
        for row in rows:
            w.writerow(row)
    print(f"wrote {OUT_CSV} ({len(rows)} windows) and {OUT_JSON}")
    for state, a in agg.items():
        print(
            f"  {state:18s} cpu={a['cpu_pct_one_core']}% main={a['main_cpu_pct']}% "
            f"other={a['other_cpu_pct']}% ctxt/s={a['ctxt_switches_per_s']} "
            f"rss={a['rss_kb_median']}kB pss={a['pss_kb_median']}kB "
            f"rtt50={a['rtt_p50_us']}us rtt95={a['rtt_p95_us']}us"
        )


if __name__ == "__main__":
    main()
