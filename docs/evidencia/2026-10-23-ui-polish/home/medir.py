#!/usr/bin/env python3
"""Mide la matriz de HOME: geometria del arbol LVGL vivo + tinta renderizada.

Issue #285 (hijo 1 de la epica #284). Lee los JSON que `capturar.py` dejo en el
scratch (`/scratch/json-<fase>/`) y los PNG curados de `png-<fase>/`, y escribe
`mediciones/`:

* `reloj_estado.csv` — por captura: cajas y tinta de la hora y de la fila de
  estado (fuente externa, icono de bateria, porcentaje), y los huecos.
* `colisiones.csv` — las capturas donde el contrato geometrico NO se cumple
  (solape de cajas/tinta, hueco < 4/6 px, recorte o desborde).
* `runa_bt.csv` — las dos lineas de la runa de BT: cajas, relacion de formas y
  la verificacion de la espina vertical sobre los pixeles.
* `reloj_hora.csv` — tinta de la hora por tema (para el "estrictamente mas alto
  y mas ancho que sony_38").
* `flushes.csv` — el proxy de costo de render (flushes / px) por captura.

Uso:  python3 medir.py --phase before|after
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
MED = HERE / "mediciones"

# Cajas del contrato (issue #285): la caja de la hora y la banda de estado
# viven en el panel del reloj, que va pegado al borde derecho (x591..799).
CLOCK_X0, CLOCK_X1 = 591, 799
CLOCK_TIME_Y1 = 39          # la caja de la hora no puede pasar de aca
CLOCK_MIN_VMARGIN = 2       # margen vertical minimo (2 px)
STATUS_GAP_MIN = 4          # icono vs porcentaje (px de hueco de caja)
STATUS_LABELS_MIN = 6       # etiquetas de la banda entre si
BT_BOX = (167, 31, 195, 61)  # media celda derecha de la celda WiFi/BT

ICON_CP = range(0xF240, 0xF245)   # U+F240..F244: los cinco niveles del icono
TIME_RE = re.compile(r"^(\d\d:\d\d:\d\d|--:--:--)$")
PCT_RE = re.compile(r"^(\d{1,3}%|--%)$")

COBERTURA_TINTA = 0.45
PAD = 2

# Umbral de tinta de la runa de BT sobre los PIXELES: la distancia (en el canal
# mas separado) al fondo de la misma fila que cuenta como trazo. 12 es el mismo
# margen que usa el A/B para ignorar el ruido de un bit y el antialiasing
# (tools/ab_testing/quality/budgets.yaml::pixel_tolerance); con ese umbral el
# zigzag de antes de #285 llega como maximo a 0.52 de la columna mas cubierta y
# la runa con espina a 0.92 (ver vertical_spine).
BT_INK_DELTA = 12
BT_SPINE_MIN_FRAC = 0.8


def walk(node, path=""):
    yield node, path
    for i, child in enumerate(node.get("children", [])):
        yield from walk(child, f"{path}/{i}")


GHOST_TEXT = "88:88:88"


def find_clock(doc):
    """El panel del reloj: el nodo con el label del VALOR `HH:MM:SS`.

    El "8 fantasma" (los temas con familia segmentada, src/clock.c) tambien
    matchea `\\d\\d:\\d\\d:\\d\\d`, asi que el valor es el label con `text_opa`
    OPACO: el fantasma se dibuja a `UI_DIGIT_SHADOW_OPA`.
    """
    for node, path in walk(doc["inspection"]["nodes"][0]):
        labels = [c for c in node.get("children", []) if c.get("class") == "lv_label"]
        values = [c for c in labels
                  if TIME_RE.match((c.get("text") or "").strip())
                  and (c.get("text") or "").strip() != GHOST_TEXT
                  and c.get("text_opa", 255) == 255]
        if values:
            return node, path
    return None, None


def ink_bbox(img, box, color, pad=PAD):
    """(x0,y0,x1,y1) de la tinta del color declarado dentro de la caja (+pad).

    `box` viene en esquinas inclusiva/exclusiva (x0,y0,x1,y1), como `box_of`.
    """
    x0, y0, x1, y1 = box
    x0 = max(0, x0 - pad)
    y0 = max(0, y0 - pad)
    x1 = min(img.shape[1], x1 + pad)
    y1 = min(img.shape[0], y1 + pad)
    sub = img[y0:y1, x0:x1].astype(float)
    tc = np.array([(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF], dtype=float)
    dist = np.abs(sub - tc).max(axis=2)
    propio = np.zeros(sub.shape[:2], bool)
    propio[box[1] - y0 : box[3] - y0, box[0] - x0 : box[2] - x0] = True
    thresh = np.percentile(dist[propio], 60) if propio.any() else 96
    lejos = propio & (dist >= thresh)
    if lejos.sum() < 4:
        lejos = propio & (dist >= 96)
    if lejos.sum() == 0:
        return None
    cols, counts = np.unique(sub[lejos], axis=0, return_counts=True)
    bg = cols[int(np.argmax(counts))].astype(float)
    den = tc - bg
    canales = np.abs(den) > 32
    if not canales.any():
        return None
    cov = np.clip(((sub - bg)[..., canales] / den[canales]).mean(axis=2), 0.0, 1.0)
    ink = (cov >= COBERTURA_TINTA) & propio
    if ink.sum() < 4:
        return None
    ys, xs = np.nonzero(ink)
    return (x0 + int(xs.min()), y0 + int(ys.min()), x0 + int(xs.max()), y0 + int(ys.max()))


def gap_x(a, b):
    """Hueco horizontal entre dos cajas (negativo = solape)."""
    if a is None or b is None:
        return None
    return max(a[0], b[0]) - min(a[2], b[2])


def _luminance(rgb):
    def canal(c):
        c = float(c) / 255.0
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4

    r, g, b = (canal(v) for v in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = _luminance(a), _luminance(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


def local_ratio(img, box, color, salto=4):
    """Contraste WCAG del texto contra el fondo RENDERIZADO bajo el glifo.

    Misma medicion que `tests/check_wcag_rendered.py`: por cada columna con tinta
    se muestrean los pixeles a `salto` px por encima del primer pixel de tinta y
    por debajo del ultimo (el fondo local de esa columna; en un bitmap no es
    uniforme) y el ratio es la MEDIANA de esos pares. Devuelve
    (mediana, percentil 5, muestras, fondo_medido) o None si no hay tinta.
    """
    x0, y0, x1, y1 = box
    px0, py0 = max(0, x0 - PAD), max(0, y0 - PAD)
    px1, py1 = min(img.shape[1], x1 + PAD), min(img.shape[0], y1 + PAD)
    sub = img[py0:py1, px0:px1].astype(float)
    tc = np.array([(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF], dtype=float)
    propio = np.zeros(sub.shape[:2], bool)
    propio[y0 - py0 : y1 - py0, x0 - px0 : x1 - px0] = True
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
    ink = (cov >= COBERTURA_TINTA) & propio
    if ink.sum() < 4:
        return None
    ratios, fondos = [], []
    for cx in range(ink.shape[1]):
        filas = np.nonzero(ink[:, cx])[0]
        if len(filas) == 0:
            continue
        for fy in (filas[0] - salto, filas[-1] + salto):
            gy, gx = py0 + fy, px0 + cx
            if not (y0 <= gy < y1 and x0 <= gx < x1):
                continue
            if cov[fy, cx] >= 0.25:
                continue
            ratios.append(contrast(img[gy, gx], tc))
            fondos.append(img[gy, gx])
    if not ratios:
        return None
    arr = np.array(ratios)
    return (round(float(np.median(arr)), 2), round(float(np.percentile(arr, 5)), 2),
            len(ratios), tuple(int(v) for v in np.median(np.array(fondos), axis=0)))


def box_of(node):
    return (node["x"], node["y"], node["x"] + node["w"], node["y"] + node["h"])


def text_box_of(node):
    """La caja del TEXTO dentro del label (centrado horizontalmente).

    La etiqueta de la hora mide todo el ancho del panel (`lv_pct(100)`) y el
    bitmap del fondo tiene zonas claras: medir la tinta sobre la caja entera
    confunde el bitmap con tinta (medido en Redshift). El texto va centrado, asi
    que su caja sale del `text_natural` del inspector.
    """
    tn = node.get("text_natural") or {}
    tw, th = tn.get("w", 0), tn.get("h", 0)
    if tw <= 0 or th <= 0 or tw > node["w"]:
        return box_of(node)
    x = node["x"] + (node["w"] - tw) // 2
    return (x, node["y"], x + tw, node["y"] + node["h"])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--phase", required=True, choices=["before", "after"])
    ap.add_argument("--scratch", required=True)
    args = ap.parse_args()

    json_dir = Path(args.scratch) / f"json-{args.phase}"
    png_dir = HERE / f"png-{args.phase}"
    MED.mkdir(parents=True, exist_ok=True)

    reloj, colisiones, runas, horas, flushes = [], [], [], [], []

    for jp in sorted(json_dir.glob("*.json")):
        name = jp.stem
        doc = json.loads(jp.read_text())
        theme = doc.get("theme", {}).get("active")
        png = png_dir / f"{name}.png"
        img = np.array(Image.open(png).convert("RGB")) if png.exists() else None

        container, cpath = find_clock(doc)
        if container is None:
            print(f"aviso: {name}: sin panel de reloj")
            continue
        labels = {c.get("text", "").strip(): c for c in container.get("children", [])
                  if c.get("class") == "lv_label"}

        time_node = next((c for c in container.get("children", []) if c.get("class") == "lv_label"
                          and TIME_RE.match((c.get("text") or "").strip())
                          and (c.get("text") or "").strip() != GHOST_TEXT), None)
        ghost_node = next((c for c in container.get("children", []) if c.get("class") == "lv_label"
                           and (c.get("text") or "").strip() == GHOST_TEXT), None)
        ext_node = next((c for c in container.get("children", []) if c.get("class") == "lv_label"
                         and (c.get("text") or "").strip().endswith("V")), None)
        pct_node = next((c for c in container.get("children", []) if c.get("class") == "lv_label"
                         and PCT_RE.match((c.get("text") or "").strip())), None)
        icon_node = next((c for c in container.get("children", []) if c.get("class") == "lv_label"
                          and len((c.get("text") or "").strip()) == 1
                          and ord(c.get("text", " ")) in ICON_CP), None)
        strip_node = next((c for c in container.get("children", []) if c.get("class") != "lv_label"
                           and (c.get("bg_opa") or 0) >= 250), None)

        filtros = []
        tb = box_of(time_node) if time_node else None
        eb = box_of(ext_node) if ext_node else None
        pb = box_of(pct_node) if pct_node else None
        ib = box_of(icon_node) if icon_node else None
        sb = box_of(strip_node) if strip_node else None

        ti = ink_bbox(img, text_box_of(time_node), int(time_node["text_color"].lstrip("#"), 16)) if (img is not None and time_node) else None
        pi = ink_bbox(img, pb, int(pct_node["text_color"].lstrip("#"), 16)) if (img is not None and pct_node) else None
        ii = ink_bbox(img, ib, int(icon_node["text_color"].lstrip("#"), 16)) if (img is not None and icon_node) else None

        # Contraste WCAG del texto contra el fondo RENDERIZADO (la misma medicion
        # del gate `tests/check_wcag_rendered.py`): la hora, la fuente externa y
        # el porcentaje. Es lo que sube con la banda opaca del #285.
        def _ratio(node):
            if img is None or node is None:
                return (None, None, None)
            got = local_ratio(img, box_of(node), int(node["text_color"].lstrip("#"), 16))
            return (got[0], got[1], got[3]) if got else (None, None, None)

        hora_ratio, hora_p05, hora_fondo = _ratio(time_node)
        ext_ratio, ext_p05, ext_fondo = _ratio(ext_node)
        pct_ratio, pct_p05, pct_fondo = _ratio(pct_node)

        # 1. La caja de la hora dentro de x591..799 y con >= 2 px al tope de la
        #    banda de estado (y <= 39).
        if tb is None:
            filtros.append("sin hora")
        else:
            if tb[0] < CLOCK_X0 or tb[2] > CLOCK_X1 + 1:
                filtros.append(f"hora fuera de x{CLOCK_X0}..{CLOCK_X1}: {tb}")
            if tb[3] > CLOCK_TIME_Y1 + 1:
                filtros.append(f"hora baja hasta y={tb[3]} (>{CLOCK_TIME_Y1})")
            margin = (sb[1] - 1 - tb[3]) if sb else None
            if margin is not None and margin < CLOCK_MIN_VMARGIN:
                filtros.append(f"margen vertical hora/banda {margin} < {CLOCK_MIN_VMARGIN}")
        if ghost_node and tb and (ghost_node["y"] + ghost_node["h"] > CLOCK_TIME_Y1 + 1):
            filtros.append("fantasma baja de y39")
        if time_node and (time_node.get("text_overflow") or time_node.get("text_truncated")):
            filtros.append("hora recortada/desbordada")
        for n, role in ((ext_node, "ext"), (pct_node, "pct"), (icon_node, "icon")):
            if n and (n.get("text_overflow") or n.get("text_truncated")):
                filtros.append(f"{role} recortado/desbordado")

        # 2. Icono vs porcentaje: >= 4 px de caja y de tinta.
        g_box = gap_x(ib, pb)
        g_ink = gap_x(ii, pi)
        if g_box is None or g_box < STATUS_GAP_MIN:
            filtros.append(f"hueco caja icono/pct {g_box} < {STATUS_GAP_MIN}")
        if g_ink is not None and g_ink < STATUS_GAP_MIN:
            filtros.append(f"hueco tinta icono/pct {g_ink} < {STATUS_GAP_MIN}")

        # 3. Etiquetas de la banda entre si: >= 6 px.
        g_ext_left = gap_x(eb, ib)
        if g_ext_left is not None and g_ext_left < STATUS_LABELS_MIN:
            filtros.append(f"hueco ext/icono {g_ext_left} < {STATUS_LABELS_MIN}")

        # 4. La banda tiene fondo opaco (el strip del #285) con el rol TC(bar).
        if strip_node is None:
            filtros.append("sin banda opaca de estado")
        else:
            if sb[1] != 40 or sb[3] != 61:
                filtros.append(f"banda en y{sb[1]}..{sb[3]} (el contrato pide y40..60)")
            if sb[2] - sb[0] < 206:
                filtros.append(f"banda no full-width: {sb[2] - sb[0]} px")
            if strip_node.get("bg_opa", 0) < 250:
                filtros.append("banda no opaca")
            if strip_node.get("clickable") or strip_node.get("scrollable"):
                filtros.append("banda clickable/scrollable")

        reloj.append(dict(
            captura=name, tema=theme,
            hora_box=tb, hora_ink=ti,
            ext_box=eb, pct_box=pb, pct_ink=pi, icon_box=ib, icon_ink=ii,
            banda_box=sb,
            hueco_caja_icono_pct=g_box, hueco_tinta_icono_pct=g_ink, hueco_ext_icono=g_ext_left,
            hora_ancho_caja=(tb[2] - tb[0]) if tb else None,
            hora_alto_caja=(tb[3] - tb[1]) if tb else None,
            hora_alto_tinta=(ti[3] - ti[1] + 1) if ti else None,
            hora_ancho_tinta=(ti[2] - ti[0] + 1) if ti else None,
            hora_ratio=hora_ratio, hora_ratio_p05=hora_p05, hora_fondo=hora_fondo,
            ext_ratio=ext_ratio, ext_ratio_p05=ext_p05, ext_fondo=ext_fondo,
            pct_ratio=pct_ratio, pct_ratio_p05=pct_p05, pct_fondo=pct_fondo,
            ok=",".join(filtros) if filtros else "OK",
        ))
        if filtros:
            colisiones.append(dict(captura=name, tema=theme, hallazgos=" | ".join(filtros)))
        if ti:
            horas.append(dict(captura=name, tema=theme, **{k: v for k, v in reloj[-1].items()
                                                           if k.startswith("hora_")}))

        flushes.append(dict(captura=name, tema=theme, **doc.get("metrics", {})))

        # 5. La runa de BT: los trazos lv_line en la media celda derecha de la
        #    barra. #285 pasa de UN zigzag a DOS trazos (brazos + espina
        #    vertical): la espina se verifica sobre los PIXELES -- una columna
        #    con tinta a lo alto de la runa -- porque el inspector no publica el
        #    color de un lv_line. La comparacion es contra el fondo de la MISMA
        #    FILA a la derecha del rune (el bitmap del panel tiene gradiente), asi
        #    que tambien funciona con el color exento del estado apagado.
        lines = [(n, p) for n, p in walk(doc["inspection"]["nodes"][0])
                 if n.get("class") == "lv_line"
                 and 150 <= n["x"] <= 205 and 20 <= n["y"] <= 65]
        if name.startswith("home-"):
            row = dict(captura=name, n_lineas=len(lines))
            if lines:
                boxes = sorted((box_of(n) for n, _ in lines), key=lambda b: b[2] - b[0])
                arm = boxes[-1]
                row["arm_box"] = arm
                row["spine_box"] = boxes[0] if len(boxes) == 2 else None
                row["spine_mas_angosta"] = len(boxes) == 2 and \
                    (boxes[0][2] - boxes[0][0]) < (arm[2] - arm[0])
                row["mismo_origen"] = len(boxes) == 2 and boxes[0][:2] == arm[:2]
                row["dentro_limites"] = all(
                    BT_BOX[0] <= b[0] and b[2] <= BT_BOX[2] + 1
                    and BT_BOX[1] <= b[1] and b[3] <= BT_BOX[3]
                    for b in boxes
                )
                if img is not None:
                    frac, col = vertical_spine(img, arm)
                    row["espina_col_frac"] = frac
                    row["espina_x"] = arm[0] + col
                    row["espina_en_eje"] = abs(row["espina_x"] - (arm[0] + arm[2]) / 2) <= 4
                    row["espina_vertical"] = frac >= BT_SPINE_MIN_FRAC
            else:
                row["dentro_limites"] = False
                row["espina_vertical"] = False
            runas.append(row)

    _write_csv(MED / f"reloj_estado-{args.phase}.csv", reloj)
    _write_csv(MED / f"colisiones-{args.phase}.csv", colisiones, "captura,tema,hallazgos")
    _write_csv(MED / f"runa_bt-{args.phase}.csv", runas)
    _write_csv(MED / f"reloj_hora-{args.phase}.csv", horas)
    _write_csv(MED / f"flushes-{args.phase}.csv", flushes)
    print(f"{args.phase}: {len(reloj)} capturas, {len(colisiones)} con hallazgos, "
          f"{len(runas)} con runa de BT")
    for c in colisiones[:20]:
        print(f"  FALLA {c['captura']}: {c['hallazgos']}")
    return 0


def vertical_spine(img, box):
    """(cobertura de la columna mas vertical, x de esa columna) sobre los pixeles.

    La runa de BT es un `lv_line` y el inspector no publica su color de trazo, asi
    que la tinta se mide contra el fondo de la MISMA FILA a la derecha del rune
    (el bitmap del panel tiene gradiente: un color modal unico confunde bitmap con
    tinta). La ESPINA es la columna que cubre a lo alto la runa (>= 80% de las
    filas con un pixel distinto del fondo local); un zigzag solo no llega (medido:
    0.35 antes de #285 contra 0.92 con la espina).
    """
    x0, y0, x1, y1 = box
    rx0, rx1 = min(x1 + 3, 191), min(x1 + 8, 195)
    ref = img[y0 : y1 + 1, rx0:rx1].mean(axis=1)
    sub = img[y0 : y1 + 1, x0 : x1 + 1].astype(float)
    dist = np.abs(sub - ref[:, None, :]).max(axis=2)
    frac = (dist >= BT_INK_DELTA).mean(axis=0)
    col = int(np.argmax(frac))
    return round(float(frac[col]), 2), col


def _write_csv(path, rows, header=None):
    if not rows:
        path.write_text((header or "captura") + "\n")
        return
    keys = list(rows[0])
    with path.open("w", newline="") as fh:
        wr = csv.DictWriter(fh, fieldnames=keys)
        wr.writeheader()
        wr.writerows(rows)


if __name__ == "__main__":
    raise SystemExit(main())
