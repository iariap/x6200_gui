#!/usr/bin/env python3
"""Gate de contraste WCAG 2.1 medido sobre la superficie RENDERIZADA.

Que hace distinto a `tests/check_wcag.py`
-----------------------------------------
`check_wcag.py` compara **roles estaticos** del tema (p. ej. `text` sobre
`dialog` = 12:1 en Sunlit) y por eso queda verde mientras el texto renderizado
es ilegible: la ventana de Ajustes no pinta el rol `dialog`, pinta el **bitmap**
`images/dialog.bin` (`bg_image: true`, `bg_opa: 0` en el `--inspect`) y la tinta
de la etiqueta sale del rol `text` (#162436 en el `base: Outdoor`), o sea oscuro
sobre oscuro (issue #277, evidencia de la epica #269). Este gate mide el par que
el usuario **ve**: toma la tinta del elemento del arbol vivo (`--inspect`) y el
fondo **realmente renderizado debajo del glifo** muestreado del PNG del arnes
(el color modal del bbox excluyendo los pixeles de la tinta).

Criterios WCAG 2.1 que aplica (citados en el codigo, abajo en `THRESHOLDS`)
---------------------------------------------------------------------------
* **SC 1.4.3 (Contrast Minimum)**: texto **normal >= 4,5:1**; **texto grande >=
  3:1**. Grande = **>= 18 pt** (24 px) o **>= 14 pt en negrita** (18,66 px). El
  ROM no tiene slots de fuente en negrita (los nombres `sony_NN` solo llevan el
  tamano en px), asi que aca "grande" es `px >= 24`; la clausula de negrita no
  es aplicable y se declara.
* **SC 1.4.11 (Non-text Contrast)**: componentes de UI / graficos (iconos,
  indicadores) **>= 3:1**.
* **Exenciones de 1.4.3** (no son un pase: son una clasificacion con motivo):
  los roles que el proyecto ya declara apagados a proposito (`btn_off_text`,
  `text_mute`, `text_faint`, `icon_off`, `switch_off`, `off`, `track`; la misma
  lista de `tests/check_wcag.py::EXENTOS` y `tools/ab_testing/quality/
  theme_pairs.yaml::exempt`) se clasifican como *disabled/incidental*; el texto
  atenuado por opacidad (`text_opa < 255`, p. ej. el "8 fantasma") se clasifica
  como *incidental (decorativo)*.

Formula: luminancia relativa WCAG (linealizacion sRGB correcta, la de la
transferencia) y `(L_claro + 0,05) / (L_oscuro + 0,05)`. Es la misma de
`tests/check_wcag.py` y `x6200_ab.contrast` (una sola formula, tres lectores).

Como mide
---------
Para cada fixture del set (matriz de temas x estados) renderiza con el arnes
real (`tools/ui_harness`, LVGL 8.3.11 -> PNG 800x480, dentro de la imagen
`x6200-uiharness`, que es lo unico que resuelve los bitmaps `images/*.bin`).
Por cada `lv_label` visible:

* **tinta**: los pixeles del bbox (+2 px de AA) cuya cobertura respecto del
  color declarado (`text_color`) llega a `COBERTURA_TINTA`; el color de fondo se
  estima como el modal de los pixeles "lejos" de esa tinta.
* **fondo bajo el glifo**: por cada columna con tinta se muestrean los pixeles a
  `SALTO` px por encima del primer pixel de tinta y por debajo del ultimo (el
  fondo local de esa columna, que en un bitmap no es uniforme); el ratio es la
  **mediana** de esos pares (el percentil 5 se informa aparte).

No infiere legibilidad fisica (eso es requires-hardware): todo es px de un
render determinista de host.

Que NO mide (declarado)
-----------------------
* Elementos **ocluidos** por una superficie dibujada despues (p. ej. la fila de
  frecuencia detras de la ventana de Ajustes): se declaran, no se miden — el
  sangrado de la tinta a traves de un panel semitransparente no es el contraste
  de un elemento operable.
* Elementos que **no exponen su color** en el arbol (los `lv_line`, como la runa
  de BT): se declaran no medibles; el inspector no publica su color de trazo.
* Tinta que no aparece en su bbox (glifos no dibujados): se declaran sin tinta.
  Si la etiqueta pinta su propio fondo opaco (`bg_opa` COVER, sin imagen), se mide
  el **par declarado** `text_color`/`bg_color` (que es el renderizado), para que
  una regresion que lleve la tinta al color del fondo no se escape como "sin
  tinta" (es el agujero que el defecto del #277 aprovecharia).

Baseline (waiver por par, versionado)
-------------------------------------
Las violaciones PREEXISTENTES (issue #277) se enumeran una por una en
`tests/wcag_rendered_baseline.json` (misma idea que los waivers del proyecto:
`rule` + `fixture` + `path` + `reason` + `author` + `until`, mas el ratio
medido). El gate:

* **PASA** si toda violacion medida esta en el baseline y ninguna empeoro.
* **FALLA** si hay una violacion **nueva** (par que no esta en el baseline) o una
  **regresion** (un par del baseline con peor ratio que el registrado).
* **FALLA** si el baseline quedo **obsoleto** (un par del baseline ya no viola):
  hay que regenerarlo con `--update-baseline`, que imprime el diff de lo que
  agrega (`+`), de lo que saca (`-`) y de lo que **cambia de ratio** (`~`) —un
  par que sigue violando pero con otro numero medido— (nada se bendice a ciegas).

El gate compara los ratios **exactos**, sin tolerancia: el render del arnes es
determinista dentro de la imagen fija y reproducible entre hosts, asi que un
ratio que se mueve entre revisiones es un cambio de codigo/render real, no ruido
de medicion (una tolerancia solo taparia regresiones chicas). Cuando el render
cambia a proposito, la accion correcta es **re-baselinar** las entradas movidas
con `--update-baseline` (el diff `~` las lista), no aflojar la comparacion.

No hay blanket-skip: el waiver es por par enumerado; nunca "saltear un tema" ni
"saltear si el ratio < X".

Uso
---
  python3 tests/check_wcag_rendered.py [--all] [--update-baseline]
  SKIP_BUILD=1             reusa tools/ui_harness/build (falla si el binario es
                           mas viejo que los fuentes: no mide codigo viejo)
  UI_HARNESS_IMAGE=<name>  override de la imagen Docker (default x6200-uiharness)

Frescura del render
-------------------
Por default el gate SIEMPRE corre cmake+make (incremental) antes de renderizar,
asi que el binario refleja el arbol; `SKIP_BUILD=1` reusa el build pero aborta si
algun fuente (`src/**`, `tools/ui_harness/**`) es mas nuevo que el binario. Es la
trampa que ya se pago en el gate de determinismo: un build reusado mide codigo
que ya no esta y da un falso verde (aqui, con el baseline intacto, 29 entradas
cuando el fix #281 ya habia resuelto 11).

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HARNESS_DIR = ROOT / "tools" / "ui_harness"
BUILD_DIR = HARNESS_DIR / "build"
HARNESS_BIN = BUILD_DIR / "ui_harness"
OUT_DIR = BUILD_DIR / "out" / "wcag-rendered"
BASELINE = ROOT / "tests" / "wcag_rendered_baseline.json"
THEME_H = ROOT / "src" / "theme.h"
THEMES_DIR = ROOT / "rootfs" / "usr" / "share" / "x6200" / "themes"

# --- criterios WCAG 2.1 (citados) -------------------------------------------
# SC 1.4.3 (AA): texto normal 4,5:1; texto grande 3:1, grande = >= 18 pt (24 px)
# o >= 14 pt negrita (18,66 px). El ROM no tiene slots en negrita (los nombres
# `sony_NN` solo llevan px), asi que grande = `font_px >= 24`.
NORMAL_TEXT_MIN = 4.5
LARGE_TEXT_MIN = 3.0
LARGE_TEXT_PX = 24
# SC 1.4.11 (AA): no-texto (componentes/graficos) 3:1.
NON_TEXT_MIN = 3.0

# --- metodo de medicion de pixeles ------------------------------------------
PAD = 2  # borde para capturar el antialiasing
COBERTURA_TINTA = 0.45  # fraccion del color declarado que define "tinta"
FONDO_COBERTURA = 0.25  # cobertura por debajo de la cual un pixel es fondo
SALTO = 4  # px por encima/debajo del glifo para muestrear el fondo local
MIN_INK_PX = 6  # tinta minima para considerar el glifo dibujado en su caja
OCCLUSION_FRAC = 0.5  # fraccion de la caja tapada por una superficie posterior
UNICODE_ICON_MIN = 0xE000  # Private Use Area: los glifos de icono del ROM

# Marcador de "no medido" del arnes (labels placeholder de los mocks inertes).
PLACEHOLDER_TEXT = "Text"

# Graficos que el inspector NO puede medir: expone el bbox y los flags, pero no
# el color de trazo (la runa de BT es un `lv_line`; `medir.py` de la auditoria lo
# resuelve leyendo el rol `icon_off` del tema). El gate los declara, no inventa el
# color: "si un elemento no expone su color, no lo midas a ciegas".
UNMEASURABLE_CLASSES = {"lv_line", "lv_arc"}

# Set acotado que cubre la **matriz de temas** (los seis que sirve la ROM) y los
# **estados** con los defectos medidos en la epica #269: etiquetas de fila de
# Ajustes (hijo B, D-1), fila de frecuencia / reloj / bateria / iconos de la
# barra de la pantalla principal (hijo A) y el dialogo de Bluetooth (hijo B,
# D-3). No se renderiza el corpus entero (205 fixtures, ~30 s) para que el gate
# corra en segundos; el corpus completo es del pipeline A/B.
THEMES = ["aurora", "cathode", "redshift", "sprint", "sunlit", "trail"]
FIXTURE_STATES = [
    "settings-tema-{t}",  # Ajustes (grupo Appearance) por tema
    "settings-grupo-operation-tema-{t}",  # Ajustes (grupo Operation) por tema
    "bt-dialog-tema-{t}",  # ventana Bluetooth por tema
    "main-tema-{t}",  # pantalla principal por tema
]

# Roles que el proyecto declara apagados a proposito (WCAG 1.4.3 disabled/
# incidental). Misma lista que `tests/check_wcag.py::EXENTOS` y
# `tools/ab_testing/quality/theme_pairs.yaml::exempt`; el motivo es el de esos
# archivos. Se clasifican (no son un pase) y no hacen fallar.
EXEMPT_ROLES = {
    "btn_off_text": "boton deshabilitado (texto dentro de un control inactivo)",
    "text_mute": "texto atenuado a proposito (cola/idle, items grises)",
    "text_faint": "texto en cola/idle (estado no operable)",
    "icon_off": "iconos apagados de la barra (estado no activo)",
    "switch_off": "switch de Ajustes apagado (control inactivo)",
    "off": "barras/segmentos sin encender (estado apagado)",
    "track": "riel de la barra de progreso (pista inactiva)",
}

# Orden de las cuatro paletas base de THEME_COLORS(X) en src/theme.h.
BASE_ORDER = ["IU1WXY", "Legacy", "Outdoor", "Classic"]


def fixtures() -> list[str]:
    """La lista de fixtures del gate (matriz de temas x estados)."""
    return [state.format(t=t) for state in FIXTURE_STATES for t in THEMES]


# ---------------------------------------------------------------------------
# Formula WCAG 2.1 (misma que tests/check_wcag.py)
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


# ---------------------------------------------------------------------------
# Paleta efectiva del tema (para clasificar la tinta por rol exento)
# ---------------------------------------------------------------------------


def _base_palettes() -> dict[str, list[str]]:
    """{rol: [4 hex]} leido de THEME_COLORS(X) en src/theme.h."""
    texto = THEME_H.read_text(errors="surrogateescape")
    m = re.search(r"#define\s+THEME_COLORS\(X\)(.*?)typedef enum", texto, re.S)
    if not m:
        raise SystemExit(f"FALLO: no encontre THEME_COLORS(X) en {THEME_H}")
    colores: dict[str, list[str]] = {}
    for renglon in m.group(1).splitlines():
        mm = re.match(
            r"\s*X\(\s*([a-z_0-9]+)\s*,\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*,"
            r"\s*(0x[0-9a-fA-F]{6})\s*,\s*(0x[0-9a-fA-F]{6})\s*\)",
            renglon,
        )
        if mm:
            colores[mm.group(1)] = [mm.group(i).lower() for i in range(2, 6)]
    if not colores:
        raise SystemExit(f"FALLO: no pude leer ningun rol de {THEME_H}")
    return colores


def theme_palette(nombre: str, bases: dict[str, list[str]]) -> dict[str, str]:
    """Paleta efectiva del tema `<nombre>.json` (base + overrides `colors`)."""
    ruta = THEMES_DIR / f"{nombre}.json"
    if not ruta.exists():
        ruta = THEMES_DIR / nombre / "theme.json"
    datos = json.loads(ruta.read_text(errors="surrogateescape"))
    base = datos.get("base", "IU1WXY")
    if base not in BASE_ORDER:
        raise SystemExit(f"FALLO: {ruta.name}: base desconocida {base!r}")
    col = BASE_ORDER.index(base)
    overrides = {k.lower(): v.lower() for k, v in (datos.get("colors") or {}).items()}
    return {rol: overrides.get(rol, valores[col]) for rol, valores in bases.items()}


def exempt_hexes(bases: dict[str, list[str]]) -> dict[str, str]:
    """{hex: motivo} de los roles exentos, por tema de la matriz."""
    out: dict[str, str] = {}
    for tema in THEMES:
        pal = theme_palette(tema.capitalize(), bases)
        for rol, motivo in EXEMPT_ROLES.items():
            if rol in pal:
                out.setdefault(pal[rol].upper(), f"rol exento {rol}: {motivo}")
    return out


# ---------------------------------------------------------------------------
# Render via el arnes de host (Docker)
# ---------------------------------------------------------------------------


def _docker_run(args: list[str], image: str) -> None:
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
        "/src/tools/ui_harness/build",
        image,
        *args,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        raise SystemExit(f"FALLO: el comando docker del arnes fallo (rc={proc.returncode})")


def _newest_input():
    """El insumo del build mas nuevo (mtime) que alimenta el binario compilado.

    Solo lo que se COMPILA dentro del arnes: los fuentes del firmware (`src/`) y
    los del arnes (`tools/ui_harness/`, sin `build/` ni `fixtures/` — los
    escenarios se leen en runtime). Los JSON de tema tambien son runtime: un
    cambio de color NO necesita recompilar.
    """
    newest = None
    for base in (
        ROOT / "src",
        ROOT / "tools" / "ui_harness",
        ROOT / "CMakeLists.txt",
        ROOT / "lvgl" / "CMakeLists.txt",
    ):
        if not base.exists():
            continue
        if base.is_file():
            candidatos = [base]
        else:
            candidatos = []
            for dirpath, dirnames, filenames in os.walk(base):
                dirnames[:] = [d for d in dirnames if d not in ("build", ".git", "fixtures")]
                candidatos.extend(Path(dirpath) / f for f in filenames)
        for p in candidatos:
            try:
                m = p.stat().st_mtime
            except OSError:
                continue
            if newest is None or m > newest[0]:
                newest = (m, p)
    return newest


def _stale_build() -> str | None:
    """Path del insumo mas nuevo que el binario, o None si el build esta al dia.

    Es la trampa del build reusado: `HARNESS_BIN.exists()` es cierto aunque la
    rama haya cambiado `src/**` despues de compilar, asi que reusar el binario
    mediria codigo que ya no esta en el arbol (falso verde). Un insumo mas nuevo
    que el binario ⇒ hay que recompilar.
    """
    if not HARNESS_BIN.exists():
        return None
    newest = _newest_input()
    if newest is None:
        return None
    mtime, path = newest
    if mtime > HARNESS_BIN.stat().st_mtime:
        try:
            return str(path.relative_to(ROOT))
        except ValueError:
            return str(path)
    return None


def _build(image: str, motivo: str) -> None:
    rev = (
        subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True
        ).stdout.strip()
        or "unknown"
    )
    print(f"  {motivo} (revision {rev})...")
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


def ensure_harness(image: str, skip_build: bool) -> None:
    """Comprueba docker/imagen y garantiza un binario del arnes FRESCO.

    Por default SIEMPRE corre cmake+make (incremental: recompila lo que cambio).
    Con `SKIP_BUILD=1` reusa el binario existente, pero **solo si esta al dia**:
    si algun fuente es mas nuevo que el binario, aborta en vez de medir codigo
    viejo (el falso verde que ya se pago en el gate de determinismo).
    """
    import shutil

    if shutil.which("docker") is None:
        raise SystemExit("FALLO: docker no esta disponible (el gate renderiza con el arnes)")
    if subprocess.run(["docker", "image", "inspect", image], capture_output=True).returncode != 0:
        raise SystemExit(
            f"FALLO: falta la imagen '{image}' "
            f"(construila: docker build -t {image} tools/ui_harness)"
        )
    if skip_build and HARNESS_BIN.exists():
        viejo = _stale_build()
        if viejo is not None:
            raise SystemExit(
                f"FALLO: SKIP_BUILD=1 reusaria un build VIEJO: {viejo} es mas nuevo que "
                f"{HARNESS_BIN.relative_to(ROOT)} (el binario no refleja el arbol: se "
                f"mediria codigo que ya no esta). Corré sin SKIP_BUILD=1 para recompilar."
            )
        print("  reusando el arnes ya construido (SKIP_BUILD=1, binario al dia)")
        return
    if not skip_build:
        _build(image, "construyendo/actualizando el arnes")
        viejo = _stale_build()
        if viejo is not None:
            raise SystemExit(
                f"FALLO: tras el build, {viejo} sigue mas nuevo que el binario "
                f"(el build no lo recompilo)"
            )
    else:
        _build(image, "construyendo el arnes (no habia binario)")


def render(image: str) -> None:
    """Renderiza las fixtures del gate (PNG + JSON con `--inspect`)."""
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "png").mkdir(exist_ok=True)
    (OUT_DIR / "json").mkdir(exist_ok=True)
    lista = " ".join(fixtures())
    script = (
        "set -e; OUT=out/wcag-rendered; rm -rf $OUT; mkdir -p $OUT/png $OUT/json; "
        f"for f in {lista}; do "
        "./ui_harness --scenario ../fixtures/$f.scn "
        "--png $OUT/png/$f.png --json $OUT/json/$f.json --inspect >/dev/null 2>&1 "
        '|| { echo "FALLO render $f" >&2; exit 1; }; done'
    )
    _docker_run(["-lc", script], image)


# ---------------------------------------------------------------------------
# Medicion sobre el PNG + el arbol `--inspect`
# ---------------------------------------------------------------------------


def _collect(root: dict):
    """DFS pre-orden (== orden de dibujo de LVGL): (node, path, index, ancestors)."""
    items: list[tuple[dict, str, int, frozenset[int]]] = []

    def rec(n: dict, path: str, anc: tuple[int, ...], my_index: int) -> None:
        items.append((n, path, my_index, frozenset(anc)))
        for i, c in enumerate(n.get("children", [])):
            rec(c, f"{path}/{i}", (*anc, my_index), len(items))

    rec(root, "", (), 0)
    return items


def _visible_box(n: dict):
    v = n.get("visible")
    if not v or v.get("w", 0) <= 0 or v.get("h", 0) <= 0:
        return None
    return (v["x"], v["y"], v["w"], v["h"])


def _intersection_area(a, b) -> int:
    x0, y0 = max(a[0], b[0]), max(a[1], b[1])
    x1, y1 = min(a[0] + a[2], b[0] + b[2]), min(a[1] + a[3], b[1] + b[3])
    if x1 <= x0 or y1 <= y0:
        return 0
    return (x1 - x0) * (y1 - y0)


def _is_drawn_surface(n: dict) -> bool:
    """Una superficie dibujada (puede tapar lo de atras).

    `opa == 0` significa que LVGL no dibuja nada (asi se ocultan los labels
    placeholder "Text" de los mocks inertes): no puede ocluir.
    """
    opa = n.get("opa", 255) or 0
    return opa > 0 and ((n.get("bg_opa", 0) or 0) >= 128 or bool(n.get("bg_image")))


def _font_px(font: str) -> int:
    m = re.search(r"_(\d+)$", font or "")
    return int(m.group(1)) if m else 0


def _ink_and_background(np, node):
    """Tinta y fondo medidos del bbox de una etiqueta."""
    x, y, w, h = node["x"], node["y"], node["w"], node["h"]
    if w <= 0 or h <= 0:
        return None
    img = node["_img"]
    x0, y0 = max(0, x - PAD), max(0, y - PAD)
    x1, y1 = min(img.shape[1], x + w + PAD), min(img.shape[0], y + h + PAD)
    sub = img[y0:y1, x0:x1]
    tc = np.array(hex_rgb(node["text_color"]), dtype=float)
    propio = np.zeros(sub.shape[:2], bool)
    propio[y - y0 : y - y0 + h, x - x0 : x - x0 + w] = True
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
        return None
    cov = np.clip(((sub - fondo)[..., canales] / den[canales]).mean(axis=2), 0.0, 1.0)
    ink = cov >= COBERTURA_TINTA
    inb = ink & propio
    if int(inb.sum()) < MIN_INK_PX:
        return None
    ratios = []
    for cx in range(inb.shape[1]):
        filas = np.nonzero(inb[:, cx])[0]
        if len(filas) == 0:
            continue
        for fy in (filas[0] - SALTO, filas[-1] + SALTO):
            gy, gx = y0 + fy, x0 + cx
            if not (y <= gy < y + h and x <= gx < x + w):
                continue
            if cov[fy, cx] >= FONDO_COBERTURA:
                continue
            ratios.append(contrast(img[gy, gx], tc))
    if not ratios:
        return None
    arr = np.array(ratios)
    return {
        "bg": fondo,
        "ratio": round(float(np.median(arr)), 2),
        "p05": round(float(np.percentile(arr, 5)), 2),
        "ink_px": int(inb.sum()),
        "samples": len(ratios),
    }


def _declared_pair(node):
    """Par declarado cuando NO se detecta la tinta y la etiqueta pinta su fondo.

    Con `bg_opa >= 250` (LV_OPA_COVER), sin imagen de fondo y tinta opaca, el par
    que el codigo dibuja ES `text_color` sobre `bg_color` (la misma regla que
    `x6200_ab.typography_checks._contrast_ratio`). Sin este fallback, una regresion
    que lleve la tinta al color del fondo (contraste ~1:1) no tendria pixeles de
    tinta que muestrear y quedaria clasificada "sin tinta" en vez de violacion:
    justo el agujero que el defecto del #277 aprovecharia.
    """
    if (node.get("bg_opa", 0) or 0) < 250 or node.get("bg_image"):
        return None
    if node.get("text_opa", 255) != 255:
        return None
    tc = hex_rgb(node["text_color"])
    bg = hex_rgb(node["bg_color"])
    ratio = round(contrast(tc, bg), 2)
    return {
        "bg": bg,
        "ratio": ratio,
        "p05": ratio,
        "ink_px": 0,
        "samples": 0,
        "method": "declarado (fondo opaco, tinta no detectable)",
    }


def measure_capture(np, image, inspection, fixture, theme, exempt_hex):
    """Mide todos los elementos del arbol de una captura.

    Devuelve: (medidas, declaradas-no-medidas). Las no medidas son las que no se
    pueden medir con confianza (ocluidas por una superficie posterior, tinta que
    no aparece en su caja, o graficos sin color de trazo expuesto).
    """
    items = _collect(inspection["nodes"][0])
    hidden = {idx for n, _, idx, _ in items if n.get("hidden")}
    surfaces = [
        (idx, _visible_box(n))
        for n, _, idx, anc in items
        if _is_drawn_surface(n)
        and _visible_box(n)
        and not n.get("hidden")
        and not (set(anc) & hidden)
    ]
    measured, skipped = [], []
    for node, path, idx, anc in items:
        if node.get("class") in UNMEASURABLE_CLASSES:
            if node.get("hidden") or (set(anc) & hidden):
                continue
            if (node.get("opa", 255) or 0) == 0 or _visible_box(node) is None:
                continue
            skipped.append(
                {
                    "fixture": fixture,
                    "theme": theme,
                    "path": path,
                    "text": "",
                    "status": "sin color expuesto",
                    "detail": f"{node['class']}: el inspector no publica su color de trazo",
                }
            )
            continue
        if node.get("class") != "lv_label":
            continue
        if node.get("hidden") or (set(anc) & hidden):
            continue
        if (node.get("opa", 255) or 0) == 0:  # LVGL no dibuja nada
            continue
        text = (node.get("text") or "").strip()
        if not text or text == PLACEHOLDER_TEXT:
            continue
        vb = _visible_box(node)
        if vb is None:
            continue
        # Oclusion: una superficie dibujada DESPUES que tapa >= OCCLUSION_FRAC.
        area = vb[2] * vb[3]
        occluded_by = None
        for sidx, sbox in surfaces:
            if sidx <= idx or sidx in anc:
                continue
            if _intersection_area(vb, sbox) >= OCCLUSION_FRAC * area:
                occluded_by = sidx
                break
        if occluded_by is not None:
            skipped.append(
                {
                    "fixture": fixture,
                    "theme": theme,
                    "path": path,
                    "text": text,
                    "status": "ocluido",
                    "detail": f"superficie posterior (idx {occluded_by}) tapa la caja",
                }
            )
            continue
        node = dict(node)
        node["_img"] = image
        m = _ink_and_background(np, node)
        if m is None:
            # Fallback: fondo opaco propio ⇒ el par declarado ES el renderizado.
            m = _declared_pair(node)
        if m is None:
            skipped.append(
                {
                    "fixture": fixture,
                    "theme": theme,
                    "path": path,
                    "text": text,
                    "status": "sin tinta",
                    "detail": "el glifo no aparece en su caja (no dibujado/atenuado)",
                }
            )
            continue
        fila = _classify(node, text, path, fixture, theme, m, exempt_hex)
        measured.append(fila)
    return measured, skipped


def _classify(node, text, path, fixture, theme, m, exempt_hex):
    """Clasifica el elemento y aplica el umbral WCAG que le corresponde."""
    color = node["text_color"].upper()
    is_icon = len(text) == 1 and ord(text) >= UNICODE_ICON_MIN
    if node.get("text_opa", 255) < 255:
        category, rule, minimum = "incidental", "incidental", None
        motivo = "texto atenuado por opacidad (text_opa < 255): decorativo"
    elif color in exempt_hex:
        category, rule, minimum = "exento", "exento", None
        motivo = exempt_hex[color]
    elif is_icon:
        category, rule, minimum = "no-texto", "non_text", NON_TEXT_MIN
        motivo = "SC 1.4.11: icono/grafico (glifo de icono unico)"
    else:
        px = _font_px(node.get("text_font", ""))
        if px >= LARGE_TEXT_PX:
            category, rule, minimum = "texto grande", "large_text", LARGE_TEXT_MIN
            motivo = f"SC 1.4.3 texto grande ({px}px >= {LARGE_TEXT_PX}px = 18pt)"
        else:
            category, rule, minimum = "texto normal", "text", NORMAL_TEXT_MIN
            motivo = f"SC 1.4.3 texto normal ({px}px)"
    viola = minimum is not None and m["ratio"] < minimum
    return {
        "fixture": fixture,
        "theme": theme,
        "path": path,
        "text": text,
        "category": category,
        "rule": rule,
        "text_color": color,
        "bg": rgb_hex(m["bg"]),
        "ratio": m["ratio"],
        "p05": m["p05"],
        "min": minimum,
        "viola": viola,
        "reason": motivo,
        "method": m.get("method", "render"),
        "font_px": _font_px(node.get("text_font", "")),
    }


def measure_all(np) -> tuple[list[dict], list[dict]]:
    from PIL import Image

    bases = _base_palettes()
    exempt = exempt_hexes(bases)
    measured, skipped = [], []
    for fixture in fixtures():
        jp = OUT_DIR / "json" / f"{fixture}.json"
        pp = OUT_DIR / "png" / f"{fixture}.png"
        data = json.loads(jp.read_text())
        image = np.array(Image.open(pp).convert("RGB")).astype(float)
        filas, saltadas = measure_capture(
            np, image, data["inspection"], fixture, data["theme"]["active"], exempt
        )
        measured.extend(filas)
        skipped.extend(saltadas)
    return measured, skipped


# ---------------------------------------------------------------------------
# Baseline
# ---------------------------------------------------------------------------


def key(fila: dict) -> tuple[str, str, str]:
    return (fila["fixture"], fila["rule"], fila["path"])


def load_baseline() -> dict:
    if not BASELINE.exists():
        return {"entries": []}
    return json.loads(BASELINE.read_text())


def _defect_group(fila: dict) -> str:
    """Contexto de la evidencia de la epica #269 para el motivo del waiver."""
    if fila["fixture"].startswith("settings") and fila["theme"] == "Sunlit":
        return (
            "hallazgo D-1 (hijo B #271): la ventana de Ajustes pinta el bitmap "
            "images/dialog.bin y la tinta sale del rol `text` del base Outdoor"
        )
    if fila["fixture"].startswith("bt-dialog"):
        return "hallazgo D-3 (hijo B #271): superficie del dialogo Bluetooth"
    return (
        "matriz de temas/estados (hijo A #270): barra de info, reloj, bateria y "
        "plan de banda de la pantalla principal"
    )


def make_entry(fila: dict) -> dict:
    return {
        "fixture": fila["fixture"],
        "theme": fila["theme"],
        "rule": fila["rule"],
        "path": fila["path"],
        "text": fila["text"],
        "text_color": fila["text_color"],
        "bg": fila["bg"],
        "ratio": fila["ratio"],
        "min": fila["min"],
        "reason": (
            f"issue #277: violacion preexistente de WCAG {fila['min']}:1 "
            f"({fila['category']}); {_defect_group(fila)}. "
            f"medido {fila['ratio']}:1 ({fila['text_color']} sobre {fila['bg']}). "
            f"Evidencia de la epica #269"
        ),
        "author": "wcag-rendered-gate",
        "until": "2027-03-31",
        "ref": "#277",
    }


def update_baseline(violaciones: list[dict]) -> int:
    previo = {
        tuple(e[k] for k in ("fixture", "rule", "path")): e
        for e in load_baseline().get("entries", [])
    }
    nuevo = {key(f): make_entry(f) for f in violaciones}
    added = sorted(set(nuevo) - set(previo))
    removed = sorted(set(previo) - set(nuevo))
    # Un par que sigue violando pero con OTRO ratio. No es un agregado ni una
    # baja: el ratio registrado cambio (p. ej. porque un cambio de render
    # revisado movio el fondo medido bajo el glifo). Sin esta linea, el diff de
    # `--update-baseline` reescribia el ratio en silencio y la revision no veia
    # que numero se estaba bendiciendo: el cambio de fondo pasaba como "sin
    # cambios".
    changed = sorted(
        k for k in set(previo) & set(nuevo) if previo[k].get("ratio") != nuevo[k]["ratio"]
    )
    print("  --- diff del baseline (--update-baseline) ---")
    for k in added:
        e = nuevo[k]
        print(
            f"  + {e['fixture']:<34} {e['rule']:<10} {e['path']:<14} "
            f"{e['ratio']:>5}:1 < {e['min']}  {e['text'][:24]!r}"
        )
    for k in changed:
        p, n = previo[k], nuevo[k]
        print(
            f"  ~ {n['fixture']:<34} {n['rule']:<10} {n['path']:<14} "
            f"ratio {p['ratio']:>5}:1 -> {n['ratio']:>5}:1  {n['text'][:24]!r}"
        )
    for k in removed:
        e = previo[k]
        print(
            f"  - {e['fixture']:<34} {e['rule']:<10} {e['path']:<14} "
            f"ratio {e['ratio']}:1  {e.get('text', '')[:24]!r}"
        )
    if not added and not removed and not changed:
        print("  (sin cambios)")
    data = {
        "schema": 1,
        "issue": 277,
        "epic": 269,
        "note": (
            "Violaciones PREEXISTENTES de contraste WCAG 2.1 medidas sobre la "
            "superficie RENDERIZADA (tests/check_wcag_rendered.py). Un par por "
            "entrada (fixture + regla + path del nodo), con el ratio medido. El "
            "gate falla ante un par nuevo o un par que empeora; regenerar con "
            "--update-baseline imprime el diff. No hay blanket-skip."
        ),
        "entries": [nuevo[k] for k in sorted(nuevo)],
    }
    BASELINE.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
    print(
        f"  baseline escrito: {BASELINE.relative_to(ROOT)} "
        f"({len(nuevo)} entradas, +{len(added)}/-{len(removed)}/~{len(changed)})"
    )
    return 0


# ---------------------------------------------------------------------------
# Chequeo
# ---------------------------------------------------------------------------


def main(argv=None) -> int:
    import numpy as np

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--all", action="store_true", help="imprime todos los elementos medidos")
    parser.add_argument(
        "--update-baseline", action="store_true", help="regenera el baseline e imprime el diff"
    )
    args = parser.parse_args(argv)

    image = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")

    print("== gate de contraste WCAG sobre la superficie renderizada (issue #277) ==")
    print(
        f"  fixtures: {len(fixtures())} (matriz de {len(THEMES)} temas x "
        f"{len(FIXTURE_STATES)} estados)"
    )
    ensure_harness(image, skip_build=os.environ.get("SKIP_BUILD") == "1")
    render(image)

    measured, skipped = measure_all(np)
    violaciones = [f for f in measured if f["viola"]]

    n_ocluidos = sum(1 for s in skipped if s["status"] == "ocluido")
    n_sin_tinta = sum(1 for s in skipped if s["status"] == "sin tinta")
    n_sin_color = sum(1 for s in skipped if s["status"] == "sin color expuesto")
    n_exentos = sum(1 for f in measured if f["rule"] == "exento")
    n_incidental = sum(1 for f in measured if f["rule"] == "incidental")
    n_nontext = sum(1 for f in measured if f["rule"] == "non_text")
    print(
        f"  etiquetas medidas: {len(measured)} (exentas {n_exentos}, "
        f"incidentales {n_incidental}, no-texto {n_nontext})"
    )
    print(
        f"  declaradas, no medidas: ocluidas {n_ocluidos}, sin tinta {n_sin_tinta}, "
        f"sin color expuesto (lv_line/lv_arc) {n_sin_color}"
    )
    print(f"  violaciones: {len(violaciones)}")

    if args.all:
        for f in sorted(measured, key=lambda f: f["ratio"]):
            mark = "VIOLA" if f["viola"] else "ok"
            print(
                f"    {f['fixture']:<34} {f['theme']:<9} {f['path']:<14} "
                f"{f['text'][:22]!r:<24} {f['ratio']:>5}:1 min {f['min']} "
                f"[{f['category']}/{f['method']}] {mark}"
            )

    print("  --- violaciones medidas ---")
    for f in sorted(violaciones, key=lambda f: f["ratio"]):
        print(
            f"    {f['fixture']:<34} {f['theme']:<9} {f['path']:<14} "
            f"{f['text'][:22]!r:<24} {f['ratio']:>5}:1 < {f['min']}  "
            f"({f['category']}, tinta {f['text_color']} sobre {f['bg']})"
        )

    if args.update_baseline:
        return update_baseline(violaciones)

    baseline = load_baseline()
    base = {
        tuple(e[k] for k in ("fixture", "rule", "path")): e for e in baseline.get("entries", [])
    }
    medido = {key(f): f for f in violaciones}

    nuevas = sorted(set(medido) - set(base))
    regresiones = sorted(
        k for k in set(medido) & set(base) if medido[k]["ratio"] < base[k]["ratio"]
    )
    obsoletas = sorted(set(base) - set(medido))

    fallos = 0
    if nuevas:
        fallos += len(nuevas)
        print("  FALLA: violacion NUEVA (no esta en el baseline):")
        for k in nuevas:
            f = medido[k]
            print(
                f"    {f['fixture']} {f['path']} {f['text']!r} = {f['ratio']}:1 "
                f"< {f['min']} ({f['text_color']} sobre {f['bg']})"
            )
    if regresiones:
        fallos += len(regresiones)
        print("  FALLA: REGRESION (un par del baseline empeoro):")
        for k in regresiones:
            e, f = base[k], medido[k]
            print(
                f"    {f['fixture']} {f['path']} {f['text']!r}: {e['ratio']}:1 "
                f"-> {f['ratio']}:1 (min {f['min']})"
            )
    if obsoletas:
        fallos += len(obsoletas)
        print("  FALLA: baseline OBSOLETO (un par del baseline ya no viola):")
        for k in obsoletas:
            e = base[k]
            print(
                f"    {e['fixture']} {e['path']} {e.get('text', '')!r} "
                f"(ratio registrado {e['ratio']}:1)"
            )
        print("    -> corré --update-baseline para quitar las entradas resueltas (imprime el diff)")

    if fallos:
        print(f"  {fallos} fallo(s): violaciones nuevas/regresiones/baseline obsoleto")
        return 1

    print(f"  solo violaciones preexistentes del baseline ({len(base)} entradas): OK")
    print("  PASS: el contraste renderizado no tiene violaciones nuevas ni regresiones")
    return 0


if __name__ == "__main__":
    sys.exit(main())
