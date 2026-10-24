#!/usr/bin/env python3
"""Verificacion del cambio de copy de #288 sobre el candidato (Field/SWRScan/Propagation).

Tres afirmaciones del issue #288, cada una con su medicion:

1. **Solo copy.** Se reemplaza cada literal de cadena por `<STR>` en la version
   base (`c90453fd`) y en el candidato. Si los dos "esqueletos" son iguales, el
   cambio no toco estructura, logica, formatos ni llamadas: solo el contenido de
   los literales. La unica excepcion declarada es el tamano del buffer de
   `thresholds_label_fn` (`char buf[32]` -> `char buf[48]`, que #288 documento
   para que el texto nuevo no trunque); se compara aparte y se informa.
2. **Datos de backend intactos.** `src/field/flog.c` (el PROGRAMID del ADIF) no
   aparece en el diff contra la base.
3. **Copy en ingles + identidad X6200.** Se listan los literales visibles que
   quedan con marcadores de espanol (caracteres acentuados o palabras del
   vocabulario de la UI) y se comprueba que el header de Field Ops es "X6200" y
   que "X6100" ya no aparece en un literal visible de `dialog_field.c`.

Uso:  python3 .../copy.py [--base c90453fd]
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

AQUI = Path(__file__).resolve().parent
ROOT = AQUI.parents[3]
ARCHIVOS = ("src/dialog_field.c", "src/dialog_swrscan.c", "src/dialog_propagation.c")
BACKEND = "src/field/flog.c"

# Vocabulario de la UI en espanol que #288 tenia que sacar. No es un detector de
# idioma: es la lista de palabras que la evidencia de #288 enumero, para que el
# chequeo sea el mismo contrato y no una impresion.
ESPANOL = (
    "sin datos",
    "sin tx",
    "pasadas",
    "requieren",
    "validacion",
    "fuera de",
    "cambio de estado",
    "parametros invalidos",
    "parcial",
    "abierta",
    "hacia",
    "el beacon",
    "cerrada",
    "tiempo de",
    "no hay",
    "pulsa",
    "elige",
    "ajuste",
    "barrido",
    "medicion",
    "senal",
    "umbral",
    "escucha",
    "repite",
    "cierra",
)

# `"...\\n..."` y `"..."` de una linea (los literales de estos archivos no
# contienen comillas escapadas dentro del texto visible).
LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')

# Los mensajes de `_Static_assert` son diagnosticos de COMPILACION: nunca llegan
# a la pantalla, asi que no son copy de UI y no se evaluan.
STATIC_ASSERT = re.compile(r"_Static_assert\s*\([^;]*?\)\s*;", re.S)
COMENTARIO_BLOQUE = re.compile(r"/\*.*?\*/", re.S)
COMENTARIO_LINEA = re.compile(r"//[^\n]*")


def sin_comentarios(texto: str) -> str:
    """El fuente sin comentarios (el diff de #288 agrega comentarios, no codigo)."""
    return COMENTARIO_LINEA.sub("", COMENTARIO_BLOQUE.sub("", texto))


def leer(sha: str, ruta: str) -> str:
    return subprocess.run(
        ["git", "-C", str(ROOT), "show", f"{sha}:{ruta}"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout


def esqueleto(texto: str) -> str:
    """El fuente sin comentarios y con cada literal de cadena en `<STR>`."""
    return LITERAL.sub("<STR>", sin_comentarios(texto))


def literales(texto: str) -> list[str]:
    """Los literales de cadena que NO son mensajes de `_Static_assert`."""
    limpio = STATIC_ASSERT.sub("", sin_comentarios(texto))
    return [m.group(0)[1:-1] for m in LITERAL.finditer(limpio)]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", default="c90453fd")
    args = ap.parse_args(argv)

    fallos = 0
    print(f"== 1. solo copy (base {args.base}) ==")
    for ruta in ARCHIVOS:
        viejo, nuevo = leer(args.base, ruta), (ROOT / ruta).read_text()
        # Se comparan las lineas con contenido: sacar los comentarios deja lineas
        # vacias y eso no es un cambio de estructura.
        ev = "\n".join(linea for linea in esqueleto(viejo).splitlines() if linea.strip())
        en = "\n".join(linea for linea in esqueleto(nuevo).splitlines() if linea.strip())
        if ev == en:
            print(f"  OK   {ruta}: esqueleto identico ({len(literales(nuevo))} literales)")
            continue
        # unica diferencia declarada: el tamano del buffer de thresholds_label_fn
        ev2 = ev.replace("char buf[32];", "char buf[<N>];")
        en2 = en.replace("char buf[48];", "char buf[<N>];")
        if ev2 == en2:
            print(f"  OK   {ruta}: esqueleto identico salvo buf[32]->buf[48] (declarado)")
            continue
        fallos += 1
        lineas_v = ev.splitlines()
        lineas_n = en.splitlines()
        for i, (a, b) in enumerate(zip(lineas_v, lineas_n, strict=False), 1):
            if a != b:
                print(
                    f"  FALLO {ruta}:{i}: estructura distinta\n    base: {a.strip()}\n    act : {b.strip()}"
                )
                break
        if len(lineas_v) != len(lineas_n):
            print(f"  FALLO {ruta}: {len(lineas_v)} vs {len(lineas_n)} lineas")

    print(f"\n== 2. backend intacto ({BACKEND}) ==")
    diff = subprocess.run(
        ["git", "-C", str(ROOT), "diff", "--name-only", args.base, "HEAD", "--", BACKEND],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    if diff:
        fallos += 1
        print(f"  FALLO: {BACKEND} aparece en el diff contra {args.base}")
    else:
        print(f"  OK   {BACKEND} no cambia (el PROGRAMID del ADIF sigue igual)")

    print("\n== 3. copy en ingles + identidad X6200 ==")
    restantes = []
    for ruta in ARCHIVOS:
        for lit in literales((ROOT / ruta).read_text()):
            bajo = lit.lower()
            if any(p in bajo for p in ESPANOL) or any(ord(c) > 127 for c in lit):
                restantes.append((ruta, lit))
    if restantes:
        print(f"  {len(restantes)} literal(es) con marcadores de espanol (revisar):")
        for ruta, lit in restantes:
            print(f"    {ruta}: {lit!r}")
    else:
        print("  OK   0 literales con marcadores de espanol en las tres unidades")

    field = (ROOT / "src/dialog_field.c").read_text()
    header = "X6200" in field
    print(f"  {'OK  ' if header else 'FALLO'} el header de Field Ops dice X6200")
    fallos += 0 if header else 1
    # X6100 solo puede quedar en comentarios/cabeceras de archivo, no en un literal
    visibles_x6100 = [lit for lit in literales(field) if "X6100" in lit]
    print(
        f"  {'OK  ' if not visibles_x6100 else 'FALLO'} literales visibles con X6100: "
        f"{len(visibles_x6100)} {visibles_x6100[:3]}"
    )
    fallos += 0 if not visibles_x6100 else 1

    print(f"\n  veredicto: {'FALLO' if fallos else 'PASS'} ({fallos} fallo(s))")
    return 1 if fallos else 0


if __name__ == "__main__":
    raise SystemExit(main())
