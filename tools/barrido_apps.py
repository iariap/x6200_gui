#!/usr/bin/env python3
"""Datos del barrido de apps del banco (`tools/barrido-apps-banco.sh`).

El framebuffer del banco es `480x800x32 BGRA` (el mismo que rota `tools/captura-banco.sh`).
Este modulo tiene las tres piezas que no conviene escribir en bash: leer el framebuffer y
decir cuantos colores tiene (una pantalla dibujada tiene cientos; una uniforme, uno solo:
es el control de "no deja evidencia falsa"), armar el JSON del barrido y decidir el
resultado de cada app a partir de las senales que midio el driver.

`clasificar` es la unica autoridad sobre "viva": no mira una sola senal. Una app esta viva
solo si el contenedor sigue arriba, el socket contesta el handshake, el framebuffer dibujo
algo y el estado dice que la app abierta es la que se pidio. Si el proceso murio (no hay
contenedor), si el socket no contesta (colgada) o si la pantalla quedo uniforme (sin-dibujo),
la app falla. El orden importa: un crash medido manda sobre un handshake que todavia
contesta (el handler de `main.c` recien sale del proceso), y "no-abrio" evita dar por viva una
app que nunca llego a abrir (la pantalla de inicio tambien dibuja).

Uso:
    barrido_apps.py captura <framebuffer> <png>       imprime "colores=N bytes=M"
    barrido_apps.py huella <framebuffer>              imprime el md5 del framebuffer
    barrido_apps.py clasificar <obs>                  imprime una linea JSON
    barrido_apps.py cerrar <jsonl> <salida> [--meta k=v ...]

`obs` es un archivo de lineas `clave=valor` (una por linea) que escribe el driver; `cerrar`
junta las lineas JSON en el documento final, con el resumen por app y por corrida.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

ANCHO, ALTO = 480, 800
# Un framebuffer dibujado tiene cientos de colores distintos; uno uniforme (la GUI no dibujo:
# pantalla negra o de un solo color) tiene exactamente uno. Este es el piso de "dibujo algo".
MIN_COLORES = 4

# Gravedad de cada resultado: el peor intento manda (una app que murio una vez y vivio otra
# sigue contando como muerta, con los dos intentos en el JSON).
GRAVEDAD = {"viva": 0, "sin-dibujo": 1, "no-abrio": 2, "muerta": 3}


def leer_obs(ruta: Path) -> dict[str, str]:
    """Lee las lineas `clave=valor` del driver (ignora vacias y comentarios)."""
    obs: dict[str, str] = {}

    for linea in ruta.read_text().splitlines():
        if not linea.strip() or linea.lstrip().startswith("#"):
            continue
        clave, _, valor = linea.partition("=")
        obs[clave.strip()] = valor.strip()
    return obs


def clasificar(obs: dict[str, str]) -> dict[str, object]:
    """El resultado de una app a partir de lo medido (ver el docstring del modulo)."""
    contenedor = obs.get("contenedor") == "si"
    handshake = obs.get("handshake") == "si"
    colores = int(obs.get("colores", "0") or "0")
    crash = obs.get("crash") == "si"
    app = obs.get("app", "")
    abierta = obs.get("app_estado", "")

    if not contenedor or not handshake or crash:
        resultado = "muerta"
    elif colores < MIN_COLORES:
        resultado = "sin-dibujo"
    elif abierta != app:
        # El estado dice que quedo abierta otra cosa (o nada): la app pedida no abrio. Sin esto,
        # una app que nunca abre se veria "viva" porque el fondo (la pantalla de inicio) dibuja.
        resultado = "no-abrio"
    else:
        resultado = "viva"

    return {
        "app": app,
        "app_estado": abierta,
        "intento": int(obs.get("intento", "1") or "1"),
        "resultado": resultado,
        "ok": resultado == "viva",
        "abre": obs.get("abre", ""),
        "dwell_s": float(obs.get("dwell_s", "0") or "0"),
        "segundos": float(obs.get("segundos", "0") or "0"),
        "contenedor": contenedor,
        "handshake": handshake,
        "colores": colores,
        "crash": crash,
        "crash_senal": obs.get("crash_senal", ""),
        "log_lineas": int(obs.get("log_lineas", "0") or "0"),
        "captura": obs.get("captura", ""),
        "log_archivo": obs.get("log_archivo", ""),
    }


def huella(ruta: Path) -> str:
    """El md5 del framebuffer: dice si la pantalla cambio entre dos capturas."""
    return hashlib.md5(ruta.read_bytes()).hexdigest()


def capturar(ruta_fb: Path, ruta_png: Path) -> tuple[int, int]:
    """Rota el framebuffer 270 grados como la radio y guarda el PNG; devuelve (colores, bytes)."""
    from PIL import Image  # solo lo necesita este camino

    raw = ruta_fb.read_bytes()
    minimo = ANCHO * ALTO * 4

    if len(raw) < minimo:
        raise ValueError(f"el framebuffer es mas chico de lo esperado ({len(raw)} < {minimo} bytes)")

    img = Image.new("RGB", (ANCHO, ALTO))
    px = img.load()

    for y in range(ALTO):
        fila = y * ANCHO
        for x in range(ANCHO):
            i = (fila + x) * 4
            px[x, y] = (raw[i + 2], raw[i + 1], raw[i])

    img.transpose(Image.ROTATE_270).save(ruta_png)
    colores = len(img.getcolors(maxcolors=1 << 20) or [])
    return colores, ruta_png.stat().st_size


def _peor(a: str, b: str) -> str:
    return a if GRAVEDAD.get(a, 9) >= GRAVEDAD.get(b, 9) else b


def cerrar(lineas: list[str], meta: dict[str, str]) -> dict[str, Any]:
    """Junta los intentos por app y arma el documento del barrido."""
    intentos = [json.loads(linea) for linea in lineas if linea.strip()]
    apps: dict[str, dict[str, Any]] = {}

    for intento in intentos:
        app = str(intento["app"])
        entrada = apps.setdefault(app, {"app": app, "resultado": "viva", "intentos": []})
        entrada["intentos"].append(intento)
        entrada["resultado"] = _peor(str(entrada["resultado"]), str(intento["resultado"]))

    orden = list(apps)
    vivas = [a for a in orden if apps[a]["resultado"] == "viva"]
    falladas = [a for a in orden if apps[a]["resultado"] != "viva"]

    return {
        "meta": meta,
        "apps": [apps[a] for a in orden],
        "resumen": {
            "apps": len(orden),
            "intentos": len(intentos),
            "vivas": len(vivas),
            "falladas": falladas,
        },
    }


def tabla(documento: dict[str, Any]) -> str:
    """La tabla del barrido para la consola (misma data que el JSON)."""
    filas = [
        f"  {'app':<10} {'resultado':<10} {'app_estado':<10} {'intentos':<8} {'colores':<8} {'segundos':<9}"
    ]
    for app in documento["apps"]:
        ultimo = app["intentos"][-1]
        filas.append(
            f"  {app['app']:<10} {app['resultado']:<10} {ultimo['app_estado']:<10} "
            f"{len(app['intentos']):<8d} {ultimo['colores']:<8d} {ultimo['segundos']:<9.1f}"
        )
    resumen = documento["resumen"]
    falladas = resumen["falladas"]
    detalle = (" (" + ", ".join(falladas) + ")") if falladas else ""
    filas.append(
        f"  {resumen['apps']} apps, {resumen['intentos']} intentos: {resumen['vivas']} vivas, "
        f"{len(falladas)} falladas{detalle}"
    )
    return "\n".join(filas)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="datos del barrido de apps del banco")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_cap = sub.add_parser("captura")
    p_cap.add_argument("framebuffer")
    p_cap.add_argument("png")

    p_hue = sub.add_parser("huella")
    p_hue.add_argument("framebuffer")

    p_cla = sub.add_parser("clasificar")
    p_cla.add_argument("obs")

    p_cer = sub.add_parser("cerrar")
    p_cer.add_argument("jsonl")
    p_cer.add_argument("salida")
    p_cer.add_argument("--meta", action="append", default=[], metavar="clave=valor")

    args = parser.parse_args(argv)

    if args.cmd == "captura":
        colores, bytes_ = capturar(Path(args.framebuffer), Path(args.png))
        print(f"colores={colores} bytes={bytes_}")
    elif args.cmd == "huella":
        print(huella(Path(args.framebuffer)))
    elif args.cmd == "clasificar":
        datos = clasificar(leer_obs(Path(args.obs)))
        print(json.dumps(datos, ensure_ascii=False, sort_keys=True))
        return 0 if datos["ok"] else 1
    elif args.cmd == "cerrar":
        meta = dict(item.split("=", 1) for item in args.meta)
        lineas = Path(args.jsonl).read_text().splitlines()
        documento = cerrar(lineas, meta)
        Path(args.salida).write_text(json.dumps(documento, ensure_ascii=False, indent=2) + "\n")
        print(tabla(documento))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
