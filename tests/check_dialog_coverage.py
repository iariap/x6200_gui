#!/usr/bin/env python3
"""Valida la tabla de cobertura de los 23 dialogos inertes (issue #222, epica #216).

El assessment #194/#197 dejo abierto un hueco que se repite: el arnes de host declara 23
constructores de dialogo como no-op (`tools/ui_harness/mocks/mock_main_screen.c`), asi que
esas ventanas NO se renderizan en el host, y el manifiesto de cobertura se puede quedar
desactualizado sin que nada se ponga rojo. Este chequeo convierte el inventario en una
invariante determinista:

  1. el set de simbolos de `tests/qemu/dialog-coverage.csv` tiene que ser EXACTAMENTE el set
     de `dialog_t *dialog_<x> = &mock_dialog` del mock (ni uno mas ni uno menos): agregar o
     sacar un dialogo del mock sin actualizar la tabla falla aca;
  2. cada fila apunta a la fuente de PRODUCCION que define el descriptor real, y esa fuente
     lo define de verdad (`dialog_t ... *dialog_<x> = &dialog`), no es otro mock;
  3. `host_status` es INERT en todas las filas: el harness de host sigue siendo sintetico
     para ellas y su etiqueta de cobertura NO se puede promover a REAL (regla del issue);
  4. los `host_fixture` citados existen en `tools/ui_harness/fixtures/`;
  5. `qemu_app` es una de las apps de `remote_server.cpp::app_names[]` (o `-`), y el barrido
     cubre las APPS_DEL_BARRIDO entradas: toda app tiene una fila o una excepcion verificada
     (Settings, BT y SWRScan son dialogos REALES; RTTY y DFLEditor no son dialogos; FT4 es
     alias de FT8);
  6. un resultado REAL/PARTIAL exige captura (`qemu_evidence` bajo `capturas/`) y NO admite
     razon de bloqueo; un BLOCKED/UNSUPPORTED exige la razon exacta y NO admite captura. Es
     la regla "nunca PASS sin salida real" escrita como chequeo.

Uso:  python3 tests/check_dialog_coverage.py [raiz]
Sale 0 si la tabla y el codigo coinciden; 1 si no.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

RAIZ = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
CSV = RAIZ / "tests" / "qemu" / "dialog-coverage.csv"
MOCK = RAIZ / "tools" / "ui_harness" / "mocks" / "mock_main_screen.c"
FIXTURES = RAIZ / "tools" / "ui_harness" / "fixtures"
REMOTE = RAIZ / "src" / "remote_server.cpp"

# Apps del barrido = `remote_server.cpp::app_names[]` sin "off". El numero es el CONTRATO
# de #222 y se fija aca a proposito: agregar una app al menu (y al control remoto) sin
# agregarla al barrido tiene que fallar, no pasar. Al entrar Logs (2026-10-27) el contrato
# paso de 25 a 26; la fila de Logs queda BLOCKED hasta que haya una corrida real.
APPS_DEL_BARRIDO = 26

ESTADOS_QEMU = {"REAL", "PARTIAL", "UNSUPPORTED", "BLOCKED"}
ESPERADOS_QEMU = {"REAL", "PARTIAL", "UNSUPPORTED"}

# Apps de app_names[] que NO tienen un dialogo mockeado propio. Cada una con el predicado
# que la justifica; si el codigo deja de cumplirlo, el chequeo falla (no es prosa).
EXCEPCIONES = {
    "FT4": "alias de FT8: `app FT4` selecciona el protocolo FT4 sobre dialog_ft8",
    "RTTY": "no es un dialogo: la ventana es src/rtty.c",
    "Settings": "dialogo REAL: src/dialog_settings.c (el mock NO lo declara)",
    "BT": "dialogo REAL: src/dialog_bt.c (el mock NO lo declara)",
    "SWRScan": "dialogo REAL: src/dialog_swrscan.c (el mock NO lo declara)",
    "DFLEditor": "no es un dialogo: editor inline src/dfl_inline.c",
}

fallos: list[str] = []


def leer(p: Path) -> str:
    if not p.is_file():
        fallos.append(f"falta {p.relative_to(RAIZ)}")
        return ""
    return p.read_text(errors="surrogateescape")


def simbolos_mock(texto: str) -> set[str]:
    return set(re.findall(r"^dialog_t\s*\*\s*(dialog_\w+)\s*=\s*&mock_dialog;", texto, re.M))


def app_names(texto: str) -> list[str]:
    m = re.search(r"app_names\[\]\s*=\s*\{(.*?)\};", texto, re.S)
    if not m:
        return []
    return re.findall(r'"([^"]+)"', m.group(1))


def simbolo_definido(simbolo: str) -> Path | None:
    """El archivo de src/ que define el descriptor REAL (no el mock)."""
    patron = re.compile(rf"^dialog_t\s+\*\s*{re.escape(simbolo)}\s*=\s*&dialog;", re.M)
    for fuente in sorted((RAIZ / "src").rglob("*.c")) + sorted((RAIZ / "src").rglob("*.cpp")):
        if patron.search(fuente.read_text(errors="surrogateescape")):
            return fuente
    return None


def main() -> int:
    texto_mock = leer(MOCK)
    texto_remote = leer(REMOTE)
    esperados = simbolos_mock(texto_mock)
    if not esperados:
        fallos.append(f"{MOCK.relative_to(RAIZ)}: no encontre ningun `= &mock_dialog;`")

    if not CSV.is_file():
        fallos.append(f"falta {CSV.relative_to(RAIZ)}")
        for f in fallos:
            print(f"  FALLA {f}")
        return 1

    lineas = [linea for linea in CSV.read_text().splitlines() if not linea.lstrip().startswith("#")]
    filas = list(csv.DictReader(lineas))

    obtenidos = {fila["symbol"] for fila in filas}
    if obtenidos != esperados:
        for simbolo in sorted(esperados - obtenidos):
            fallos.append(f"el mock declara {simbolo} y la tabla no lo tiene")
        for simbolo in sorted(obtenidos - esperados):
            fallos.append(f"la tabla lista {simbolo} y el mock no lo declara")
    if len(filas) != len(obtenidos):
        fallos.append(
            f"hay filas duplicadas en la tabla ({len(filas)} filas, {len(obtenidos)} simbolos)"
        )

    apps = [a for a in app_names(texto_remote) if a != "off"]
    if len(apps) != APPS_DEL_BARRIDO:
        fallos.append(
            f"remote_server.cpp::app_names[] tiene {len(apps)} apps "
            f"(el contrato de #222 dice {APPS_DEL_BARRIDO})"
        )
    apps_cubiertas: set[str] = set()

    for fila in filas:
        simbolo = fila["symbol"]
        fuente = simbolo_definido(simbolo)
        if fuente is None:
            fallos.append(f"{simbolo}: ninguna fuente de src/ define el descriptor real")
        elif fuente.relative_to(RAIZ).as_posix() != fila["source"]:
            fallos.append(
                f"{simbolo}: la tabla dice {fila['source']} y el descriptor vive en "
                f"{fuente.relative_to(RAIZ)}"
            )

        if fila["host_status"] != "INERT":
            fallos.append(
                f"{simbolo}: host_status={fila['host_status']!r}; el harness de host "
                "es INERT para los 22 (no se promueve a REAL)"
            )

        fixture = fila["host_fixture"]
        if fixture != "-" and not (FIXTURES / f"{fixture}.scn").is_file():
            fallos.append(
                f"{simbolo}: host_fixture={fixture!r} no existe en tools/ui_harness/fixtures/"
            )

        app = fila["qemu_app"]
        if app != "-":
            if app not in apps:
                fallos.append(f"{simbolo}: qemu_app={app!r} no esta en app_names[]")
            apps_cubiertas.add(app)

        if fila["qemu_expected"] not in ESPERADOS_QEMU:
            fallos.append(
                f"{simbolo}: qemu_expected={fila['qemu_expected']!r} "
                f"no es uno de {sorted(ESPERADOS_QEMU)}"
            )
        if fila["qemu_status"] not in ESTADOS_QEMU:
            fallos.append(
                f"{simbolo}: qemu_status={fila['qemu_status']!r} "
                f"no es uno de {sorted(ESTADOS_QEMU)}"
            )

        evidencia = fila["qemu_evidence"].strip()
        razon = fila["qemu_reason"].strip()
        if fila["qemu_status"] in ("REAL", "PARTIAL"):
            if not evidencia.startswith("capturas/"):
                fallos.append(
                    f"{simbolo}: qemu_status={fila['qemu_status']} "
                    f"sin captura real (qemu_evidence={evidencia!r})"
                )
            if razon:
                fallos.append(
                    f"{simbolo}: qemu_status={fila['qemu_status']} "
                    f"con razon de bloqueo ({razon[:60]!r})"
                )
        else:
            if not razon.startswith(fila["qemu_status"] + ":"):
                fallos.append(
                    f"{simbolo}: qemu_status={fila['qemu_status']} "
                    f"sin razon exacta (qemu_reason={razon[:60]!r})"
                )
            if evidencia != "-":
                fallos.append(
                    f"{simbolo}: qemu_status={fila['qemu_status']} "
                    f"no puede citar captura ({evidencia!r})"
                )

    # Las apps del barrido, contadas: cada una con fila propia o excepcion verificada.
    sin_cubrir = set(apps) - apps_cubiertas - set(EXCEPCIONES)
    if sin_cubrir:
        fallos.append(
            f"apps del barrido sin cobertura ni excepcion documentada: {sorted(sin_cubrir)}"
        )
    sobran = set(EXCEPCIONES) - set(apps)
    if sobran:
        fallos.append(f"excepciones que ya no son apps de app_names[]: {sorted(sobran)}")

    texto_mock_sin_comentarios = re.sub(r"/\*.*?\*/", "", texto_mock, flags=re.S)
    for app, por_que in EXCEPCIONES.items():
        if app not in apps:
            continue
        if app in ("Settings", "BT", "SWRScan"):
            simbolo = {
                "Settings": "dialog_settings",
                "BT": "dialog_bt",
                "SWRScan": "dialog_swrscan",
            }[app]
            if simbolo_definido(simbolo) is None:
                fallos.append(f"{app}: {por_que} pero src/ ya no define {simbolo}")
            patron_mock = rf"^\s*(dialog_t\s*\*\s*)?{simbolo}\s*="
            if re.search(patron_mock, texto_mock_sin_comentarios, re.M):
                fallos.append(f"{app}: {simbolo} volvio a declararse en el mock ({por_que})")
        else:
            simbolo = f"dialog_{app.lower()}"
            if simbolo_definido(simbolo) is not None:
                fallos.append(f"{app}: {por_que} pero src/ ahora define {simbolo}")

    if fallos:
        for f in fallos:
            print(f"  FALLA {f}")
        return 1

    print(
        f"  OK: {len(filas)} dialogos inertes (mock y tabla coinciden), "
        f"{len(apps_cubiertas)} apps con fila y {len(EXCEPCIONES)} excepciones verificadas "
        f"sobre las {len(apps)} apps del barrido"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
