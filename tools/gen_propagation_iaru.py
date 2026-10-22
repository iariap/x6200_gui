#!/usr/bin/env python3
"""Genera src/propagation_data.c desde la lista oficial de radiofaros IARU R2 de 10 m.

La fuente es el ODS que publica la IARU R2:

    https://storage.iaru-r2.org/index.php/s/mDCqpdaf8AXt3An

(hojas "ACTIVE Reg 2 10m BCNS" e "INactive 10m BCNS, QRT & QSY"). El blob que
sale de aca va empaquetado en el firmware: la app Propagation no consulta
internet. El formato del registro esta documentado en src/propagation_data.h.

Uso:

    python3 tools/gen_propagation_iaru.py --ods /ruta/RegionTwo10mBCNs.ods
    python3 tools/gen_propagation_iaru.py --ods ... --out src/propagation_data.c

Reglas (deterministas, para que el archivo no cambie entre corridas):

  - se normaliza el indicativo a mayusculas y se recortan los espacios;
  - se descartan las filas sin indicativo o sin frecuencia parseable;
  - se descartan los indicativos de mas de 10 caracteres y los marcadores que
    no son indicativos (TENTATIVE, K7RA(SK) y cualquier "..." con parentesis);
  - dos filas con el mismo (indicativo, frecuencia) son el mismo radiofaro: se
    fusionan y queda activo si alguna de las dos lo estaba;
  - los registros se ordenan por frecuencia y, a igual frecuencia, por
    indicativo.

El ODS es la autoridad: si la IARU publica una lista nueva, se baja, se corre
esto de nuevo y el test nativo (tests/native/test_propagation.c) avisa si
cambiaron los numeros.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree

TABLE = "{urn:oasis:names:tc:opendocument:xmlns:table:1.0}"
OFFICE = "{urn:oasis:names:tc:opendocument:xmlns:office:1.0}"

SHEETS = (
    ("ACTIVE Reg 2 10m BCNS", True),
    ("INactive 10m BCNS, QRT & QSY", False),
)

CALL_LEN = 10
FREQ_LEN = 8


def rows(root: ElementTree.Element, name: str):
    for table in root.iter(TABLE + "table"):
        if table.get(TABLE + "name") != name:
            continue
        for row in table.findall(TABLE + "table-row"):
            cells = []
            for cell in row.findall(TABLE + "table-cell"):
                value = cell.get(OFFICE + "value")
                text = "".join(cell.itertext()).strip()
                cells.append(value if value is not None else text)
            yield cells


def parse(root: ElementTree.Element) -> list[tuple[str, int, bool]]:
    found: dict[tuple[str, int], bool] = {}

    for sheet, active in SHEETS:
        for row in list(rows(root, sheet))[1:]:
            if len(row) < 2:
                continue

            call = (row[0] or "").strip().upper()
            freq = (row[1] or "").strip()

            if not call or not freq:
                continue

            # La planilla usa la O con barra (Ø) como cero en algunos indicativos
            # (WDØAKX, WBØGOA): en el registro va el cero ASCII.
            call = call.replace("\u00d8", "0").replace("\u00f8", "0")

            if not call.isascii():
                raise ValueError(f"indicativo no ASCII en la lista: {call!r}")

            try:
                hz = round(float(freq.replace(",", ".")) * 1_000_000)
            except ValueError:
                continue

            # Marcadores de la hoja que no son radiofaros (o son SK).
            if call == "TENTATIVE" or "(" in call or len(call) > CALL_LEN:
                continue

            key = (call, hz)
            found[key] = found.get(key, False) or active

    return [(call, hz, active) for (call, hz), active in found.items()]


def c_string(chunk: str) -> str:
    return '"' + chunk.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit(records: list[tuple[str, int, bool]], source: str, sha: str) -> str:
    lines = []
    lines.append("/*")
    lines.append(" *  SPDX-License-Identifier: LGPL-2.1-or-later")
    lines.append(" *")
    lines.append(" *  Xiegu X6200 LVGL GUI - Registro IARU Region 2 de radiofaros de 10 m (datos)")
    lines.append(" *")
    lines.append(" *  Generado por tools/gen_propagation_iaru.py desde la lista oficial de la IARU R2")
    lines.append(' *  (RegionTwo10mBCNs.ods, hojas "ACTIVE Reg 2 10m BCNS" e "INactive 10m BCNS, QRT &')
    lines.append(
        f' *  QSY"), {len(records)} radiofaros de 10 m ordenados por frecuencia. NO se edita a mano.'
    )
    lines.append(
        f" *  El formato de cada registro de {CALL_LEN + FREQ_LEN + 1} bytes esta en src/propagation_data.h."
    )
    lines.append(" *")
    lines.append(f" *  Fuente: {source}")
    lines.append(f" *  sha256: {sha}")
    lines.append(" */")
    lines.append("")
    lines.append('#include "propagation_data.h"')
    lines.append("")
    lines.append(
        "/* Ver el formato en propagation_data.h. Cada registro: "
        f"call({CALL_LEN}) + freq_hz({FREQ_LEN}) + active(1). */"
    )
    lines.append("const char propagation_iaru_blob[] =")

    blob = "".join(f"{call:<{CALL_LEN}}{hz:0{FREQ_LEN}d}{1 if active else 0}" for call, hz, active in records)

    # 4 registros por linea: el archivo queda legible y el diff de una lista nueva no
    # reescribe todo el blob por un cambio de largo de linea.
    per_line = CALL_LEN + FREQ_LEN + 1
    for i in range(0, len(blob), per_line * 4):
        chunk = blob[i : i + per_line * 4]
        pieces = [c_string(chunk[j : j + per_line]) for j in range(0, len(chunk), per_line)]
        lines.append("    " + " ".join(pieces))
    lines.append("    ;")
    lines.append("")
    lines.append("_Static_assert((sizeof(propagation_iaru_blob) - 1) % PROPAGATION_IARU_REC == 0,")
    lines.append('               "el blob del registro IARU R2 no es un multiplo exacto del registro");')
    lines.append("")
    lines.append("const unsigned propagation_iaru_blob_len = sizeof(propagation_iaru_blob) - 1;")
    lines.append("")

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ods", required=True, help="RegionTwo10mBCNs.ods descargado de la IARU R2")
    parser.add_argument("--out", default="src/propagation_data.c", help="archivo C a escribir")
    args = parser.parse_args()

    ods = Path(args.ods)
    with zipfile.ZipFile(ods) as archive:
        root = ElementTree.fromstring(archive.read("content.xml"))

    records = parse(root)
    records.sort(key=lambda r: (r[1], r[0]))

    sha = hashlib.sha256(ods.read_bytes()).hexdigest()
    text = emit(records, "https://storage.iaru-r2.org/index.php/s/mDCqpdaf8AXt3An", sha)

    out = Path(args.out)
    out.write_text(text, encoding="utf-8")

    active = sum(1 for _, _, a in records if a)
    print(f"  registros: {len(records)} | activos: {active} | max call: {max(len(c) for c, _, _ in records)}")
    print(f"  escrito: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
