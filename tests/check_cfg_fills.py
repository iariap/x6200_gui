#!/usr/bin/env python3
"""
Comprueba que TODOS los campos de la struct `cfg` se inicialicen con fill_cfg_item().

Por qué: init_params_cfg() recorre la struct como si fuera un arreglo de cfg_item_t

    cfg_item_t *cfg_arr  = (cfg_item_t *)&cfg;
    uint32_t    cfg_size = sizeof(cfg) / sizeof(*cfg_arr);
    init_items(cfg_arr, cfg_size, ...);

así que cualquier campo sin fill_cfg_item() llega con punteros NULL y
subject_add_observer(NULL, ...) mata el proceso. Pasó con cwt_level y cwt_group: la GUI se
moría antes de dibujar y la radio quedaba en el logo de arranque.

Este chequeo compara la cantidad y el orden de los campos de `cfg_t` (incluidos los de la
sub-struct eq) con las llamadas a fill_cfg_item() de init_params_cfg(), y falla si no
coinciden. Uso: python3 tests/check_cfg_fills.py [raiz del proyecto]

SPDX-License-Identifier: LGPL-2.1-or-later
"""

import re
import sys
from pathlib import Path

raiz = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
header = raiz / "src" / "cfg" / "cfg.h"
fuente = raiz / "src" / "cfg" / "cfg.c"

h = header.read_text(encoding="utf-8", errors="surrogateescape")
i = h.index("/* configuration structs")
cuerpo = re.search(r"typedef struct \{(.*?)\n\} cfg_t;", h[i:], re.S).group(1)

# Recorrer la struct EN ORDEN: los cfg_item_t sueltos y, donde aparece, el bloque del eq
# (que a su vez son tres cfg_eq_t de seis cfg_item_t cada uno). El orden importa: el
# recorrido va de a un cfg_item_t por campo, así que un campo corrido carga el valor del
# vecino.
campos = []
bloque_eq = re.search(r"struct \{(.*?)\} eq;", cuerpo, re.S)
resto = cuerpo
if bloque_eq:
    antes = cuerpo[:bloque_eq.start()]
    despues = cuerpo[bloque_eq.end():]
    campos += re.findall(r"cfg_item_t\s+(\w+);", antes)
    for sub in re.finditer(r"cfg_eq_t\s+(\w+);", bloque_eq.group(1)):
        for f in ("en", "p1", "p2", "p3", "p4", "p5"):
            campos.append("eq.%s.%s" % (sub.group(1), f))
    campos += re.findall(r"cfg_item_t\s+(\w+);", despues)
else:
    campos = re.findall(r"cfg_item_t\s+(\w+);", cuerpo)

s = fuente.read_text(encoding="utf-8", errors="surrogateescape")
inicio = s.index("static int init_params_cfg(sqlite3 *db) {")
fn = s[inicio:s.index("\n}", inicio)]
usados = re.findall(r"fill_cfg_item(?:_float)?\(&cfg\.([A-Za-z0-9_.]+)", fn)

print("  campos en cfg_t:      %d" % len(campos))
print("  fill_cfg_item():      %d" % len(usados))

problemas = []
if len(campos) != len(usados):
    problemas.append("la cantidad no coincide: %d campos, %d inicializaciones" % (len(campos), len(usados)))

faltan = [c for c in campos if c not in usados]
sobran = [u for u in usados if u not in campos]
if faltan:
    problemas.append("sin inicializar: %s" % ", ".join(faltan))
if sobran:
    problemas.append("inicializados que no son campos de cfg_t: %s" % ", ".join(sobran))

# el orden importa: el recorrido va de a un cfg_item_t por campo
if not faltan and not sobran and campos != usados:
    problemas.append("el orden no coincide con el de la struct")

if problemas:
    print("FALLA: la struct cfg no se inicializa completa")
    for p in problemas:
        print("  - %s" % p)
    print("  (ver el comentario de este script: un campo sin inicializar mata la GUI)")
    sys.exit(1)

print("  OK: los %d campos se inicializan, en orden" % len(campos))
