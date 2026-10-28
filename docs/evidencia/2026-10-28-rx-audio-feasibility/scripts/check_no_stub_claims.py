#!/usr/bin/env python3
"""Control negativo del issue #345: ningun resultado de STUB se reporta como real.

Tres comprobaciones deterministas sobre los artefactos de
`docs/evidencia/2026-10-28-rx-audio-feasibility/`:

(a) Toda fila `MEASURED-HOST` de `measurements.csv` tiene que venir del banco
    con libspeexdsp REAL (`./bench` dentro de `x6200-speexdsp-bench`, o el
    `docker run ... run_in_container.sh` que lo lanza), con `exit_code = 0`, y
    no puede citar como origen la suite nativa con stubs (`tests/native`).
    Las filas `MEASURED-HARDWARE`, si existieran, tienen que citar la radio;
    el banco de host no puede declararse hardware.

(b) Ninguna fila del CSV ni ningun documento puede atribuir a un stub un
    resultado de calidad / CPU / latencia / atenuacion. La unidad de analisis
    es la clausula (separada por `;` y `|`): se marca violacion solo si en la
    MISMA clausula aparecen (1) la palabra stub, (2) un verbo de medicion
    (`prueba|mide|demuestra|valida|confirma|garantiza`) y (3) una magnitud
    (`dB|dBFS|ms|%|CPU|latencia|calidad|atenuacion|RMS|inteligibilidad`). Sin
    la magnitud no hay atribucion de resultado: "que prueba el stub" describe
    la cobertura del test, no un resultado. La comprobacion es textual y
    determinista: no pretende ser analisis semantico, sino impedir la frase
    que el issue prohibe (`--self-test` demuestra que las regex detectan las
    formas malas y dejan pasar las buenas).

(c) Los ids de request del stub NO pueden coincidir con los de la lib real
    donde no deben: se leen del header del stub
    (`tests/native/stubs/speex/speex_preprocess.h`) y el valor real queda
    documentado en `speex-audit.md` (`SPEEX_PREPROCESS_SET_NOISE_SUPPRESS`
    = 18 en libspeexdsp 1.2.1; el stub usa 1). Ademas, el stub del banco
    (`scripts/bench/stubs_bench.c`) no puede definir simbolos de Speex: si los
    definiera, el banco dejaria de enlazar la lib real y (a) seria mentira.

(d) La tabla comparativa de la tanda D (`rnnoise-vs-speex.csv`) declara
    `audio_destination = OFFLINE_CAPTURE_ONLY` en TODAS las filas (nunca
    `SPEAKER`), cubre exactamente los 4 vectores x 5 metodos (+ la sonda de
    latencia), y cada fila cita la version real que la produjo: commit
    pinneado de RNNoise y sha256 del modelo para `rnnoise*`, libspeexdsp 1.2.1
    para `speex*`.

(e) RNNoise no entra al producto: `git ls-files` no puede contener ningun
    archivo de upstream (pesos `rnnoise_data*.c/.h`, `librnnoise.a`, tarball
    del modelo, `*.pth`) y `src/`, `rootfs/`, `rom-build/` y `tests/` no pueden
    mencionar `rnnoise` en absoluto.

(f) `rnnoise-manifest.json` declara que los pesos NO se versionan, NO se
    redistribuyen y NO entran al firmware, con el sha256 pinneado y
    `rnnoise_get_frame_size()` = 480.

Uso:  python3 check_no_stub_claims.py [--quiet]
Salida: una linea por comprobacion y `OK` / `FAIL` final. Exit 0 = OK, 1 = FAIL.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
from pathlib import Path

EVIDENCE_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = EVIDENCE_DIR.parents[2]

CSV_PATH = EVIDENCE_DIR / "measurements.csv"
COMPARATIVE_CSV = EVIDENCE_DIR / "rnnoise-vs-speex.csv"
RNNOISE_MANIFEST = EVIDENCE_DIR / "rnnoise-manifest.json"
AUDIT_DOC = EVIDENCE_DIR / "speex-audit.md"
STUB_HEADER = REPO_ROOT / "tests/native/stubs/speex/speex_preprocess.h"
BENCH_STUB_SRC = EVIDENCE_DIR / "scripts/bench/stubs_bench.c"
BENCH_RUNNER = EVIDENCE_DIR / "scripts/bench/run_in_container.sh"

# Marca de procedencia del banco con lib real (binario ./bench del contenedor).
REAL_BENCH_MARKERS = (
    "./bench",
    "x6200-speexdsp-bench",
    "run_in_container.sh",
    "rnnoise_build.sh",
    "x6200-rnnoise-bench",
)

# Tanda D: pin de RNNoise y matriz esperada.
RNNOISE_COMMIT = "70f1d256acd4b34a572f999a05c87bf00b67730d"
RNNOISE_MODEL_SHA = "0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37"
RNNOISE_VECTORS = ("silence", "sine1k", "multisine", "noise")
RNNOISE_METHODS = ("off", "resample", "speex", "rnnoise", "rnnoise_norm")
RNNOISE_PROBES = (("noise", "speex_b256"),)
RNNOISE_REQUIRED_COLUMNS = (
    "vector",
    "method",
    "version",
    "rate_hz",
    "samples",
    "rms_dbfs",
    "peak_dbfs",
    "clip_count",
    "discontinuities",
    "latency_ms",
    "frame_ms_p50",
    "frame_ms_p95",
    "frame_ms_p99",
    "total_ms",
    "peak_rss_kb",
    "pct_frames_over_10ms",
    "sha256",
    "audio_destination",
    "evidence_class",
    "notes",
)
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
# Marcas de que la cifra salio de la suite nativa con stubs.
STUB_SOURCE_MARKERS = ("tests/native", "stub")

# (b) Aserciones prohibidas: "el stub <verbo de medicion> ... <magnitud>" y la
# forma inversa, dentro de la misma clausula (se corta por ';' y '|').
STUB_WORD = r"stubs?"
VERB = r"(prueba|prueban|demuestra|demuestran|mide|miden|valida|validan|confirma|confirman|garantiza)"
METRIC = r"(dBFS|dB|ms|%|cpu|latencia|calidad|atenuaci[oó]n|rms|inteligibilidad)"
FORBIDDEN_CLAIM_PATTERNS = (
    re.compile(rf"\b{STUB_WORD}\b[^;|]{{0,60}}\b{VERB}\b[^;|]{{0,60}}{METRIC}", re.IGNORECASE),
    re.compile(rf"\b{VERB}\b[^;|]{{0,60}}\b{STUB_WORD}\b[^;|]{{0,60}}{METRIC}", re.IGNORECASE),
)
# La clausula corta en `;`, `|` y fin de oracion (`.` seguido de espacio): el
# corte por oracion deja pasar los decimales de las magnitudes (-13.5 dB, 5.8 ms).
CLAUSE_SPLIT = re.compile(r"[;|]|\.\s")
# Clausula que niega el verbo: no es una atribucion, es la advertencia correcta.
# Tolera el marcado markdown entre la negacion y el verbo ("**no** prueba").
NEGATED_VERB = re.compile(rf"\b(no|nunca|jamas|jamás)\b[^;|]{{0,20}}\b{VERB}\b", re.IGNORECASE)

# Docs a barrer en (b): los informes y el log de comandos del directorio.
SCAN_SUFFIXES = (".md", ".csv", ".log")

failures: list[str] = []
checks: list[str] = []


def ok(label: str, detail: str = "") -> None:
    checks.append(f"OK   {label}" + (f": {detail}" if detail else ""))


def fail(label: str, detail: str) -> None:
    checks.append(f"FAIL {label}: {detail}")
    failures.append(f"{label}: {detail}")


def rel(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def read_rows() -> list[dict[str, str]]:
    with CSV_PATH.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def check_measured_host_provenance(rows: list[dict[str, str]]) -> None:
    """(a) MEASURED-HOST solo desde el banco con lib real; hardware solo desde la radio."""
    measured_host = [r for r in rows if r["evidence_class"] == "MEASURED-HOST"]
    if not measured_host:
        fail("(a) procedencia MEASURED-HOST", "no hay ninguna fila MEASURED-HOST en el CSV")
        return

    bad_rows = 0
    for row in measured_host:
        case = row["case"]
        command = row["command"]
        notes = row["notes"]
        haystack = f"{command} {notes}".lower()

        if not any(marker in command for marker in REAL_BENCH_MARKERS):
            fail(
                "(a) procedencia MEASURED-HOST",
                f"fila {case}: el command no cita el banco con lib real "
                f"({', '.join(REAL_BENCH_MARKERS)}): {command!r}",
            )
            bad_rows += 1
            continue
        stub_hit = [marker for marker in STUB_SOURCE_MARKERS if marker in haystack]
        if stub_hit:
            fail(
                "(a) procedencia MEASURED-HOST",
                f"fila {case}: cita la suite con stubs ({', '.join(stub_hit)}); "
                "un resultado de stub no puede ser MEASURED-HOST",
            )
            bad_rows += 1
            continue
        if row["exit_code"] != "0":
            fail(
                "(a) procedencia MEASURED-HOST",
                f"fila {case}: exit_code = {row['exit_code']!r}, se esperaba '0'",
            )
            bad_rows += 1

    if bad_rows == 0:
        ok(
            "(a) procedencia MEASURED-HOST",
            f"{len(measured_host)} filas del banco con lib real, exit 0, sin citar tests/native",
        )

    hardware = [r for r in rows if r["evidence_class"] == "MEASURED-HARDWARE"]
    radio_markers = ("radio", "r16", "parec", "device")
    for row in hardware:
        haystack = f"{row['command']} {row['notes']}".lower()
        if any(marker in row["command"] for marker in REAL_BENCH_MARKERS):
            fail(
                "(a) procedencia MEASURED-HARDWARE",
                f"fila {row['case']}: declara hardware pero cita el banco de host",
            )
            continue
        if not any(marker in haystack for marker in radio_markers):
            fail(
                "(a) procedencia MEASURED-HARDWARE",
                f"fila {row['case']}: declara hardware sin citar la radio ni la medicion fisica",
            )
    ok(
        "(a) procedencia MEASURED-HARDWARE",
        f"{len(hardware)} filas (0 esperadas en esta corrida: no hubo medicion en radio)",
    )


def forbidden_claim(text: str) -> str | None:
    """Devuelve la clausula que atribuye un resultado a un stub, o None.

    Una clausula que NIEGA el verbo ("el stub no prueba ...") no es una
    atribucion: es la advertencia que el issue pide. La negacion se evalua
    sobre la misma clausula; si la clausula mezclara negacion y afirmacion
    ("no prueba X pero mide Y"), este control no la marcaria: es un limite
    textual conocido, y el self-test fija el comportamiento.
    """
    for clause in CLAUSE_SPLIT.split(text):
        if NEGATED_VERB.search(clause):
            continue
        for pattern in FORBIDDEN_CLAIM_PATTERNS:
            match = pattern.search(clause)
            if match:
                return match.group(0).strip()
    return None


def check_no_stub_claims(rows: list[dict[str, str]]) -> None:
    """(b) Nadie atribuye a un stub calidad / CPU / latencia / atenuacion."""
    scanned = 0
    for row in rows:
        for field in ("case", "command", "notes"):
            scanned += 1
            claim = forbidden_claim(row[field])
            if claim:
                fail(
                    "(b) sin atribuciones al stub",
                    f"{rel(CSV_PATH)} fila {row['case']} campo {field}: "
                    f"asercion prohibida {claim!r}",
                )

    docs = sorted(
        path
        for path in EVIDENCE_DIR.rglob("*")
        if path.is_file() and path.suffix in SCAN_SUFFIXES and path != Path(__file__)
    )
    for path in docs:
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            scanned += 1
            claim = forbidden_claim(line)
            if claim:
                fail(
                    "(b) sin atribuciones al stub",
                    f"{rel(path)}:{lineno}: asercion prohibida {claim!r}",
                )

    if not any(item.startswith("(b)") for item in failures):
        ok("(b) sin atribuciones al stub", f"{scanned} campos/lineas barridos, 0 aserciones prohibidas")


def check_stub_ids_do_not_match(rows: list[dict[str, str]]) -> None:
    """(c) Los ids del stub no coinciden con los reales donde no deben."""
    if not STUB_HEADER.is_file():
        fail("(c) ids del stub", f"no existe {rel(STUB_HEADER)}")
        return
    stub_text = STUB_HEADER.read_text(encoding="utf-8")
    stub_match = re.search(
        r"#define\s+SPEEX_PREPROCESS_SET_NOISE_SUPPRESS\s+(\d+)", stub_text
    )
    if not stub_match:
        fail("(c) ids del stub", f"{rel(STUB_HEADER)} no define SET_NOISE_SUPPRESS")
        return
    stub_value = int(stub_match.group(1))

    if not AUDIT_DOC.is_file():
        fail("(c) ids del stub", f"no existe {rel(AUDIT_DOC)}")
        return
    audit_text = AUDIT_DOC.read_text(encoding="utf-8")
    real_match = re.search(
        r"#define\s+SPEEX_PREPROCESS_SET_NOISE_SUPPRESS\s+(\d+)", audit_text
    )
    if not real_match:
        fail(
            "(c) ids del stub",
            f"{rel(AUDIT_DOC)} no documenta el valor real de SET_NOISE_SUPPRESS",
        )
        return
    real_value = int(real_match.group(1))

    if stub_value != 1:
        fail(
            "(c) ids del stub",
            f"el stub declara SET_NOISE_SUPPRESS = {stub_value}; se esperaba 1 "
            "(si el stub cambio, hay que reauditar los tests y este control)",
        )
    if real_value != 18:
        fail(
            "(c) ids del stub",
            f"el valor real documentado es {real_value}; libspeexdsp 1.2.1 define 18",
        )
    if stub_value == real_value:
        fail(
            "(c) ids del stub",
            f"stub y lib real comparten SET_NOISE_SUPPRESS = {stub_value}: el stub "
            "dejaria de delatar un id equivocado",
        )

    # El doc tiene que registrar la discrepancia, no solo el numero real.
    if not re.search(
        r"SPEEX_PREPROCESS_SET_NOISE_SUPPRESS`?\s*\|\s*\*{0,2}1\*{0,2}\s*\|\s*\*{0,2}18",
        audit_text,
    ):
        fail(
            "(c) ids del stub",
            f"{rel(AUDIT_DOC)} no tiene la fila stub=1 / real=18 que funda la discrepancia",
        )

    ok(
        "(c) ids del stub",
        f"stub SET_NOISE_SUPPRESS = {stub_value} != real = {real_value} (documentado en speex-audit.md)",
    )

    # El banco no puede stubbear Speex: si lo hiciera, (a) seria falso.
    if not BENCH_STUB_SRC.is_file():
        fail("(c) banco sin stub de Speex", f"no existe {rel(BENCH_STUB_SRC)}")
        return
    bench_text = BENCH_STUB_SRC.read_text(encoding="utf-8")
    if "SPEEX_PREPROCESS_SET_NOISE_SUPPRESS" in bench_text or re.search(
        r"\b(speex_preprocess_state_init|speex_preprocess_run|speex_preprocess_ctl)\b",
        bench_text,
    ):
        fail(
            "(c) banco sin stub de Speex",
            f"{rel(BENCH_STUB_SRC)} define simbolos de Speex: el banco dejaria de "
            "enlazar la lib real",
        )
        return
    if not BENCH_RUNNER.is_file() or "speexdsp" not in BENCH_RUNNER.read_text(encoding="utf-8"):
        fail(
            "(c) banco sin stub de Speex",
            f"{rel(BENCH_RUNNER)} no enlaza speexdsp via pkg-config",
        )
        return
    ok(
        "(c) banco sin stub de Speex",
        f"{rel(BENCH_STUB_SRC)} no define simbolos de Speex; {rel(BENCH_RUNNER)} enlaza speexdsp",
    )

    # Y ninguna fila MEASURED-HOST que produzca PCM puede tener un sha ausente:
    # el banco deja sha256 del .raw por cada caso (las sondas no escriben PCM).
    missing_sha = [
        row["case"]
        for row in rows
        if row["evidence_class"] == "MEASURED-HOST"
        and "--case" in row["command"]
        and row["sha"] in ("", "UNKNOWN")
    ]
    if missing_sha:
        fail(
            "(c) banco sin stub de Speex",
            f"filas MEASURED-HOST sin sha del PCM procesado: {', '.join(missing_sha)}",
        )
        return
    ok("(c) sha del PCM procesado", "todas las filas MEASURED-HOST con --case traen sha")



def check_comparative_table() -> None:
    """(d) La tabla de la tanda D es OFFLINE_CAPTURE_ONLY y cita la version real."""
    if not COMPARATIVE_CSV.is_file():
        fail("(d) tabla comparativa", f"no existe {rel(COMPARATIVE_CSV)}")
        return

    with COMPARATIVE_CSV.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        columns = tuple(reader.fieldnames or ())
        rows = list(reader)

    missing_columns = [c for c in RNNOISE_REQUIRED_COLUMNS if c not in columns]
    if missing_columns:
        fail("(d) tabla comparativa", f"faltan columnas: {', '.join(missing_columns)}")
        return

    if not rows:
        fail("(d) tabla comparativa", f"{rel(COMPARATIVE_CSV)} no tiene filas")
        return

    bad = 0
    for row in rows:
        where = f"{row['vector']}/{row['method']}"
        if row["audio_destination"] != "OFFLINE_CAPTURE_ONLY":
            fail(
                "(d) tabla comparativa",
                f"fila {where}: audio_destination = {row['audio_destination']!r}; "
                "tiene que ser OFFLINE_CAPTURE_ONLY (nunca SPEAKER)",
            )
            bad += 1
        if row["evidence_class"] not in ("MEASURED-HOST", "SKIPPED", "BLOCKED"):
            fail("(d) tabla comparativa", f"fila {where}: evidence_class {row['evidence_class']!r}")
            bad += 1
        if row["method"].startswith("rnnoise") and (
            RNNOISE_COMMIT[:8] not in row["version"] or RNNOISE_MODEL_SHA[:8] not in row["version"]
        ):
            fail(
                "(d) tabla comparativa",
                f"fila {where}: la version no cita el commit pinneado "
                f"({RNNOISE_COMMIT[:8]}) y el modelo ({RNNOISE_MODEL_SHA[:8]}): {row['version']!r}",
            )
            bad += 1
        if row["method"].startswith("speex") and "libspeexdsp 1.2.1" not in row["version"]:
            fail(
                "(d) tabla comparativa",
                f"fila {where}: la version no cita libspeexdsp 1.2.1: {row['version']!r}",
            )
            bad += 1
        if row["evidence_class"] == "MEASURED-HOST" and not SHA256_RE.match(row["sha256"]):
            fail(
                "(d) tabla comparativa",
                f"fila {where}: sha256 ausente o invalido ({row['sha256']!r})",
            )
            bad += 1

    expected = {(v, m) for v in RNNOISE_VECTORS for m in RNNOISE_METHODS} | set(RNNOISE_PROBES)
    actual = {(row["vector"], row["method"]) for row in rows}
    if actual != expected:
        fail(
            "(d) tabla comparativa",
            f"la matriz no es la esperada; faltan {sorted(expected - actual)} y sobran "
            f"{sorted(actual - expected)}",
        )
        return

    if bad:
        return
    ok(
        "(d) tabla comparativa",
        f"{len(rows)} filas ({len(RNNOISE_VECTORS)} vectores x {len(RNNOISE_METHODS)} metodos + "
        f"{len(RNNOISE_PROBES)} sonda), todas OFFLINE_CAPTURE_ONLY, version y sha declarados",
    )


def check_rnnoise_not_vendored() -> None:
    """(e) Los pesos y la libreria de upstream no estan en el repo ni en el runtime."""
    listed = subprocess.run(
        ["git", "-C", str(REPO_ROOT), "ls-files", "-z"], capture_output=True, text=True
    )
    if listed.returncode != 0:
        fail("(e) RNNoise fuera del repo", "no pude leer git ls-files")
        return

    vendored_pattern = re.compile(
        r"(^|/)(rnnoise_data(_little)?\.[ch]|librnnoise\.a|rnnoise_data-[0-9a-f]+\.tar\.gz|[^/]+\.pth)$"
    )
    vendored = [
        path for path in listed.stdout.split("\0") if path and vendored_pattern.search(path)
    ]
    if vendored:
        fail(
            "(e) RNNoise fuera del repo",
            f"archivos de upstream versionados: {', '.join(vendored[:5])}",
        )
        return

    runtime_dirs = ("src", "rootfs", "rom-build", "tests")
    mentions = []
    for directory in runtime_dirs:
        base = REPO_ROOT / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.is_symlink():
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if "rnnoise" in text.lower():
                mentions.append(rel(path))
    if mentions:
        fail(
            "(e) RNNoise fuera del repo",
            f"el runtime menciona rnnoise: {', '.join(mentions[:5])}",
        )
        return

    ok(
        "(e) RNNoise fuera del repo",
        "sin pesos/libreria de upstream versionados y sin menciones en src/rootfs/rom-build/tests",
    )


def check_rnnoise_manifest() -> None:
    """(f) El manifest deja fijado modelo, licencia y que nada entra al producto."""
    if not RNNOISE_MANIFEST.is_file():
        fail("(f) manifest de RNNoise", f"no existe {rel(RNNOISE_MANIFEST)}")
        return
    try:
        manifest = json.loads(RNNOISE_MANIFEST.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail("(f) manifest de RNNoise", f"JSON invalido: {exc}")
        return

    rnnoise = manifest.get("rnnoise", {})
    model = rnnoise.get("model", {})
    problems = []
    if rnnoise.get("commit") != RNNOISE_COMMIT:
        problems.append(f"commit {rnnoise.get('commit')!r} != {RNNOISE_COMMIT}")
    if model.get("sha256") != RNNOISE_MODEL_SHA:
        problems.append(f"sha256 del modelo {model.get('sha256')!r} != {RNNOISE_MODEL_SHA}")
    if not str(model.get("url", "")).startswith("https://media.xiph.org/rnnoise/models/"):
        problems.append(f"url del modelo no es la de Xiph: {model.get('url')!r}")
    for flag in ("vendored_in_repo",):
        if rnnoise.get(flag) is not False:
            problems.append(f"{flag} tiene que ser false")
    for flag in ("redistributed_in_repo", "in_firmware"):
        if model.get(flag) is not False:
            problems.append(f"model.{flag} tiene que ser false")
    if rnnoise.get("frame_size") != 480:
        problems.append(f"frame_size {rnnoise.get('frame_size')!r} != 480")
    if "BSD-3-Clause" not in str(rnnoise.get("license", "")):
        problems.append(f"licencia no declarada BSD-3-Clause: {rnnoise.get('license')!r}")

    if problems:
        fail("(f) manifest de RNNoise", "; ".join(problems))
        return
    ok(
        "(f) manifest de RNNoise",
        f"commit {RNNOISE_COMMIT[:8]}, modelo {RNNOISE_MODEL_SHA[:8]}, BSD-3-Clause, "
        "no versionado / no redistribuido / fuera del firmware, frame_size 480",
    )


SELF_TEST_CASES = (
    # (texto, se espera violacion)
    ("el stub mide 0.2% de CPU", True),
    ("el bench con stubs demuestra -13.5 dB de atenuacion", True),
    ("un PASS del stub prueba calidad de audio", True),
    ("el stub prueba la latencia", True),
    ("la suite con stubs valida 5.8 ms de retardo", True),
    ("el stub atenua 6 dB fijos y usa ids de request distintos", False),
    ("lo que prueba el stub: troceado, tail y clamps", False),
    ("el stub no prueba atenuacion, calidad, CPU ni latencia", False),
    ("el stub **no** prueba atenuacion de 13 dB", False),
    ("Un PASS de la suite host prueba troceado; no prueba calidad", False),
)


def self_test() -> int:
    """Demuestra que las regex de (b) no son vacuas y no comen frases correctas."""
    bad = 0
    for text, expected in SELF_TEST_CASES:
        detected = forbidden_claim(text) is not None
        status = "OK  " if detected == expected else "FAIL"
        bad += 0 if detected == expected else 1
        print(f"{status} self-test(b): esperado={expected!s:5} detectado={detected!s:5} {text!r}")
    if bad:
        print(f"FAIL: {bad} caso(s) de self-test")
        return 1
    print("OK: las regex de (b) detectan las 5 formas prohibidas y dejan pasar las 5 correctas")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quiet", action="store_true", help="solo OK/FAIL final")
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="prueba que las regex de (b) detectan las formas prohibidas",
    )
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    rows = read_rows()
    check_measured_host_provenance(rows)
    check_no_stub_claims(rows)
    check_stub_ids_do_not_match(rows)
    check_comparative_table()
    check_rnnoise_not_vendored()
    check_rnnoise_manifest()

    if not args.quiet:
        for line in checks:
            print(line)
    if failures:
        print(f"FAIL: {len(failures)} comprobacion(es) del control negativo")
        return 1
    print("OK: ningun resultado de stub se reporta como real")
    return 0


if __name__ == "__main__":
    sys.exit(main())
