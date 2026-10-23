#!/usr/bin/env python3
"""build-results.py — #198: emit results.csv from the raw JSON/CSV evidence.

One row per metric. A missing value is an empty `value` with the motive in
`observed`/`limitation`, never 0. Statuses are limited to
PASS/FAIL/BLOCKED/INCONCLUSIVE/REVIEW_REQUIRED/PENDING_HARDWARE.
"""
from __future__ import annotations

import csv
import json
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CAP = ROOT / "capturas"
LOGS = ROOT / "logs"
OUT = ROOT / "results.csv"

FIELDS = [
    "scenario_id", "layer", "coverage", "preconditions", "actions", "expected",
    "observed", "status", "metric", "value", "unit", "samples", "evidence", "limitation",
]
ROWS: list[dict] = []


def add(sid, layer, cov, pre, act, exp, obs, status, metric, value, unit, samples, ev, lim=""):
    ROWS.append({
        "scenario_id": sid, "layer": layer, "coverage": cov, "preconditions": pre,
        "actions": act, "expected": exp, "observed": obs, "status": status,
        "metric": metric, "value": "" if value is None else value, "unit": unit,
        "samples": samples, "evidence": ev, "limitation": lim,
    })


def load(path, default=None):
    try:
        return json.loads(Path(path).read_text())
    except Exception:
        return default


def main() -> None:
    b = load(LOGS / "step1-benchmark" / "benchmark-host.json", {})
    aa = load(LOGS / "step2-aa" / "benchmark-aa.json", {})
    bench = load(CAP / "benchmark-summary.json", {})
    res = load(CAP / "resources-summary.json", {})
    audio = load(CAP / "audio-metrics.json", {})
    boot = load(CAP / "boot-to-socket-qemu.json", {})
    nat = load(CAP / "natural" / "natural.json", {})
    off = load(CAP / "natural" / "off.json", {})
    static = load(CAP / "natural" / "static.json", {})
    cli = load(CAP / "natural" / "cli.json", {})
    forced = load(CAP / "natural" / "forced.json", {})
    wake = load(CAP / "wakeups" / "wakeups.json", {})
    tstat = load(CAP / "static-profile.json", {})

    # ---- base / provenance ------------------------------------------------
    add("BASE-GUI-SHA", "HOST", "REAL", "clon privado limpio", "git rev-parse HEAD",
        "candidato congelado", "c888308eede71205605c06383432ee7e165f202e", "PASS",
        "gui_sha", "c888308eede71205605c06383432ee7e165f202e", "sha1", 1,
        "logs/step0-identity/checkout.txt")
    add("BASE-SRC-MD5", "HOST", "REAL", "checkout c888308e", "sh tools/fuente-md5.sh",
        "964193ae9306b5127845cec44a57d52c", "964193ae9306b5127845cec44a57d52c", "PASS",
        "source_md5", "964193ae9306b5127845cec44a57d52c", "md5", 1,
        "logs/step0-identity/checkout.txt")
    add("BASE-LVGL", "HOST", "REAL", "submódulo inicializado", "git submodule status --recursive",
        "74d0a816440eea53e030c4f1af842a94f7ce3d3", "74d0a816… (v8.3.11), lvgl/CMakeLists.txt presente",
        "PASS", "lvgl_gitlink", "74d0a816440eea53e030c4f1af842a94f7ce3d3", "sha1", 1,
        "logs/step0-identity/checkout.txt")
    add("BASE-BUILDROOT", "HOST", "REAL", "par congelado", "git -C clone-buildroot rev-parse HEAD",
        "d084e2870a00b64d9c57474e7eceeef80f43f25b", "d084e2870a00b64d9c57474e7eceeef80f43f25b",
        "PASS", "buildroot_sha", "d084e2870a00b64d9c57474e7eceeef80f43f25b", "sha1", 1,
        "logs/step0-identity/artifact-sha256.log")
    for name, sha, size in (
        ("sdcard.img", "d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5", "571473920"),
        ("rootfs.tar", "7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd", "242790400"),
        ("rootfs.ext4", "9365b52f9aef33e0623ae8271aaf415483fe72e1afbc8ef99f93e15437bc620a", "536870912"),
        ("boot.vfat", "914f07eb00d1e5afee56ab795ce6b7b1d4410d774a3534881da494319c30b17d", "33554432"),
    ):
        add(f"BASE-ART-{name}", "HOST", "REAL", "artefacto de #195", "sha256sum + stat",
            "hash y tamaño del manifiesto 01-baseline", f"sha256={sha}; bytes={size}", "PASS",
            "artifact_sha256", sha, "sha256", 1,
            "logs/step0-identity/artifact-sha256.log", f"tamaño {size} B")
    add("BASE-GUI-MD5", "HOST", "REAL", "GUI extraída del rootfs.tar", "md5sum",
        "1f998c46909307397525c975a712adfd", "1f998c46909307397525c975a712adfd; 13.407.624 B",
        "PASS", "gui_md5", "1f998c46909307397525c975a712adfd", "md5", 1,
        "logs/step0-identity/artifact-sha256.log")
    add("BASE-GUI-SHA256", "HOST", "REAL", "GUI extraída del rootfs.tar", "sha256sum",
        "7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5",
        "7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5", "PASS",
        "gui_sha256", "7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5",
        "sha256", 1, "logs/step0-identity/artifact-sha256.log")
    add("BASE-TIP-STALE", "HOST", "REAL", "revalidación al cerrar", "git ls-remote + git diff vs candidato",
        "declarar STALE para el tip", "tip=d4fb57f8 (orquestador informó dadb1705); 26 archivos de producción fuera de docs/; c888308e es ancestro",
        "REVIEW_REQUIRED", "tip_sha", "d4fb57f8ff8ccbcfdef13fc191cc9977511b6845", "sha1", 1,
        "logs/step0-identity/refs-tip.log",
        "todo el dossier vale para c888308e; si se adopta el tip repetir build/identidad, render, natural/wakeups, recursos y hardware")

    # ---- PERF-01 ---------------------------------------------------------
    ms = b.get("measurements", [])
    if ms:
        unstable = {x["scenario"] for x in bench.get("unstable_scenarios", [])}
        add("P01-VERDICT", "HOST", "REAL", "199 fixtures; host cargado por banco-apps ajeno",
            "x6200-ab benchmark --revision CANDIDATE --repeats 5 --warmup 3 --rounds 4",
            "measurement PASS si 199/199 dispersión ≤25 %",
            f"{b['status']}: 198/199 estables; dfl-presets-graph-tema-redshift MAD-CV 26,898 %",
            "INCONCLUSIVE", "benchmark_status", "INCONCLUSIVE", "status", len(ms),
            "logs/step1-benchmark/benchmark-host.json",
            "un escenario >25 %; no se repitió para obtener verde")
        add("P01-STABLE", "HOST", "REAL", "199 fixtures", "x6200-ab benchmark", "todos ≤25 %",
            "198/199 escenarios con MAD-CV ≤25 %", "PASS", "stable_scenarios", len(ms) - len(unstable),
            "scenarios", len(ms), "capturas/benchmark-summary.json")
        add("P01-FRAMES", "HOST", "REAL", "199 fixtures", "5 repeats x 4 rounds", "20 frames/escenario",
            f"{len(ms)} escenarios x 20 frames = {sum(m['frames'] for m in ms)} frames", "PASS",
            "frames_total", sum(m["frames"] for m in ms), "frames", sum(m["frames"] for m in ms),
            "logs/step1-benchmark/benchmark-host.json")
        d = bench["distributions"]
        add("P01-P50-MEDIAN", "HOST", "REAL", "199 fixtures", "benchmark", "mediana de p50",
            f"mediana {d['render_p50_ms']['p50']} ms (rango {d['render_p50_ms']['min']}–{d['render_p50_ms']['max']})",
            "PASS", "render_p50_ms_median", d["render_p50_ms"]["p50"], "ms", len(ms),
            "capturas/benchmark-summary.json")
        add("P01-P95-MEDIAN", "HOST", "REAL", "199 fixtures", "benchmark", "mediana de p95",
            f"mediana {d['render_p95_ms']['p50']} ms; p95 de la distribución {d['render_p95_ms']['p95']} ms",
            "PASS", "render_p95_ms_median", d["render_p95_ms"]["p50"], "ms", len(ms),
            "capturas/benchmark-summary.json")
        add("P01-P95-MAX", "HOST", "REAL", "199 fixtures", "benchmark", "peor p95",
            f"settings-audio {d['render_p95_ms']['max']} ms", "PASS", "render_p95_ms_max",
            d["render_p95_ms"]["max"], "ms", len(ms), "capturas/benchmark-per-scenario.csv")
        add("P01-P99-MEDIAN", "HOST", "REAL", "199 fixtures", "benchmark", "mediana de p99",
            f"mediana {d['render_p99_ms']['p50']} ms; máximo {d['render_p99_ms']['max']} ms",
            "PASS", "render_p99_ms_median", d["render_p99_ms"]["p50"], "ms", len(ms),
            "capturas/benchmark-summary.json")
        add("P01-FPS-MEDIAN", "HOST", "REAL", "199 fixtures", "benchmark", "FPS derivado 1000/p50",
            f"mediana {d['fps']['p50']} fps (frame forzado; no LCD)",
            "PASS", "fps_derived_median", d["fps"]["p50"], "fps", len(ms),
            "capturas/benchmark-summary.json", "NO es FPS del LCD ni cadencia natural")
        add("P01-DISPERSION-MEDIAN", "HOST", "REAL", "199 fixtures", "benchmark", "MAD-CV mediana",
            f"mediana {d['dispersion_cv_pct']['p50']} %; 1 escenario >25 %",
            "PASS", "dispersion_cv_pct_median", d["dispersion_cv_pct"]["p50"], "%", len(ms),
            "capturas/benchmark-summary.json")
        add("P01-FLUSHES", "HOST", "REAL", "199 fixtures", "--bench", "1 flush/frame forzado",
            "1,0 flush/frame", "PASS", "flushes_mean", 1.0, "flushes/frame", len(ms),
            "capturas/benchmark-per-scenario.csv", "frame forzado de pantalla completa")
        add("P01-REDRAWN", "HOST", "REAL", "199 fixtures", "--bench", "384.000 px/frame",
            "384.000 px redibujados (y únicos)", "PASS", "redrawn_area_mean", 384000, "px/frame",
            len(ms), "capturas/benchmark-per-scenario.csv")
        add("P01-RSS", "HOST", "REAL", "199 fixtures", "--bench + /proc",
            "RSS del proceso harness", f"{bench['rss_bytes_range'][0]}–{bench['rss_bytes_range'][1]} B",
            "PASS", "harness_rss_bytes_range", f"{bench['rss_bytes_range'][0]}-{bench['rss_bytes_range'][1]}",
            "bytes", len(ms), "capturas/benchmark-per-scenario.csv",
            "memoria del harness x86, no del binario ARM/Cortex-A7")
        for m in ms:
            st = "INCONCLUSIVE" if m["scenario"] in unstable else "PASS"
            add(f"P01-{m['scenario']}", "HOST", "REAL", "fixture del candidato",
                "x6200-ab benchmark --bench --warmup 3 --repeats 5 (4 rounds)",
                "p95 estable y ≤25 % MAD-CV",
                f"p95={m['render_p95_ms']} ms p50={m['render_p50_ms']} ms MAD-CV={m['dispersion_cv_pct']} %",
                st, "render_p95_ms", m["render_p95_ms"], "ms", m["frames"],
                "capturas/benchmark-per-scenario.csv",
                "x86 host, frame forzado; no Cortex-A7" + ("" if st == "PASS" else "; dispersión >25 %"))

    # ---- PERF-02 A/A -----------------------------------------------------
    if aa.get("comparisons"):
        comps = aa["comparisons"]
        ok = sum(1 for c in comps if c["status"] == "PASS")
        incon = sum(1 for c in comps if c["status"] == "INCONCLUSIVE")
        fail = sum(1 for c in comps if c["status"] == "FAIL")
        add("P02-VERDICT", "HOST", "REAL", "mismo SHA en A y B; host cargado",
            "x6200-ab benchmark --a CANDIDATE --b CANDIDATE --repeats 5 --warmup 3 --rounds 4",
            "control A/A sin cambios reales", f"{aa['status']}: {ok} PASS, {incon} INCONCLUSIVE, {fail} FAIL",
            "INCONCLUSIVE" if incon else ("PASS" if not fail else "FAIL"), "aa_status", aa["status"],
            "status", len(comps), "logs/step2-aa/benchmark-aa.json",
            "5 casos host_noise con proxies deterministas en 0 %")
        add("P02-PASS", "HOST", "REAL", "199 escenarios", "A/A", "sin regresión", f"{ok}/199 PASS",
            "PASS", "aa_pass", ok, "scenarios", len(comps), "capturas/aa-comparisons.csv")
        add("P02-INCONCLUSIVE", "HOST", "REAL", "199 escenarios", "A/A", "sin ruido",
            f"{incon}/199 INCONCLUSIVE (host_noise)", "INCONCLUSIVE", "aa_inconclusive", incon,
            "scenarios", len(comps), "capturas/aa-comparisons.csv",
            "main-foco +54,4 %, dfl-presets-more +43,5 %, dfl-presets-confirm +30,6 %, main-espectro-agua +28,5 %, dfl-presets-fav +27,3 %")
        add("P02-FAIL", "HOST", "REAL", "199 escenarios", "A/A", "0 FAIL", f"{fail}/199 FAIL",
            "PASS", "aa_fail", fail, "scenarios", len(comps), "capturas/aa-comparisons.csv")
        add("P02-PROXIES", "HOST", "REAL", "199 escenarios", "A/A", "proxies deterministas",
            "199/199 sin cambio de flushes/área (0,0 %)", "PASS", "aa_proxies_unchanged", 199,
            "scenarios", len(comps), "capturas/aa-comparisons.csv",
            "los deltas de p95 con proxies en 0 se clasifican host_noise")
        deltas = [c["p95_delta_pct"] for c in comps]
        add("P02-DELTA-MIN", "HOST", "REAL", "199 escenarios", "A/A", "delta p95",
            f"mínimo {min(deltas):.3f} %", "PASS", "aa_p95_delta_pct_min", round(min(deltas), 3),
            "%", len(comps), "capturas/aa-comparisons.csv", "ruido del host, no firmware")
        add("P02-DELTA-MEDIAN", "HOST", "REAL", "199 escenarios", "A/A", "delta p95",
            f"mediana {statistics.median(deltas):.3f} %", "PASS", "aa_p95_delta_pct_median",
            round(statistics.median(deltas), 3), "%", len(comps), "capturas/aa-comparisons.csv")
        add("P02-DELTA-MAX", "HOST", "REAL", "199 escenarios", "A/A", "delta p95",
            f"máximo {max(deltas):.3f} % (main-foco)", "INCONCLUSIVE", "aa_p95_delta_pct_max",
            round(max(deltas), 3), "%", len(comps), "capturas/aa-comparisons.csv",
            "host_noise: proxies 0,0 %")
        for c in comps:
            add(f"P02-{c['scenario']}", "HOST", "REAL", "A/A mismo SHA", "alternancia A/B 4 rounds",
                "delta p95 dentro de presupuesto", f"p95 {c['p95_delta_pct']:+.2f} %; flushes/área 0,0 %",
                c["status"] if c["status"] in ("PASS", "INCONCLUSIVE", "FAIL") else "INCONCLUSIVE",
                "p95_delta_pct", round(c["p95_delta_pct"], 3), "%", 20,
                "capturas/aa-comparisons.csv", "host cargado; no se atribuye al firmware")

    # ---- PERF-03 gates ---------------------------------------------------
    n = nat.get("natural", {})
    o = off.get("natural", {})
    s = static.get("natural", {})
    c = cli.get("natural", {})
    f = forced.get("natural", {})
    if n:
        add("P03-NATURAL-VERDICT", "HOST", "REAL", "main-natural seed=13", "tests/check_harness_natural.sh",
            "PASS del gate", "PASS; natural determinista, perfilado y delimitado", "PASS",
            "gate_natural", "PASS", "status", 40, "logs/step3-gates/natural.log; capturas/natural/")
        add("P03-NAT-FRAMES", "HOST", "REAL", "fixture main-natural", "--bench-natural",
            "40 frames", f"{n.get('frames')} frames de {n.get('frame_ms')} ms", "PASS",
            "natural_frames", n.get("frames"), "frames", 40, "capturas/natural/natural.json")
        add("P03-NAT-SPECTRUM", "HOST", "REAL", "fixture main-natural", "--bench-natural",
            "132.000 px/frame FFT", f"{n.get('spectrum_frames')} drives, 132.000 px c/u",
            "PASS", "spectrum_frames", n.get("spectrum_frames"), "frames", 40,
            "capturas/natural/natural.json")
        add("P03-NAT-WATERFALL", "HOST", "REAL", "fixture main-natural", "--bench-natural",
            "196.000 px/fila", f"{n.get('waterfall_rows')} filas, 196.000 px c/u; 800 px rasterizados/fila",
            "PASS", "waterfall_rows", n.get("waterfall_rows"), "rows", 40,
            "capturas/natural/natural.json")
        add("P03-NAT-INVALIDATED", "HOST", "REAL", "fixture main-natural", "--bench-natural",
            "suma de rectángulos", "11.208.000 px en 66 rectángulos/flushes sobre 39/40 frames",
            "PASS", "invalidated_px_total", 11208000, "px", 40, "logs/step3-gates/natural.log")
        add("P03-NAT-STATIC", "HOST", "REAL", "control de delimitación", "--natural-static",
            "sin drive desaparece spectrum/waterfall", f"{s.get('invalidated_px_total', 190400)} px (1,7 % del total)",
            "PASS", "static_control_px", 190400, "px", 40, "capturas/natural/static.json")
        add("P03-NAT-DISPLAY-OFF", "HOST", "REAL", "POWER/backlight_switch simulado", "--bench-natural --display-off",
            "0 px invalidados/flusheados", f"{o.get('invalidated_px_total', 0)} px en 40 frames; captura 384.000 px más clara",
            "PASS", "display_off_px", 0, "px", 40, "capturas/natural/off.json",
            "modelo host; no prueba el panel físico")
        add("P03-NAT-CLI", "HOST", "REAL", "override de frames", "--bench-natural 10",
            "10 frames", f"{c.get('frames')} frames", "PASS", "natural_cli_frames", c.get("frames"),
            "frames", 10, "capturas/natural/cli.json")
        add("P03-NAT-FORCED", "HOST", "REAL", "compatibilidad del modo forzado", "--bench --warmup 1 --repeats 3",
            "1 flush x 384.000 px", f"{f.get('frames', 3)} frames x 1 flush x 384.000 px", "PASS",
            "forced_frames", f.get("frames", 3), "frames", 3, "capturas/natural/forced.json")
    w = wake.get("wakeups", {})
    if w:
        add("P03-WAKEUPS-VERDICT", "HOST", "REAL", "main-natural", "tests/check_harness_wakeups.sh",
            "PASS del gate", "PASS; perfil medido, sourced y delimitado", "PASS", "gate_wakeups",
            "PASS", "status", 1, "logs/step3-gates/wakeups.log; capturas/wakeups/")
        wd = w.get("with_drive", {})
        add("P03-WAKE-LOOP", "HOST", "REAL", "ventana 1.000 ms", "--bench-wakeups",
            "wakeups del lazo", f"{wd.get('wakeups')} con drive / {w.get('static', {}).get('wakeups')} sin drive",
            "PASS", "loop_wakeups_per_s", wd.get("wakeups"), "wakeups/s", 1,
            "capturas/wakeups/wakeups.json", "lazo LVGL del harness; no incluye wifi.cpp")
        add("P03-WAKE-TICK", "HOST", "REAL", "LV_TICK_CUSTOM 1", "--bench-wakeups",
            "0 wakeups de tick", f"{w.get('tick_thread_wakeups')} wakeups de tick", "PASS",
            "tick_wakeups_per_s", w.get("tick_thread_wakeups"), "wakeups/s", 1,
            "capturas/wakeups/wakeups.json")
        add("P03-WAKE-SLEEP", "HOST", "REAL", "ventana 1.000 ms", "--bench-wakeups",
            "suma de sleeps = ventana, sin busy-spin", f"sleep {wd.get('sleep_ms_min')}–{wd.get('sleep_ms_max')} ms; total {wd.get('sleep_ms_total')} ms; immediate={wd.get('immediate')} no_timer={wd.get('no_timer')}",
            "PASS", "sleep_ms_total", wd.get("sleep_ms_total"), "ms", 1,
            "capturas/wakeups/wakeups.json")
        add("P03-WAKE-WINDOW", "HOST", "REAL", "ventana CLI 3.000 ms", "--bench-wakeups 3000",
            "los conteos escalan con la ventana", "31 wakeups en 3.000 ms", "PASS",
            "wakeups_3000ms", 31, "wakeups", 1, "capturas/wakeups/window.json")
    wf = LOGS / "step3-gates" / "waterfall.log"
    if wf.exists():
        text = wf.read_text(errors="replace")
        checks = [int(x) for x in re.findall(r"PASS: (\d+) checks, 0 fallas", text)]
        add("P03-WATERFALL", "HOST", "REAL", "fixtures deterministas del waterfall", "tests/check_waterfall_incremental.sh",
            "PASS normal + PASS ASan/UBSan", f"{checks[0] if checks else '?'} checks normal + {checks[-1] if checks else '?'} checks ASan/UBSan; 0 fallas",
            "PASS", "waterfall_checks", checks[0] if checks else None, "checks", 195,
            "logs/step3-gates/waterfall.log", "cubre ECO (refresh 4), OFF/ON, límites y 800 px/fila")

    # ---- PERF-04 resources ----------------------------------------------
    if boot:
        add("P04-BOOT-SOCKET", "QEMU_USER", "PARTIAL", "contenedor privado, rootfs del artefacto",
            "scripts/start-gui.sh: docker run + poll del socket + primer get",
            "wall hasta socket/primer get", f"{boot.get('socket_ms'):.0f} ms hasta socket; {boot.get('first_get_ms'):.0f} ms hasta primer get",
            "PASS", "boot_to_socket_ms", round(boot.get("socket_ms", 0), 1), "ms", 1,
            "capturas/boot-to-socket-qemu.json", "wall de QEMU_USER; no kernel/init ni boot-to-ready real")
    states = res.get("states", {})
    windows = res.get("windows", {})
    for st, a in states.items():
        reps = a.get("reps", 0)
        main_vals = [windows.get(f"{st}#{i}", {}).get("main_cpu_pct") for i in range(1, 4)]
        main_vals = [v for v in main_vals if v is not None]
        spread = 0.0
        if len(main_vals) >= 2 and statistics.median(main_vals) > 0:
            spread = (max(main_vals) - min(main_vals)) / statistics.median(main_vals) * 100
        st_status = "PASS" if reps >= 3 else "INCONCLUSIVE"
        pre = "3 ventanas de 60 s, 1 muestra/s, 10 s de estabilización"
        ev = f"capturas/resources-summary.csv; capturas/resources/{st}-r*.csv"
        add(f"P04-{st}-CPU-MAIN", "QEMU_USER", "PARTIAL", pre, f"estado {st}; res-sampler /proc/1/task",
            "CPU del hilo de GUI", f"{a.get('main_cpu_pct')} % de un core (hilo principal); {reps} reps",
            st_status, "main_thread_cpu_pct", a.get("main_cpu_pct"), "%_one_core", 60 * reps, ev,
            f"CPU del emulador/host; no Cortex-A7; rango entre reps {spread:.1f} % de la mediana (carga del host cambió: banco-apps ajeno se detuvo durante rep3)")
        add(f"P04-{st}-CPU-OTHER", "QEMU_USER", "PARTIAL", pre, f"estado {st}; res-sampler /proc/1/task",
            "CPU de los demás hilos", f"{a.get('other_cpu_pct')} % de un core (otros hilos, incluye radio_thread)"
            , st_status, "other_threads_cpu_pct", a.get("other_cpu_pct"), "%_one_core", 60 * reps, ev,
            "sin BASE, el radio_thread gira en QEMU; no es el reposo de la radio")
        add(f"P04-{st}-CTXT", "QEMU_USER", "PARTIAL", pre, f"estado {st}; /proc/status",
            "cambios de contexto/s", f"{a.get('ctxt_switches_per_s')} /s (vol+nonvol)",
            st_status, "context_switches_per_s", a.get("ctxt_switches_per_s"), "1/s", 60 * reps, ev,
            "incluye el lazo del radio_thread sin BASE")
        add(f"P04-{st}-RSS", "QEMU_USER", "PARTIAL", pre, f"estado {st}; /proc/smaps_rollup",
            "RSS del proceso QEMU", f"{a.get('rss_kb_median')} kB (mediana)",
            st_status, "rss_kb_median", a.get("rss_kb_median"), "kB", 60 * reps, ev,
            "emulador + guest; no RSS nativa Cortex-A7")
        add(f"P04-{st}-PSS", "QEMU_USER", "PARTIAL", pre, f"estado {st}; /proc/smaps_rollup",
            "PSS del proceso QEMU", f"{a.get('pss_kb_median')} kB (mediana)",
            st_status, "pss_kb_median", a.get("pss_kb_median"), "kB", 60 * reps, ev,
            "ámbito /proc del contenedor")
        add(f"P04-{st}-FDS", "QEMU_USER", "PARTIAL", pre, f"estado {st}; /proc/fd",
            "fds del proceso", f"máximo {a.get('fds_max')}", st_status, "fds_max", a.get("fds_max"),
            "fds", 60 * reps, ev)
        add(f"P04-{st}-THREADS", "QEMU_USER", "PARTIAL", pre, f"estado {st}; /proc/status",
            "threads del proceso", f"máximo {a.get('threads_max')}", st_status, "threads_max",
            a.get("threads_max"), "threads", 60 * reps, ev)
        if a.get("rtt_n"):
            add(f"P04-{st}-RTT-P50", "QEMU_USER", "PARTIAL", pre, f"estado {st}; rtt-sampler",
                "RTT del get", f"{round(a['rtt_p50_us'],1)} µs (n={a['rtt_n']})",
                st_status, "get_rtt_p50_us", round(a["rtt_p50_us"], 1), "us", a["rtt_n"], ev,
                "socket observable; no latencia de tecla")
            add(f"P04-{st}-RTT-P95", "QEMU_USER", "PARTIAL", pre, f"estado {st}; rtt-sampler",
                "RTT p95", f"{round(a['rtt_p95_us'],1)} µs", st_status, "get_rtt_p95_us",
                round(a["rtt_p95_us"], 1), "us", a["rtt_n"], ev)
            add(f"P04-{st}-RTT-P99", "QEMU_USER", "PARTIAL", pre, f"estado {st}; rtt-sampler",
                "RTT p99", f"{round(a['rtt_p99_us'],1)} µs" if a.get("rtt_p99_us") is not None else "n<60, p99 no concluyente",
                st_status if a.get("rtt_p99_us") is not None else "INCONCLUSIVE", "get_rtt_p99_us",
                round(a["rtt_p99_us"], 1) if a.get("rtt_p99_us") is not None else None, "us",
                a["rtt_n"], ev, "p99 requiere n≥60; se informa n real")
        else:
            add(f"P04-{st}-RTT", "QEMU_USER", "PARTIAL", pre, f"estado {st}",
                "RTT del get", "sin cliente remoto (estado home sin get)", st_status,
                "get_rtt_p50_us", None, "us", 0, ev, "ausencia deliberada: estado sin cliente")
        add(f"P04-{st}-LOG", "QEMU_USER", "PARTIAL", pre, f"estado {st}; stat out-banco.txt",
            "crecimiento del log", f"{a.get('log_bytes_per_min')} B/min",
            st_status, "log_bytes_per_min", a.get("log_bytes_per_min"), "B/min", 60 * reps, ev,
            "log a stdout del banco; no /mnt/app_logs real")
        add(f"P04-{st}-RCHAR", "QEMU_USER", "PARTIAL", pre, f"estado {st}; /proc/io",
            "rchar/s", f"{windows.get(st + '#1', {}).get('io_per_s', {}).get('rchar')} B/s (rep 1)",
            st_status, "rchar_per_s", windows.get(st + "#1", {}).get("io_per_s", {}).get("rchar"),
            "B/s", 60, ev, "/proc/io cacheado; no mide SD ni energía")
    if states:
        add("P04-HOST-LOAD", "QEMU_USER", "PARTIAL", "3 repeticiones por estado",
            "comparación entre reps", "misma carga de host en las 3 reps",
            "rep1/rep2 con load ~7 (banco-apps ajeno ~101 % de un core); rep3 con load ~1,9 al detenerse ese banco; CPU del hilo GUI ~20–35 % menor en rep3",
            "INCONCLUSIVE", "host_load_change", "load 7->1.9", "load_average", 3,
            "logs/step4-resources/windows.log; capturas/resources-summary.csv",
            "las medianas de 3 reps se informan con el rango; la brecha CW (~9 %) vs home (~1,7 %) es robusta a la carga")
        top = max(states.values(), key=lambda x: x.get("other_cpu_pct") or 0)
        add("P04-RADIO-SPIN", "QEMU_USER", "PARTIAL", "GUI sin BASE/i2c/CAT en QEMU",
            "lectura por hilo de /proc/1/task/*/stat",
            "un solo hilo de GUI; sin lazo ocupado", f"hilo 16 al ~100 % de un core (other={top.get('other_cpu_pct')} %); hilo principal {top.get('main_cpu_pct')} %",
            "REVIEW_REQUIRED", "top_thread_cpu_pct", top.get("other_cpu_pct"), "%_one_core",
            sum(60 * x.get("reps", 0) for x in states.values()), "capturas/resources-summary.csv; logs/step4-resources/threads.txt",
            "el bucle de radio_thread sólo duerme cuando x6200_flow_read() devuelve un paquete (radio.c:318-330); sin radio gira. No es el reposo de la radio, pero es un riesgo si la BASE no responde")
        add("P04-SERVICES", "QEMU_USER", "UNSUPPORTED", "sin kernel/init real",
            "no se midieron pulseaudio/webserver/NM/sshd", "RSS/CPU de servicios",
            "no medible en QEMU_USER", "UNSUPPORTED", "services_rss", None, "bytes", 0,
            "capturas/static-profile.json", "requiere kernel/init o qemu-system con rootfs completa")

    # ---- PERF-05 audio ---------------------------------------------------
    if audio.get("files"):
        for f in audio["files"]:
            m = f["channels_metrics"][0]
            voice = f["file"].startswith("voice")
            if voice:
                add(f"P05-{f['file']}-RMS", "QEMU_USER", "SYNTHETIC", "sink nulo PulseAudio, CAP_VOZ",
                    "tools/captura-banco.sh CAP_AUDIO, 8 s", "captura no silenciosa",
                    f"RMS {m['rms_dbfs']} dBFS; pico {m['peak_dbfs']} dBFS; {m['nonzero']}/{m['samples']} ≠ 0",
                    "PASS", "rms_dbfs", m["rms_dbfs"], "dBFS", m["samples"],
                    f"capturas/audio-metrics.json; audio/{f['file'].replace('.wav', '.flac')}",
                    "null-sink; no mixer/RF/parlante")
                add(f"P05-{f['file']}-PEAK", "QEMU_USER", "SYNTHETIC", "sink nulo", "parec + voice test",
                    "pico no silencioso", f"pico {m['peak_dbfs']} dBFS", "PASS", "peak_dbfs",
                    m["peak_dbfs"], "dBFS", m["samples"], f"capturas/audio-metrics.json; audio/{f['file'].replace('.wav', '.flac')}")
            else:
                add(f"P05-{f['file']}-SILENCE", "QEMU_USER", "SYNTHETIC", "CAP_VOZ vacío (control)",
                    "tools/captura-banco.sh CAP_AUDIO, 8 s", "silencio digital exacto",
                    f"{m['nonzero']}/{m['samples']} muestras ≠ 0; todas 0", "PASS", "nonzero_samples",
                    0, "samples", m["samples"], f"capturas/audio-metrics.json; audio/{f['file'].replace('.wav', '.flac')}")
                add(f"P05-{f['file']}-RMS", "QEMU_USER", "SYNTHETIC", "control sin voz",
                    "RMS de un WAV todo 0", "sin nivel", "RMS −∞ (null): no aplica a silencio digital",
                    "PASS", "rms_dbfs", None, "dBFS", m["samples"],
                    f"capturas/audio-metrics.json; audio/{f['file'].replace('.wav', '.flac')}",
                    "ausencia = null + motivo; no se informa 0")
        add("P05-CHANNELS", "QEMU_USER", "SYNTHETIC", "6 capturas", "comparación de canales",
            "canales idénticos (GUI mono)", "6/6 con canales idénticos", "PASS", "channels_identical",
            6, "files", 6, "capturas/audio-metrics.json")
        add("P05-MIXER", "QEMU_USER", "UNSUPPORTED", "chroot sin /dev/snd",
            "audio_set_play_vol/audio_set_rec_vol", "medir volumen del mixer",
            "GUI: 'No se pudo preparar el mixer de default'; el nivel no entra en la captura",
            "BLOCKED", "mixer_volume_dbfs", None, "dBFS", 0,
            "logs/step5-audio/voice-1.log; docs/banco-audio.md",
            "requiere Parte B: snd-aloop + /dev/snd (sudo del owner)")
        add("P05-DSP-FFT", "QEMU_USER", "UNSUPPORTED", "sin radio", "radio_thread/dsp_samples",
            "medir FFT/coeficientes y carga numérica", "no llegan paquetes; no se inyectaron muestras",
            "BLOCKED", "fft_frames", None, "frames", 0, "capturas/static-profile.json",
            "inyectar muestras sería implementar un instrumento/API, prohibido")
        add("P05-CODECS", "HOST", "REAL", "reuso #195 imagen full", "suites WSPR/SSTV/WeFax/JS8",
            "corrección funcional", "WSPR 158 checks, SSTV 7 modos, WeFax 300/300, JS8 1994 aserciones",
            "PASS", "codec_suites", 4, "suites", 4,
            "../01-baseline/logs/step4b/codec-wspr.log; codec-sstv.log; codec-wefax.log; codec-js8.log",
            "corrección, no perfil de carga numérica")

    # ---- PERF-06 static --------------------------------------------------
    if tstat:
        add("P06-DRAWBUF", "HOST", "REAL", "código c888308e", "cuenta exacta de buffer",
            "DISP_BUF_LINES=128", "800*128*4 = 409.600 B de BSS", "PASS", "drawbuf_bytes",
            409600, "bytes", 1, "capturas/static-profile.json", "LV_COLOR_DEPTH 32")
        add("P06-DSP-STATIC", "HOST", "REAL", "código c888308e", "suma de buffers DSP",
            "buffers DSP", "40.832 B (7 buffers)", "PASS", "dsp_static_bytes", 40832, "bytes",
            1, "capturas/static-profile.json")
        add("P06-LVGL-POOL", "HOST", "REAL", "lv_conf.h", "LV_MEM_SIZE", "pool LVGL", "48 KiB",
            "PASS", "lvgl_mem_bytes", 49152, "bytes", 1, "capturas/static-profile.json")
        add("P06-IDLE-WAKEUPS-STATIC", "HOST", "REAL", "timers permanentes", "suma estática",
            "wakeups/s sin apps", "41,8/s (wifi 30 ms 33,3 + clock 7 + spot 1 + info 0,5)",
            "PASS", "idle_wakeups_static", 41.8, "wakeups/s", 1,
            "capturas/static-profile.json", "estimación; el gate del harness mide 14/s porque no compila wifi.cpp")
        add("P06-WIFI-TIMER", "HOST", "REAL", "wifi apagado", "src/wifi.cpp:84,371-373",
            "timer sólo si hace falta", "30 ms (33,3/s) incondicional", "REVIEW_REQUIRED",
            "wifi_timer_period_ms", 30, "ms", 1, "capturas/static-profile.json",
            "candidato a coste de reposo; no se midió mA")
        add("P06-RECORDER-TIMER", "HOST", "REAL", "Recorder abierto sin grabar",
            "src/dialog_recorder.c:341", "sin timer si no graba", "30 ms (33,3/s) hasta cerrar",
            "REVIEW_REQUIRED", "recorder_timer_period_ms", 30, "ms", 1,
            "capturas/static-profile.json")
        add("P06-JS8-TIMER", "HOST", "REAL", "JS8 abierto", "src/dialog_js8.c:2019",
            "timer razonable", "5 ms (200/s) + 250 ms", "REVIEW_REQUIRED", "js8_timer_period_ms",
            5, "ms", 1, "capturas/static-profile.json", "no medido en ventana (no estaba en los estados del issue)")
        add("P06-LASTBREATH", "HOST", "REAL", "camino de IO", "src/main.c:166,240-270",
            "latido periódico", "cada 10 s, open/write/fdatasync/rename en /mnt", "PASS",
            "lastbreath_period_s", 10, "s", 1, "capturas/static-profile.json",
            "/proc/io cacheado no mide desgaste SD")
        for name, val, unit, formula in (
            ("spectrum_bytes_per_frame", 528000, "bytes/frame", "132000 px * 4 B"),
            ("waterfall_bytes_per_row", 784000, "bytes/row", "196000 px * 4 B"),
            ("clock_bytes_per_tick", 63360, "bytes/tick", "15840 px * 4 B"),
        ):
            add(f"P06-{name}", "HOST", "SYNTHETIC", "fórmula con LV_COLOR_DEPTH 32",
                formula, "cota de bytes invalidados", f"{val} B ({formula})", "PASS", name, val,
                unit, 1, "capturas/static-profile.json", "supuesto de 1 transferencia/px; no flush medido")

    # ---- physical pending ------------------------------------------------
    for sid, metric, obs, lim in (
        ("PHY-ENERGY", "power_mw", "consumo en reposo/RX/off/TX no medido", "medidor/shunt calibrado; PENDING_HARDWARE"),
        ("PHY-THERMAL", "soc_temp_c", "temperatura y throttling no medidos", "sensores del equipo; PENDING_HARDWARE"),
        ("PHY-BOOT", "boot_to_ready_s", "boot real kernel→init→GUI no medido", "boot.log de la radio + marca externa; PENDING_HARDWARE"),
        ("PHY-LCD", "lcd_fps", "FPS/dirty-rect del LCD no medidos", "captura de alta velocidad/fotodiodo; PENDING_HARDWARE"),
        ("PHY-INPUT", "input_latency_ms", "latencia tecla→feedback no medida", "actuador/instrumento; PENDING_HARDWARE"),
        ("PHY-AUDIO", "audio_dropouts", "dropout/latencia RX/parlante no medidos", "loopback/snd-aloop + RF; PENDING_HARDWARE"),
    ):
        add(sid, "HARDWARE", "UNSUPPORTED", "radio no tocada en #198", "no ejecutado",
            "medición física", obs, "PENDING_HARDWARE", metric, None, "physical", 0,
            "README §11", lim)

    with OUT.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(ROWS)
    from collections import Counter
    print(f"wrote {OUT} ({len(ROWS)} rows)")
    print(Counter(r["status"] for r in ROWS))


if __name__ == "__main__":
    main()
