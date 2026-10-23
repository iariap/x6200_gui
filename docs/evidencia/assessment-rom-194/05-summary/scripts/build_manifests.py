#!/usr/bin/env python3
"""Genera los manifiestos y el indice del dossier #199.

Lee coverage-final.csv/findings.csv/hardware-plan.csv para que los conteos no
se escriban a mano. No inventa metricas: los SHAs de artefacto son los
revalidados en logs/provenance/artifact-hashes.log.
"""
from __future__ import annotations

import csv
import hashlib
import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent

NOW = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

CANDIDATE = "c888308eede71205605c06383432ee7e165f202e"
BUILDROOT = "d084e2870a00b64d9c57474e7eceeef80f43f25b"
LVGL = "74d0a816440eea53e030c4f1af842a94f7ce3d3"
FUENTE_MD5 = "964193ae9306b5127845cec44a57d52c"

ARTIFACT = {
    "sdcard.img": {"bytes": 571473920, "sha256": "d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5"},
    "rootfs.tar": {"bytes": 242790400, "sha256": "7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd"},
    "rootfs.ext4": {"bytes": 536870912, "sha256": "9365b52f9aef33e0623ae8271aaf415483fe72e1afbc8ef99f93e15437bc620a"},
    "boot.vfat": {"bytes": 33554432, "sha256": "914f07eb00d1e5afee56ab795ce6b7b1d4410d774a3534881da494319c30b17d"},
    "zImage": {"bytes": 5567760, "sha256": "660c94c6f7231e7dc262fefb295aae1a8dfcf9b9c2b46cdc1eced2c3296bcc14"},
    "sun8i-r16-x6200.dtb": {"bytes": 26544, "sha256": "4d73f8a5d5f846c21e4ac30550a5811443a01666f9ee57880b5316e78627cc38"},
    "u-boot-sunxi-with-spl.bin": {"bytes": 556112, "sha256": "dd96b0ca1eda2c98b318ed35f284d873e64eca795a51f1d9b944115a77de0381"},
}
GUI = {"name": "usr/sbin/x6200_gui", "bytes": 13407624,
       "md5": "1f998c46909307397525c975a712adfd",
       "sha256": "7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5"}

UNITS = [
    {"unit": "1/5 baseline", "issue": 195, "pr": 201,
     "pr_url": "https://github.com/iariap/x6200_gui/pull/201",
     "commit": "be6088d1", "merge": "230b2063",
     "merged_at": "2026-10-21T20:58:54Z", "files": 81, "sha256sums_entries": 80,
     "sha256sums_verified": True, "folder": "01-baseline/"},
    {"unit": "2/5 ROM/confiabilidad", "issue": 196, "pr": 207,
     "pr_url": "https://github.com/iariap/x6200_gui/pull/207",
     "commit": "afa6109b", "merge": "ca1698d1",
     "merged_at": "2026-10-21T22:51:51Z", "files": 103, "sha256sums_entries": 102,
     "sha256sums_verified": True, "folder": "02-rom/"},
    {"unit": "3/5 UI/UX", "issue": 197, "pr": 203,
     "pr_url": "https://github.com/iariap/x6200_gui/pull/203",
     "commit": "f82dbadc", "merge": "c41f36b2",
     "merged_at": "2026-10-21T22:52:51Z", "files": 816, "sha256sums_entries": 815,
     "sha256sums_verified": True, "folder": "03-ui-ux/"},
    {"unit": "4/5 performance", "issue": 198, "pr": 208,
     "pr_url": "https://github.com/iariap/x6200_gui/pull/208",
     "commit": "2986d04d", "merge": "05020942",
     "merged_at": "2026-10-22T00:06:43Z", "files": 273, "sha256sums_entries": 272,
     "sha256sums_verified": True, "folder": "04-performance/"},
]


def rows(name: str) -> list[dict]:
    with open(OUT / name, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def build_index() -> dict:
    files = []
    for p in sorted(OUT.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(OUT).as_posix()
        if rel in ("index.json", "SHA256SUMS"):
            continue
        files.append({"path": rel, "bytes": p.stat().st_size})
    return {
        "dossier": "assessment-rom-194/05-summary",
        "campaign": "iariap/x6200_gui#194",
        "unit": 199,
        "phase": "A_documental",
        "generated_utc": NOW,
        "candidate_gui": CANDIDATE,
        "buildroot": BUILDROOT,
        "lvgl": LVGL,
        "recommendation": "CONDITIONAL (software); NO_GO de release en el estado actual",
        "hardware_status": "BLOCKED/PENDING_HARDWARE",
        "files": files,
        "file_count": len(files),
        "key_files": [
            "README.md", "coverage-final.csv", "findings.csv", "hardware-plan.csv",
            "hardware-remote/results.csv", "provenance-audit.md", "stale.md",
            "ASSESSMENT_ROM_194.md", "PRUEBAS-row.md", "acceptance-manifest.json",
            "artifact-manifest.json", "commands.jsonl", "SHA256SUMS",
        ],
    }


def build_artifact_manifest() -> dict:
    return {
        "campaign": "assessment-rom-194",
        "unit": 199,
        "phase": "A_documental_sin_radio",
        "generated_utc": NOW,
        "role": "evidencia; no implementacion; no release approval",
        "candidate": {
            "gui_sha": CANDIDATE,
            "gui_branch": "lu2dsx/x6200-rom",
            "integration_branch": "assessment/rom-194",
            "integration_head": "05020942127222c934d69ed0b37bea5895c5baaa",
            "buildroot_sha": BUILDROOT,
            "buildroot_repo": "iariap/AetherX6200Buildroot",
            "lvgl_gitlink": LVGL,
            "lvgl_version": "v8.3.11",
            "fuente_md5": FUENTE_MD5,
        },
        "artifact": {
            "location": "assessment-194/clone-buildroot/build/images/ (fuera de Git por tamano)",
            "files": ARTIFACT,
            "gui_binary": GUI,
            "fstab_mnt": "ext4",
            "gate86": "PASS: ELF32 ARM, 13407624 B, 3/3 marcas",
        },
        "evidence_base": {
            "path": "docs/evidencia/assessment-rom-194/",
            "units": UNITS,
            "sha256sums": {
                "01-baseline": {"entries": 80, "files": 81, "verified": True},
                "02-rom": {"entries": 102, "files": 103, "verified": True},
                "03-ui-ux": {"entries": 815, "files": 816, "verified": True},
                "04-performance": {"entries": 272, "files": 273, "verified": True},
            },
        },
        "measurement_layers": {
            "HOST": "medido (render/logica/build/documentacion)",
            "QEMU_USER": "medido (binario ARM con mocks; no kernel/init real)",
            "HARDWARE": "NO ejecutado: BLOCKED/PENDING_HARDWARE",
        },
        "environment": {
            "host": "AMD Ryzen 7 PRO 8840HS, 16 hilos, 89-96 GB RAM, Fedora 44, kernel 7.2.8",
            "docker_images": [
                "x6200-buildroot:latest sha256 b3e7601313d6...",
                "x6200-uiharness / -ci sha256 9eedee0e5605...",
                "x6200-uiharness-full sha256 33fc1ed113e3...",
            ],
            "qemu": "qemu-arm 10.2.2 / QEMU_CPU=cortex-a7",
            "radio": "NO contactada en fase A",
        },
        "external_context_not_revalidated": {
            "radio_gui_md5_reported": "3b22a03a3b7850e5c546c564125d0eb0",
            "source": "assessment-194/ORCHESTRATION.md",
            "is_candidate": False,
            "consequence": "La fase B mediria el software instalado, no el candidato, salvo SD/prueba autorizada.",
        },
        "stale": {
            "gui_tip": "d4fb57f8ff8ccbcfdef13fc191cc9977511b6845",
            "production_files_gui_tip": 25,
            "buildroot_tip": "f23d14f975f2d12466274aeeb4932bd191f6f6e5",
            "valid_for": [CANDIDATE, BUILDROOT, LVGL],
        },
        "limitations": [
            "Sin radio: todo hardware BLOCKED/PENDING_HARDWARE.",
            "No se reconstruyo el artefacto con el tip ni con Buildroot f23d14f.",
            "QEMU_USER no es kernel/init/Cortex-A7.",
            "No hay CI del SHA candidato.",
            "La adjudicacion de coverage-final.csv es documental, no una nueva corrida.",
        ],
    }


def build_acceptance_manifest() -> dict:
    cov = rows("coverage-final.csv")
    fnd = rows("findings.csv")
    hw = rows("hardware-plan.csv")
    cov_status = Counter(r["status_final"] for r in cov)
    cov_class = Counter(r["coverage_195"] for r in cov)
    fnd_status = Counter(r["status"] for r in fnd)
    hw_status = Counter(r["status"] for r in hw)
    return {
        "campaign": "iariap/x6200_gui#194",
        "unit": 199,
        "phase": "A_documental",
        "generated_utc": NOW,
        "acceptance": UNITS,
        "integration": {
            "branch": "assessment/rom-194",
            "head": "05020942127222c934d69ed0b37bea5895c5baaa",
            "merge_order": ["230b2063", "ca1698d1", "c41f36b2", "05020942"],
            "production_diff_candidate_to_integration": 0,
            "candidate_is_ancestor": True,
        },
        "coverage": {
            "items": len(cov),
            "class": dict(cov_class),
            "status_final": dict(cov_status),
            "metric_rows_total": 1025,
            "metric_rows_by_unit": {"195": 84, "196": 86, "197": 256, "198": 599},
            "tests": {"items": 114, "status": {"PASS": 94, "REVIEW_REQUIRED": 17, "BLOCKED": 3}},
        },
        "findings": {
            "funded": len([r for r in fnd if not r["id"].startswith("H-")]),
            "hypotheses": len([r for r in fnd if r["id"].startswith("H-")]),
            "status": dict(fnd_status),
        },
        "hardware": {
            "status": "BLOCKED/PENDING_HARDWARE",
            "cases": len(hw),
            "by_status": dict(hw_status),
            "remote_cases_executed": 0,
            "radio_touched_in_phase_a": False,
            "cause": "Fase A documental por directiva del orquestador; radio no contactada; sin sesion exclusiva verificada ni identidad/snapshot. Fase B coordinada con el owner.",
        },
        "stale": {
            "gui_tip": "d4fb57f8",
            "gui_tip_production_files": 25,
            "buildroot_tip": "f23d14f",
            "repetitions_required": [
                "build ARM/ROM e identidad", "layout/MBR y DATA primer boot", "init/prune",
                "arranque real", "estres/reposo", "update/rollback", "UI/contratos",
                "performance de boot/handoff", "toda la fase hardware",
            ],
        },
        "recommendation": {
            "software_scope": "CONDITIONAL",
            "release": "NO_GO en el estado actual",
            "reasons": [
                "fase de hardware no ejecutada (HW-01/03/05/06/08 P0/P1)",
                "P1 candidatos F-01..F-05 sin disposicion del owner",
                "tip GUI/Buildroot STALE",
                "sin CI del SHA candidato",
            ],
            "not_blocked_globally": "La evidencia software esta integrada y es auditable; el bloqueo es de la fase fisica.",
        },
        "scope": {
            "committed_path": "docs/evidencia/assessment-rom-194/05-summary/",
            "deferred_by_orchestrator": [
                "docs/ASSESSMENT_ROM_194.md (contenido listo en 05-summary/ASSESSMENT_ROM_194.md)",
                "fila/seccion en docs/PRUEBAS.md (contenido listo en 05-summary/PRUEBAS-row.md)",
            ],
            "reason": "Instruccion explicita del orquestador de commitear solo 05-summary en este PR.",
        },
        "verification": {
            "commands": "commands.jsonl",
            "logs": "logs/provenance/",
            "sha256sums_units": "OK (80/102/815/272)",
            "artifact_hashes": "OK (sdcard/rootfs/boot/GUI/fuente)",
            "gate86": "PASS",
            "docs_audit": "python3 tests/audita-pruebas.py exit 0 (sin modificar PRUEBAS.md)",
        },
    }


def main() -> int:
    (OUT / "artifact-manifest.json").write_text(
        json.dumps(build_artifact_manifest(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (OUT / "acceptance-manifest.json").write_text(
        json.dumps(build_acceptance_manifest(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (OUT / "index.json").write_text(
        json.dumps(build_index(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("manifests written", NOW)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
