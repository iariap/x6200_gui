#!/usr/bin/env python3
"""render-resources-md.py — #198: markdown table for README §6 from the summary."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
data = json.loads((ROOT / "capturas" / "resources-summary.json").read_text())
states = data.get("states", {})
windows = data.get("windows", {})

labels = {
    "home-rx": "home RX (sin cliente)",
    "home-rx-get1hz": "home RX + get 1 Hz",
    "settings": "Settings abierto",
    "dfl-editor": "DFLEditor abierto",
    "recorder-idle": "Recorder inactivo",
    "cw-decoder": "CW decoder sin señal",
    "prop-open": "Propagation abierto",
    "prop-sweep": "Propagation barrido",
    "prop-cancel": "Propagation cancelado",
    "home-rx-get5hz": "home RX + get 5 Hz",
}

print("| Estado | Reps | CPU hilo GUI | CPU otros hilos | ctxt/s | RSS (kB) | PSS (kB) | fds | threads | RTT p50/p95/p99 (µs) | log (B/min) |")
print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|")
for st, a in states.items():
    rtt = "—"
    if a.get("rtt_n"):
        vals = [a.get("rtt_p50_us"), a.get("rtt_p95_us"), a.get("rtt_p99_us")]
        rtt = "/".join("—" if v is None else f"{v:.0f}" for v in vals) + f" (n={a['rtt_n']})"
    vals = [windows.get(f"{st}#{i}", {}).get("main_cpu_pct") for i in range(1, 4)]
    vals = [v for v in vals if v is not None]
    rng = f"{min(vals):.2f}–{max(vals):.2f}" if vals else "—"
    print(
        f"| {labels.get(st, st)} | {a.get('reps')} | {a.get('main_cpu_pct')} % ({rng}) | "
        f"{a.get('other_cpu_pct')} % | {a.get('ctxt_switches_per_s')} | "
        f"{a.get('rss_kb_median')} | {a.get('pss_kb_median')} | {a.get('fds_max')} | "
        f"{a.get('threads_max')} | {rtt} | {a.get('log_bytes_per_min')} |"
    )

print()
print("Reps = ventanas de 60 s; CPU% = porcentaje de UN core del host (QEMU_USER), no Cortex-A7. ")
print("`otros hilos` incluye el lazo degradado del radio_thread sin BASE (~100 %); no es reposo del equipo.")
print("RTT = socket `get` observable; p99 sólo si n≥60.")
