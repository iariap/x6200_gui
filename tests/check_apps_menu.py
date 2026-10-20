#!/usr/bin/env python3
"""Comprueba que TODAS las apps esten en el menu de apps (las paginas APP) y que el boton
que las abre llegue DE VERDAD al dispatcher que ese boton usa.

El port tenia 23 apps y solo 9 con boton en las paginas APP: el resto solo se alcanzaba
asignandolas a un long press desde Ajustes, asi que no se encontraban (el usuario reporto "la de
Field Ops no la veo"). Las paginas APP tienen todas y esto lo verifica.

El conteo de los `case ACTION_APP_*` mira SOLO dentro de la funcion que despacha cada boton, no
todo main_screen.c: los botones de app (`make_app_btn`) llaman a `main_screen_start_app()` y los
de accion (`make_action_btn`) a `main_screen_action()`. Antes se leia el archivo entero, asi que
el boton About (que existe en `main_screen_action` pero no en `main_screen_start_app`, por donde
pasa el boton del menu) pasaba el chequeo estando muerto. Ver docs/HALLAZGOS.md 29 y el fix del
follow-up.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
MAIN = SRC / "main_screen.c"
BOTONES = SRC / "buttons.cpp"

fallos = []


def cuerpo_de_funcion(texto, firma):
    """Devuelve el cuerpo de `firma(...) {...}` balanceando llaves, para mirar solo sus `case`.

    Si la firma no aparece devuelve None (el llamador lo reporta como FALLA en vez de contar
    cero apps en silencio).
    """
    m = re.search(rf"\b{re.escape(firma)}\s*\([^;]{{0,200}}\)\s*\{{", texto)
    if not m:
        return None

    profundidad = 1
    i = m.end()
    while i < len(texto) and profundidad:
        if texto[i] == "{":
            profundidad += 1
        elif texto[i] == "}":
            profundidad -= 1
        i += 1
    return texto[m.end():i - 1]


def cases(texto):
    return set(re.findall(r"case (ACTION_APP_[A-Z0-9_]+)\s*:", texto))


main_txt = MAIN.read_text(errors="surrogateescape")
inicio_app = cuerpo_de_funcion(main_txt, "main_screen_start_app")
accion = cuerpo_de_funcion(main_txt, "main_screen_action")

# El dispatcher de cada boton (src/buttons.cpp): make_app_btn -> main_screen_start_app,
# make_action_btn -> main_screen_action. Sin las dos funciones no hay nada que medir.
if inicio_app is None or accion is None:
    print("  FALLA no encontre main_screen_start_app() o main_screen_action() en main_screen.c")
    sys.exit(1)

en_start_app = cases(inicio_app)      # donde despacha el boton de app del menu
en_action = cases(accion)             # donde despachan los botones de accion (QTH, Callsign)

# Botones de las paginas APP: se leen SOLO los cuerpos de "static buttons_page_t page_app_N" y se
# resuelve cada &btn_x a su definicion (tipo de helper + accion).
texto = BOTONES.read_text(errors="surrogateescape")
boton = {
    btn: (tipo, accion_btn)
    for btn, tipo, accion_btn in re.findall(
        r"static button_item_t\s+(btn_\w+)\s*=\s*make_(app|action)_btn\([^,]+,\s*(ACTION_APP_[A-Z0-9_]+)\)",
        texto)
}
paginas = re.findall(r"static buttons_page_t (page_app_\d+)\s*=\s*\{(.*?)\n\};", texto, re.S)

en_menu = {}
por_tipo = {"app": set(), "action": set()}
for nombre, cuerpo in paginas:
    for btn in re.findall(r"&(btn_\w+)", cuerpo):
        definicion = boton.get(btn)
        if definicion:
            tipo, accion_btn = definicion
            en_menu.setdefault(accion_btn, []).append(nombre)
            por_tipo[tipo].add(accion_btn)
            # El boton tiene que llegar al dispatcher que le corresponde: el de app pasa por
            # main_screen_start_app() y el de accion por main_screen_action(). Un `case` en la
            # OTRA funcion no alcanza (es el defecto de About).
            destino = en_start_app if tipo == "app" else en_action
            funcion = "main_screen_start_app" if tipo == "app" else "main_screen_action"
            if accion_btn not in destino:
                fallos.append(
                    f"{nombre}: el boton {btn} ({accion_btn}) no tiene case en {funcion}() "
                    f"(el boton del menu queda muerto)")
        elif btn not in ("btn_app_p1", "btn_app_p2", "btn_app_p3", "btn_app_p4", "btn_app_p5", "btn_app_p6"):
            print(f"  (aviso) {nombre} usa {btn}, que no es una app")

# Ninguna pagina puede tener mas botones que BUTTONS (el limite del marco)
limite = int(re.search(r"#define BUTTONS\s+(\d+)", (SRC / "buttons.h").read_text(errors="surrogateescape")).group(1))
for nombre, cuerpo in paginas:
    n = len(re.findall(r"&btn_\w+", cuerpo))
    if n > limite:
        fallos.append(f"{nombre} tiene {n} botones y el marco admite {limite}")

# Toda app despachada (por cualquiera de las dos funciones) tiene que estar en una pagina APP:
# una accion sin boton solo se alcanzaria por long press. El editor DFL (issue #173, PR 2) es la
# excepcion deliberada: se abre desde la QUINTA CELDA de la pagina DFL (no de una pagina APP) y
# desde el long press fisico DFL; por eso no vive en `page_app_*`.
for app in sorted(en_start_app | en_action):
    if app == "ACTION_APP_DFL_EDITOR":
        continue
    if app not in en_menu:
        fallos.append(f"{app} no esta en ninguna pagina APP (solo se llegaria por long press)")

ok_app = [a for a in por_tipo["app"] if a in en_start_app]
ok_accion = [a for a in por_tipo["action"] if a in en_action]
print(f"  apps en las paginas APP: {len(en_menu)} | make_app_btn -> main_screen_start_app(): "
      f"{len(ok_app)}/{len(por_tipo['app'])} | make_action_btn -> main_screen_action(): "
      f"{len(ok_accion)}/{len(por_tipo['action'])} | paginas: {len(paginas)}")
duplicadas = {a: p for a, p in en_menu.items() if len(p) > 1}
for a, p in sorted(duplicadas.items()):
    print(f"  (aviso) {a} aparece en {len(p)} paginas: {', '.join(p)}")

# El control remoto (src/remote_server.cpp) abre las apps por su nombre llamando a
# main_screen_start_app(): TODAS sus ACTION_APP_* tienen que tener `case` ahi, o la app no
# abre por el socket (ni desde la pagina /remote del webserver) y el barrido del banco no la
# puede medir. QTH y Callsign se abrian solo desde main_screen_action() y quedaban sin abrir
# por el control remoto (docs/HALLAZGOS.md 39).
remote_txt = (SRC / "remote_server.cpp").read_text(errors="surrogateescape")
nombres = re.search(r"app_names\[\]\s*=\s*\{(.*?)\};", remote_txt, re.S)
acciones = re.search(r"actions\[\]\s*=\s*\{(.*?)\};", remote_txt, re.S)

if not nombres or not acciones:
    fallos.append("no encontre app_names[]/actions[] en remote_server.cpp")
else:
    n_nombres = len(re.findall(r'"[^"]+"', nombres.group(1)))
    lista_acciones = re.findall(r"ACTION_[A-Z0-9_]+", acciones.group(1))
    print(f"  control remoto: {n_nombres} apps en app_names[] | {len(lista_acciones)} acciones")

    if n_nombres != len(lista_acciones):
        fallos.append(
            f"remote_server.cpp: app_names[] tiene {n_nombres} apps y actions[] "
            f"{len(lista_acciones)} acciones (el mapeo por indice queda desalineado)"
        )

    acciones_app = {a for a in lista_acciones if a.startswith("ACTION_APP_")}

    for accion_remota in sorted(acciones_app):
        if accion_remota not in en_start_app:
            fallos.append(
                f"remote_server.cpp: {accion_remota} no tiene case en main_screen_start_app() "
                f"(la app no abre por el control remoto)"
            )

    # Y al reves: toda app que el menu sabe abrir (un `case` en main_screen_start_app) tiene que
    # estar en `actions[]` del control remoto, o el socket contesta "Unknown app" aunque el
    # dialogo exista. Es el defecto del issue #78: el SWR Scan tenia `case` y dialogo pero no
    # estaba en la lista, y el barrido del banco no lo podia medir (22 de 23 apps).
    for app in sorted(en_start_app):
        if app not in acciones_app:
            fallos.append(
                f"remote_server.cpp: {app} tiene case en main_screen_start_app() pero no esta en "
                f"actions[] del control remoto (la app no abre por el socket: 'Unknown app')"
            )

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  todas las apps del menu llegan al dispatcher que usa su boton")
