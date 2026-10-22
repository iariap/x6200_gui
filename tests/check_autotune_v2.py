#!/usr/bin/env python3
"""Comprueba el contrato estatico de Auto Tune v2 (issue #190, CW/CWR).

Lo que el arnes de UI y el banco no pueden ver se fija aca, sobre el fuente:

1. Los NUEVE textos de UX exactos (issue #190, seccion 6), en ingles.
2. La entrada exacta de Settings > Actions (`{ .label = " Auto Tune ",
   .action = ACTION_ATUNE }`, seccion 6) y que ninguna entrada PREEXISTENTE de
   `long_action_items[]` haya desaparecido o cambiado de accion.
3. Que `ACTION_ATUNE` siga valiendo 129 (seccion 6: no renumerar ids
   persistidos).
4. Que los defaults de las pulsaciones largas y de MFK no se hayan movido
   (seccion 6: "No modificar los defaults existentes").
5. Que el nucleo (`src/autotune.c`) siga siendo puro: sin LVGL, sin radio, sin
   `cfg`/`params` y sin el S-meter del barrido v1 (seccion 1 y 4).
6. Que la capa de UI sea estrictamente RX: sin `radio_set_ptt`, sin tocar el
   pitch (`cfg.key_tone`) ni el RIT (`params.rit`), y sin volver a leer el
   S-meter (seccion 1, 2 y 5).

Por que un chequeo estatico y no un test de ejecucion: los mensajes pasan por
`event_send()`, que en el arnes es un no-op documentado (la barra no se dibuja),
y las prohibiciones (no tocar TX/pitch/RIT/S-meter) son ausencias de llamadas,
que ningun fixture puede observar.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
fallos: list[str] = []

AUTOTUNE_C = SRC / "autotune.c"
AUTOTUNE_UI_C = SRC / "autotune_ui.c"
SETTINGS_C = SRC / "dialog_settings.c"
PARAMS_H = SRC / "params" / "params.h"
PARAMS_C = SRC / "params" / "params.c"

MENSAJES_EXACTOS = [
    "Auto Tune: tuning...",
    "Auto Tune: tuned",
    "Auto Tune: no reliable adjustment",
    "Auto Tune: cancelled",
    "Auto Tune: CW/CWR only",
    "Auto Tune: RX only",
    "Auto Tune: frequency locked",
    "Auto Tune: disable RIT",
    "Auto Tune: close current app",
]

# Entradas preexistentes de `long_action_items[]` que NO pueden desaparecer ni
# cambiar de accion (el orden no importa: se permite agregar, no quitar).
ENTRADAS_PREEXISTENTES = [
    (" None ", "ACTION_NONE"),
    (" Screenshot ", "ACTION_SCREENSHOT"),
    (" Screen rec ", "ACTION_SCREEN_REC"),
    (" Recorder on/off ", "ACTION_RECORDER"),
    (" Mute ", "ACTION_MUTE"),
    (" Voice mode ", "ACTION_VOICE_MODE"),
    (" ATT ", "ACTION_ATT_TOGGLE"),
    (" PRE ", "ACTION_PRE_TOGGLE"),
    (" ATU ", "ACTION_ATU_TOGGLE"),
    (" ATU tune ", "ACTION_ATU_TUNE"),
    (" APP Bands ", "ACTION_APP_BANDS"),
    (" Battery info ", "ACTION_BAT_INFO"),
    (" APP RTTY ", "ACTION_APP_RTTY"),
    (" APP Callsign", "ACTION_APP_CALLSIGN"),
    (" APP WiFi", "ACTION_APP_WIFI"),
    (" APP EQ", "ACTION_APP_EQ"),
    (" APP DFL editor", "ACTION_APP_DFL_EDITOR"),
    (" APP FT8 ", "ACTION_APP_FT8"),
    (" APP SWR Scan ", "ACTION_APP_SWRSCAN"),
    (" APP Settings", "ACTION_APP_SETTINGS"),
    (" APP Recorder", "ACTION_APP_RECORDER"),
    (" QTH Grid", "ACTION_APP_QTH"),
    (" APP WSPR", "ACTION_APP_WSPR"),
    (" APP NavTex", "ACTION_APP_NAVTEX"),
    (" APP SSTV", "ACTION_APP_SSTV"),
    (" APP WeFax", "ACTION_APP_WEFAX"),
    (" APP JS8", "ACTION_APP_JS8"),
    (" APP Cluster", "ACTION_APP_CLUSTER"),
    (" APP Field Ops", "ACTION_APP_FIELD"),
    (" APP GPS", "ACTION_APP_GPS"),
    (" APP CW Keyboard", "ACTION_APP_CW_KBD"),
    (" APP CW Trainer", "ACTION_APP_CWTRAINER"),
    (" APP Bluetooth", "ACTION_APP_BT"),
    (" APP Status", "ACTION_APP_STATUS"),
    (" APP Propagation", "ACTION_APP_PROPAGATION"),
    (" Theme", "ACTION_THEME"),
]

# Defaults de las pulsaciones largas y de MFK (src/params/params.c): Auto Tune v2
# no cambia ninguno (el chequeo solo exige que ninguno pase a ACTION_ATUNE).
CLAVES_DE_DEFAULTS = (
    "long_gen",
    "long_app",
    "long_key",
    "long_msg",
    "long_dfn",
    "long_dfl",
    "long_f1",
    "long_f2",
    "press_f1",
    "press_f2",
)


def sin_comentarios(texto: str) -> str:
    """El fuente sin comentarios de bloque y de linea, respetando literales."""
    salida = []
    i = 0
    n = len(texto)
    while i < n:
        c = texto[i]
        if c in "\"'":
            comilla = c
            salida.append(c)
            i += 1
            while i < n:
                salida.append(texto[i])
                if texto[i] == "\\":
                    i += 1
                    if i < n:
                        salida.append(texto[i])
                elif texto[i] == comilla:
                    break
                i += 1
            i += 1
        elif texto.startswith("/*", i):
            fin = texto.find("*/", i + 2)
            i = n if fin < 0 else fin + 2
            salida.append(" ")
        elif texto.startswith("//", i):
            fin = texto.find("\n", i)
            i = n if fin < 0 else fin
        else:
            salida.append(c)
            i += 1
    return "".join(salida)


def valor_de_action(texto: str, nombre: str) -> int | None:
    """El valor entero de un enumerador de press_action_t (params.h)."""
    m = re.search(r"typedef enum\s*\{(.*?)\}\s*press_action_t\s*;", sin_comentarios(texto), re.S)
    if not m:
        return None
    valor = -1
    for item in m.group(1).split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            nom, val = (x.strip() for x in item.split("=", 1))
            if not val.lstrip("-").isdigit():
                return None
            valor = int(val)
        else:
            nom = item
            valor += 1
        if nom == nombre:
            return valor
    return None


def entradas_long_action(texto: str) -> list[tuple[str, str]]:
    """Los pares (label, action) del array long_action_items[], en orden."""
    san = sin_comentarios(texto)
    bloque = re.search(r"long_action_items\[\]\s*=\s*\{(.*?)\n\};", san, re.S)
    if not bloque:
        return []
    pares = []
    for m in re.finditer(r'\.label\s*=\s*"([^"]*)"\s*,\s*\.action\s*=\s*(ACTION_[A-Z0-9_]+)', bloque.group(1)):
        pares.append((m.group(1), m.group(2)))
    return pares


def main() -> int:
    autotune_ui = AUTOTUNE_UI_C.read_text(errors="surrogateescape")
    autotune = AUTOTUNE_C.read_text(errors="surrogateescape")
    settings = SETTINGS_C.read_text(errors="surrogateescape")
    params_h = PARAMS_H.read_text(errors="surrogateescape")
    params_c = PARAMS_C.read_text(errors="surrogateescape")

    # 1. textos exactos
    for texto in MENSAJES_EXACTOS:
        if f'"{texto}"' not in autotune_ui:
            fallos.append(f'falta el texto exacto "{texto}" en src/autotune_ui.c')

    # 2. Settings > Actions
    esperada = '{ .label = " Auto Tune ", .action = ACTION_ATUNE }'
    if esperada not in settings:
        fallos.append(f"falta la entrada exacta `{esperada}` en src/dialog_settings.c")

    pares = entradas_long_action(settings)
    if (" Auto Tune ", "ACTION_ATUNE") not in pares:
        fallos.append("long_action_items[] no ofrece Auto Tune con ACTION_ATUNE")
    for label, accion in ENTRADAS_PREEXISTENTES:
        if (label, accion) not in pares:
            fallos.append(f"long_action_items[] perdio la entrada preexistente {label!r} -> {accion}")

    # 3. id persistido
    valor = valor_de_action(params_h, "ACTION_ATUNE")
    if valor != 129:
        fallos.append(f"ACTION_ATUNE vale {valor} y tiene que seguir siendo 129")

    # 4. defaults de pulsaciones largas y MFK
    defaults = {m.group(1): m.group(2) for m in re.finditer(
        r"\.(long_gen|long_app|long_key|long_msg|long_dfn|long_dfl|long_f1|long_f2|press_f1|press_f2)"
        r"\s*=\s*(ACTION_[A-Z0-9_]+)", sin_comentarios(params_c))}
    faltantes = [c for c in CLAVES_DE_DEFAULTS if c not in defaults]
    if faltantes:
        fallos.append(f"no pude leer los defaults de pulsaciones: {faltantes}")
    for clave, accion in defaults.items():
        if accion == "ACTION_ATUNE":
            fallos.append(f"el default {clave} quedo en ACTION_ATUNE: Auto Tune no es un default")

    # 5. el nucleo sigue puro
    for prohibido in ("lvgl", "radio.h", "cfg/", "params/", "meter", "subject", "msg.h"):
        if re.search(rf'#include\s*"[^"]*{re.escape(prohibido)}[^"]*"', autotune):
            fallos.append(f"src/autotune.c incluye algo de {prohibido!r}: el nucleo tiene que seguir puro")
    for prohibido in ("meter_get_raw_db", "rssi", "RSSI", "autotune_plan", "autotune_probe_freq"):
        if prohibido in autotune:
            fallos.append(f"src/autotune.c todavia menciona {prohibido!r} (el barrido v1 tiene que estar fuera)")

    # 6. la UI es RX puro y no toca pitch/RIT/S-meter
    if "radio_set_ptt" in autotune_ui:
        fallos.append("src/autotune_ui.c llama radio_set_ptt (Auto Tune es estrictamente RX)")
    if re.search(r'subject_set_int\(\s*cfg\.key_tone', autotune_ui):
        fallos.append("src/autotune_ui.c escribe cfg.key_tone (el pitch manual no se toca)")
    if re.search(r"params\.rit\s*=", autotune_ui):
        fallos.append("src/autotune_ui.c escribe params.rit (el RIT no se toca)")
    if re.search(r"\bmeter_[a-z_]*\s*\(", autotune_ui):
        fallos.append("src/autotune_ui.c sigue usando el S-meter (issue #190, seccion 1)")

    print(
        f"  textos: {len(MENSAJES_EXACTOS)} | long_action_items: {len(pares)} entradas | "
        f"ACTION_ATUNE={valor}"
    )

    if fallos:
        for f in fallos:
            print("  FALLO:", f)
        return 1

    print("  contrato estatico de Auto Tune v2 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
