#!/usr/bin/env python3
"""make-manifest.py — #198: emit artifact-manifest.json with the measured base."""
from __future__ import annotations

import hashlib
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CAP = ROOT / "capturas"
LOGS = ROOT / "logs"
ART = Path("/home/pablo/desa/x6200/assessment-194/clone-buildroot/build/images")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load(path: Path, default=None):
    try:
        return json.loads(path.read_text())
    except Exception:
        return default


def main() -> None:
    env = load(ROOT / "environment.json", {})
    bench = load(CAP / "benchmark-summary.json", {})
    res = load(CAP / "resources-summary.json", {})
    audio = load(CAP / "audio-metrics.json", {})
    boot = load(CAP / "boot-to-socket-qemu.json", {})
    nat = load(CAP / "natural" / "natural.json", {})
    wake = load(CAP / "wakeups" / "wakeups.json", {})
    wf = (LOGS / "step3-gates" / "waterfall.log")
    wf_text = wf.read_text(errors="replace") if wf.exists() else ""

    files = {}
    for name, path in {
        "sdcard.img": ART / "sdcard.img",
        "rootfs.tar": ART / "rootfs.tar",
        "rootfs.ext4": ART / "rootfs.ext2",
        "boot.vfat": ART / "boot.vfat",
        "zImage": ART / "zImage",
        "dtb": ART / "sun8i-r16-x6200.dtb",
    }.items():
        if path.is_file():
            files[name] = {"path": str(path), "bytes": path.stat().st_size, "sha256": sha256(path)}
    gui = Path("/home/pablo/desa/x6200/assessment-194/scratch-198/rootfs-res/usr/sbin/x6200_gui")

    results_csv = ROOT / "results.csv"
    status_counts = {}
    if results_csv.exists():
        import csv
        with results_csv.open(newline="") as f:
            status_counts = dict(Counter(row["status"] for row in csv.DictReader(f)))

    manifest = {
        "unit": 198,
        "campaign": "assessment pre-release ROM #194 (iariap/x6200_gui)",
        "title": "Performance and efficiency evidence (render, resources, audio)",
        "generated_utc": __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat(),
        "author_role": "ejecutor de evidencia; no implementador",
        "revision": {
            "candidate": "c888308eede71205605c06383432ee7e165f202e",
            "buildroot": "d084e2870a00b64d9c57474e7eceeef80f43f25b",
            "lvgl_gitlink": "74d0a816440eea53e030c4f1af842a94f7ce3d3",
            "source_md5": "964193ae9306b5127845cec44a57d52c",
            "branch_development": "lu2dsx/x6200-rom",
            "tip_at_close": "d4fb57f8ff8ccbcfdef13fc191cc9977511b6845",
            "tip_reported_by_orchestrator": "dadb1705",
            "candidate_is_ancestor_of_tip": True,
            "stale": "todos los resultados valen para c888308e, no para dadb1705/d4fb57f8; ver README §1.1 y logs/step0-identity/refs-tip.log",
        },
        "artifact": {
            "source": "01-baseline/artifact-manifest.json (#195), copia durable local fuera de Git",
            "path": str(ART),
            "files": files,
            "gui_in_rootfs": {
                "path": str(gui),
                "bytes": gui.stat().st_size if gui.exists() else None,
                "md5": "1f998c46909307397525c975a712adfd",
                "sha256": sha256(gui) if gui.exists() else None,
            },
            "params_default_db": {
                "bytes": 102400,
                "sha256": "bc18ba10ca503b518771fa9c9a5649644f675d7ff0b8c87d03940969f64c2867",
            },
        },
        "host": env.get("host"),
        "software": env.get("software"),
        "docker_images": env.get("docker_images"),
        "isolation": env.get("isolation"),
        "measurement": {
            "budgets": env.get("benchmark"),
            "layers": ["HOST", "QEMU_USER", "HARDWARE (no ejecutada)"],
            "containers": ["assessment198-res", "banco-audio (efímero)"],
            "qemu_user": env.get("qemu_user"),
            "boot_to_socket": boot,
            "scenarios": {
                "PERF-01": {"status": bench.get("status"), "scenarios": bench.get("scenarios"),
                            "frames": bench.get("total_frames"), "unstable": bench.get("unstable_scenarios")},
                "PERF-02": bench.get("aa"),
                "PERF-03": {
                    "natural": {"status": "PASS" if nat.get("natural") else "MISSING",
                                "frames": nat.get("natural", {}).get("frames"),
                                "invalidated_px": 11208000, "rectangles": 66,
                                "display_off_px": 0},
                    "wakeups": {"status": "PASS" if wake.get("wakeups") else "MISSING",
                                "loop_with_drive": wake.get("wakeups", {}).get("with_drive", {}).get("wakeups"),
                                "tick": wake.get("wakeups", {}).get("tick_thread_wakeups")},
                    "waterfall": {"status": "PASS" if "PASS: 195 checks" in wf_text else "MISSING",
                                  "checks_normal_and_asan": wf_text.count("PASS: 195 checks, 0 fallas")},
                },
                "PERF-04": {"states": len(res.get("states", {})),
                            "windows": len(res.get("windows", {})),
                            "formula": "CPU% = 100*(delta(utime+stime)/CLK_TCK)/delta_t_s; RTT = recv_complete - send_start",
                            "scope": "QEMU process /proc inside the container; not Cortex-A7; services UNSUPPORTED"},
                "PERF-05": {"method": "GUI voice test -> PulseAudio null sink -> parec",
                            "files": len(audio.get("files", [])),
                            "sample_rate_hz": 44100,
                            "status": "PASS voice/control; mixer/DSP/RF BLOCKED/PENDING_HARDWARE"},
                "PERF-06": {"file": "capturas/static-profile.json", "coverage": "REAL (código), estimaciones separadas"},
            },
        },
        "results_status_counts": status_counts,
        "large_files": {
            "artifact": "fuera de Git en assessment-194/clone-buildroot/build/images/ (SHA256 en este manifiesto)",
            "audio_wav": "WAV originales no retenidos como archivo; SHA256/tamaño en capturas/audio-metrics.json; en Git van FLAC sin pérdida (PCM verificado idéntico)",
            "logs": "dentro de Git en logs/",
        },
        "limitations": env.get("limitations", []),
    }
    (ROOT / "artifact-manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False))
    print(f"wrote {ROOT / 'artifact-manifest.json'}")


if __name__ == "__main__":
    main()
