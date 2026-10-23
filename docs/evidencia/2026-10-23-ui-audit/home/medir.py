#!/usr/bin/env python3
"""medir.py -- mediciones de las capturas de HOME (issue #270, epica #269).

Lee, por cada captura, el par `json/<out>.json` (arbol LVGL vivo del arnes,
`--inspect`) y `png/<out>.png` (800x480, sin escalar) y escribe:

  mediciones/arbol-<out>.csv    el arbol completo: clase, caja, visible, fuente,
                                texto, metricas de texto, colores, flags.
  mediciones/elementos.csv      los elementos del alcance A: geometria REAL del
                                arbol + medicion de PIXELES (caja de tinta,
                                color de fondo medido y contraste WCAG medido).
  mediciones/solapamientos.csv  intersecciones entre cajas de hermanos de la
                                barra superior (medidas del arbol, no del ojo).
  mediciones/regiones.csv       contenido de las areas de dibujo (espectro,
                                waterfall, S-meter, softkeys): tamano, colores
                                distintos, fraccion de pixeles no-fondo.
  mediciones/png.csv            saneamiento del PNG: modo, tamano, sha256.

Metodo de la medicion de pixeles (declarado, no implicito):
  * `tinta`  = pixeles dentro de la caja del elemento (mas un borde de `PAD`
               px para capturar el AA) cuya distancia maxima por canal al color
               llega al color DECLARADO por el arbol (cobertura >= COBERTURA_TINTA
               sobre el fondo medido). Es el color declarado el que identifica la
               tinta; no hay OCR ni umbral de gris, y el criterio de cobertura vale
               igual para familias con antialiasing fuerte (Redshift) que sin el.
  * `fondo`  = pixeles de la caja del elemento que NO son tinta (dilatada 1 px
               para sacar el AA). El color de fondo se reporta como el modal y
               como el MAS CLARO de la caja: sobre los bitmaps de la barra el
               fondo no es uniforme, asi que el peor caso se mide, no se supone.
  * `contraste` = formula WCAG 2.1 entre el color declarado y cada fondo (la
               misma de tests/check_wcag.py).

No infiere legibilidad fisica: todo es px de un render de host.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

import csv
import hashlib
import json
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

RAIZ = Path(__file__).resolve().parent
PAD = 2              # borde para capturar el antialiasing
COBERTURA_TINTA = 0.45  # fraccion del color declarado que define "tinta" (ver _fondo_y_cobertura)
FONDO_COBERTURA = 0.25  # cobertura por debajo de la cual un pixel es fondo
AA_GAP = 1           # dilatacion del fondo para sacar el AA

# Reglas de identificacion de los elementos del alcance A. Cada una es
# (rol, predicado sobre el nodo, indice entre los hijos del padre que cumplen).
def es_obj(n):
    return n["class"] in ("lv_obj", "lv_img")


def clase(n, c):
    return n["class"] == c


def buscar(nodo, pred, acc):
    if pred(nodo):
        acc.append(nodo)
    for h in nodo.get("children", []):
        buscar(h, pred, acc)
    return acc


def hijos(nodo, pred=None):
    hs = nodo.get("children", [])
    if pred is None:
        return hs
    return [h for h in hs if pred(h)]


def hex2rgb(h):
    return np.array([int(h[1:3], 16), int(h[3:5], 16), int(h[5:7], 16)], dtype=float)


def _canal(c):
    c = c / 255.0
    return np.where(c <= 0.03928, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def luminancia(c):
    c = np.asarray(c, dtype=float)
    return 0.2126 * _canal(c[0]) + 0.7152 * _canal(c[1]) + 0.0722 * _canal(c[2])


def contraste(a, b):
    la, lb = luminancia(a), luminancia(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


def dilatar(m):
    r = m.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            r |= np.roll(np.roll(m, dy, axis=0), dx, axis=1)
    return r


def rect_intersec(a, b):
    x0 = max(a[0], b[0])
    y0 = max(a[1], b[1])
    x1 = min(a[0] + a[2], b[0] + b[2])
    y1 = min(a[1] + a[3], b[1] + b[3])
    if x1 <= x0 or y1 <= y0:
        return None
    return (x0, y0, x1 - x0, y1 - y0)


class Captura:
    def __init__(self, out, jpath, ppath):
        self.out = out
        self.j = json.loads(Path(jpath).read_text())
        self.png = Path(ppath)
        self.img = np.array(Image.open(self.png).convert("RGB")).astype(float)
        self.ins = self.j["inspection"]
        self.raiz = self.ins["nodes"][0]

    def caja(self, n):
        return (n["x"], n["y"], n["w"], n["h"])

    def _fondo_y_cobertura(self, n, sub, propio, tc):
        """Fondo y cobertura de tinta, relativo al color declarado.

        El fondo se estima como el color modal de los pixeles de la caja que
        estan LEJOS del color declarado (el 40 % mas lejano): en una celda con
        fondo opaco es el color de la celda, en una etiqueta sobre el bitmap de
        la barra es el bitmap. La COBERTURA de cada pixel es la fraccion del
        color declarado que ese pixel alcanza sobre ese fondo:
        cobertura = mean((px - fondo) / (color_declarado - fondo)).
        Es la medida que no depende del antialiasing de la familia del tema: un
        trazo de 1 px de una fuente con AA no llega a 1.0, y por eso la tinta se
        define por cobertura (`COBERTURA_TINTA`), no por distancia absoluta.
        """
        dist = np.abs(sub - tc).max(axis=2)
        umbral_lejos = np.percentile(dist[propio], 60) if propio.any() else 96
        lejos = propio & (dist >= umbral_lejos)
        if lejos.sum() < 4:
            lejos = propio & (dist >= 96)
        if lejos.sum() == 0:
            return None, None
        cols, counts = np.unique(sub[lejos], axis=0, return_counts=True)
        fondo = cols[int(np.argmax(counts))].astype(float)
        den = tc - fondo
        canales = np.abs(den) > 32
        if not canales.any():
            return fondo, None
        cov = ((sub - fondo)[..., canales] / den[canales]).mean(axis=2)
        cov = np.clip(cov, 0.0, 1.0)
        return fondo, cov

    def _fondo_bajo_glifo(self, ink, cov, x0, y0, x, y, w, h, tc, salto=4):
        """Contraste WCAG medido CONTRA EL FONDO QUE ESTA DEBAJO DEL GLIFO.

        Por cada columna con tinta se muestrean los pixeles a `salto` px por
        encima del primer pixel de tinta y por debajo del ultimo (los dos, si
        existen, dentro de la caja propia). Es el fondo local de esa columna, no
        el promedio de la caja: sobre el bitmap de la barra el fondo no es
        uniforme y el brillo cambia columna a columna. Se descartan las muestras
        con cobertura >= FONDO_COBERTURA (no son fondo, son el borde del glifo).
        """
        ratios = []
        for cx in range(ink.shape[1]):
            filas = np.nonzero(ink[:, cx])[0]
            if len(filas) == 0:
                continue
            for fy in (filas[0] - salto, filas[-1] + salto):
                gy = y0 + fy
                gx = x0 + cx
                if not (y <= gy < y + h and x <= gx < x + w):
                    continue
                if cov[fy, cx] >= FONDO_COBERTURA:
                    continue
                ratios.append(contraste(self.img[gy, gx], tc))
        if not ratios:
            return None, None, None
        r = np.array(ratios)
        return round(float(np.percentile(r, 5)), 2), round(float(np.median(r)), 2), round(float((r < 4.5).mean()), 4)

    def medir_label(self, n, pad=PAD):
        """Tinta y contraste medidos en los pixeles de la caja de una etiqueta.

        La tinta es `cobertura >= COBERTURA_TINTA` (ver `_fondo_y_cobertura`):
        el criterio vale igual para una familia con antialiasing fuerte (donde el
        trazo nunca llega al color nominal) que para una sin el.
        """
        x, y, w, h = self.caja(n)
        if w <= 0 or h <= 0:
            return {}
        x0, y0 = max(0, x - pad), max(0, y - pad)
        x1, y1 = min(self.img.shape[1], x + w + pad), min(self.img.shape[0], y + h + pad)
        sub = self.img[y0:y1, x0:x1]
        tc = hex2rgb(n.get("text_color", "#FFFFFF"))
        ox0, oy0 = x - x0, y - y0
        propio = np.zeros(sub.shape[:2], bool)
        propio[oy0:oy0 + h, ox0:ox0 + w] = True
        fondo, cov = self._fondo_y_cobertura(n, sub, propio, tc)
        r = {}
        if fondo is None or cov is None:
            return {"metodo_fondo": "sin fondo medible", "bg_modal": None, "ratio_modal": None,
                    "cobertura_tinta_max": None, "ratio_renderizado": None,
                    "bg_claro": None, "ratio_claro": None, "frac_bg_bajo_45": None,
                    "ratio_glifo_p05": None, "ratio_glifo_mediana": None,
                    "frac_glifo_bajo_45": None, "frac_bg_bajo_3": None,
                    "ink_bbox": None, "ink_bbox_caja": None,
                    "ink_px": 0, "ink_px_caja": 0, "ink_fuera_caja": 0, "ratio_declarado": None,
                    "color_tinta": None}
        ink = cov >= COBERTURA_TINTA
        ys, xs = np.nonzero(ink)
        inb = ink & propio
        ys2, xs2 = np.nonzero(inb)
        r.update({
            "ink_px": int(ink.sum()),
            "ink_px_caja": int(inb.sum()),
            "ink_fuera_caja": int(ink.sum() - inb.sum()),
            "ink_bbox": None if len(xs) == 0 else (int(x0 + xs.min()), int(y0 + ys.min()),
                                                   int(x0 + xs.max()), int(y0 + ys.max())),
            "ink_bbox_caja": None if len(xs2) == 0 else (int(x + xs2.min()), int(y + ys2.min()),
                                                         int(x + xs2.max()), int(y + ys2.max())),
        })
        rmin, rmed, frac = self._fondo_bajo_glifo(inb, cov, x0, y0, x, y, w, h, tc)
        r["ratio_glifo_p05"] = rmin
        r["ratio_glifo_mediana"] = rmed
        r["frac_glifo_bajo_45"] = frac

        # fondo de la caja: los pixeles con cobertura baja
        filtro_fondo = propio & (cov < FONDO_COBERTURA)
        bg = sub[filtro_fondo]
        if len(bg) == 0:
            bg = sub[propio]
        cols, counts = np.unique(bg, axis=0, return_counts=True)
        modal = cols[int(np.argmax(counts))]
        lum = np.array([luminancia(c) for c in cols])
        # peor fondo: el mas cercano en luminancia al color declarado
        ratios_bg = np.array([contraste(c, tc) for c in cols])
        peor = cols[int(np.argmin(ratios_bg))]
        r.update({
            "bg_modal": tuple(int(v) for v in modal),
            "ratio_modal": round(float(contraste(modal, tc)), 2),
            "bg_claro": tuple(int(v) for v in peor),
            "ratio_claro": round(float(ratios_bg.min()), 2),
            "frac_bg_bajo_45": round(float(counts[ratios_bg < 4.5].sum() / counts.sum()), 4),
            "frac_bg_bajo_3": None,
            "metodo_fondo": "cobertura: fondo = pixeles de la caja con cobertura < 0.25",
        })
        declarado = hex2rgb(n.get("bg_color", "#000000"))
        proprio_fondo = (int(n.get("bg_opa", 0)) >= 250) and not n.get("bg_image")
        r["ratio_declarado"] = (round(float(contraste(declarado, tc)), 2) if proprio_fondo else None)
        # cobertura maxima y contraste del trazo mas fuerte
        idx = np.unravel_index(int(np.argmax(cov)), cov.shape)
        r["cobertura_tinta_max"] = round(float(cov[idx]), 3)
        r["ratio_renderizado"] = round(float(contraste(sub[idx], fondo)), 2)
        r["color_tinta"] = tuple(int(v) for v in sub[idx])
        return r

    def colores_tema(self):
        """Los colores declarados del tema de esta captura (los mismos archivos
        que instala la imagen, rootfs/usr/share/x6200/themes/<nombre>.json)."""
        nombre = self.j["theme"]["active"]
        ruta = RAIZ.parents[3] / "rootfs/usr/share/x6200/themes" / f"{nombre}.json"
        return json.loads(ruta.read_text())["colors"]

    def medir_runa(self, n, cov_ink=0.45, cov_bg=0.25):
        """La runa de BT es un `lv_line`: el inspector no emite su color de trazo
        (`line_color`, no `text_color`). El color se toma del tema declarado
        (`icon_off`, el estado "apagado" que el mock del arnes fija) y se mide la
        cobertura contra los pixeles de la ventana de la runa (mismo criterio que
        las etiquetas, `_fondo_y_cobertura`). El fondo es el bitmap de la barra: la
        runa no esta dentro de la celda de WiFi, comparte el fondo del contenedor.
        """
        x, y, w, h = self.caja(n)
        colores = self.colores_tema()
        trazo = hex2rgb(colores["icon_off"])
        sub = self.img[y:y + h, x:x + w]
        propio = np.ones(sub.shape[:2], bool)
        fondo_px, cov = self._fondo_y_cobertura(n, sub, propio, trazo)
        if fondo_px is None or cov is None:
            return {"metodo_fondo": "sin fondo medible", "bg_modal": None, "ratio_modal": None,
                    "bg_claro": None, "ratio_claro": None, "frac_bg_bajo_45": None,
                    "color_tinta": tuple(int(v) for v in trazo), "ink_px": 0, "ink_px_caja": 0,
                    "ink_fuera_caja": 0, "ink_bbox": None, "ink_bbox_caja": None,
                    "ratio_glifo_p05": None, "ratio_glifo_mediana": None,
                    "frac_glifo_bajo_45": None, "frac_bg_bajo_3": None, "ratio_declarado": None,
                    "cobertura_tinta_max": None, "ratio_renderizado": None}
        ink = cov >= cov_ink
        fondo = sub[propio & (cov < cov_bg)]
        idx = np.unravel_index(int(np.argmax(cov)), cov.shape)
        r = {"color_tinta": tuple(int(v) for v in trazo),
             "metodo_fondo": "color de trazo declarado por el tema (icon_off) + cobertura de pixeles",
             "cobertura_tinta_max": round(float(cov[idx]), 3),
             "ratio_renderizado": round(float(contraste(sub[idx], fondo_px)), 2),
             "ratio_declarado": None, "frac_glifo_bajo_45": None,
             "ratio_glifo_p05": None, "ratio_glifo_mediana": None}
        ys, xs = np.nonzero(ink)
        r["ink_px"] = int(ink.sum())
        r["ink_px_caja"] = int(ink.sum())
        r["ink_fuera_caja"] = 0
        r["ink_bbox"] = None if len(xs) == 0 else (int(x + xs.min()), int(y + ys.min()),
                                                   int(x + xs.max()), int(y + ys.max()))
        r["ink_bbox_caja"] = r["ink_bbox"]
        if len(fondo) == 0:
            r.update({"bg_modal": None, "ratio_modal": None, "bg_claro": None,
                      "ratio_claro": None, "frac_bg_bajo_45": None, "frac_bg_bajo_3": None})
            return r
        cols, counts = np.unique(fondo, axis=0, return_counts=True)
        modal = cols[int(np.argmax(counts))]
        # para un trazo oscuro el peor fondo es el mas cercano en luminancia
        ratios = np.array([contraste(c, trazo) for c in cols])
        peor = cols[int(np.argmin(ratios))]
        r.update({
            "bg_modal": tuple(int(v) for v in modal),
            "ratio_modal": round(float(contraste(modal, trazo)), 2),
            "bg_claro": tuple(int(v) for v in peor),
            "ratio_claro": round(float(ratios.min()), 2),
            # fraccion de los pixeles de fondo con contraste < 3:1 (WCAG 1.4.11,
            # el umbral del grafico no textual). La runa NO es texto: su umbral es
            # 3.0, y va en su propia columna para que nadie lo lea como 4.5.
            "frac_bg_bajo_45": None,
            "frac_bg_bajo_3": round(float(counts[ratios < 3.0].sum() / counts.sum()), 4),
        })
        return r

    def elementos(self):
        """Los elementos del alcance A, por rol, medidos del arbol real."""
        el = {}
        pantalla = self.raiz
        toplevel = [n for n in hijos(pantalla)]

        # --- barra de info (arriba a la izquierda), S-meter y panel del reloj
        barras = [n for n in toplevel if clase(n, "lv_obj") and n["h"] == 61 and n["y"] == 0]
        info = next((n for n in barras if n["x"] < 300), None)
        clock = next((n for n in barras if n["x"] > 500), None)
        meter = next((n for n in barras if n not in (info, clock)), None)
        if meter is not None:
            el["s_meter"] = meter
        if info is not None:
            el["info_bar"] = info
            filas = hijos(info, lambda n: clase(n, "lv_obj"))
            for i, fila in enumerate(filas, 1):
                el[f"info_fila_{i}"] = fila
                celdas = hijos(fila, lambda n: clase(n, "lv_label"))
                # el orden de las celdas lo fija src/info.c: (VFO, MODE, AGC) / (PRE_ATT, ATU, WIFI)
                nombres = (("info_celda_vfo", "info_celda_modo", "info_celda_agc") if i == 1
                           else ("info_celda_pre_att", "info_celda_atu", "info_celda_wifi"))
                for nombre, celda in zip(nombres, celdas):
                    el[nombre] = celda
            runa = hijos(info, lambda n: clase(n, "lv_line"))
            if runa:
                el["bt_runa"] = runa[0]
        if clock is not None:
            el["clock_panel"] = clock
            # El orden de creacion en src/clock.c es: fantasma (solo con familia
            # segmentada), hora, fuente externa, porcentaje, icono de bateria.
            etiquetas = hijos(clock, lambda n: clase(n, "lv_label"))
            for c in etiquetas:
                t = c.get("text", "") or ""
                if t == "88:88:88":
                    el["reloj_fantasma"] = c
                elif re.fullmatch(r"\d\d:\d\d:\d\d|--:--:--", t):
                    el["reloj_hora"] = c
                elif re.fullmatch(r"\d+%|--%", t):
                    el["reloj_bateria_pct"] = c
                elif len(t) == 1 and 0xF240 <= ord(t) <= 0xF244:
                    el["reloj_bateria_icono"] = c
            # la fuente externa es la que queda abajo a la izquierda (oculta sin fuente)
            usadas = {id(el.get(k)) for k in ("reloj_fantasma", "reloj_hora",
                                              "reloj_bateria_pct", "reloj_bateria_icono")}
            resto = [c for c in etiquetas if id(c) not in usadas]
            if resto:
                el["reloj_fuente_ext"] = resto[0]

        # --- fila de frecuencia (debajo del espectro)
        fila = next((n for n in toplevel
                     if clase(n, "lv_obj") and n["h"] == 36 and n["w"] == 800), None)
        if fila is not None:
            labels = hijos(fila, lambda n: clase(n, "lv_label"))
            for nombre, lab in zip(("frecuencia_izq", "frecuencia_centro", "frecuencia_der"), labels):
                el[nombre] = lab
        # el "8 fantasma" de la fila vive en la pantalla (no en el flex): 500x36
        fantasma_freq = [n for n in toplevel if clase(n, "lv_label") and n["w"] == 500 and n["h"] == 36]
        if fantasma_freq:
            el["frecuencia_fantasma"] = fantasma_freq[0]

        # --- areas de dibujo y softkeys
        grandes = [n for n in toplevel
                   if clase(n, "lv_obj") and n["w"] == 800 and n["h"] >= 150 and not n.get("hidden")]
        if grandes:
            el["espectro"] = grandes[0]
        if len(grandes) > 1:
            el["waterfall"] = grandes[1]
        objetivos = [n for n in toplevel if clase(n, "lv_obj") and n["y"] == 418 and n["w"] == 160]
        for i, b in enumerate(objetivos, 1):
            el[f"softkey_{i}"] = b
        txi = [n for n in toplevel if clase(n, "lv_obj") and n["w"] >= 377 and n["h"] == 123]
        if txi:
            el["tx_info"] = txi[0]
        return el


def medir_reloj_bateria(cap, el):
    """Icono de bateria vs porcentaje en la linea de estado del reloj.

    El contrato (src/clock.c) ancla el porcentaje al borde derecho del panel y
    corre el icono STATUS_GAP px a su izquierda con
    `lv_obj_align_to(..., LV_ALIGN_OUT_LEFT_BOTTOM, -STATUS_GAP, 0)`. Medido: las
    dos cajas se SOLAPAN (el signo del offset produce un solape, no una
    separacion). Lo que decide si eso se ve es la tinta: este bloque mide las
    corridas de columnas con tinta de la zona y el hueco entre la corrida del
    icono y la del porcentaje. Devuelve None si falta alguno de los dos.
    """
    icono = el.get("reloj_bateria_icono")
    pct = el.get("reloj_bateria_pct")
    if icono is None or pct is None:
        return None
    ci, cp = cap.caja(icono), cap.caja(pct)
    solape = rect_intersec(ci, cp)
    x0 = min(ci[0], cp[0]) - 3
    x1 = max(ci[0] + ci[2], cp[0] + cp[2]) + 3
    y0 = min(ci[1], cp[1])
    y1 = max(ci[1] + ci[3], cp[1] + cp[3])
    sub = cap.img[y0:y1, x0:x1]
    tc_i = hex2rgb(icono.get("text_color", "#FFFFFF"))
    tc_p = hex2rgb(pct.get("text_color", "#FFFFFF"))
    _fondo_rb, cov_rb = cap._fondo_y_cobertura(None, sub, np.ones(sub.shape[:2], bool), tc_p)
    ink = cov_rb >= 0.4    # corridas de columnas con tinta (>=1 pixel de tinta en la columna)
    cols = ink.any(axis=0)
    corridas = []
    ini = None
    for i, v in enumerate(cols):
        if v and ini is None:
            ini = i
        elif not v and ini is not None:
            corridas.append((x0 + ini, x0 + i - 1))
            ini = None
    if ini is not None:
        corridas.append((x0 + ini, x0 + len(cols) - 1))
    # la primera corrida es la del icono (esta anclado a la izquierda)
    tinta_icono = corridas[0] if corridas else None
    tinta_pct = None
    if len(corridas) >= 2:
        fin = corridas[0][1]
        resto = [c for c in corridas[1:] if c[0] > fin]
        if resto:
            tinta_pct = (resto[0][0], resto[-1][1])
    hueco = None
    colision = None
    if tinta_icono and tinta_pct:
        hueco = tinta_pct[0] - tinta_icono[1] - 1
        colision = hueco < 1
    m = cap.medir_label(pct)
    # Tinta del porcentaje FUERA de la caja del icono: es la parte del texto que
    # no se puede atribuir al icono, y la que dice como se lee el numero.
    xf = ci[0] + ci[2] + 1
    reg = cap.img[cp[1]:cp[1] + cp[3], xf:cp[0] + cp[2] + 1]
    ratio_render = cob = px_fuera = None
    if reg.size:
        d = np.abs(reg - tc_p).max(axis=2)
        fondo = reg[d >= 96]
        if len(fondo):
            bg = np.array([np.bincount(fondo[:, c].astype(int), minlength=256).argmax()
                           for c in range(3)], dtype=float)
            den = tc_p - bg
            canales = np.abs(den) > 32
            px_fuera = int((d <= 96).sum())
            if canales.any():
                cob_map = ((reg - bg)[..., canales] / den[canales]).mean(axis=2)
                idx = np.unravel_index(int(np.argmax(cob_map)), cob_map.shape)
                cob = round(float(cob_map[idx]), 3)
                ratio_render = round(float(contraste(reg[idx], bg)), 2)
    return {
        "texto_pct": pct.get("text"),
        "caja_icono": ci, "caja_pct": cp,
        "solape_cajas_px": None if solape is None else solape[2],
        "tinta_icono_x": tinta_icono, "tinta_pct_x": tinta_pct,
        "hueco_tinta_px": hueco, "colision_tinta": colision,
        "tinta_icono_fuera_caja_px": None if tinta_icono is None else max(0, tinta_icono[1] - (ci[0] + ci[2] - 1)),
        "tinta_pct_entra_caja_px": None if tinta_pct is None else tinta_pct[0] - cp[0],
        "tinta_pct_fuera_icono_px": px_fuera,
        "cobertura_tinta_pct_fuera_icono": cob,
        "ratio_renderizado_pct_fuera_icono": ratio_render,
        "color_icono": icono.get("text_color"), "color_pct": pct.get("text_color"),
        "colores_iguales": tc_i.tolist() == tc_p.tolist(),
        "ratio_glifo_p05_pct": m.get("ratio_glifo_p05"),
        "ratio_glifo_mediana_pct": m.get("ratio_glifo_mediana"),
        "ratio_declarado_pct": m.get("ratio_declarado"),
    }


def analisis_colision(base, out_dir=None):
    """Colision icono de bateria / porcentaje, por grupo de simbolo.

    Metodo (sin atribucion de glifos): para cada tema y simbolo del icono
    (nivel de `battery_cap`, src/clock.c) se toman las capturas del grupo y se
    calcula la INTERSECCION de sus mascaras de tinta dentro de una ventana
    anclada a la caja del icono. Lo que aparece en TODOS los estados del mismo
    simbolo es el icono; lo que aparece solo en algunos es texto del porcentaje
    dibujado dentro de esa ventana. Es una cota INFERIOR del texto superpuesto:
    dos estados que compartan el mismo digito inicial (p.ej. 70% y 76%) dejan
    en la interseccion los pixeles que comparten.

    Se corre solo con pares reales del mismo tema: los capturados en Aurora
    (todas las familias de glifos del tema pueden diferir entre temas).
    """
    cap_por_captura = {}
    tsv = base / "logs/capturas.tsv"
    if not tsv.exists():
        return None
    import io
    filas = list(csv.DictReader(io.StringIO(tsv.read_text()), delimiter="\t"))
    for f in filas:
        nombre = f["fixture"]
        ruta = (base / "scenarios" / Path(nombre).name) if nombre.startswith("scn/") \
            else (RAIZ.parents[3] / "tools/ui_harness/fixtures" / nombre)
        cap = None
        if ruta.exists():
            for linea in ruta.read_text().splitlines():
                if linea.startswith("battery_cap"):
                    cap = int(linea.split()[1])
        cap_por_captura[f["out"]] = cap

    def simbolo(cap, texto):
        if cap is None:
            return "UNKNOWN"
        if texto == "--%":
            return "BATTERY_EMPTY (muestra invalida)"
        if cap <= 10:
            return "BATTERY_EMPTY"
        if cap <= 35:
            return "BATTERY_1"
        if cap <= 60:
            return "BATTERY_2"
        if cap <= 85:
            return "BATTERY_3"
        return "BATTERY_FULL"

    # estado por captura: caja del icono, texto del porcentaje, tema, mascara
    estados = []
    el = {}
    with open(base / "mediciones/elementos.csv") as f:
        for r in csv.DictReader(f):
            if r["rol"] in ("reloj_bateria_icono", "reloj_bateria_pct"):
                el[(r["captura"], r["rol"])] = r
    for out in sorted(cap_por_captura):
        icono = el.get((out, "reloj_bateria_icono"))
        pct = el.get((out, "reloj_bateria_pct"))
        if icono is None or pct is None:
            continue
        cap = Captura(out, base / "json" / f"{out}.json", base / "png" / f"{out}.png")
        x, y, w, h = int(icono["x"]), int(icono["y"]), int(icono["w"]), int(icono["h"])
        tc = hex2rgb(pct["text_color"])
        sub = cap.img[y:y + h + 2, x:x + w + 6]
        propio = np.ones(sub.shape[:2], bool)
        _fondo, cov = cap._fondo_y_cobertura(None, sub, propio, tc)
        mask = cov >= 0.3      # tinta + su antialiasing
        estados.append({
            "out": out, "tema": cap.j["theme"]["active"],
            "simbolo": simbolo(cap_por_captura[out], pct["texto"]),
            "cap": cap_por_captura[out], "texto": pct["texto"],
            "caja_icono": (x, y, w, h), "mask": mask,
            "caja_pct": (int(pct["x"]), int(pct["y"]), int(pct["w"]), int(pct["h"])),
        })

    filas_out = []
    for tema in sorted({e["tema"] for e in estados}):
        for sim in sorted({e["simbolo"] for e in estados}):
            grupo = [e for e in estados if e["tema"] == tema and e["simbolo"] == sim]
            if len(grupo) < 2:
                continue
            formas = {e["mask"].shape for e in grupo}
            if len(formas) != 1:
                continue
            inter = grupo[0]["mask"].copy()
            for e in grupo[1:]:
                inter &= e["mask"]
            for e in grupo:
                extra = e["mask"] & ~inter
                x, y, w, h = e["caja_icono"]
                ys, xs = np.nonzero(extra)
                dentro = int(extra[:, :w].sum())
                filas_out.append({
                    "captura": e["out"], "tema": tema, "simbolo": sim, "cap": e["cap"],
                    "texto_pct": e["texto"], "caja_icono": (x, y, w, h), "caja_pct": e["caja_pct"],
                    "tinta_icono_interseccion_px": int(inter.sum()),
                    "tinta_extra_px": int(extra.sum()),
                    "tinta_extra_bbox": None if len(xs) == 0 else
                        (x + int(xs.min()), y + int(ys.min()), x + int(xs.max()), y + int(ys.max())),
                    "tinta_extra_dentro_caja_icono_px": dentro,
                    "colision": bool(dentro > 0),
                    "metodo": "interseccion de mascaras entre estados del mismo simbolo y tema",
                })
    if not filas_out:
        return None
    campos = list(filas_out[0].keys())
    destino = (out_dir or (base / "mediciones")) / "colision_bateria.csv"
    with open(destino, "w", newline="") as f:
        w = csv.DictWriter(f, campos)
        w.writeheader()
        for fila in filas_out:
            w.writerow(fila)
    return destino


def main():
    base = RAIZ
    outs = sorted(p.stem for p in (base / "json").glob("*.json"))
    if not outs:
        print("no hay json/", file=sys.stderr)
        return 1
    (base / "mediciones").mkdir(exist_ok=True)

    campos_arbol = ["captura", "prof", "clase", "x", "y", "w", "h", "visible", "hidden",
                    "overflow_visible", "scroll_dir", "texto", "fuente", "text_natural",
                    "text_box", "text_overflow", "text_truncado", "text_color", "bg_color",
                    "bg_opa", "bg_image", "glyphs_faltantes", "glyphs_fallback"]
    campos_el = ["captura", "tema", "rol", "clase", "x", "y", "w", "h", "visible", "hidden",
                 "fuente", "texto", "text_natural", "text_box", "text_overflow", "text_truncado",
                 "text_color", "bg_color", "bg_opa", "metodo_fondo", "ink_bbox", "ink_bbox_caja",
                 "ink_px", "ink_px_caja", "ink_fuera_caja", "color_tinta", "bg_modal", "ratio_modal",
                 "bg_claro", "ratio_claro", "ratio_declarado", "ratio_glifo_p05",
                 "ratio_glifo_mediana", "frac_glifo_bajo_45", "frac_bg_bajo_45",
                 "frac_bg_bajo_3",
                 "cobertura_tinta_max", "ratio_renderizado",
                 "glyphs_faltantes", "glyphs_fallback"]
    campos_solape = ["captura", "a", "b", "caja_a", "caja_b", "interseccion", "area_px"]
    campos_reg = ["captura", "region", "x", "y", "w", "h", "colores_distintos",
                  "frac_no_modal", "lum_min", "lum_max", "lum_media"]
    campos_png = ["captura", "archivo", "modo", "ancho", "alto", "bytes", "sha256"]
    campos_bat = ["captura", "tema", "texto_pct", "caja_icono", "caja_pct", "solape_cajas_px",
                  "tinta_icono_x", "tinta_pct_x", "hueco_tinta_px", "colision_tinta",
                  "tinta_icono_fuera_caja_px", "tinta_pct_entra_caja_px",
                  "tinta_pct_fuera_icono_px", "cobertura_tinta_pct_fuera_icono",
                  "ratio_renderizado_pct_fuera_icono", "color_icono",
                  "color_pct", "colores_iguales", "ratio_glifo_p05_pct",
                  "ratio_glifo_mediana_pct", "ratio_declarado_pct"]
    f_el = open(base / "mediciones/elementos.csv", "w", newline="")
    w_el = csv.DictWriter(f_el, campos_el)
    w_el.writeheader()
    f_sol = open(base / "mediciones/solapamientos.csv", "w", newline="")
    w_sol = csv.DictWriter(f_sol, campos_solape)
    w_sol.writeheader()
    f_reg = open(base / "mediciones/regiones.csv", "w", newline="")
    w_reg = csv.DictWriter(f_reg, campos_reg)
    w_reg.writeheader()
    f_png = open(base / "mediciones/png.csv", "w", newline="")
    w_png = csv.DictWriter(f_png, campos_png)
    w_png.writeheader()
    f_bat = open(base / "mediciones/reloj_bateria.csv", "w", newline="")
    w_bat = csv.DictWriter(f_bat, campos_bat)
    w_bat.writeheader()

    for out in outs:
        cap = Captura(out, base / "json" / f"{out}.json", base / "png" / f"{out}.png")

        # 1. arbol completo
        with open(base / f"mediciones/arbol-{out}.csv", "w", newline="") as f:
            wr = csv.DictWriter(f, campos_arbol)
            wr.writeheader()

            def volcar(n, prof):
                wr.writerow({
                    "captura": out, "prof": prof, "clase": n["class"],
                    "x": n["x"], "y": n["y"], "w": n["w"], "h": n["h"],
                    "visible": n.get("visible"), "hidden": n.get("hidden"),
                    "overflow_visible": n.get("overflow_visible"), "scroll_dir": n.get("scroll_dir"),
                    "texto": n.get("text"), "fuente": n.get("text_font"),
                    "text_natural": n.get("text_natural"), "text_box": n.get("text_box"),
                    "text_overflow": n.get("text_overflow"), "text_truncado": n.get("text_truncated"),
                    "text_color": n.get("text_color"), "bg_color": n.get("bg_color"),
                    "bg_opa": n.get("bg_opa"), "bg_image": n.get("bg_image"),
                    "glyphs_faltantes": n.get("missing_glyphs"), "glyphs_fallback": n.get("fallback_glyphs"),
                })
                for h in n.get("children", []):
                    volcar(h, prof + 1)

            volcar(cap.raiz, 0)

        # 2. elementos del alcance A
        el = cap.elementos()
        for rol, n in el.items():
            if n is None:
                continue
            # La medicion de tinta/contraste solo tiene sentido donde hay dibujo
            # de primer plano: etiquetas y la runa. Los contenedores se miden en
            # `regiones.csv` (contenido) y en `solapamientos.csv` (cajas).
            if rol == "bt_runa":
                m = cap.medir_runa(n)
            elif n["class"] == "lv_label":
                m = cap.medir_label(n)
            else:
                m = {}
            fila = {
                "captura": out, "tema": cap.j["theme"]["active"], "rol": rol,
                "clase": n["class"], "x": n["x"], "y": n["y"], "w": n["w"], "h": n["h"],
                "visible": n.get("visible"), "hidden": n.get("hidden"),
                "fuente": n.get("text_font"), "texto": n.get("text"),
                "text_natural": n.get("text_natural"), "text_box": n.get("text_box"),
                "text_overflow": n.get("text_overflow"), "text_truncado": n.get("text_truncated"),
                "text_color": n.get("text_color"), "bg_color": n.get("bg_color"),
                "bg_opa": n.get("bg_opa"),
            }
            for k in campos_el:
                if k not in fila:
                    fila[k] = m.get(k)
            w_el.writerow(fila)

        # 3. solapamientos entre las cajas de los elementos del alcance A:
        #    solo se reportan los solapes PARCIALES (una caja que contiene a la
        #    otra es anidamiento del layout, no defecto).
        roles = [(r, n) for r, n in el.items() if n is not None]
        for i in range(len(roles)):
            for k in range(i + 1, len(roles)):
                ra, na = roles[i]
                rb, nb = roles[k]
                ca, cb = cap.caja(na), cap.caja(nb)
                ia = rect_intersec(ca, cb)
                if ia is None:
                    continue
                contiene = (ca[0] <= cb[0] and ca[1] <= cb[1] and
                            ca[0] + ca[2] >= cb[0] + cb[2] and ca[1] + ca[3] >= cb[1] + cb[3])
                contenido = (cb[0] <= ca[0] and cb[1] <= ca[1] and
                             cb[0] + cb[2] >= ca[0] + ca[2] and cb[1] + cb[3] >= ca[1] + ca[3])
                if contiene or contenido:
                    continue
                w_sol.writerow({"captura": out, "a": ra, "b": rb,
                                "caja_a": ca, "caja_b": cb,
                                "interseccion": ia, "area_px": ia[2] * ia[3]})

        # 4. contenido de las areas de dibujo.
        #    Bandas LIMPIAS (sin los widgets de la barra superior ni los paneles
        #    que se dibujan encima): asi "el espectro/waterfall no esta en blanco"
        #    se mide sobre pixeles del propio trazado.
        bandas = {
            "espectro_banda_limpia": (0, 62, 209, 97),
            "waterfall_banda_limpia": (100, 290, 600, 170),
        }
        for nombre, (bx, by, bw, bh) in bandas.items():
            sub = cap.img[by:by + bh, bx:bx + bw]
            if sub.size == 0:
                continue
            col, cnt = np.unique(sub.reshape(-1, 3), axis=0, return_counts=True)
            lum = np.array([luminancia(c) for c in col])
            w_reg.writerow({
                "captura": out, "region": nombre, "x": bx, "y": by, "w": bw, "h": bh,
                "colores_distintos": len(col),
                "frac_no_modal": round(float(1 - cnt.max() / cnt.sum()), 4),
                "lum_min": round(float(lum.min()), 4), "lum_max": round(float(lum.max()), 4),
                "lum_media": round(float((lum * cnt).sum() / cnt.sum()), 4),
            })
        for region in ("tx_info",):
            n = el.get(region)
            if n is None or n.get("hidden"):
                continue
            x, y, w, h = cap.caja(n)
            sub = cap.img[y:y + h, x:x + w]
            if sub.size == 0:
                continue
            col, cnt = np.unique(sub.reshape(-1, 3), axis=0, return_counts=True)
            lum = np.array([luminancia(c) for c in col])
            w_reg.writerow({
                "captura": out, "region": region, "x": x, "y": y, "w": w, "h": h,
                "colores_distintos": len(col),
                "frac_no_modal": round(float(1 - cnt.max() / cnt.sum()), 4),
                "lum_min": round(float(lum.min()), 4), "lum_max": round(float(lum.max()), 4),
                "lum_media": round(float((lum * cnt).sum() / cnt.sum()), 4),
            })

        # 5. saneamiento del PNG
        raw = cap.png.read_bytes()
        with Image.open(cap.png) as im:
            modo, (pw, ph) = im.mode, im.size
        w_png.writerow({"captura": out, "archivo": f"png/{out}.png", "modo": modo,
                        "ancho": pw, "alto": ph, "bytes": len(raw),
                        "sha256": hashlib.sha256(raw).hexdigest()})

        # 6. icono de bateria vs porcentaje (la linea de estado del reloj)
        bat = medir_reloj_bateria(cap, el)
        if bat is not None:
            fila = {"captura": out, "tema": cap.j["theme"]["active"]}
            fila.update(bat)
            w_bat.writerow(fila)
        print(f"{out}: ok")

    for f in (f_el, f_sol, f_reg, f_png, f_bat):
        f.close()

    destino = analisis_colision(base)
    print("analisis de colision icono/porcentaje:", destino)
    return 0


if __name__ == "__main__":
    sys.exit(main())
