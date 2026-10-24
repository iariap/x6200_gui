#!/usr/bin/env python3
"""Gate de legibilidad del texto que dibujan los WIDGETS dentro de la ventana modal.

Por que existe ademas de `tests/check_wcag_rendered.py`
-------------------------------------------------------
Ese gate mide `lv_label`s: toma la tinta del arbol vivo (`--inspect`) y el fondo
renderizado debajo del glifo. Hay una familia de texto que **no** es un `lv_label`
y por eso no medía: el valor de un `lv_dropdown`, el de un `lv_spinbox` y el de un
`lv_textarea` los dibuja el widget con el estilo de la fila (`dialog_item_style`,
`src/styles.c`). El gate viejo los veía en dos formas, ninguna buena:

* el texto del dropdown no aparece como etiqueta en el arbol ⇒ **no existía** para
  el gate;
* el del textarea/spinbox sí es una `lv_label` interna, pero cuando no dibujaba un
  solo píxel el gate lo anotaba como "sin tinta" (una declaración, no un fallo).

El QA final de la epica #284 (issue #287) midió ese hueco: en el tema **Sunlit**
los valores de `Font`, `Visual profile`, `GEN long press`, `APP long press`,
`Accel threshold` y `Pair / connect` quedaban **invisibles** (7 widgets con 0 px de
tinta, blanco sobre blanco). La causa era `dialog_item_style` con
`lv_style_set_text_color(..., lv_color_white())` mientras #286 cambió la superficie
del dialogo a `TC(dialog)` (#FFFFFF en Sunlit). El gate de contraste no lo vio
porque mide etiquetas. Este gate cierra esa clase: mide, por tema, **todo** el
texto de la ventana modal —etiquetas y valores de widget— sobre los píxeles y
falla si un texto no dibuja tinta o no llega al umbral.

Que mide
--------
Para cada fixture de la matriz (Ajustes + Bluetooth x los seis temas) renderiza
con el arnes real (`tools/ui_harness`, LVGL 8.3.11 -> PNG 800x480, dentro de la
imagen `x6200-uiharness`, que es lo unico que resuelve los bitmaps
`images/*.bin`) y recorre el arbol LVGL vivo (`--inspect`) **dentro** de la
superficie modal (796x348, la caja de `dialog_style`). Por cada nodo de texto
(`lv_label`, `lv_dropdown`, `lv_textarea`, `lv_roller`, `lv_spinbox`,
`lv_checkbox`):

* **tinta**: los píxeles de su caja visible cuya cobertura respecto del color
  declarado (`text_color`) llega a `COBERTURA_TINTA`. El color declarado es el
  que el widget usa para dibujar; no se adivina por el nombre del rol.
* **fondo bajo el glifo**: por cada columna con tinta se muestrean los píxeles a
  `SALTO` px por encima del primer píxel de tinta y por debajo del último (el
  fondo local de esa columna); el ratio es la **mediana** de esos pares. Si la
  caja es tan ajustada al glifo que no hay muestra vertical, se usa el color
  modal de los píxeles que no son tinta (la superficie medida en esa caja).
* **umbral WCAG 2.1**: el mismo de `tests/check_wcag_rendered.py` — SC 1.4.3
  texto normal >= 4,5:1, texto grande >= 3:1 (grande = >= 24 px, el ROM no tiene
  negrita). El tamano sale de `text_font` del arbol (el inspector publica la
  fuente resuelta de **todo** nodo, no solo de las etiquetas).

Un texto con **0 px de tinta** es un fallo, no una declaración: es exactamente el
defecto del #287. La unica excepcion es un `lv_textarea` vacio por diseno: para
esa clase, 0 tinta es fallo solo si el mismo widget (familia + clase + caja) tiene
tinta en otro tema de la matriz (la regla comparativa del QA), porque entonces la
diferencia es el tema, no el contenido.

Estados que NO se miden (declarados, con motivo)
------------------------------------------------
* **Foco / edicion / hover / pressed**: el tema default de LVGL **anima**
  `bg_opa` entre estados (`lv_theme_default.c`, TRANSITION_TIME). Un frame fijo
  capturado justo despues del cambio muestra el overlay en su valor inicial, asi
  que sus píxeles son un frame de transicion, no el par que el codigo declara
  (`dialog_item_focus_style`: tinta negra sobre un overlay blanco al 50 %). Se
  cuentan y se informan, no se miden como si fueran el diseno.
* **Deshabilitado**: WCAG 1.4.3 exime los componentes de UI inactivos (misma
  clasificacion que `tests/check_wcag.py::EXENTOS`).
* **Fuera de vista / oculto / opacidad 0**: no hay pixel que medir.
* **Texto atenuado por opacidad** (`text_opa < 255`): incidental (decorativo).
* **Etiqueta con markup de recolor y mas de un color**: no hay un unico par
  declarado; se declara no medible (no se inventa).

Cobertura (anti falso verde)
----------------------------
Un gate que no encuentra nada y pasa es un falso verde. Por eso:

* cada fixture tiene que tener **exactamente una** superficie modal (796x348) y
  al menos un nodo de texto medido; si no, FALLA;
* las fixtures de Ajustes tienen que seguir rindiendo sus controles: `>= 4`
  desplegables en `settings-tema-*` y `>= 1` desplegable + `>= 1` spinbox en
  `settings-grupo-operation-*`. Si un refactor de la pantalla deja de crearlos,
  el gate lo dice en vez de medir una matriz vacia.

Control negativo
----------------
Con la tinta fija en blanco de vuelta en `dialog_item_style`, el gate **falla**:
los 7 widgets de Sunlit quedan con 0 px de tinta (medido antes del fix en el PR
del #287). Es la prueba de que el gate ve la clase y no solo el estado actual.

Uso
---
  python3 tests/check_widget_text.py [--all]
  UI_HARNESS_IMAGE=<name>   override de la imagen Docker (default x6200-uiharness)

Siempre corre cmake+make (incremental) antes de renderizar: reusar un binario sin
verificar es el falso verde que ya se pago en el gate de determinismo.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HARNESS_DIR = ROOT / "tools" / "ui_harness"
BUILD_DIR = HARNESS_DIR / "build"
HARNESS_BIN = BUILD_DIR / "ui_harness"
OUT_DIR = BUILD_DIR / "out" / "widget-text"

# La caja de `dialog_style`: `src/styles.c` (width 796) y `src/styles.h`
# (DIALOG_BOX_H 348). Es la superficie modal sobre la que se dibuja el texto que
# este gate mide; si la geometria cambia, la cobertura falla y hay que ajustarla.
DIALOG_W = 796
DIALOG_H = 348

# --- criterios WCAG 2.1 (los mismos numeros y fuentes que check_wcag_rendered.py) --
NORMAL_TEXT_MIN = 4.5
LARGE_TEXT_MIN = 3.0
LARGE_TEXT_PX = 24

# --- metodo de medicion de pixeles -------------------------------------------
# El borde/indicador del widget no es texto: se mide el interior de la caja, con
# el mismo inset de 5 px que uso la medicion del QA (#287). Sin el, el borde
# blanco de un desplegable cuenta como "tinta" cuando el color declarado es
# blanco (falso positivo medido: la franja recortada de 13 px de Ajustes daba
# 592 px de "tinta" que eran el borde, no el valor).
INSET = 5
COBERTURA_TINTA = 0.45
FONDO_COBERTURA = 0.25
SALTO = 4
MIN_INK_PX = 6

# Marcador de "no medido" del arnes (labels placeholder de los mocks inertes).
PLACEHOLDER_TEXT = "Text"

# Clases cuyo texto dibuja el widget (no son `lv_label` en el arbol).
WIDGET_CLASSES = {"lv_dropdown", "lv_textarea", "lv_roller", "lv_spinbox", "lv_checkbox"}
TEXT_CLASSES = WIDGET_CLASSES | {"lv_label"}
# Controles que siempre muestran un valor: 0 tinta es siempre un defecto. Un
# textarea puede estar vacio por diseno, asi que se juzga comparando temas.
ALWAYS_VALUE = {"lv_dropdown", "lv_spinbox", "lv_roller", "lv_checkbox"}

# Estados con transicion de `bg_opa` (frame no representativo) y estado exento.
TRANSIENT_STATES = {"focused", "focus_key", "edited", "pressed", "hovered"}
DISABLED_STATE = "disabled"

# Markup de recolor de LVGL: "#RRGGBB texto".
RECOLOR_MARKUP = re.compile(r"^#([0-9A-Fa-f]{6}) (.*)$", re.S)

THEMES = ["aurora", "cathode", "redshift", "sprint", "sunlit", "trail"]
FIXTURE_STATES = [
    "settings-tema-{t}",  # Ajustes (grupo Appearance): desplegables de valor
    "settings-grupo-operation-tema-{t}",  # Ajustes (Operation): spinbox + boton
    "bt-dialog-tema-{t}",  # ventana Bluetooth: etiquetas de la tabla
]
SUFIJO_TEMA = re.compile(r"-tema-(aurora|cathode|redshift|sprint|sunlit|trail)$")


def fixtures() -> list[str]:
    """La lista de fixtures del gate (matriz de temas x estados de Ajustes/BT)."""
    return [state.format(t=t) for state in FIXTURE_STATES for t in THEMES]


def familia(nombre: str) -> str:
    """La fixture sin el sufijo del tema (para comparar el mismo widget)."""
    return SUFIJO_TEMA.sub("", nombre)


def tema(nombre: str) -> str:
    """El tema de la fixture, o 'default' si no declara uno."""
    m = SUFIJO_TEMA.search(nombre)
    return m.group(1) if m else "default"


# ---------------------------------------------------------------------------
# Formula WCAG 2.1 (misma que tests/check_wcag.py y tests/check_wcag_rendered.py)
# ---------------------------------------------------------------------------


def _canal(c: float) -> float:
    """Normaliza un canal 0..255 a 0..1 y aplica la transferencia sRGB."""
    c = c / 255.0
    if c <= 0.03928:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def luminance(rgb) -> float:
    """Luminancia relativa WCAG de un (r, g, b) 0..255."""
    r, g, b = (float(v) for v in rgb)
    return 0.2126 * _canal(r) + 0.7152 * _canal(g) + 0.0722 * _canal(b)


def contrast(a, b) -> float:
    """Ratio de contraste WCAG entre dos (r, g, b), siempre >= 1."""
    la, lb = luminance(a), luminance(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


def hex_rgb(value: str) -> tuple[float, float, float]:
    value = value.lstrip("#")[-6:]
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16))


def rgb_hex(rgb) -> str:
    return "#{:02X}{:02X}{:02X}".format(*(round(v) for v in rgb))


def font_px(font: str) -> int:
    """El tamano en px que codifica el nombre de la fuente (`sony_36` -> 36)."""
    m = re.search(r"_(\d+)$", font or "")
    return int(m.group(1)) if m else 0


# ---------------------------------------------------------------------------
# Render via el arnes de host (Docker)
# ---------------------------------------------------------------------------


def _docker_run(args: list[str], image: str) -> None:
    """Corre un comando en el arnes con el cwd en `tools/ui_harness`."""
    cmd = [
        "docker",
        "run",
        "--rm",
        "--entrypoint",
        "bash",
        "--user",
        f"{os.getuid()}:{os.getgid()}",
        "-v",
        f"{ROOT}:/src",
        "-w",
        "/src/tools/ui_harness",
        image,
        *args,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        raise SystemExit(f"FALLO: el comando docker del arnes fallo (rc={proc.returncode})")


def ensure_harness(image: str) -> None:
    """Comprueba docker/imagen y compila el arnes (cmake+make incremental).

    Se compila SIEMPRE (es incremental: sin cambios es un no-op) para que el
    binario refleje el arbol. Reusar un binario existente sin verificar la
    frescura es el falso verde que ya se pago en el gate de determinismo: un
    build viejo mide codigo que ya no esta.
    """
    if shutil.which("docker") is None:
        raise SystemExit("FALLO: docker no esta disponible (el gate renderiza con el arnes)")
    if subprocess.run(["docker", "image", "inspect", image], capture_output=True).returncode != 0:
        raise SystemExit(
            f"FALLO: falta la imagen '{image}' "
            f"(construila: docker build -t {image} tools/ui_harness)"
        )
    rev = (
        subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True
        ).stdout.strip()
        or "unknown"
    )
    print(f"  construyendo/actualizando el arnes (revision {rev})...")
    _docker_run(
        [
            "-lc",
            "cd /src/tools/ui_harness && mkdir -p build && cd build && "
            f"cmake .. -DHARNESS_REVISION={rev} >/dev/null && make -j$(nproc) >/dev/null",
        ],
        image,
    )
    if not HARNESS_BIN.exists():
        raise SystemExit(f"FALLO: el build no produjo {HARNESS_BIN}")


def render(image: str) -> None:
    """Renderiza las fixtures del gate (PNG + JSON con `--inspect`)."""
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "png").mkdir(exist_ok=True)
    (OUT_DIR / "json").mkdir(exist_ok=True)
    lista = " ".join(fixtures())
    script = (
        "set -e; cd /src/tools/ui_harness/build; "
        "OUT=out/widget-text; rm -rf $OUT; mkdir -p $OUT/png $OUT/json; "
        f"for f in {lista}; do "
        "./ui_harness --scenario ../fixtures/$f.scn "
        "--png $OUT/png/$f.png --json $OUT/json/$f.json --inspect >/dev/null 2>&1 "
        '|| { echo "FALLO render $f" >&2; exit 1; }; done'
    )
    _docker_run(["-lc", script], image)


# ---------------------------------------------------------------------------
# Medicion sobre el PNG + el arbol `--inspect`
# ---------------------------------------------------------------------------


def _is_dialog_surface(node: dict) -> bool:
    """La superficie modal: la caja visible de `dialog_style` (796x348)."""
    v = node.get("visible")
    return bool(v) and v["w"] == DIALOG_W and v["h"] == DIALOG_H


def _walk(node: dict, inside: bool, hidden_ancestor: bool):
    """DFS pre-orden (== orden de dibujo de LVGL): (node, dentro, oculto por ancestro)."""
    yield node, inside, hidden_ancestor
    oculto = hidden_ancestor or bool(node.get("hidden"))
    for child in node.get("children") or []:
        yield from _walk(child, inside or _is_dialog_surface(node), oculto)


def _has_text_label(node: dict) -> bool:
    """True si el nodo tiene una `lv_label` hija con texto (el valor del textarea)."""
    for child in node.get("children") or []:
        if child.get("class") == "lv_label" and (child.get("text") or "").strip():
            return True
    return False


def _visible_box(node: dict):
    v = node.get("visible")
    if not v or v.get("w", 0) <= 1 or v.get("h", 0) <= 1:
        return None
    return (v["x"], v["y"], v["w"], v["h"])


def _measure(np, img, box, ink_hex):
    """Tinta y fondo medidos del bbox visible de un nodo de texto.

    Devuelve `None` cuando no hay fondo distinguible de la tinta (no hay ningun
    pixel que no sea del color declarado): en ese caso no se puede dibujar nada.
    """
    x, y, w, h = box
    x0, y0 = max(0, x + INSET), max(0, y + INSET)
    x1, y1 = min(img.shape[1], x + w - INSET), min(img.shape[0], y + h - INSET)
    if x1 <= x0 or y1 <= y0:
        # Caja mas chica que el inset: se mide entera (no hay borde que excluir).
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(img.shape[1], x + w), min(img.shape[0], y + h)
    if x1 <= x0 or y1 <= y0:
        return None
    sub = img[y0:y1, x0:x1]
    propio = np.ones(sub.shape[:2], bool)
    tc = np.array(hex_rgb(ink_hex), dtype=float)
    dist = np.abs(sub - tc).max(axis=2)
    umbral = np.percentile(dist[propio], 60) if propio.any() else 96
    lejos = propio & (dist >= umbral)
    if lejos.sum() < 4:
        lejos = propio & (dist >= 96)
    if lejos.sum() == 0:
        return None
    cols, counts = np.unique(sub[lejos], axis=0, return_counts=True)
    fondo = cols[int(np.argmax(counts))].astype(float)
    den = tc - fondo
    canales = np.abs(den) > 32
    if not canales.any():
        # Tinta y fondo son el mismo color: no hay glifo distinguible.
        return {"ink_px": 0, "bg": fondo, "ratio": 1.0, "method": "tinta == fondo"}
    cov = np.clip(((sub - fondo)[..., canales] / den[canales]).mean(axis=2), 0.0, 1.0)
    inb = (cov >= COBERTURA_TINTA) & propio
    ink_px = int(inb.sum())
    if ink_px < MIN_INK_PX:
        return {"ink_px": ink_px, "bg": fondo, "ratio": None, "method": "sin glifo"}
    ratios = []
    for cx in range(inb.shape[1]):
        filas = np.nonzero(inb[:, cx])[0]
        if len(filas) == 0:
            continue
        for fy in (filas[0] - SALTO, filas[-1] + SALTO):
            gy, gx = y0 + fy, x0 + cx
            if not (y0 <= gy < y1 and x0 <= gx < x1):
                continue
            if cov[fy, cx] >= FONDO_COBERTURA:
                continue
            ratios.append(contrast(img[gy, gx], tc))
    if ratios:
        arr = np.array(ratios)
        return {
            "ink_px": ink_px,
            "bg": fondo,
            "ratio": round(float(np.median(arr)), 2),
            "method": "render",
        }
    # Caja tan ajustada al glifo que no hay muestra vertical: el fondo local es
    # el color modal de los píxeles que no son tinta (la superficie de esa caja).
    return {
        "ink_px": ink_px,
        "bg": fondo,
        "ratio": round(contrast(fondo, tc), 2),
        "method": "fondo local",
    }


def _declared_ink(node: dict) -> tuple[str, str] | None:
    """Color de tinta y texto visible de un nodo de texto.

    Para un widget (dropdown/spinbox/...) el texto lo dibuja el widget y no esta
    en el arbol: solo hay color declarado. Para una etiqueta con markup de
    recolor el color real es el del markup, no el del estilo; si hay mas de uno,
    no hay un unico par declarado y se devuelve `None` (no medible).
    """
    text = (node.get("text") or "").strip()
    if node.get("class") != "lv_label":
        return node["text_color"], ""
    if not node.get("recolor"):
        return node["text_color"], text
    colores = re.findall(r"#([0-9A-Fa-f]{6})", text)
    if len({c.upper() for c in colores}) > 1:
        return None
    m = RECOLOR_MARKUP.match(text)
    if m:
        return f"#{m.group(1)}", m.group(2).strip()
    return node["text_color"], text


def measure_fixture(np, fixture: str) -> tuple[list[dict], list[dict], list[str]]:
    """Mide los nodos de texto de una captura.

    Devuelve (filas medidas, filas declaradas no medidas, errores de cobertura).
    """
    from PIL import Image

    data = json.loads((OUT_DIR / "json" / f"{fixture}.json").read_text())
    img = np.array(Image.open(OUT_DIR / "png" / f"{fixture}.png").convert("RGB"))
    items = list(_walk(data["inspection"]["nodes"][0], False, False))
    superficies = [n for n, _, oculto in items if _is_dialog_surface(n) and not oculto]
    errores: list[str] = []
    if len(superficies) != 1:
        errores.append(
            f"{fixture}: se esperaba UNA superficie modal {DIALOG_W}x{DIALOG_H} y hay "
            f"{len(superficies)} (¿cambió la caja de dialog_style?)"
        )

    medidos, declarados = [], []
    for node, inside, oculto in items:
        if not inside:
            continue
        clase = node.get("class")
        if clase not in TEXT_CLASSES:
            continue
        texto = (node.get("text") or "").strip()
        fila = {
            "fixture": fixture,
            "familia": familia(fixture),
            "tema": tema(fixture),
            "clase": clase,
            "texto": texto,
        }
        if oculto or node.get("hidden"):
            fila["estado"] = "no medido: oculto"
            declarados.append(fila)
            continue
        if (node.get("opa") or 0) == 0:
            fila["estado"] = "no medido: opacidad 0"
            declarados.append(fila)
            continue
        box = _visible_box(node)
        if box is None:
            fila["estado"] = "no medido: fuera de vista"
            declarados.append(fila)
            continue
        if texto == PLACEHOLDER_TEXT:
            fila["estado"] = "no medido: placeholder del arnes"
            declarados.append(fila)
            continue
        if clase in {"lv_textarea", "lv_spinbox"} and _has_text_label(node):
            # El textarea/spinbox no dibuja su texto: lo dibuja una `lv_label`
            # interna, que se mide como tal. Medir el contenedor daria 0 tinta
            # aunque el valor se vea perfecto (doble conteo del mismo texto).
            fila["estado"] = "no medido: el valor lo dibuja su etiqueta interna"
            declarados.append(fila)
            continue
        # Recortado por un ancestro (p. ej. el borde del area scrolleable de
        # Ajustes): la caja visible es una franja y el glifo puede quedar fuera.
        # Se mide igual (lo que se ve), pero un 0 de tinta en una franja no es
        # concluyente por si solo: se juzga comparando temas (mas abajo).
        visible = node.get("visible")
        fila["recortado"] = visible != {
            "x": node["x"],
            "y": node["y"],
            "w": node["w"],
            "h": node["h"],
        }
        states = set((node.get("state") or "").split())
        if states & TRANSIENT_STATES:
            fila["estado"] = "no medido: estado transitorio (foco/edicion)"
            declarados.append(fila)
            continue
        if DISABLED_STATE in states:
            fila["estado"] = "no medido: control inactivo (WCAG 1.4.3 exento)"
            declarados.append(fila)
            continue
        if node.get("text_opa", 255) != 255:
            fila["estado"] = "no medido: texto atenuado por opacidad (incidental)"
            declarados.append(fila)
            continue
        declarado = _declared_ink(node)
        if declarado is None:
            fila["estado"] = "no medido: markup de recolor con varios colores"
            declarados.append(fila)
            continue
        ink_hex, visible_text = declarado
        if clase == "lv_label" and not visible_text:
            fila["estado"] = "no medido: etiqueta sin texto"
            declarados.append(fila)
            continue
        fila["texto"] = visible_text
        fila["caja"] = f"{box[0]},{box[1]},{box[2]},{box[3]}"
        fila["tinta"] = ink_hex.upper()
        fila["fuente_px"] = font_px(node.get("text_font", ""))
        m = _measure(np, img, box, ink_hex)
        if m is None:
            fila.update(
                {
                    "px_tinta": 0,
                    "fondo": "?",
                    "ratio": None,
                    "metodo": "sin fondo distinguible",
                    "min": None,
                    "estado": "sin tinta",
                }
            )
            medidos.append(fila)
            continue
        minimo = LARGE_TEXT_MIN if fila["fuente_px"] >= LARGE_TEXT_PX else NORMAL_TEXT_MIN
        fila.update(
            {
                "px_tinta": m["ink_px"],
                "fondo": rgb_hex(m["bg"]),
                "ratio": m["ratio"],
                "metodo": m["method"],
                "min": minimo,
            }
        )
        if m["ink_px"] < MIN_INK_PX:
            fila["estado"] = "sin tinta"
        elif m["ratio"] is None or m["ratio"] < minimo:
            fila["estado"] = "bajo umbral"
        else:
            fila["estado"] = "ok"
        medidos.append(fila)
    if not medidos:
        errores.append(f"{fixture}: no se midio ningun texto (la ventana modal no dibujo texto)")
    return medidos, declarados, errores


# ---------------------------------------------------------------------------
# Cobertura esperada (anti falso verde)
# ---------------------------------------------------------------------------

# Controles que cada fixture tiene que seguir rindiendo: si un refactor de la
# pantalla deja de crearlos, el gate mediria una matriz vacia y pasaria. Se
# codifica aca, no en prosa.
COBERTURA_ESPERADA = {
    "settings-tema": {"lv_dropdown": 4},
    "settings-grupo-operation-tema": {"lv_dropdown": 1, "lv_spinbox": 1},
}


# Un nodo que no esta en pantalla no prueba que la fixture siga rindiendo el
# control: el conteo de cobertura mira solo los que se ven (medidos o
# declarados por otra razon, como el contenedor del spinbox o el foco).
NO_CUENTAN = {
    "no medido: fuera de vista",
    "no medido: oculto",
    "no medido: opacidad 0",
}


def _cobertura(filas: list[dict]) -> list[str]:
    """Errores de cobertura: cada fixture rinde los controles que el gate mide."""
    errores = []
    for fixture in fixtures():
        fam = familia(fixture)
        if fam not in COBERTURA_ESPERADA:
            continue
        de_la_fixture = [
            f for f in filas if f["fixture"] == fixture and f["estado"] not in NO_CUENTAN
        ]
        for clase, minimo in COBERTURA_ESPERADA[fam].items():
            n = sum(1 for f in de_la_fixture if f["clase"] == clase)
            if n < minimo:
                errores.append(
                    f"{fixture}: se esperaban >= {minimo} {clase} visibles y hay {n} "
                    f"(¿la pantalla dejo de crear el control?)"
                )
    return errores


# ---------------------------------------------------------------------------
# Veredicto
# ---------------------------------------------------------------------------


def _veredicto(
    medidos: list[dict], declarados: list[dict], errores: list[str], verbose: bool
) -> int:
    """Aplica la regla comparativa y decide el resultado del gate."""
    grupos: dict[tuple[str, str, str], dict[str, int]] = {}
    for f in medidos:
        clave = (f["familia"], f["clase"], f["caja"])
        grupos.setdefault(clave, {})[f["tema"]] = f["px_tinta"]

    hallazgos = []
    for f in medidos:
        clave = (f["familia"], f["clase"], f["caja"])
        con_otros = any(px > 0 for t, px in grupos[clave].items() if t != f["tema"])
        if f["estado"] == "sin tinta":
            if f["clase"] in ALWAYS_VALUE or f["texto"]:
                if f.get("recortado") and not con_otros:
                    # Franja recortada sin glifo visible: no dice nada del color
                    # (pasa en los seis temas). El control se mide en las filas
                    # que si entran completas.
                    f["estado"] = "no medido: recortado y sin glifo en la franja visible"
                else:
                    hallazgos.append(f)
            elif con_otros:
                # El mismo widget dibuja en otro tema: el tema es la causa.
                hallazgos.append(f)
            else:
                f["estado"] = "no medido: vacio por diseno (sin tinta en ningun tema)"
        elif f["estado"] == "bajo umbral":
            hallazgos.append(f)

    print("== gate de legibilidad del texto de los WIDGETS en la ventana modal (issue #287) ==")
    print(
        f"  fixtures: {len(fixtures())} (matriz de {len(THEMES)} temas x "
        f"{len(FIXTURE_STATES)} estados de Ajustes/BT)"
    )
    print(f"  nodos de texto medidos: {len(medidos)}; declarados no medidos: {len(declarados)}")
    por_clase: dict[str, int] = {}
    for f in medidos:
        por_clase[f["clase"]] = por_clase.get(f["clase"], 0) + 1
    print("  por clase: " + ", ".join(f"{k} {v}" for k, v in sorted(por_clase.items())))
    motivos: dict[str, int] = {}
    for f in declarados:
        motivos[f["estado"]] = motivos.get(f["estado"], 0) + 1
    for motivo, n in sorted(motivos.items()):
        print(f"    declarados, no medidos: {n:>3}  {motivo}")

    if verbose:
        print("  --- todos los nodos medidos ---")
        for f in sorted(medidos, key=lambda f: (f["fixture"], f["caja"])):
            ratio = "—" if f["ratio"] is None else f"{f['ratio']:>5}:1"
            minimo = "—" if f["min"] is None else str(f["min"])
            print(
                f"    {f['fixture']:<40} {f['clase']:<12} {f['texto'][:18]!r:<20} "
                f"caja {f['caja']:<18} tinta {f['px_tinta']:>4} {ratio} "
                f"min {minimo:<3} [{f['metodo']}] {f['estado']}"
            )

    print("  --- hallazgos ---")
    for f in hallazgos:
        ratio = "sin tinta" if f["ratio"] is None else f"{f['ratio']}:1"
        print(
            f"    {f['fixture']:<40} {f['clase']:<12} {f['texto'][:18]!r:<20} "
            f"caja {f['caja']:<18} px_tinta {f['px_tinta']:>4} {ratio} "
            f"({f['tinta']} sobre {f['fondo']}, min {f['min']})"
        )

    if errores:
        print("  FALLA: cobertura del gate:")
        for e in errores:
            print(f"    {e}")
    if hallazgos:
        print(
            f"  {len(hallazgos)} hallazgo(s): un texto de la ventana modal no dibuja "
            f"tinta o no llega al umbral WCAG (la clase del #287)"
        )
        return 1
    if errores:
        return 1
    print("  PASS: todo el texto de la ventana modal dibuja tinta y llega al umbral WCAG")
    return 0


def main(argv=None) -> int:
    import numpy as np

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--all", action="store_true", help="imprime todos los nodos medidos")
    args = parser.parse_args(argv)

    image = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")
    ensure_harness(image)
    render(image)

    medidos, declarados, errores = [], [], []
    for fixture in fixtures():
        m, d, e = measure_fixture(np, fixture)
        medidos.extend(m)
        declarados.extend(d)
        errores.extend(e)
    errores.extend(_cobertura(medidos + declarados))
    return _veredicto(medidos, declarados, errores, args.all)


if __name__ == "__main__":
    sys.exit(main())
