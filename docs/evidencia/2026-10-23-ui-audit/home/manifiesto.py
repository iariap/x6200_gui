#!/usr/bin/env python3
"""manifiesto.py -- `manifest.csv` de la corrida de evidencia de HOME (#270).

Une lo que ya esta medido y lo que ya se corrio, sin volver a correr nada:

  logs/capturas.tsv        out, fixture, args, exit code, sha256, bytes, w x h
  logs/comandos.log        el comando exacto de cada corrida
  json/<out>.json          revision embebida, tema activo, cobertura por componente
  mediciones/*.csv         las mediciones por elemento y la colision icono/porcentaje

Columnas (las de la epica #269, punto 6): screen, state, theme, source_coverage,
sha, command, exit_code, png, width, height, issue_id, severity, source_path,
observation.

La severidad sale de una regla explicita, no de una impresion (ver README):
  P1  si la captura tiene colision MEDIDA de tinta del texto dentro de la caja
      del icono (mediciones/colision_bateria.csv, columna colision).
  P2  si el contraste renderizado del porcentaje (la tinta fuera de la caja del
      icono) queda por debajo de 4.5:1, o si la muestra es invalida (--%).
  P3  el resto: no hay defecto medido en esa captura.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

import csv
import io
import json
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
SHA_CONGELADO = "d0ff5cd196b43ba0190d4ccad03f559a2954daf6"
ISSUE = "#270"

# Descripcion del estado de radio de cada captura (el JSON del arnes no lo lleva:
# sale del fixture, y el fixture esta versionado).
ESTADOS = {
    "home-rx-base": "RX / USB / 14.074 MHz / 20 m / bateria 76 % (nivel 3) / fuente externa 13.8 V / sin carga",
    "home-rx-tema-aurora": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / tema Aurora explicito",
    "home-rx-tema-cathode": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / tema Cathode",
    "home-rx-tema-redshift": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / tema Redshift",
    "home-rx-tema-sprint": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / tema Sprint",
    "home-rx-tema-sunlit": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / tema Sunlit",
    "home-rx-tema-trail": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / tema Trail",
    "home-tx-tema-aurora": "TX / USB / split 14.076 MHz / bateria 70 % / fuente externa 13.8 V / tema Aurora",
    "home-tx-tema-cathode": "TX / USB / split 14.076 MHz / bateria 70 % / tema Cathode",
    "home-tx-tema-redshift": "TX / USB / split 14.076 MHz / bateria 70 % / tema Redshift",
    "home-tx-tema-sprint": "TX / USB / split 14.076 MHz / bateria 70 % / tema Sprint",
    "home-tx-tema-sunlit": "TX / USB / split 14.076 MHz / bateria 70 % / tema Sunlit",
    "home-tx-tema-trail": "TX / USB / split 14.076 MHz / bateria 70 % / tema Trail",
    "home-rx-fuente-externa-cargando": "RX / USB / 14.074 MHz / bateria 76 % / fuente externa 13.8 V / CARGANDO",
    "home-rx-bateria-7": "RX / LSB / 7.100 MHz / bateria 7 % (icono vacio) / sin fuente externa",
    "home-rx-bateria-112-sin-fuente": "RX / LSB / 7.100 MHz / bateria 7 % / sin fuente externa / politica visual ECO",
    "home-rx-frecuencia-extrema": "RX / USB / 14.350 MHz (borde de 20 m) / banda ATU 3 / bateria 88 %",
    "home-rx-espectro-agua": "RX / USB / 14.074 MHz / bateria 84 % / espectro lleno + picos + linea central",
    "home-rx-digitos-dseg7": "RX / USB / 14.074 MHz / bateria 76 % / familia de digitos segmentada (dseg7): reloj y frecuencia con capa de \"8 fantasma\"",
    "home-rx-natural": "RX / USB / 14.074 MHz / bateria 76 % / espectro y waterfall empujados cuadro a cuadro (--bench-natural)",
    "home-rx-base-sin-inspect": "RX / USB / 14.074 MHz / mismo estado que home-rx-base, corrida SIN --inspect y sin --json (control de bytes)",
}
for _c in ("0", "7", "10", "11", "20", "30", "36", "50", "85", "99", "100"):
    ESTADOS[f"home-rx-bateria-{_c}"] = (
        f"RX / USB / 14.074 MHz / bateria {_c} % / fuente externa 13.8 V / sin carga"
        " (estado de la linea de estado del reloj; ver scenarios/)")
for _t in ("cathode", "redshift", "sprint", "sunlit", "trail"):
    ESTADOS[f"home-rx-bateria-85-{_t}"] = (
        f"RX / USB / 14.074 MHz / bateria 85 % / tema {_t.title()} (par del 76 % para aislar la tinta del icono)")

# Fuentes que dibujan cada captura (alcance A). El arbol completo, por captura,
# esta en mediciones/arbol-<out>.csv.
FUENTES = ("src/main_screen.c (layout), src/info.c + src/ui_tokens.h (barra), "
           "src/clock.c + src/styles.c (reloj), src/spectrum.c/src/waterfall.c (areas de dibujo), "
           "src/meter.c, src/buttons.cpp")


def cobertura_resumen(j):
    """Cobertura declarada por el arnes, agrupada por nivel."""
    por_nivel = {}
    for c in j["coverage"]:
        por_nivel.setdefault(c["level"], []).append(c["component"])
    orden = ["REAL", "PARTIAL", "SYNTHETIC", "UNSUPPORTED"]
    partes = []
    for nivel in orden:
        if nivel in por_nivel:
            partes.append(f"{nivel}: " + ", ".join(sorted(por_nivel[nivel])))
    return " | ".join(partes)


def main():
    tsv = list(csv.DictReader(io.StringIO((RAIZ / "logs/capturas.tsv").read_text()), delimiter="\t"))
    comandos = {}
    for linea in (RAIZ / "logs/comandos.log").read_text().splitlines():
        if not linea.strip():
            continue
        # el comando y el `--png` identifican la corrida
        for f in tsv:
            if f"/png/{f['out']}.png" in linea:
                comandos[f["out"]] = " ".join(linea.split())
                break
    colisiones = {}
    ruta_col = RAIZ / "mediciones/colision_bateria.csv"
    if ruta_col.exists():
        for r in csv.DictReader(ruta_col.open()):
            colisiones[r["captura"]] = r["colision"] == "True"
    bat = {r["captura"]: r for r in csv.DictReader((RAIZ / "mediciones/reloj_bateria.csv").open())}

    campos = ["screen", "state", "theme", "source_coverage", "sha", "command", "exit_code",
              "png", "width", "height", "issue_id", "severity", "source_path", "observation"]
    filas = []
    for f in tsv:
        out = f["out"]
        jruta = RAIZ / "json" / f"{out}.json"
        j = json.loads(jruta.read_text()) if jruta.exists() else None
        sha = j["revision"] if j else "n/a (corrida sin --json)"
        if j and j["revision"] != SHA_CONGELADO:
            print(f"ERROR: {out} trae revision {j['revision']} != SHA congelado", file=sys.stderr)
            return 1
        ancho, alto = (f["png_wh"].split("x") if "x" in f["png_wh"] else ("?", "?"))
        obs = []
        if out in bat:
            b = bat[out]
            obs.append(f"cajas icono/pct solapan {b['solape_cajas_px']} px; "
                       f"tinta del pct fuera del icono {b['tinta_pct_fuera_icono_px']} px, "
                       f"cobertura {b['cobertura_tinta_pct_fuera_icono']}, "
                       f"contraste renderizado {b['ratio_renderizado_pct_fuera_icono']}:1")
        if colisiones.get(out):
            obs.append("colision medida: tinta del porcentaje dentro de la caja del icono")
        if not obs:
            obs.append("sin defecto medido en esta captura")
        severidad = "P1" if colisiones.get(out) else "P3"
        if severidad == "P3" and out in bat:
            r = bat[out]["ratio_renderizado_pct_fuera_icono"]
            if r not in ("", None) and float(r) < 4.5:
                severidad = "P2"
        if not f["png_wh"].endswith("800x480"):
            obs.append("OJO: el PNG no es 800x480")
        filas.append({
            "screen": "home (main_screen)",
            "state": ESTADOS.get(out, f"ver fixture {f['fixture']}"),
            "theme": j["theme"]["active"] if j else "n/a",
            "source_coverage": cobertura_resumen(j) if j else "n/a (sin --json)",
            "sha": sha,
            "command": comandos.get(out, "ver logs/comandos.log"),
            "exit_code": f["exit_code"],
            "png": f"png/{out}.png",
            "width": ancho, "height": alto,
            "issue_id": ISSUE,
            "severity": severidad,
            "source_path": f"{f['fixture']}; {FUENTES}",
            "observation": "; ".join(obs),
        })
    destino = RAIZ / "manifest.csv"
    with destino.open("w", newline="") as fh:
        w = csv.DictWriter(fh, campos)
        w.writeheader()
        for fila in filas:
            w.writerow(fila)
    print(f"manifest.csv: {len(filas)} capturas")
    return 0


if __name__ == "__main__":
    sys.exit(main())
