#!/usr/bin/env python3
"""analyse-bench.py — arma `cpu-arm.csv` del gate G2 (issue #347) a partir de los
JSON crudos del banco ARM, los exit codes y el monitoreo 1 Hz de la radio.

Directorio de datos (<logs>):
  bench_<variant>_<tag>.json   una linea JSON por pasada/medicion (salida del banco)
  exit_<variant>_<tag>.txt     exit code de cada pasada (124 = timeout de 30 s)
  protocol-timeout.json        pasada de protocolo (60 s de audio) que excedio el timeout
  monitor.log                  muestreo 1 Hz de las mediciones acotadas (monitor.sh)
  monitor-protocol.log         muestreo 1 Hz de la pasada de protocolo
  control-frame-size.txt / control-off.json / gui-sha post (se resumen en README)

Salida: <evidencia>/cpu-arm.csv con las columnas pedidas por el issue #347.
No juzga GO/NO-GO: el veredicto vive en README.md.

Uso: python3 analyse-bench.py [--data DIR] [--out CSV]
SPDX-License-Identifier: LGPL-2.1-or-later
"""
import argparse
import csv
import json
import re
import statistics
import sys
from pathlib import Path

EV = Path(__file__).resolve().parent.parent
DEFAULT_DATA = EV / "logs" / "arm-bench"
DEFAULT_OUT = EV / "cpu-arm.csv"

ARM_ARCH = "ARMv7 Cortex-A7 x4 (sun8i-a33, rev 5 v7l), 1008 MHz performance"
TOOLCHAIN = "arm-buildroot-linux-gnueabihf-gcc 12.3.0 (Buildroot 2023.02.11-dirty) -O2"
GUI_SHA = "b1157fdb3be99302fc46b609228f4eb4bd84cfe0510ee580efa642fa7bdf0022"
INPUT_SHA = "b718cf2c118cfb465a0e7894a7eaa7baa23719406736e494d0fbf7fcd1aaa56e"
COLS = ["case", "arm_arch", "toolchain", "gui_sha", "input_sha", "method", "frame_samples",
        "sample_rate", "run", "p50_ms", "p95_ms", "p99_ms", "mean_ms", "over_10ms",
        "rss_kb", "gui_cpu_pct", "run_exit", "evidence_class", "notes"]


def parse_monitor(path: Path):
    """Estadisticas del muestreo 1 Hz (CPU GUI en % de 1 nucleo, idle total, load1)."""
    if not path.exists():
        return None
    gui, idle, load, aborts = [], [], [], []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        if line.startswith("SAFETY") or line.startswith("MONITOR"):
            aborts.append(line.strip())
            continue
        p = line.split()
        if len(p) < 8:
            continue
        try:
            g, i = float(p[6]), float(p[7])
            if g == 0.0 and i == 0.0:
                continue  # primera muestra sin delta
            load.append(float(p[3]))
            gui.append(g)
            idle.append(i)
        except ValueError:
            continue
    if not gui:
        return None
    return {"n": len(gui), "gui_med": round(statistics.median(gui), 2), "gui_max": round(max(gui), 2),
            "idle_med": round(statistics.median(idle), 2), "idle_min": round(min(idle), 2),
            "load_med": round(statistics.median(load), 2), "load_max": round(max(load), 2),
            "aborts": aborts}


def load_entries(data: Path):
    entries = []
    for f in sorted(data.glob("bench_*.json")):
        m = re.match(r"bench_(default|neonoff)_(.+)\.json$", f.name)
        if not m:
            continue
        lines = [ln for ln in f.read_text(encoding="utf-8", errors="replace").splitlines() if ln.strip()]
        if not lines:
            continue
        try:
            d = json.loads(lines[-1])
        except json.JSONDecodeError:
            continue
        variant, tag = m.group(1), m.group(2)
        exit_f = data / f"exit_bench_{variant}_{tag}.txt"
        rc = exit_f.read_text(encoding="utf-8").strip() if exit_f.exists() else "0"
        d.update(_variant=variant, _tag=tag, _exit=rc)
        entries.append(d)
    return entries


def row_from_entry(d, gui_cpu, measured=True):
    units = d.get("units") or 0
    total = d.get("total_ms") if isinstance(d.get("total_ms"), (int, float)) else d.get("wall_ms")
    mean = round(total / units, 6) if units and isinstance(total, (int, float)) else "UNKNOWN"
    stages = (f"resample_up_p50={d.get('up_p50_ms')} rnnoise_p50={d.get('nr_p50_ms')} "
              f"resample_down_p50={d.get('down_p50_ms')} rnnoise_frames={d.get('rnnoise_frames')} "
              f"total_ms={total} units={units}")
    variant = d.get("_variant")
    if variant == "neonoff":
        stages += " build=DISABLE_NEON (C portable, sonda de sensibilidad)"
    else:
        stages += " build=toolchain-default (__ARM_NEON fijado por el toolchain)"
    if not measured:
        stages += " (diagnostico acotado, no protocolo de 60 s)"
    return {
        "case": d.get("case", ""),
        "arm_arch": ARM_ARCH,
        "toolchain": TOOLCHAIN,
        "gui_sha": GUI_SHA,
        "input_sha": INPUT_SHA,
        "method": d.get("method", "rnnoise"),
        "frame_samples": "441 in / 480 at 48k",
        "sample_rate": "44100 (48000 interno)",
        "run": d.get("_tag", ""),
        "p50_ms": d.get("frame_p50_ms"),
        "p95_ms": d.get("frame_p95_ms"),
        "p99_ms": d.get("frame_p99_ms"),
        "mean_ms": mean,
        "over_10ms": d.get("over_10ms"),
        "rss_kb": d.get("rss_peak_kb"),
        "gui_cpu_pct": gui_cpu,
        "run_exit": d.get("_exit", "UNKNOWN"),
        "evidence_class": "MEASURED-HARDWARE",
        "notes": stages,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, default=DEFAULT_DATA)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()

    data = args.data
    entries = load_entries(data)
    mon = parse_monitor(data / "monitor.log")
    mon_proto = parse_monitor(data / "monitor-protocol.log")
    gui = mon["gui_med"] if mon else "UNKNOWN"
    rows = []

    # 1) pasada de protocolo (60 s de audio por pasada, timeout 30 s): la que fallo
    proto_f = data / "protocol-timeout.json"
    if proto_f.exists():
        p = json.loads(proto_f.read_text(encoding="utf-8"))
        rows.append({
            "case": p["case"], "arm_arch": ARM_ARCH, "toolchain": TOOLCHAIN, "gui_sha": GUI_SHA,
            "input_sha": INPUT_SHA, "method": "rnnoise", "frame_samples": "441 in / 480 at 48k",
            "sample_rate": "44100 (48000 interno)", "run": "warmup1 (protocolo 60 s)",
            "p50_ms": "UNKNOWN", "p95_ms": "UNKNOWN", "p99_ms": "UNKNOWN", "mean_ms": "UNKNOWN",
            "over_10ms": "UNKNOWN", "rss_kb": "UNKNOWN", "gui_cpu_pct": (mon_proto or {}).get("gui_med", "UNKNOWN"),
            "run_exit": "124", "evidence_class": "MEASURED-HARDWARE",
            "notes": ("timeout de 30 s por pasada (mandato del issue) con 60 s de audio: NO se completaron "
                      "los 6000 frames; cota inferior mean > 5.0 ms/frame. La pasada de 60 s se aborto como "
                      "manda el protocolo (no se corrieron las 3+5). Monitor: " +
                      (f"load1 max={mon_proto['load_max']} gui_cpu% mediana={mon_proto['gui_med']} "
                       f"max={mon_proto['gui_max']} idle% min={mon_proto['idle_min']}"
                       if mon_proto else "sin datos")),
        })

    # 2) mediciones acotadas (10 s de audio = 1000 frames), mismo binario y fixture
    for d in entries:
        method = d.get("method", "rnnoise")
        measured = method != "off"
        rows.append(row_from_entry(d, gui if measured else 0, measured))

    # 3) agregado por variante: mediana de las pasadas de rnnoise (nunca de las sondas)
    for variant in ("default", "neonoff"):
        mp = [d for d in entries if d.get("_variant") == variant
              and d.get("method") == "rnnoise" and d.get("_tag", "").startswith("pass")]
        if not mp:
            continue
        means, over, rss = [], 0, 0
        for d in mp:
            units = d.get("units") or 0
            tot = d.get("total_ms") if isinstance(d.get("total_ms"), (int, float)) else d.get("wall_ms")
            if units and isinstance(tot, (int, float)):
                means.append(tot / units)
            over += d.get("over_10ms") or 0
            rss = max(rss, d.get("rss_peak_kb") or 0)
        med = lambda k: (round(statistics.median([d[k] for d in mp if isinstance(d.get(k), (int, float))]), 6)
                         if any(isinstance(d.get(k), (int, float)) for d in mp) else "UNKNOWN")
        rows.append({
            "case": f"aggregate_{variant}", "arm_arch": ARM_ARCH, "toolchain": TOOLCHAIN,
            "gui_sha": GUI_SHA, "input_sha": INPUT_SHA, "method": "rnnoise",
            "frame_samples": "441 in / 480 at 48k", "sample_rate": "44100 (48000 interno)",
            "run": f"median-of-{len(mp)}",
            "p50_ms": med("frame_p50_ms"), "p95_ms": med("frame_p95_ms"), "p99_ms": med("frame_p99_ms"),
            "mean_ms": round(statistics.median(means), 6) if means else "UNKNOWN",
            "over_10ms": over, "rss_kb": rss or "UNKNOWN",
            "gui_cpu_pct": gui if variant == "default" else "",
            "run_exit": "0" if all(d.get("_exit", "0") == "0" for d in mp) else "MIXED",
            "evidence_class": "MEASURED-HARDWARE",
            "notes": (f"mediana de {len(mp)} pasadas medidas de 10 s de audio (no protocolo 60 s: el protocolo "
                      f"no pasa el timeout de 30 s); over_10ms=suma; rss=max; umbral G2: mean<5 ms, "
                      f"p99<10 ms, 0 frames >10 ms, 0 xrun/dano. gui_cpu_pct=mediana monitor 1 Hz."),
        })

    with args.out.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=COLS)
        w.writeheader()
        for r in rows:
            w.writerow(r)

    print(f"escrito {args.out} ({len(rows)} filas)")
    for m, name in ((mon_proto, "protocolo-60s"), (mon, "mediciones-acotadas")):
        if m:
            print(f"monitor[{name}]: n={m['n']} gui%med={m['gui_med']} max={m['gui_max']} "
                  f"idle%min={m['idle_min']} load1 max={m['load_max']} aborts={m['aborts'] or 'ninguno'}")
    for r in rows:
        if str(r["case"]).startswith("aggregate_"):
            print(f"[{r['case']}] p50={r['p50_ms']} p95={r['p95_ms']} p99={r['p99_ms']} "
                  f"mean={r['mean_ms']} over_10ms={r['over_10ms']} exit={r['run_exit']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
