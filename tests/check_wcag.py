#!/usr/bin/env python3
"""Comprueba que los colores de los temas cumplan WCAG 2.1 (contraste).

AGENTS.md dice "Use WCAG coloring" y hasta ahora no habia ningun chequeo: un
tema podia dejar texto gris sobre fondo gris y nadie se enteraba. Este script
lee las cuatro paletas base de src/theme.h (las plantillas internas que el
`base` de un archivo hereda; no son temas, issue #74) y los seis temas JSON que
van en la imagen (Aurora, Cathode, Redshift, Sprint, Sunlit y Trail), y mide cada
par (color de texto sobre superficie) que el codigo realmente dibuja.

No usa nada fuera de la stdlib: la formula es la de WCAG 2.1.

Umbrales:
  4.5:1  texto normal (WCAG 1.4.3, nivel AA)
  3.0:1  texto grande (>= 24 px normal o >= 18.66 px bold) y no-texto:
         bordes, anillos de foco y barras de estado (WCAG 1.4.11)

Exenciones (WCAG 1.4.3, "disabled/incidental"): los colores apagados por
diseno (boton deshabilitado, texto en cola, iconos apagados, rieles de barra)
se informan con su ratio pero NO hacen fallar el chequeo. Son:
  btn_off_text, text_mute, text_faint, icon_off, switch_off, off, track.

Salida: la tabla de lo que no esta OK (fallas y exenciones) y un resumen por
tema. Sale con 1 si hay alguna falla.

Uso:
  python3 tests/check_wcag.py [--all]     --all imprime tambien los pares OK.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import json
import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
THEME_H = RAIZ / "src" / "theme.h"
THEME_C = RAIZ / "src" / "theme.c"
CW_TUNE_C = RAIZ / "src" / "cw_tune_ui.c"
TEMAS_DIR = RAIZ / "rootfs" / "usr" / "share" / "x6200" / "themes"


def temas_json():
    """Los temas de la imagen: `<nombre>.json` y `<nombre>/theme.json` (los seis)."""
    return sorted(TEMAS_DIR.glob("*.json")) + sorted(TEMAS_DIR.glob("*/theme.json"))

fallos = []
avisos = []

# ---------------------------------------------------------------------------
# Formula WCAG 2.1
# ---------------------------------------------------------------------------


def _canal(c):
    """Normaliza un canal 0..255 a 0..1 y aplica la transferencia sRGB."""
    c = c / 255.0
    if c <= 0.03928:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def luminancia(hexval):
    """Luminancia relativa WCAG de un 0xRRGGBB."""
    r = (hexval >> 16) & 0xFF
    g = (hexval >> 8) & 0xFF
    b = hexval & 0xFF
    return 0.2126 * _canal(r) + 0.7152 * _canal(g) + 0.0722 * _canal(b)


def contraste(a, b):
    """Ratio de contraste WCAG entre dos 0xRRGGBB, siempre >= 1."""
    la, lb = luminancia(a), luminancia(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


# Cruce de luminancia para elegir blanco o negro: el que mas contraste tenga. Es el mismo valor que
# usan src/band_plan.c (LUM_CORTE) y src/cw_tune_ui.c, donde el borde se elige por luminancia del
# relleno (bloque activo) o del panel del widget (barras inactivas).
LUM_CORTE = 0.18


def mezclar(fg, bg, opa):
    """Compone fg sobre bg con alfa 0..255 (lo que hace LVGL al dibujar al 50%)."""
    salida = []
    for desp in (16, 8, 0):
        f = (fg >> desp) & 0xFF
        b = (bg >> desp) & 0xFF
        salida.append((f * opa + b * (255 - opa)) // 255)
    return (salida[0] << 16) | (salida[1] << 8) | salida[2]


# ---------------------------------------------------------------------------
# Lectura de las paletas
# ---------------------------------------------------------------------------


def leer_theme_h():
    """Devuelve {rol: [4 hexes]} con la linea de cada rol.

    Cada renglon de THEME_COLORS(X) es `X(rol, 0x..., 0x..., 0x..., 0x...)`:
    cuatro paletas base en orden IU1WXY, Legacy, Outdoor, Classic (las plantillas
    internas que el `base` de un archivo hereda; no son temas, issue #74). Si un
    color no tiene cuatro valores, es un error (TC_COUNT se genera de esta macro).
    """
    texto = THEME_H.read_text(errors="surrogateescape")
    m = re.search(r"#define\s+THEME_COLORS\(X\)(.*?)typedef enum", texto, re.S)
    if not m:
        fallos.append("src/theme.h: no encontre la macro THEME_COLORS(X)")
        return {}

    colores = {}
    for renglon in m.group(1).splitlines():
        mm = re.match(
            r"\s*X\(\s*([a-z_0-9]+)\s*,\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*,"
            r"\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*\)",
            renglon,
        )
        if not mm:
            continue
        rol = mm.group(1)
        valores = [int(mm.group(i), 16) for i in range(2, 6)]
        if len(valores) != 4:       # el regex ya lo garantiza: queda por claridad
            fallos.append(f"src/theme.h: {rol} no tiene 4 valores")
            continue
        colores[rol] = valores

    if not colores:
        fallos.append("src/theme.h: no pude leer ningun color de THEME_COLORS")

    # Toda entrada tiene que tener rol + 4 valores: si una linea X( no se pudo
    # leer, o hay dos roles con el mismo nombre, el conteo no cierra.
    entradas = [l for l in m.group(1).splitlines() if re.search(r"\bX\s*\(", l)]
    if len(entradas) != len(colores):
        fallos.append(
            f"src/theme.h: {len(entradas)} entradas X( pero {len(colores)} roles con 4 valores"
        )

    # El enum sale de la misma macro, asi que TC_COUNT tiene que ser la cantidad
    # de roles.
    if not re.search(r"THEME_COLORS\(THEME_ENUM\)", texto):
        fallos.append("src/theme.h: THEME_COLORS(THEME_ENUM) no esta: TC_COUNT quedaria en 0")
    return colores


def leer_formas():
    """Forma de cada paleta base: las banderas que deciden que par existe.

    Un borde de ancho 0 no se dibuja (no hay contraste que exigir) y sin
    gradiente no hay *_grad. Estas son las mismas banderas de src/theme.c
    (base_shapes[]) y de los JSON.
    """
    texto = THEME_C.read_text(errors="surrogateescape")
    forma = {}
    for acronimo in ("BASE_IU1WXY", "BASE_LEGACY", "BASE_OUTDOOR", "BASE_CLASSIC"):
        m = re.search(r"\[" + acronimo + r"\]\s*=\s*\{(.*?)\}", texto, re.S)
        if not m:
            fallos.append(f"src/theme.c: no encontre la forma de {acronimo}")
            continue
        cuerpo = m.group(1)

        def num(clave):
            mm = re.search(r"\." + clave + r"\s*=\s*(-?\d+)", cuerpo)
            return int(mm.group(1)) if mm else 0

        def flag(clave):
            mm = re.search(r"\." + clave + r"\s*=\s*(true|false)", cuerpo)
            return mm is not None and mm.group(1) == "true"

        forma[acronimo] = {
            "border": num("border"),
            "pill_border": num("pill_border"),
            "btn_gradient": flag("btn_gradient"),
            "box_gradient": flag("box_gradient"),
            "mode_box": flag("mode_box"),
        }
    return forma


def armar_paletas(colores):
    """Las diez paletas que sirve la ROM (las cuatro base y los seis archivos)."""
    formas = leer_formas()
    acronimo = {
        "IU1WXY": "BASE_IU1WXY", "Legacy": "BASE_LEGACY", "Outdoor": "BASE_OUTDOOR",
        "Classic": "BASE_CLASSIC",
    }
    cuatro = ["IU1WXY", "Legacy", "Outdoor", "Classic"]
    paletas, shapes = {}, {}
    for i, nombre in enumerate(cuatro):
        paletas[nombre] = {rol: v[i] for rol, v in colores.items()}
        shapes[nombre] = dict(formas.get(acronimo[nombre], {}))

    # JSON: base + colors por nombre de clave (el nombre del color sin TC_), y
    # las banderas de forma que el archivo pise.
    for ruta in temas_json():
        if not ruta.exists():
            fallos.append(f"falta el tema {ruta.relative_to(RAIZ)}")
            continue
        try:
            datos = json.loads(ruta.read_text(errors="surrogateescape"))
        except json.JSONDecodeError as e:
            fallos.append(f"{ruta.relative_to(RAIZ)}: JSON invalido: {e}")
            continue
        base = datos.get("base", "IU1WXY")
        if base not in paletas:
            fallos.append(f"{ruta.relative_to(RAIZ)}: base desconocida {base!r}")
            continue
        nombre = ruta.stem if ruta.name != "theme.json" else ruta.parent.name
        paleta = dict(paletas[base])
        forma = dict(shapes[base])
        for clave, valor in (datos.get("colors") or {}).items():
            if clave not in colores:
                fallos.append(f"{ruta.relative_to(RAIZ)}: color desconocido {clave!r} (no es un TC_)")
                continue
            if not isinstance(valor, str) or not re.fullmatch(r"#?[0-9a-fA-F]{6}", valor):
                fallos.append(f"{ruta.relative_to(RAIZ)}: {clave} no es #rrggbb: {valor!r}")
                continue
            paleta[clave] = int(valor.lstrip("#"), 16)
        for clave in ("border", "pill_border", "btn_gradient", "box_gradient", "mode_box"):
            if clave in datos:
                forma[clave] = datos[clave]
        paletas[nombre], shapes[nombre] = paleta, forma

    # Cada paleta queda con los literales adentro: un par puede entonces ser
    # rol-sobre-rol, hexadecimal-sobre-rol, etc. (Clase 2 del pedido).
    for paleta in paletas.values():
        for nombre, valor in LITERALES.items():
            paleta[nombre] = valor

    # El widget de CW tune (src/cw_tune_ui.c) son dos capas semitransparentes: su panel es negro al
    # 50% sobre el fondo del tema (`lv_style_set_opa(&cw_tune_style, LV_OPA_50)`, src/styles.c) y
    # sus barras son gris 0x7f7f7f al 50% sobre ese panel. Los dos se calculan aca desde `bg`, como
    # los compone LVGL, para poder medir el par que el codigo dibuja de verdad (issue #122,
    # docs/HALLAZGOS.md 49). El borde de las barras inactivas es blanco o negro, el que mas
    # contraste tenga contra el panel: el mismo cruce de luminancia que band_plan.c y el bloque
    # activo (LUM_CORTE), y se recalcula en cada dibujo (src/cw_tune_ui.c, update_cb).
    for paleta in paletas.values():
        paleta["cw_tune_panel"] = mezclar(0x000000, paleta["bg"], 128)
        paleta["cw_tune_barra"] = mezclar(0x7F7F7F, paleta["cw_tune_panel"], 128)
        paleta["cw_tune_borde"] = (
            0xFFFFFF if luminancia(paleta["cw_tune_panel"]) < LUM_CORTE else 0x000000
        )

    return paletas, shapes


# ---------------------------------------------------------------------------
# La tabla de pares (fg sobre bg) que el codigo dibuja de verdad
# ---------------------------------------------------------------------------

# Texto generico del tema: se dibuja sobre cualquier superficie de contenido.
# Superficies de contenido: donde el codigo pone un fondo y encima una etiqueta.
#   bg            styles.c:318/349 (fondo de pantalla), dialog_field.c:1805
#   bar/bar_grad  dialog_field.c:1813-1827 (cabecera de FIELD)
#   dialog[/_grad] app_frame.c:41 + app_frame.h:31-32 (cabecera y estado del app)
#   panel         app_frame.c:52 + dialog_field.c:465/853
#   list          dialog_js8.c:2729 + 2669
#   row           dialog_js8.c:2667 + 2669
#   popup         dialog_sstv.c:1808, dialog_cluster.c:959
#   side          cluster_column.c:400 + 418/427/428/437
#   chip          dialog_cw_kbd.c:164, dialog_field.c:900
#   frame         dialog_wefax.c:1131
# card queda afuera de la tabla de texto a proposito: es un borde (theme.h:49),
# no una superficie de texto; se mide como no-texto mas abajo. Lo mismo
# sel_bg/focus/btn*: su texto es sel_text/focus_text/btn*_text, no el generico.
SUPERFICIES = [
    "bg", "bar", "bar_grad", "dialog", "dialog_grad",
    "panel", "list", "row", "popup", "side", "chip", "frame",
]
TEXTOS = ["text", "text_value", "text_head", "text_dim"]

# Colors that are NOT roles: literals the UI writes by hand (Clase 2) and the
# colors the default LVGL theme compiles in (Clase 1). They are the same in the
# fifteen themes, so what changes per theme is the surface each one lands on; the
# table below only lists the pairs that really fail on some theme.
#
# Los de LVGL salen de libreria, no de IGNORE: `lv_conf.h` deja
# LV_USE_THEME_DEFAULT=1 y LV_THEME_DEFAULT_DARK=0, y lv_hal_disp.c:216 crea el
# tema con lv_palette_main(LV_PALETTE_BLUE/RED). De ahi:
#   lvgl_item    bg_color_white    = LIGHT_COLOR_CARD = blanco (lv_theme_default.c:24,372)
#   lvgl_focused bg_color_secondary= rojo #F44336 (LV_PALETTE_RED)
#   lvgl_focuskey bg_color_primary = azul #2196F3 (LV_PALETTE_BLUE)
LITERALES = {
    "lvgl_item": 0xFFFFFF,
    "lvgl_focused": 0xF44336,
    "lvgl_focuskey": 0x2196F3,
    "hex_ffffff": 0xFFFFFF,
    "hex_000000": 0x000000,
    "hex_3366cc": 0x3366CC,
    "hex_33aa55": 0x33AA55,
    "hex_888888": 0x888888,
    "hex_cc8800": 0xCC8800,
    "hex_7f7f7f": 0x7F7F7F,
    "hex_80ff80": 0x80FF80,
    "hex_ffff30": 0xFFFF30,
    "hex_ff8080": 0xFF8080,
    "hex_ff4444": 0xFF4444,
    "hex_aaaaaa": 0xAAAAAA,
    "hex_ff0000": 0xFF0000,
    "hex_aa0000": 0xAA0000,
    "hex_aaaa00": 0xAAAA00,
}

# De donde sale cada rol de texto generico en el codigo.
USO_TEXTO = {
    "text": "dialog_field.c:534, dialog_cluster.c:254",
    "text_value": "cluster_column.c:427, freq_list.c:145",
    "text_head": "cluster_column.c:418, app_frame.h:31",
    "text_dim": "app_frame.h:32, dialog_field.c:557, dialog_cluster.c:248",
}

# Requisitos de forma: un par solo existe si el tema dibuja esa superficie.
# None = siempre. "border" = ancho de borde > 0. "box_gradient"/"btn_gradient"
# = el tema usa el gradiente. "mode_box" = el modo va en su caja.
# (fg, bg, umbral, requisito, archivo:linea de donde sale el par)
PARES_ESPECIFICOS = [
    # Seleccion, foco y botones: cada rol de texto tiene su superficie.
    ("sel_text", "sel_bg", 4.5, None, "dialog_bands.c:87+194, dialog_sstv.c:1833+1840"),
    ("sel_text", "near_bg", 4.5, None, "freq_list.c:31+134+144"),
    ("focus_text", "focus", 4.5, None, "theme.h:50-51, styles.c:229-233"),
    ("text", "focus_bg", 4.5, None, "dialog_js8.c:2668-2669"),
    # Fila seleccionada de los listados: el texto va sobre TC(ok)/TC(warn)
    # Barra de estado de arriba: los items pintan su celda con TC(bar) y el texto con roles
    # (info.c; antes se apoyaban en el bitmap top_short*.bin, que tiene partes blancas y hacia
    # fallar el gris #99A0A7 y el rojo #FF8080 con 2.3 y 2.0:1)
    ("text", "bar", 4.5, None, "info.c:84-90 (VFO, AGC, PRE/ATT, ATU, hora)"),
    ("text_dim", "bar", 4.5, None, "info.c:93+277 (WiFi/BT y ATU apagado)"),
    ("text_mute", "bar", 4.5, None, "info.c:260 (modo bloqueado)"),
    ("mode_dig", "bar", 4.5, None, "info.c:258 (modo digital)"),
    ("text_dim", "bar", 4.5, None, "info.c:289 (ATU sin cargar)"),
    ("sel_text", "sel_bg", 4.5, None, "info.c:275-300 (ATU cargado) y PRE/ATT activo"),
    ("text", "bar", 4.5, None, "info.c:275-300 (ATU apagado)"),
    # Switch y slider de Ajustes: pintados por partes con roles (styles.c styles_switch_init/
    # styles_slider_init, antes los pintaba LVGL con su tema: riel #E0E0E0 = 2.78:1)
    # Etiquetas y encabezados de los dialogos con roles del tema (Ajustes y las apps
    # que usan dialog_field.c; ver tambien quality/theme_pairs.yaml)
    ("text", "dialog", 4.5, None, "dialog_field.c:534 (etiquetas de los dialogos)"),
    ("text_dim", "dialog", 4.5, None, "dialog_field.c:557 (notas y valores atenuados)"),
    ("text_head", "dialog", 4.5, None, "dialog_field.c:151, dialog_bt.c:514 (encabezados)"),
    # Switch y slider de Ajustes: pista control_border, relleno text, perilla panel
    # (styles.c styles_switch_init/styles_slider_init)
    ("control_border", "dialog", 3.0, None, "styles.c:switch/slider (pista vs dialogo)"),
    ("control_border", "panel", 3.0, None, "styles.c:switch/slider (pista vs panel)"),
    ("panel", "control_border", 3.0, None, "styles.c:switch/slider (perilla vs pista)"),
    ("panel", "ok", 4.5, None, "dialog_field.c:578-583"),
    ("panel", "warn", 4.5, None, "dialog_field.c:578-583"),
    ("btn_text", "btn", 4.5, None, "theme.h:34+37, styles.c:162"),
    ("btn_text", "btn_grad", 4.5, "btn_gradient", "theme.h:35+37"),
    ("btn_active_text", "btn_active", 4.5, None, "theme.h:38+41"),
    ("btn_active_text", "btn_active_grad", 4.5, "btn_gradient", "theme.h:39+41"),
    ("btn_page_text", "btn_page", 4.5, None, "theme.h:42-43"),
    # Encabezados de lista (theme.h).
    ("title_text", "list", 4.5, None, "dialog_js8.c:3945"),
    ("mode_dig", "bar", 4.5, None, "info.c:256, theme.h:118 (info box)"),
    # Estados sobre las superficies donde el codigo los escribe.
    ("alert_text", "alert_bg", 4.5, None, "cluster_column.c:114+135"),
    ("text", "alert_box", 4.5, None, "dialog_cluster.c:233+254"),
    ("text_dim", "alert_box", 4.5, None, "dialog_cluster.c:233+248"),
    ("text_value", "vfo_bg", 4.5, None, "cluster_column.c:114+129"),
    ("vfo", "vfo_bg", 4.5, None, "cluster_column.c:114+254"),
    ("vfo", "side", 4.5, None, "cluster_column.c:84+254+400"),
    ("digi", "side", 4.5, None, "cluster_column.c:84+400"),
    ("warn", "side", 4.5, None, "cluster_column.c:84+400"),
    ("text_value", "side", 4.5, None, "cluster_column.c:84+427+400"),
    ("new", "side", 4.5, None, "cluster_column.c:61+136+400"),
    ("action", "row", 4.5, None, "dialog_js8.c:2667+2739"),
    ("ok", "row", 4.5, None, "dialog_js8.c:2667+3957"),
    ("info", "panel", 4.5, None, "dialog_field.c:148-151+850"),
    ("ok", "panel", 4.5, None, "dialog_field.c:148+850"),
    ("err", "panel", 4.5, None, "dialog_field.c:850+860"),
    ("accent", "panel", 4.5, None, "js8_map.c:186+196"),
    ("info", "bg", 4.5, None, "main_screen.c:986+994"),

    # --- Colores de LVGL que el proyecto NO pisaba (Clase 1) ---
    # lv_table dibuja la celda activa con LV_STATE_FOCUSED/FOCUS_KEY/EDITED
    # falsificado solo para esa celda (lvgl/src/widgets/lv_table.c:788-792) y el
    # tema por defecto le pone SUS colores. Esos pares son los que se ven cuando
    # la tabla no pisa el estado. Los literales de LVGL son los mismos en los
    # quince temas, asi que lo que cambia es el color de texto de cada tema.
    ("text", "lvgl_item", 4.5, "solo:Outdoor", "dialog_msg_cw.c:173 (texto del tema sobre la celda de LVGL)"),
    ("text_value", "lvgl_item", 4.5, "solo:Outdoor", "freq_list.c:145 (valor sobre la celda de LVGL)"),
    ("text", "lvgl_focused", 4.5, None, "lv_table.c:790 + lv_theme_default.c:349 (celda elegida, rojo #F44336)"),
    ("text_value", "lvgl_focused", 4.5, None, "lv_table.c:790 (valor sobre el rojo de LVGL)"),
    ("text", "lvgl_focuskey", 4.5, None, "lv_table.c:791 + lv_theme_default.c:349 (celda con foco de teclado, azul #2196F3)"),
    ("btn_text", "lvgl_focuskey", 4.5, None, "lv_keyboard: btnmatrix pinta la tecla con FOCUS_KEY (lv_btnmatrix.c:756)"),

    # --- Colores a mano de la UI (Clase 2) ---
    # band_plan.c:18: la etiqueta se elige por luminancia del color de la franja
    # (band_plan.c:88-96), asi que se mide el que el codigo pone para cada una.
    ("hex_000000", "hex_33aa55", 4.5, None, "band_plan.c:19+93 (etiqueta sobre el verde de DIG)"),
    ("hex_000000", "hex_cc8800", 4.5, None, "band_plan.c:19+93 (etiqueta sobre el naranja de BCN)"),
    ("hex_000000", "hex_888888", 4.5, None, "band_plan.c:19+93 (etiqueta sobre el gris de ALL MODES)"),
    ("hex_ffffff", "hex_3366cc", 4.5, None, "band_plan.c:19+93 (etiqueta sobre el azul de CW)"),
    # band_plan.c: color del segmento CB 27 MHz (issue #169): rel_lum() < LUM_CORTE,
    # asi que la etiqueta sale blanca; medido, supera AA con margen.
    ("hex_ffffff", "hex_5a2ca0", 4.5, None, "band_plan.c:19+93 (etiqueta blanca sobre el violeta CB, issue #169)"),
    # dialog_field.c:1098: la etiqueta del chip (VOL/MFK) va sobre el fondo del
    # chip, que es TC(bg); con negro literal daba 1.00:1 en Classic.
    ("text", "bg", 4.5, None, "dialog_field.c:1098 (etiqueta del chip sobre TC(bg))"),
    # cw_tune_ui.c:45: los bloques van al 50% sobre el espectro, gris literal; el
    # bloque activo usa COLOR_LIGHT_GREEN/YELLOW/RED (cw_tune_ui.c:40-42) y lleva
    # borde negro o blanco elegido por luminancia (cw_tune_ui.c:96-105), asi que el
    # par que manda es borde sobre relleno del bloque activo.
    ("hex_000000", "hex_80ff80", 3.0, None, "cw_tune_ui.c:40+103 (borde del bloque afinado, verde)"),
    ("hex_000000", "hex_ffff30", 3.0, None, "cw_tune_ui.c:41+103 (borde del bloque, amarillo)"),
    ("hex_000000", "hex_ff8080", 3.0, None, "cw_tune_ui.c:42+103 (borde del bloque desentonado, rojo)"),
    # cw_tune_ui.c: las barras INACTIVAS contra el panel del widget (issue #122,
    # docs/HALLAZGOS.md 49). El relleno es gris fijo al 50% y sobre el panel del tema
    # claro cae en el mismo valor (1.01:1 medido en Sunlit): lo que da el 3:1 de WCAG
    # 1.4.11 para el componente es el contorno, que se elige por luminancia del panel
    # (blanco sobre panel oscuro, negro sobre panel claro). El relleno se mide aparte,
    # como par informativo.
    ("cw_tune_borde", "cw_tune_panel", 3.0, None, "cw_tune_ui.c:update_cb (borde de las barras inactivas vs el panel)"),
]

# Pares que se MIDEN pero no hacen fallar: el defecto existe en el color que
# compila LVGL, pero el codigo ya pinta ese part/estado con los roles del tema
# (asi que en la UI no se ve) y no se puede arreglar desde el proyecto. Se
# informan para que quede el numero; si alguien saca el override, el par deja
# de estar cubierto y hay que volver a mirarlo.
# (fg, bg, umbral, requisito, archivo:linea)
PARES_INFORMATIVOS = [
    ("text", "lvgl_focused", 4.5, None, "tablas: el par del tema pisa el rojo #F44336 de LVGL"),
    ("text_value", "lvgl_focused", 4.5, None, "tablas: idem para los valores"),
    ("text", "lvgl_focuskey", 4.5, None, "tablas: el par del tema pisa el azul #2196F3 de LVGL"),
    ("btn_text", "lvgl_focuskey", 4.5, None, "lv_keyboard: TC(focus)/TC(focus_text) pisan el azul de LVGL"),
    # El relleno de las barras inactivas de CW tune, al 50% sobre el panel del widget: en el tema
    # claro cae en el mismo gris que el panel (1.01:1 medido en Sunlit, docs/HALLAZGOS.md 49). Se
    # informa porque el relleno es incidental: lo que identifica el componente (WCAG 1.4.11) es su
    # contorno, que se mide arriba con umbral 3:1.
    ("cw_tune_barra", "cw_tune_panel", 3.0, None, "cw_tune_ui.c:60-63 (relleno de las barras al 50%)"),
]

# No-texto (WCAG 1.4.11): bordes, anillos y barras contra lo que tienen al lado.
# Los bordes solo cuentan si el tema los dibuja (requisito "border").
# (fg, bg, requisito, archivo:linea)
PARES_NO_TEXTO = [
    ("bar_border", "bar", "border", "theme.h:30, dialog_field.c:1813"),
    ("dialog_border", "dialog", "border", "theme.h:33, app_frame.c:54"),
    ("panel_border", "panel", "border", "theme.h:56, app_frame.c:52-54"),
    ("btn_border", "btn", "border", "theme.h:36"),
    ("btn_active_border", "btn_active", "border", "theme.h:40"),
    ("popup_border", "popup", "border", "dialog_cluster.c:959-961"),
    ("card", "bg", "border", "theme.h:49 (borde de las tarjetas de Ajustes)"),
    ("control_border", "panel", "border", "theme.h:111 (sliders y toggles de Ajustes)"),
    ("sel_border", "sel_bg", None, "freq_list.c:134-135, dialog_sstv.c:1833-1835"),
    ("pill_border", "bar", "pill_border", "theme.h:120 (items del info box)"),
    ("ok_bar", "track", None, "app_frame.c:36+65 (barra de la app)"),
]

# Exenciones por "disabled/incidental" (WCAG 1.4.3): se miden y se informan,
# pero no hacen fallar. Son los colores apagados a proposito.
EXENTOS = {"btn_off_text", "text_mute", "text_faint", "icon_off", "switch_off", "off", "track"}

# Donde se ve cada color exento: se mide igual y se informa su peor ratio, pero
# no hace fallar el chequeo (WCAG 1.4.3, texto deshabilitado o incidental).
# (fg, bg, umbral, archivo:linea)
PARES_EXENTOS = [
    ("btn_off_text", "btn", 4.5, "theme.h:44 (boton deshabilitado)"),
    ("btn_off_text", "btn_grad", 4.5, "theme.h:44"),
    ("text_mute", "bg", 4.5, "theme.h:76, spectrum.c:101"),
    ("text_mute", "bar", 4.5, "theme.h:76"),
    ("text_mute", "panel", 4.5, "theme.h:76 (items grises)"),
    ("text_mute", "list", 4.5, "theme.h:76, styles.c:176"),
    ("text_mute", "side", 4.5, "cluster_column.c:64"),
    ("text_faint", "panel", 4.5, "theme.h:77 (en cola, idle)"),
    ("text_faint", "bg", 4.5, "dialog_wspr.c:464, js8_map.c"),
    ("text_faint", "list", 4.5, "theme.h:77"),
    ("icon_off", "bar", 3.0, "theme.h:109 (iconos apagados de la barra)"),
    ("switch_off", "panel", 3.0, "theme.h:108 (switch de Ajustes apagado)"),
    ("off", "panel", 3.0, "theme.h:107 (barras sin encender)"),
    ("off", "bar", 3.0, "theme.h:107"),
    ("track", "panel", 3.0, "theme.h:105, app_frame.c:36 (riel de la barra)"),
    ("track", "bar", 3.0, "theme.h:105"),
]


def armar_pares():
    pares = []
    for fg in TEXTOS:
        for bg in SUPERFICIES:
            req = "box_gradient" if bg == "dialog_grad" else None
            pares.append((fg, bg, 4.5, req, f"theme.h:{fg} sobre {bg}; uso: {USO_TEXTO[fg]}"))
    for fg, bg, umbral, req, donde in PARES_ESPECIFICOS:
        pares.append((fg, bg, umbral, req, donde))
    for fg, bg, req, donde in PARES_NO_TEXTO:
        pares.append((fg, bg, 3.0, req, donde))
    for fg, bg, umbral, donde in PARES_EXENTOS:
        pares.append((fg, bg, umbral, None, donde))
    for fg, bg, umbral, req, donde in PARES_INFORMATIVOS:
        pares.append((fg, bg, umbral, req, donde))
    return pares


def aplica(req, forma, tema=None):
    """Si ese par existe en un tema con esa forma.

    Los requisitos son banderas de forma ("border", "btn_gradient", ...). Un
    par que depende del TEMA y no de la forma se marca "solo:<tema>,<tema>".
    """
    if req is None:
        return True
    if req == "border":
        return forma.get("border", 0) > 0
    if isinstance(req, str) and req.startswith("solo:"):
        return tema in req[5:].split(",")
    return bool(forma.get(req, False))


# ---------------------------------------------------------------------------
# Chequeo
# ---------------------------------------------------------------------------


def main():
    imprimir_todo = "--all" in sys.argv[1:]

    colores = leer_theme_h()
    if not colores:
        for f in fallos:
            print("  FALLO:", f)
        return 1

    paletas, shapes = armar_paletas(colores)
    pares = armar_pares()

    print(f"  roles en src/theme.h: {len(colores)} (4 valores cada uno)")
    print(f"  temas: {len(paletas)} | pares definidos: {len(pares)}")
    print()

    encabezado = f"  {'tema':<16} {'par (fg sobre bg)':<34} {'ratio':>6} {'min':>5}  resultado"
    total_ok = total_falla = total_exento = total_info = 0
    resumen = []
    peores_exentos = {}     # (fg, bg) -> (peor ratio, tema, donde)

    for tema, paleta in paletas.items():
        forma = shapes.get(tema, {})
        filas = []
        aplicables = 0
        for fg, bg, umbral, req, donde in pares:
            if not aplica(req, forma, tema):
                continue
            if fg not in paleta or bg not in paleta:
                fallos.append(f"{tema}: falta el rol {fg if fg not in paleta else bg}")
                continue
            aplicables += 1
            ratio = contraste(paleta[fg], paleta[bg])
            exento = fg in EXENTOS
            informativo = exento or any((fg, bg) == (f, b) for f, b, *_ in PARES_INFORMATIVOS)
            ok = ratio >= umbral or informativo
            filas.append((ok, informativo, ratio, umbral, fg, bg, donde))
            if informativo:
                if exento:
                    total_exento += 1
                else:
                    total_info += 1
                if (fg, bg) not in peores_exentos or ratio < peores_exentos[(fg, bg)][0]:
                    peores_exentos[(fg, bg)] = (ratio, tema, donde)
            elif ok:
                total_ok += 1
            else:
                total_falla += 1
        resumen.append((tema, aplicables))

        mostrar = [f for f in filas if imprimir_todo or not f[0]]
        if not mostrar:
            print(f"  {tema:<16} sin fallas")
            continue

        print(f"  === {tema} ===")
        print(encabezado)
        for ok, informativo, ratio, umbral, fg, bg, donde in sorted(mostrar, key=lambda f: f[2]):
            if informativo:
                estado = "exento" if fg in EXENTOS else "informa"
            elif ok:
                estado = "OK"
            else:
                estado = "FALLA"
                fallos.append(f"{tema}: {fg} sobre {bg} = {ratio:.2f}:1 (< {umbral}) [{donde}]")
            print(f"  {'':<16} {fg + ' sobre ' + bg:<34} {ratio:>6.2f} {umbral:>5.1f}  {estado}")
        print()

    if peores_exentos:
        print("  === exenciones WCAG 1.4.3 y pares informativos (no fallan) ===")
        for (fg, bg), (ratio, tema, donde) in sorted(peores_exentos.items(), key=lambda k: k[1][0]):
            print(f"  {'':<16} {fg + ' sobre ' + bg:<34} {ratio:>6.2f}  peor en {tema}  [{donde}]")
        print()

    # El modelo de arriba mide colores derivados; para que el par del borde de las barras de CW tune
    # valga, el codigo tiene que dibujar ese borde. Se mira la fuente (misma idea que la lectura de
    # theme.h/theme.c de mas arriba): sin esto el chequeo mediria un color que nadie pinta y el
    # issue #122 (docs/HALLAZGOS.md 49) podria volver sin que nadie se entere.
    cw_tune = CW_TUNE_C.read_text(errors="surrogateescape")
    if not re.search(r"rect_dsc\.border_width\s*=\s*[1-9]", cw_tune):
        fallos.append(
            "src/cw_tune_ui.c: las barras inactivas no llevan borde (rect_dsc.border_width): sin el, "
            "sobre el panel del tema claro caen en el mismo gris (1.01:1 medido, HALLAZGOS 49)"
        )
    if not re.search(r"rect_dsc\.border_color\s*=\s*rel_lum\(panel_color\(\)\)\s*<\s*LUM_CORTE", cw_tune):
        fallos.append(
            "src/cw_tune_ui.c: el borde de las barras inactivas no se elige por la luminancia del "
            "panel (rel_lum(panel_color()) < LUM_CORTE): el color tiene que ser el que mas contraste "
            "tenga contra la superficie que lo rodea"
        )

    print("================ resumen ================")
    for tema, n in resumen:
        print(f"  {tema:<16} {n} pares aplicables")
    print(f"  pares OK: {total_ok} | exentos: {total_exento} | informativos: {total_info} | FALLAS: {total_falla}")

    if avisos:
        for a in avisos:
            print("  AVISO:", a)
    if fallos:
        for f in fallos:
            print("  FALLO:", f)
        return 1

    print("  todos los pares cumplen WCAG 2.1 (AA)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
