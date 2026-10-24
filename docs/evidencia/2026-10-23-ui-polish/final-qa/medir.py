#!/usr/bin/env python3
"""Mediciones estructurales de la matriz del QA final (epica #284 / issue #287).

Lee los JSON del arnes (`--inspect`: arbol LVGL VIVO, sin OCR) y escribe:

  mediciones/elementos.csv      un renglon por etiqueta con texto: caja, metrica
                                natural de la fuente, recorte, glifos, visibilidad
  mediciones/hallazgos.csv      un renglon por fixture con los CONTADORES de
                                defectos estructurales medidos
  mediciones/solapamientos.csv  pares de cajas de texto que se intersectan
  mediciones/alineacion.csv     hechos de alineacion (barra de info, reloj, fila
                                de frecuencia, softkeys, grilla de Ajustes)
  mediciones/resumen.csv        agregado por grupo (home / dialogs / apps / gaps)
  mediciones/cobertura.csv      cobertura declarada por el arnes, por captura

Que se mide y que no
--------------------
* `text_natural` lo calcula LVGL (`lv_txt_get_size`), no una estimacion: el
  "bounds de fuente" es la caja natural del texto contra la caja del objeto.
* `missing_glyphs` son code points placeholder en TODA la cadena de fuentes;
  `fallback_glyphs` los que resuelven en otra fuente de la cadena (esperado para
  los iconos). Ninguno de los dos es una impresion: los publica el inspector.
* El solapamiento es interseccion de cajas VISIBLES (caja ∩ clip de los
  ancestros). No dice si el solape es un defecto: los pares legitimos (la capa
  "8 fantasma" debajo del valor, la etiqueta encima de su propio fondo) se
  listan con su contexto y se clasifican en el README.
* Nada de esto es legibilidad fisica: es geometria y pixeles de un render de
  host 800x480 (ver README §limites).

Uso:  python3 docs/evidencia/2026-10-23-ui-polish/final-qa/medir.py
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

AQUI = Path(__file__).resolve().parent

# Grupo por nombre de captura (el contrato de #287 pide separar home, dialogos
# y apps). El corpus del arnes no tiene un campo de grupo: se clasifica por el
# prefijo de la fixture, que es la convencion del repo desde #269.
PREFIJOS_HOME = (
    "home-",
    "main-",
    "net-",
    "finalqa-",
    "bar-legacy",
    "vision-home",
)
PREFIJOS_APPS = ("apps-menu-", "app-bands", "app-close-home", "locked-feedback")
PREFIJOS_DIALOGO = (
    "settings-",
    "bt-dialog",
    "cw-tune",
    "dfl-",
    "swrscan-",
    "autotune",
    "dialog-",
)


def grupo(nombre: str) -> str:
    if nombre.startswith(PREFIJOS_APPS):
        return "apps-menu"
    for p in PREFIJOS_DIALOGO:
        if nombre.startswith(p):
            return "dialogos"
    for p in PREFIJOS_HOME:
        if nombre.startswith(p):
            return "home"
    return "otros"


def texto_visible(n: dict) -> bool:
    v = n.get("visible")
    if not v:
        return False
    if v["w"] <= 0 or v["h"] <= 0:
        return False
    return not n.get("hidden") and (n.get("opa") or 0) > 0


def walk(n: dict, path: str = "0"):
    yield path, n
    for i, c in enumerate(n.get("children") or []):
        yield from walk(c, f"{path}/{i}")


def intersect(a, b) -> int:
    x0, y0 = max(a["x"], b["x"]), max(a["y"], b["y"])
    x1 = min(a["x"] + a["w"], b["x"] + b["w"])
    y1 = min(a["y"] + a["h"], b["y"] + b["h"])
    if x1 <= x0 or y1 <= y0:
        return 0
    return (x1 - x0) * (y1 - y0)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--base", type=Path, default=AQUI, help="directorio con json/ (entrada) y mediciones/ (salida)"
    )
    args = ap.parse_args(argv)
    JSON_DIR = args.base / "json"
    MED = args.base / "mediciones"
    MED.mkdir(exist_ok=True)
    elementos, hallazgos, solapes, alineacion, cobertura = [], [], [], [], []
    resumen: dict[str, dict[str, int]] = {}

    for ruta in sorted(JSON_DIR.glob("*.json")):
        d = json.loads(ruta.read_text(errors="surrogateescape"))
        nombre = ruta.stem
        g = grupo(nombre)
        r = resumen.setdefault(
            g,
            {
                "capturas": 0,
                "labels": 0,
                "overflow": 0,
                "truncated": 0,
                "missing": 0,
                "fallback": 0,
                "clip": 0,
                "invisibles_con_texto": 0,
                "solapes": 0,
                "fuera_de_pantalla": 0,
            },
        )
        r["capturas"] += 1
        for c in d.get("coverage") or []:
            cobertura.append(
                {"captura": nombre, "componente": c["component"], "nivel": c["level"], "nota": c["note"]}
            )

        nodes = (d.get("inspection") or {}).get("nodes") or []
        if not nodes:
            hallazgos.append({"captura": nombre, "grupo": g, "instrumentado": 0})
            continue

        textos = []
        for path, n in walk(nodes[0]):
            if n.get("class") != "lv_label":
                continue
            txt = (n.get("text") or "").replace("\n", "\\n")
            vis = n.get("visible")
            fuera = vis is None
            recortado = bool(vis and (vis["w"] < n["w"] or vis["h"] < n["h"]))
            missing = n.get("missing_glyphs") or []
            fallback = n.get("fallback_glyphs") or []
            elementos.append(
                {
                    "captura": nombre,
                    "grupo": g,
                    "path": path,
                    "texto": txt,
                    "fuente": n.get("text_font"),
                    "x": n["x"],
                    "y": n["y"],
                    "w": n["w"],
                    "h": n["h"],
                    "natural_w": (n.get("text_natural") or {}).get("w"),
                    "natural_h": (n.get("text_natural") or {}).get("h"),
                    "texto_w": (n.get("text_box") or {}).get("w"),
                    "texto_h": (n.get("text_box") or {}).get("h"),
                    "overflow": int(bool(n.get("text_overflow"))),
                    "truncado": int(bool(n.get("text_truncated"))),
                    "missing": ";".join(missing),
                    "fallback": ";".join(fallback),
                    "oculto": int(bool(n.get("hidden"))),
                    "opa": n.get("opa"),
                    "clip_recorta": int(recortado),
                    "fuera_de_pantalla": int(fuera),
                    "visible": json.dumps(vis, sort_keys=True),
                }
            )
            r["labels"] += 1
            r["overflow"] += int(bool(n.get("text_overflow")))
            r["truncated"] += int(bool(n.get("text_truncated")))
            r["missing"] += len(missing)
            r["fallback"] += len(fallback)
            r["clip"] += int(recortado)
            r["fuera_de_pantalla"] += int(fuera)
            if txt and not vis:
                r["invisibles_con_texto"] += 1
            if txt and txt != "Text" and vis:
                textos.append((path, txt, vis, n.get("text_font")))

        # solapamientos entre cajas de texto visibles, sin contar contencion total
        for i in range(len(textos)):
            for j in range(i + 1, len(textos)):
                pa, ta, va, fa = textos[i]
                pb, tb, vb, fb = textos[j]
                area = intersect(va, vb)
                if area == 0:
                    continue
                a = va["w"] * va["h"]
                b = vb["w"] * vb["h"]
                if area >= min(a, b):
                    # contencion total: una etiqueta dentro de otra (p. ej. el
                    # valor del reloj dentro de su panel) no es un solape de
                    # glifos.
                    continue
                if ta == tb:
                    continue
                solapes.append(
                    {
                        "captura": nombre,
                        "grupo": g,
                        "area_px": area,
                        "a_path": pa,
                        "a_texto": ta,
                        "a_fuente": fa,
                        "a_box": f"{va['x']},{va['y']},{va['w']},{va['h']}",
                        "b_path": pb,
                        "b_texto": tb,
                        "b_fuente": fb,
                        "b_box": f"{vb['x']},{vb['y']},{vb['w']},{vb['h']}",
                    }
                )
                r["solapes"] += 1

        # hechos de alineacion que el contrato pide mirar (no interpretacion)
        por_texto = {t: (v, f) for _p, t, v, f in textos}
        for etiqueta in ("VFO-A", "SPL-A", "USB", "AUTO", "P/A", "ATU1"):
            if etiqueta in por_texto:
                v, f = por_texto[etiqueta]
                alineacion.append(
                    {
                        "captura": nombre,
                        "hecho": f"barra-info:{etiqueta}",
                        "x": v["x"],
                        "y": v["y"],
                        "w": v["w"],
                        "h": v["h"],
                        "fuente": f,
                    }
                )
                break
        for etiqueta in ("00:00:00",):
            if etiqueta in por_texto:
                v, f = por_texto[etiqueta]
                alineacion.append(
                    {
                        "captura": nombre,
                        "hecho": "reloj:hora",
                        "x": v["x"],
                        "y": v["y"],
                        "w": v["w"],
                        "h": v["h"],
                        "fuente": f,
                    }
                )
        for etiqueta in ("76%", "--%", "0%", "100%", "45%", "82%", "88%"):
            if etiqueta in por_texto:
                v, f = por_texto[etiqueta]
                alineacion.append(
                    {
                        "captura": nombre,
                        "hecho": "reloj:porcentaje",
                        "x": v["x"],
                        "y": v["y"],
                        "w": v["w"],
                        "h": v["h"],
                        "fuente": f,
                    }
                )
                break

        hallazgos.append(
            {
                "captura": nombre,
                "grupo": g,
                "instrumentado": 1,
                "labels": r["labels"],
                "overflow": sum(1 for e in elementos if e["captura"] == nombre and e["overflow"]),
                "truncado": sum(1 for e in elementos if e["captura"] == nombre and e["truncado"]),
                "missing": sum(1 for e in elementos if e["captura"] == nombre and e["missing"]),
                "clip": sum(1 for e in elementos if e["captura"] == nombre and e["clip_recorta"]),
                "solapes": sum(1 for s in solapes if s["captura"] == nombre),
            }
        )

    def escribir(nombre: str, filas: list[dict]) -> None:
        destino = MED / nombre
        campos = list(filas[0].keys()) if filas else ["vacio"]
        with destino.open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=campos)
            w.writeheader()
            w.writerows(filas)
        print(f"  {destino.name}: {len(filas)} filas")

    escribir("elementos.csv", elementos)
    escribir("hallazgos.csv", hallazgos)
    escribir("solapamientos.csv", solapes)
    escribir("alineacion.csv", alineacion)
    escribir("cobertura.csv", cobertura)
    escribir("resumen.csv", [{"grupo": g, **v} for g, v in sorted(resumen.items())])

    print("\n  resumen por grupo:")
    for g, v in sorted(resumen.items()):
        print(
            f"    {g:<12} capturas={v['capturas']:>3} labels={v['labels']:>4} "
            f"overflow={v['overflow']} truncated={v['truncated']} "
            f"missing={v['missing']} clip={v['clip']} solapes={v['solapes']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
