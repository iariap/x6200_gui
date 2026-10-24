#!/usr/bin/env python3
"""Comparacion baseline (c90453fd) vs candidato (1845cb77) sobre la misma matriz.

El baseline se construye y captura con `construir.sh` / `capturar.sh` en un arbol
aparte (`/tmp/opencode/qa284-base`, SOLO_CORPUS=1), asi que las dos corridas
tienen exactamente las mismas 218 fixtures del repo. Este script compara:

  comparacion-png.csv        sha256 por fixture: que cambio de pixeles
  comparacion-metricas.csv   flushes / flushed_pixels (proxy determinista de costo)
  comparacion-contratos.csv  finding por fixture+regla: nuevo / resuelto / persiste
  comparacion-estructura.csv overflow / clip / glifos faltantes / solapes
  comparacion-geometria.csv  cajas del reloj, la bateria y la barra de info

Uso:
  python3 .../comparar.py --base /tmp/opencode/qa284-base/docs/evidencia/2026-10-23-ui-polish/final-qa

Nada de esto infiere legibilidad fisica ni costo en el Cortex-A7: es el render de
host y los contadores deterministas del arnes.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import statistics
from collections import defaultdict
from pathlib import Path

AQUI = Path(__file__).resolve().parent
MED = AQUI / "mediciones"


def leer_tsv(ruta: Path) -> dict[str, dict[str, str]]:
    filas = {}
    if not ruta.exists():
        return filas
    for linea in ruta.read_text().splitlines():
        f = linea.split("\t")
        if len(f) >= 5:
            filas[f[0]] = {"bin": f[1], "scn": f[2], "rc": f[3], "sha": f[4]}
    return filas


def leer_contratos(ruta: Path) -> dict[tuple[str, str], bool]:
    if not ruta.exists():
        return {}
    datos = json.loads(ruta.read_text())
    return {(f["captura"], f["regla"]): bool(f["ok"]) for f in datos["findings"]}


def leer_elementos(ruta: Path) -> dict[str, dict[str, int]]:
    """Contadores estructurales por captura, solo para etiquetas VISIBLES con
    texto real (se excluye el placeholder `Text` de los mocks inertes)."""
    if not ruta.exists():
        return {}
    out: dict[str, dict[str, int]] = defaultdict(
        lambda: {"overflow": 0, "clip": 0, "missing": 0, "solapes": 0, "labels": 0}
    )
    for r in csv.DictReader(ruta.open()):
        visible = r["visible"] not in ("null", "") and r["oculto"] == "0" and int(r["opa"] or 0) > 0
        if not visible or r["texto"] in ("", "Text"):
            continue
        c = out[r["captura"]]
        c["labels"] += 1
        c["overflow"] += int(r["overflow"])
        c["clip"] += int(r["clip_recorta"])
        c["missing"] += int(bool(r["missing"]))
    for r in csv.DictReader((ruta.parent / "solapamientos.csv").open()):
        out[r["captura"]]["solapes"] += 1
    return dict(out)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", type=Path, required=True)
    args = ap.parse_args(argv)
    base = args.base

    tsv_base = leer_tsv(base / "logs" / "capturas.tsv")
    tsv_act = leer_tsv(AQUI / "logs" / "capturas.tsv")
    comunes = sorted(set(tsv_base) & set(tsv_act))
    print(f"== {len(comunes)} fixtures comunes (base {len(tsv_base)}, actual {len(tsv_act)}) ==")

    # 1. pixeles
    png = []
    for n in comunes:
        png.append(
            {
                "fixture": n,
                "sha_base": tsv_base[n]["sha"],
                "sha_actual": tsv_act[n]["sha"],
                "cambia": int(tsv_base[n]["sha"] != tsv_act[n]["sha"]),
            }
        )

    # 2. metricas deterministas
    metricas = []
    deltas = defaultdict(list)
    for n in comunes:
        jb = json.loads((base / "json" / f"{n}.json").read_text(errors="surrogateescape"))
        ja = json.loads((AQUI / "json" / f"{n}.json").read_text(errors="surrogateescape"))
        mb, ma = jb.get("metrics", {}), ja.get("metrics", {})
        fb, fa = int(mb.get("flushes", 0)), int(ma.get("flushes", 0))
        pb, pa = int(mb.get("flushed_pixels", 0)), int(ma.get("flushed_pixels", 0))
        metricas.append(
            {
                "fixture": n,
                "flushes_base": fb,
                "flushes_actual": fa,
                "delta_flushes": fa - fb,
                "flushed_px_base": pb,
                "flushed_px_actual": pa,
                "delta_flushed_px": pa - pb,
                "delta_pct": round((pa - pb) * 100.0 / pb, 3) if pb else "",
            }
        )
        deltas["flushes"].append(fa - fb)
        deltas["flushed_px"].append(pa - pb)

    # 3. contratos
    cb = leer_contratos(base / "mediciones" / "contratos.json")
    ca = leer_contratos(AQUI / "mediciones" / "contratos.json")
    contratos = []
    for clave in sorted(set(cb) | set(ca)):
        fixture, regla = clave
        if fixture not in tsv_act:
            continue
        ob, oa = cb.get(clave), ca.get(clave)
        if ob is None and oa is False:
            estado = "nuevo"
        elif ob is False and oa is True:
            estado = "resuelto"
        elif ob is False and oa is False:
            estado = "persiste"
        elif ob is True and oa is False:
            estado = "regresion"
        else:
            continue
        contratos.append(
            {"fixture": fixture, "regla": regla, "base_ok": ob, "actual_ok": oa, "estado": estado}
        )

    # 4. estructura
    eb = leer_elementos(base / "mediciones" / "elementos.csv")
    ea = leer_elementos(AQUI / "mediciones" / "elementos.csv")
    estructura = []
    for n in comunes:
        b = eb.get(n, {})
        a = ea.get(n, {})
        if not b and not a:
            continue
        estructura.append(
            {
                "fixture": n,
                "labels_base": b.get("labels", 0),
                "labels_actual": a.get("labels", 0),
                "overflow_base": b.get("overflow", 0),
                "overflow_actual": a.get("overflow", 0),
                "clip_base": b.get("clip", 0),
                "clip_actual": a.get("clip", 0),
                "missing_base": b.get("missing", 0),
                "missing_actual": a.get("missing", 0),
                "solapes_base": b.get("solapes", 0),
                "solapes_actual": a.get("solapes", 0),
            }
        )

    # 5. geometria de la fila de estado del reloj (lo que toco #285)
    geometria = []
    for n in comunes:
        if not (
            n.startswith("main-")
            or n.startswith("home-")
            or n.startswith("net-")
            or n.startswith("gaps-net-")
            or n.startswith("finalqa-")
        ):
            continue
        ja = json.loads((AQUI / "json" / f"{n}.json").read_text(errors="surrogateescape"))
        jb = json.loads((base / "json" / f"{n}.json").read_text(errors="surrogateescape"))

        def cajas(d):
            out = {}

            def walk(node):
                if node.get("class") == "lv_label":
                    t = (node.get("text") or "").strip()
                    if (
                        t in ("VFO-A", "USB", "AUTO", "P/A", "ATU1")
                        or t.endswith("%")
                        or t.startswith("\uf1e6")
                        or t == "00:00:00"
                        or t in ("\uf1f1", "\uf1eb", "\uf241")
                    ):
                        out.setdefault(t, f"{node['x']},{node['y']},{node['w']},{node['h']}")
                for c in node.get("children") or []:
                    walk(c)

            for raiz in (d.get("inspection") or {}).get("nodes") or []:
                walk(raiz)
            return out

        gb, ga = cajas(jb), cajas(ja)
        for t in sorted(set(gb) | set(ga)):
            if gb.get(t) != ga.get(t):
                geometria.append(
                    {"fixture": n, "elemento": t, "caja_base": gb.get(t, ""), "caja_actual": ga.get(t, "")}
                )

    MED.mkdir(exist_ok=True)
    for nombre, filas in (
        ("comparacion-png.csv", png),
        ("comparacion-metricas.csv", metricas),
        ("comparacion-contratos.csv", contratos),
        ("comparacion-estructura.csv", estructura),
        ("comparacion-geometria.csv", geometria),
    ):
        with (MED / nombre).open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(filas[0].keys()) if filas else ["vacio"])
            w.writeheader()
            w.writerows(filas)
        print(f"  {nombre}: {len(filas)} filas")

    cambiados = sum(r["cambia"] for r in png)
    print(f"\n  PNG que cambian: {cambiados}/{len(png)}")
    for k, v in deltas.items():
        print(f"  delta {k}: suma={sum(v)} min={min(v)} max={max(v)} p50={statistics.median(v)}")
    for estado in ("nuevo", "regresion", "resuelto", "persiste"):
        filas = [c for c in contratos if c["estado"] == estado]
        print(f"  contratos {estado}: {len(filas)}")
        for f in filas[:10]:
            print(f"    {f['fixture']}: {f['regla']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
