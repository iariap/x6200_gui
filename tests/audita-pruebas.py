#!/usr/bin/env python3
"""Audita el registro de pruebas (docs/PRUEBAS.md).

Es el hermano de .agents/hallazgos-fundados/scripts/audita-hallazgos.sh, para el registro de
pruebas. Revisa tres cosas:

  1. Estructura: cada build (md5) de la tabla de resumen tiene su seccion, y al reves.
  2. Evidencia: cada archivo citado en los enlaces (evidencia/...) existe de verdad. Un registro
     de pruebas con referencias muertas no sirve para lo unico que sirve: poder volver a mirar.
  3. Candidatos a revisar: numeros estimados (marcadores como "~", "casi", "probablemente") fuera
     del anexo B. No son errores automaticos: son lineas para mirar a mano.

Uso:  python3 tests/audita-pruebas.py [docs/PRUEBAS.md]
Sale 0 si la estructura y la evidencia estan bien; 1 si no.
"""

import os
import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
REGISTRO = Path(sys.argv[1]) if len(sys.argv) > 1 else RAIZ / "docs" / "PRUEBAS.md"

MARCADORES = r"(~\d|estimad|\bcasi\b|probablemente|deber[ií]a|\bcreo\b|parece|supuest|de memoria|\baprox)"

errores, revisar = [], []

if not REGISTRO.exists():
    print(f"  FALLA no existe {REGISTRO}")
    sys.exit(1)

texto = REGISTRO.read_text(encoding="utf-8", errors="ignore")
lineas = texto.splitlines()

# --- secciones y tabla
secciones = re.findall(r"^## .*?`([0-9a-f]{12})`", texto, re.M)
# Solo la tabla que esta debajo del encabezado "## Resumen" (no los md5 de ejemplo de la tabla
# "como se registra una corrida", que esta mas arriba).
ini = next((i for i, l in enumerate(lineas) if l.startswith("## Resumen")), None)
fin = next((i for i, l in enumerate(lineas) if ini is not None and i > ini and l.startswith("## ")), len(lineas))
resumen = [l for l in lineas[ini:fin]] if ini is not None else []
resumen = [l for l in resumen if l.startswith("| ") and re.search(r"`[0-9a-f]{12}`", l)]
en_tabla = re.findall(r"`([0-9a-f]{12})`", "\n".join(resumen))

if not en_tabla:
    errores.append("la tabla de resumen no tiene ningun build (md5)")
for md5 in en_tabla:
    if md5 not in secciones:
        errores.append(f"el build {md5} esta en la tabla pero no tiene seccion")
for md5 in secciones:
    if md5 not in en_tabla:
        errores.append(f"la seccion del build {md5} no esta en la tabla de resumen")

# --- evidencia citada
enlaces = re.findall(r"\]\((?!#)([^)]+)\)", texto)
faltan = []
for e in enlaces:
    if e.startswith("http"):
        continue
    destino = (REGISTRO.parent / e).resolve()
    if not destino.exists():
        faltan.append(e)
if faltan:
    errores.append(f"evidencia citada que no existe: {', '.join(faltan)}")

# --- candidatos a revisar (fuera del anexo B)
en_anexo = False
for i, l in enumerate(lineas, 1):
    if re.match(r"^##\s*Anexo", l):
        en_anexo = True
    if l.startswith("## ") and "Anexo" not in l:
        en_anexo = False
    if en_anexo or l.startswith("|"):
        continue
    m = re.search(MARCADORES, l, re.I)
    if m:
        revisar.append(f"{i}: {l.strip()[:100]}")

print(f"  builds en la tabla: {len(en_tabla)} | secciones: {len(secciones)} | enlaces: {len(enlaces)}")
if errores:
    print("  ERRORES:")
    for e in errores:
        print(f"    - {e}")
if revisar:
    print(f"  para revisar a mano ({len(revisar)}):")
    for r in revisar[:8]:
        print(f"    ? {r}")
if not errores:
    print("  OK: estructura y evidencia en orden")
sys.exit(1 if errores else 0)
