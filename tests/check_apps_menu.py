#!/usr/bin/env python3
"""Comprueba que TODAS las apps esten en el menu de apps (las paginas APP).

El port tenia 23 apps y solo 9 con boton en las paginas APP: el resto solo se alcanzaba
asignandolas a un long press desde Ajustes, asi que no se encontraban (el usuario reporto "la de
Field Ops no la veo"). Ahora las paginas APP tienen todas y esto lo verifica.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
MAIN = SRC / "main_screen.c"
BOTONES = SRC / "buttons.cpp"

fallos = []

# Apps que el equipo puede abrir: los case ACTION_APP_* de main_screen_start_app
apps = set(re.findall(r"case (ACTION_APP_[A-Z0-9_]+)\s*:", MAIN.read_text(errors="surrogateescape")))

# Lo que hay en las paginas APP: se leen SOLO los cuerpos de "static buttons_page_t page_app_N" y
# se resuelve cada &btn_x a la accion de su definicion (make_app_btn / make_action_btn).
texto = BOTONES.read_text(errors="surrogateescape")
btn_a_accion = dict(re.findall(
    r"static button_item_t\s+(btn_\w+)\s*=\s*make_(?:app|action)_btn\([^,]+,\s*(ACTION_APP_[A-Z0-9_]+)\)", texto))
paginas = re.findall(r"static buttons_page_t (page_app_\d+)\s*=\s*\{(.*?)\n\};", texto, re.S)

en_menu = {}
for nombre, cuerpo in paginas:
    for btn in re.findall(r"&(btn_\w+)", cuerpo):
        accion = btn_a_accion.get(btn)
        if accion:
            en_menu.setdefault(accion, []).append(nombre)
        elif btn not in ("btn_app_p1", "btn_app_p2", "btn_app_p3", "btn_app_p4", "btn_app_p5", "btn_app_p6"):
            print(f"  (aviso) {nombre} usa {btn}, que no es una app")

# Ninguna pagina puede tener mas botones que BUTTONS (el limite del marco)
limite = int(re.search(r"#define BUTTONS\s+(\d+)", (SRC / "buttons.h").read_text(errors="surrogateescape")).group(1))
for nombre, cuerpo in paginas:
    n = len(re.findall(r"&btn_\w+", cuerpo))
    if n > limite:
        fallos.append(f"{nombre} tiene {n} botones y el marco admite {limite}")

for app in sorted(apps):
    if app not in en_menu:
        fallos.append(f"{app} no esta en ninguna pagina APP (solo se llegaria por long press)")

print(f"  apps abribles: {len(apps)} | en las paginas APP: {len(en_menu)} | paginas: {len(paginas)}")
duplicadas = {a: p for a, p in en_menu.items() if len(p) > 1}
for a, p in sorted(duplicadas.items()):
    print(f"  (aviso) {a} aparece en {len(p)} paginas: {', '.join(p)}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  todas las apps estan en el menu de apps")
