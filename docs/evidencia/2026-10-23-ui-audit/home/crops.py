#!/usr/bin/env python3
"""crops.py -- recortes ampliados (referencia visual) de la corrida de HOME.

Los PNG de `png/` son 800x480 SIN escalar (la evidencia). Estos recortes son
ayuda de lectura: se amplian con vecino mas cercano (NEAREST), sin
interpolacion, para poder mirar los glifos de 13 px del reloj. **Una captura
ampliada no prueba legibilidad fisica**: solo muestra los pixeles del render de
host, que es lo que se mide en `mediciones/`.

Salidas (todas se citan desde README.md):
  crops/bateria-<estado>-x8.png       zona del icono+porcentaje de un estado
  crops/bateria-temas-x8.png          la misma zona en los seis temas, apilada
  crops/reloj-panel-temas-x3.png      el panel del reloj (209x61) en los seis temas
  crops/barra-info-temas-x3.png       la barra de info (209x61) en los seis temas
  crops/barra-superior-temas-x2.png   la barra superior completa en los seis temas
  crops/comparativa-pct-x12.png       los estados de bateria de Aurora, en fila

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from pathlib import Path

from PIL import Image

RAIZ = Path(__file__).resolve().parent
PNG = RAIZ / "png"
CROPS = RAIZ / "crops"

TEMAS = [("home-rx-tema-aurora", "Aurora"), ("home-rx-tema-cathode", "Cathode"),
         ("home-rx-tema-redshift", "Redshift"), ("home-rx-tema-sprint", "Sprint"),
         ("home-rx-tema-sunlit", "Sunlit"), ("home-rx-tema-trail", "Trail")]
BATERIA = [("home-rx-bateria-0", "0%"), ("home-rx-bateria-7", "7%"),
           ("home-rx-bateria-10", "10%"), ("home-rx-bateria-11", "11%"),
           ("home-rx-bateria-36", "36%"), ("home-rx-bateria-50", "50%"),
           ("home-rx-base", "76%"), ("home-rx-bateria-99", "99%"),
           ("home-rx-bateria-100", "100%"), ("home-rx-bateria-desconocida", "--%")]


def recorte(captura, caja, escala, destino):
    im = Image.open(PNG / f"{captura}.png").convert("RGB").crop(caja)
    w, h = im.size
    im.resize((w * escala, h * escala), Image.NEAREST).save(CROPS / destino)


def apilado(filas, escala, destino, caja):
    """Apila los recortes con una franja separadora (el ancho del mayor)."""
    ims = []
    for cap, _ in filas:
        im = Image.open(PNG / f"{cap}.png").convert("RGB").crop(caja)
        ims.append(im.resize((im.size[0] * escala, im.size[1] * escala), Image.NEAREST))
    sep = 6
    ancho = max(i.size[0] for i in ims)
    alto = sum(i.size[1] for i in ims) + sep * (len(ims) - 1)
    out = Image.new("RGB", (ancho, alto), (255, 0, 0))
    y = 0
    for i in ims:
        out.paste(i, (0, y))
        y += i.size[1] + sep
    out.save(CROPS / destino)


def main():
    CROPS.mkdir(exist_ok=True)
    caja_bat = (748, 38, 800, 60)          # icono + porcentaje
    caja_reloj = (588, 0, 800, 61)         # panel del reloj completo
    caja_barra = (0, 0, 209, 61)           # barra de info completa
    caja_sup = (0, 0, 800, 61)             # barra superior completa
    for cap, nom in BATERIA + [(c, n) for c, n in TEMAS]:
        recorte(cap, caja_bat, 8, f"bateria-{nom}-x8.png")
    apilado(TEMAS, 8, "bateria-temas-x8.png", caja_bat)
    apilado(TEMAS, 3, "reloj-panel-temas-x3.png", caja_reloj)
    apilado(TEMAS, 3, "barra-info-temas-x3.png", caja_barra)
    apilado(TEMAS, 2, "barra-superior-temas-x2.png", caja_sup)
    ims = []
    for cap, _ in BATERIA:
        im = Image.open(PNG / f"{cap}.png").convert("RGB").crop(caja_bat)
        ims.append(im.resize((im.size[0] * 4, im.size[1] * 4), Image.NEAREST))
    sep = 6
    ancho = sum(i.size[0] for i in ims) + sep * (len(ims) - 1)
    alto = max(i.size[1] for i in ims)
    out = Image.new("RGB", (ancho, alto), (255, 0, 0))
    x = 0
    for i in ims:
        out.paste(i, (x, 0))
        x += i.size[0] + sep
    out.save(CROPS / "comparativa-pct-x12.png")
    print("recortes:", len(list(CROPS.glob('*.png'))))


if __name__ == "__main__":
    main()
