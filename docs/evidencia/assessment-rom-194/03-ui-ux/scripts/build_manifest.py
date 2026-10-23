#!/usr/bin/env python3
"""Write commands.jsonl, artifact-manifest.json and SHA256SUMS for #197.

Commands are the exact ones run for this evidence unit; the hashes and sizes
are read from the frozen candidate's clean checkout and the run artifacts.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
DST = Path("/home/pablo/desa/asf/state/worktrees/x6200/assessment-197-cand")
CLONE = DST / "clone-gui"
CAND = "c888308eede71205605c06383432ee7e165f202e"


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for c in iter(lambda: fh.read(65536), b""):
            h.update(c)
    return h.hexdigest()


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(CLONE), *args], capture_output=True,
                          text=True, check=True).stdout.strip()


COMMANDS = [
    {
        "id": "clone",
        "command": "git clone --no-checkout https://github.com/iariap/x6200_gui.git clone-gui && git checkout c888308e... && git submodule update --init --recursive",
        "cwd": str(DST), "vars": {"CANDIDATE": CAND},
        "start_utc": "2026-10-21T21:43:00Z", "end_utc": "2026-10-21T21:44:00Z", "exit": 0,
        "stdout": "clone.log (CLONE_OK, HEAD c888308e, lvgl 74d0a816 v8.3.11)", "log": "logs/step1/identity.log",
    },
    {
        "id": "identity",
        "command": "git rev-parse HEAD; git ls-tree HEAD lvgl; git submodule status --recursive; test -f lvgl/CMakeLists.txt; git status --porcelain",
        "cwd": str(CLONE), "vars": {}, "start_utc": "2026-10-21T21:45:00Z", "end_utc": "2026-10-21T21:45:00Z",
        "exit": 0, "stdout": "HEAD=c888308e; lvgl 74d0a816a440eea53e030c4f1af842a94f7ce3d3; clean=0", "log": "logs/step1/identity.log",
    },
    {
        "id": "preflight-docker",
        "command": "docker image inspect x6200-uiharness/-full/-ci; docker ps -a | grep '^banco'; which qemu",
        "cwd": str(CLONE), "vars": {}, "start_utc": "2026-10-21T21:45:00Z", "end_utc": "2026-10-21T21:45:00Z",
        "exit": 0,
        "stdout": "x6200-uiharness=9eedee0e5605 (ci, tag preexistente); x6200-uiharness-full=33fc1ed1; banco-apps Ocupado (Up 4h)",
        "log": "logs/step1/preflight-docker.log",
    },
    {
        "id": "determinism",
        "command": "bash tests/check_harness_determinism.sh",
        "cwd": str(CLONE), "vars": {"UI_HARNESS_IMAGE": "x6200-uiharness"}, "start_utc": "2026-10-21T21:46:04Z",
        "end_utc": "2026-10-21T21:46:15Z", "exit": 0,
        "stdout": "199 fixtures x2 -> PASS: deterministic (identical PNGs for every fixture)",
        "log": "logs/step2/determinism.log",
    },
    {
        "id": "uv-sync",
        "command": "uv sync --frozen",
        "cwd": str(CLONE / "tools/ab_testing"), "vars": {"UV_CACHE_DIR": str(DST / "cache/uv"),
        "UV_PROJECT_ENVIRONMENT": str(DST / "uv-venv")}, "start_utc": "2026-10-21T21:45:00Z",
        "end_utc": "2026-10-21T21:45:05Z", "exit": 0,
        "stdout": "Installed 11 packages (pytest 9.1.1, pyyaml 6.0.3, ruff 0.17.0, ty 0.0.87, pillow 12.3.0, numpy 2.5.4)",
        "log": "logs/step2/ab-sync.log",
    },
    {
        "id": "themes",
        "command": 'uv run x6200-ab themes --repo <clone> --json',
        "cwd": str(CLONE / "tools/ab_testing"), "vars": {"CANDIDATE": CAND}, "start_utc": "2026-10-21T21:45:09Z",
        "end_utc": "2026-10-21T21:45:10Z", "exit": 0,
        "stdout": "status=PASS; themes=10; pairs_checked=90; failures=0; errors=0", "log": "logs/step3/themes.json",
    },
    {
        "id": "capture",
        "command": 'uv run x6200-ab capture --revision "$CANDIDATE" --label candidate --out "$OUT/captures" --json',
        "cwd": str(CLONE / "tools/ab_testing"), "vars": {"CANDIDATE": CAND, "OUT": str(HERE)},
        "start_utc": "2026-10-21T21:45:58Z", "end_utc": "2026-10-21T21:48:47Z", "exit": 0,
        "stdout": "199 captures PNG+JSON", "log": "logs/step3/capture.log",
    },
    {
        "id": "audit",
        "command": 'uv run x6200-ab audit --revision "$CANDIDATE" --json --keep-workspace',
        "cwd": str(CLONE / "tools/ab_testing"), "vars": {"CANDIDATE": CAND}, "start_utc": "2026-10-21T21:47:37Z",
        "end_utc": "2026-10-21T21:49:33Z", "exit": 0,
        "stdout": "status=PASS; 0 structural failures over 26455 objects / 199 scenarios", "log": "logs/step3/audit-contracts.log",
    },
    {
        "id": "contracts",
        "command": 'uv run x6200-ab contracts --revision "$CANDIDATE" --json --keep-workspace',
        "cwd": str(CLONE / "tools/ab_testing"), "vars": {"CANDIDATE": CAND}, "start_utc": "2026-10-21T21:49:33Z",
        "end_utc": "2026-10-21T21:51:24Z", "exit": 0,
        "stdout": "status=PASS; 2056 checks; 0 failures; 0 unchanged defects", "log": "logs/step3/audit-contracts.log",
    },
    {
        "id": "analysis",
        "command": "python3 scripts/build_evidence.py && python3 scripts/make_operator_tasks.py",
        "cwd": str(HERE), "vars": {}, "start_utc": "2026-10-21T21:51:00Z", "end_utc": "2026-10-21T21:52:00Z",
        "exit": 0, "stdout": "results.csv, ui-coverage.csv, atlas.md, operator-tasks.csv", "log": "scripts/",
    },
]


def main() -> int:
    (HERE / "commands.jsonl").write_text("".join(json.dumps(c, ensure_ascii=False) + "\n" for c in COMMANDS))

    bin_path = CLONE / "tools/ui_harness/build/ui_harness"
    import glob
    seeds = sorted({json.loads(Path(f).read_text()).get("seed") for f in glob.glob(str(HERE / "captures/*.json"))})
    manifest = {
        "unit": "#197 UI/UX assessment evidence",
        "epic": "iariap/x6200_gui#194",
        "campaign": "assessment pre-release ROM #194",
        "measured_candidate": CAND,
        "candidate_fuente_md5": "964193ae9306b5127845cec44a57d52c",
        "gui_sha": CAND,
        "lvgl_gitlink": "74d0a816a440eea53e030c4f1af842a94f7ce3d3",
        "lvgl_version": "v8.3.11",
        "buildroot_sha": "d084e2870a00b64d9c57474e7eceeef80f43f25b",
        "buildroot_repo": "iariap/AetherX6200Buildroot (lu2dsx/x6200-rom)",
        "integration_branch": "assessment/rom-194",
        "integration_branch_tip": "230b20634fd240fe5836cd7d10e39646c83cd93a",
        "report_branch": "assessment/197-evidence",
        "report_base": "230b20634fd240fe5836cd7d10e39646c83cd93a",
        "measurement_checkout": {
            "path": str(CLONE), "head": CAND, "clean": True,
            "submodules": "lvgl 74d0a816a440eea53e030c4f1af842a94f7ce3d3 (v8.3.11)",
            "note": "clon privado y limpio del candidato congelado; submodulos inicializados",
        },
        "containers": {
            "harness_image_tag": "x6200-uiharness (= x6200-uiharness-ci)",
            "harness_image_id": "sha256:9eedee0e5605a774dc13851518a9100422d6da4678ffc1763d315fc4ff224be4",
            "harness_image_created": "2026-10-14T17:19:28-03:00",
            "full_image_id": "sha256:33fc1ed113e31978c600df82375628fe9d979d5c3408b4e16a42f43ae2f9312c",
            "ownership": "tag preexistente (preexistente a esta campana, registrado por #195); no se reconstruyo ni se retagueo",
        },
        "toolchain": {"git": "2.55.0", "gh": "2.101.0", "python": "3.14.7", "uv": "0.9.18",
                      "cmake": "4.3.0", "gcc_host": "16.2.1", "docker": "29.8.2",
                      "qemu": "qemu-system-x86_64 presente; qemu-user-static ausente"},
        "harness_binary": {"path": "tools/ui_harness/build/ui_harness", "bytes": bin_path.stat().st_size,
                           "sha256": sha256_file(bin_path)},
        "artifacts": {
            "captures_png_json": {"dir": "captures/", "count": len(list((HERE / "captures").glob("*.png")))},
            "inspection_gz": {"dir": "inspection/", "audit": 199, "contracts": 199,
                              "note": "arbol de objetos LVGL (--inspect) por fixture, gzip sin perdida"},
            "seeds": seeds,
        },
        "rom_artifacts": {
            "note": "no se reconstruyo ROM en #197; se referencian los artefactos del #195 (misma base congelada)",
            "rootfs_tar_sha256": "7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd",
            "sdcard_img_sha256": "d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5",
            "gui_rootfs_md5": "1f998c46909307397525c975a712adfd",
        },
        "dependency_196": {
            "satisfied": False,
            "evidence": "git ls-tree -r origin/assessment/rom-194 docs/evidencia/assessment-rom-194 -> solo 01-baseline/",
            "consequence": "todo lo que exigia capturas/estado de apps del #196 queda BLOCKED con causa",
        },
        "isolation": {
            "banco_apps": "Ocupado (x6200-buildroot, Up 4h) -> no usado, no detenido; BLOCKED para escenarios que lo requieran",
            "banco_audio": "no presente",
            "foreign_paths_touched": "ninguno; no se toco /home/pablo/desa/x6200/assessment-194 ni la radio",
            "tmp_space_issue": "el scratch se movio de /tmp/opencode (tmpfs con ~9.6 GB libres por otra sesion) a /home/.../assessment-197-cand",
        },
        "limitations": [
            "dependencia #196 no integrada: dialogos que el host no compila quedan BLOCKED",
            "22 constructores de dialogo inertes en el harness (mocks/mock_main_screen.c)",
            "sin FFT en el harness y sin RF: Auto Tune sin medida confiable no se puede forzar",
            "determinismo medido solo A/A (no contra firmware anterior)",
            "contrastes calculados != legibilidad al sol / LCD real",
            "touch/encoders/audio/RF en radio quedan PENDING_HARDWARE (fase #199)",
        ],
    }
    (HERE / "artifact-manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")

    # SHA256SUMS over every regular file in the evidence folder (except SHA256SUMS itself).
    lines = []
    for p in sorted(HERE.rglob("*")):
        if p.is_file() and p.name != "SHA256SUMS":
            lines.append(f"{sha256_file(p)}  {p.relative_to(HERE)}")
    (HERE / "SHA256SUMS").write_text("\n".join(lines) + "\n")
    print(f"wrote commands.jsonl ({len(COMMANDS)}), artifact-manifest.json, SHA256SUMS ({len(lines)} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
