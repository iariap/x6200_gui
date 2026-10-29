#!/usr/bin/env python3
"""Datos del barrido QEMU de las 26 apps reales (issue #222, epica #216).

Hermano de `tools/barrido_apps.py` (el del banco del owner), pero con el contrato
del issue #222: 3 rondas completas de las entradas de
`remote_server.cpp::app_names` (sin `off`), con la permanencia, la vuelta a la
home y la estabilidad de la GUI como senales de primera clase. Este modulo tiene
lo que no conviene escribir en bash:

  - `estado`   parsea la linea `{"t":"state",...}` que contesta el socket;
  - `clasificar` decide el resultado de UNA app en UNA ronda a partir de lo que
    midio el driver (no de una sola senal: ver el docstring);
  - `cerrar`   junta las lineas JSON en `barrido.json`, `results.csv` e
    `index.txt` (PASS/FAIL/PARTIAL/BLOCKED por app y por ronda);
  - `selftest` prueba `clasificar` contra casos sinteticos. Es un test del
    REPORTEADOR, no evidencia de cobertura: las filas que produce llevan
    `capa: "SELFTEST"` y nunca se mezclan con las de una corrida real.

Reglas del contrato que estan codificadas aca (issue #222 §2 y §7):

  - una corrida fallida NO se descarta ni se repite para obtener verde: las 3
    rondas quedan y el peor resultado manda;
  - la GUI puede mostrar un error controlado cuando falta un servicio o un
    periferico (GPS, red, Bluetooth) sin que eso sea un defecto, pero SIGSEGV,
    TX no solicitada, estado persistido sin querer, fugas de fds/hilos o un
    paso que tarda mas de 5 s SI son fallo;
  - una app cuyo resultado depende de un periferico ausente se declara PARTIAL
    (foto real del mensaje), nunca PASS del dispositivo;
  - nunca PASS sin salida real: sin captura con dibujo, sin handshake o sin
    confirmacion de la app, el resultado es FAIL o BLOCKED, nunca PASS.

Uso:
    barrido_qemu.py captura <framebuffer> <png>       imprime "colores=N bytes=M"
    barrido_qemu.py estado <linea-json>               imprime el estado normalizado
    barrido_qemu.py clasificar <obs>                  imprime una linea JSON
    barrido_qemu.py cerrar <jsonl> <salida> [--meta k=v ...]
    barrido_qemu.py selftest                          corre los casos del reporteador

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

ANCHO, ALTO = 480, 800
# Un framebuffer dibujado tiene cientos de colores distintos; uno uniforme (la GUI no
# dibujo) tiene exactamente uno. Es el mismo piso que el banco del owner.
MIN_COLORES = 4
# Un paso del barrido que tarda mas que esto es un fallo (issue #222 §2).
TECHO_PASO_S = 5.0
# Crecimiento tolerado de fds/hilos entre la primera y la ultima ronda. El firmware
# abre y cierra los recursos de cada app al abrirla/cerrarla; un crecimiento sostenido
# es una fuga. El margen cubre el ruido de LVGL/socket, no una fuga real.
TECHO_FDS = 16
TECHO_HILOS = 4

# Las apps del menu: remote_server.cpp::app_names[] sin "off" (FT4 es el alias de
# FT8 y se documenta, no se doble-cuenta codigo). Logs entro el 2026-10-27 (a78e39fb)
# y el contrato de #222 paso de 25 a 26 (tests/check_dialog_coverage.py lo fija).
APPS = [
    "FT8",
    "FT4",
    "JS8",
    "WSPR",
    "SSTV",
    "WeFax",
    "NavTex",
    "RTTY",
    "CW",
    "Field",
    "GPS",
    "Bands",
    "Settings",
    "BT",
    "Cluster",
    "CwTrainer",
    "Eq",
    "Status",
    "QTH",
    "Callsign",
    "Recorder",
    "WiFi",
    "SWRScan",
    "Propagation",
    "DFLEditor",
    "Logs",
]

# Apps cuyo resultado depende de un servicio o periferico que el chroot no tiene.
# El render es REAL y el mensaje controlado es evidencia; la funcion NO se valida,
# asi que la ronda es PARTIAL y nunca PASS del dispositivo.
PARTIAL_POR_PERIFERICO = {
    "GPS": "sin receptor GNSS en el chroot: se espera el mensaje controlado de la app",
    "WiFi": "sin NetworkManager ni adaptador en el chroot: se espera el estado controlado",
    "BT": "sin adaptador Bluetooth: se espera el estado controlado",
    "Cluster": "sin red: la columna de spots no se puede validar (solo el render)",
    "Field": "sin red (QRZ/POTA): la consulta no se puede validar (solo el render)",
    "Propagation": "sin spots de cluster ni NTP: el barrido de beacons no se puede validar",
    "WSPR": "la subida a WSPRnet necesita red: el render si, la subida no",
}

# Apps que abren por una tecla larga (no por el menu APP) o que no tienen boton en las
# paginas APP: el barrido por socket no las alcanza y quedan UNSUPPORTED para ESTE
# runner (no es un defecto del firmware).
FUERA_DEL_MENU = {
    "FT4": "alias de FT8: `app FT4` selecciona el protocolo FT4 sobre dialog_ft8",
}

# Gravedad: el peor resultado de las 3 rondas manda.
GRAVEDAD = {"PASS": 0, "PARTIAL": 1, "BLOCKED": 2, "FAIL": 3}


def leer_estado(linea: str) -> dict[str, Any]:
    """Normaliza la linea `{"t":"state",...}` del socket (o devuelve {} si no lo es)."""
    linea = linea.strip()
    inicio = linea.find("{")
    if inicio < 0:
        return {}
    try:
        datos = json.loads(linea[inicio:])
    except json.JSONDecodeError:
        return {}
    if datos.get("t") != "state":
        return {}
    return datos


def _flotante(obs: dict[str, str], clave: str) -> float:
    try:
        return float(obs.get(clave, "0") or "0")
    except ValueError:
        return 0.0


def _entero(obs: dict[str, str], clave: str) -> int:
    try:
        return int(obs.get(clave, "0") or "0")
    except ValueError:
        return 0


def clasificar(obs: dict[str, str]) -> dict[str, Any]:
    """El resultado de una app en una ronda a partir de lo medido por el driver.

    No mira una sola senal. Orden de decision:

      1. si el banco no estaba arriba o el socket no contesto, es BLOCKED (no se
         midio nada: no es un fallo de la app ni un pase);
      2. crash/CRASHLOG, TX no solicitada, fuga de fds/hilos, estado persistido
         cambiado o un paso de mas de 5 s son FAIL, siempre;
      3. la app pedida tiene que quedar abierta (`app` del estado == pedida, o la
         excepcion documentada) y el cierre tiene que devolver la home: si no,
         FAIL;
      4. si todo eso esta bien pero la app necesita un periferico/servicio que el
         chroot no tiene, es PARTIAL (render real, funcion no validada);
      5. recien ahi PASS.
    """
    app = obs.get("app", "")
    ronda = _entero(obs, "ronda") or 1
    contenedor = obs.get("contenedor") == "si"
    handshake = obs.get("handshake") == "si"

    resultado: dict[str, Any] = {
        "app": app,
        "ronda": ronda,
        "app_estado": obs.get("app_estado", ""),
        "segundos": _flotante(obs, "segundos"),
        "colores": _entero(obs, "colores"),
        "captura": obs.get("captura", ""),
        "log_archivo": obs.get("log_archivo", ""),
        "capa": obs.get("capa", "QEMU_USER"),
        "home_px": obs.get("home_px", ""),
        "motivo": "",
        "checks": [],
    }

    def check(nombre: str, ok: bool, detalle: str = "") -> bool:
        resultado["checks"].append({"check": nombre, "ok": ok, "detalle": detalle})
        return ok

    if not contenedor or not handshake:
        resultado["resultado"] = "BLOCKED"
        resultado["motivo"] = (
            "BLOCKED: el banco no estaba arriba o el socket no contesto el handshake "
            f"(contenedor={obs.get('contenedor', '')}, handshake={obs.get('handshake', '')})"
        )
        return resultado

    fallos: list[str] = []

    if obs.get("crash") == "si":
        fallos.append(f"crash: {obs.get('crash_senal', 'CRASHLOG')}")
    if obs.get("sigsegv") == "si":
        fallos.append("SIGSEGV")
    if obs.get("tx") == "si":
        fallos.append("TX no solicitada (el barrido solo abre apps en RX)")
    if obs.get("timeout") == "si":
        fallos.append(f"un paso tardo mas de {TECHO_PASO_S:g} s")

    pid_antes, pid_despues = obs.get("pid_antes", ""), obs.get("pid_despues", "")
    if pid_antes and pid_despues and pid_antes != pid_despues:
        fallos.append(f"el PID de la GUI cambio durante la app ({pid_antes} -> {pid_despues})")
    did_antes, did_despues = obs.get("did_antes", ""), obs.get("did_despues", "")
    if did_antes and did_despues and did_antes != did_despues:
        fallos.append(f"el boot_id cambio ({did_antes} -> {did_despues})")
    if (
        obs.get("logrev_antes")
        and obs.get("logrev_despues")
        and obs["logrev_antes"] != obs["logrev_despues"]
    ):
        fallos.append(
            f"el log de la GUI cambio (logrev {obs['logrev_antes']} -> {obs['logrev_despues']})"
        )
    for campo in ("freq", "mode", "band"):
        antes, despues = obs.get(f"{campo}_antes", ""), obs.get(f"{campo}_despues", "")
        if antes and despues and antes != despues:
            fallos.append(f"estado persistido cambiado sin querer: {campo} {antes} -> {despues}")

    fds = _entero(obs, "fds_crecidos")
    hilos = _entero(obs, "hilos_crecidos")
    if fds > TECHO_FDS:
        fallos.append(f"fuga de fds (+{fds} entre la primera y la ultima ronda)")
    if hilos > TECHO_HILOS:
        fallos.append(f"fuga de hilos (+{hilos})")

    esperada = obs.get("app_esperada", app)
    if not check(
        "app-abrio", resultado["app_estado"] == esperada, f"{resultado['app_estado']} != {esperada}"
    ):
        fallos.append(f"la app pedida no abrio (app={resultado['app_estado']!r})")
    # La home se confirma por el estado (`app` vacio despues de `app off`), no por la
    # huella del framebuffer: el reloj de la barra cambia de segundo en segundo, asi que
    # una huella distinta NO es un fallo (se registra como `home_px` informativo).
    cerrada = obs.get("app_cerrada", "")
    if not check("home-restaurada", cerrada == "", f"app={cerrada!r} despues de `app off`"):
        fallos.append(f"`app off` no devolvio la home (quedo app={cerrada!r})")
    if not check("dibujo", resultado["colores"] >= MIN_COLORES, f"colores={resultado['colores']}"):
        fallos.append("la captura quedo uniforme (la GUI no dibujo)")

    if fallos:
        resultado["resultado"] = "FAIL"
        resultado["motivo"] = "FAIL: " + "; ".join(fallos)
        return resultado

    if app in PARTIAL_POR_PERIFERICO:
        resultado["resultado"] = "PARTIAL"
        resultado["motivo"] = f"PARTIAL: {PARTIAL_POR_PERIFERICO[app]}"
        return resultado

    resultado["resultado"] = "PASS"
    resultado["motivo"] = "PASS: app abierta, captura real, home restaurada y GUI estable"
    return resultado


def huella(ruta: Path) -> str:
    """El md5 del framebuffer: dice si la pantalla volvio a la home."""
    return hashlib.md5(ruta.read_bytes()).hexdigest()


def capturar(ruta_fb: Path, ruta_png: Path) -> tuple[int, int]:
    """Rota el framebuffer 270 grados como la radio y guarda el PNG (devuelve colores, bytes)."""
    from PIL import Image  # solo lo necesita este camino

    raw = ruta_fb.read_bytes()
    minimo = ANCHO * ALTO * 4
    if len(raw) < minimo:
        raise ValueError(
            f"el framebuffer es mas chico de lo esperado ({len(raw)} < {minimo} bytes)"
        )

    img = Image.new("RGB", (ANCHO, ALTO))
    px = img.load()
    if px is None:  # el stub de Pillow tipa `load()` como opcional
        raise ValueError("no pude acceder a los pixeles de la imagen")
    for y in range(ALTO):
        fila = y * ANCHO
        for x in range(ANCHO):
            i = (fila + x) * 4
            px[x, y] = (raw[i + 2], raw[i + 1], raw[i])

    ruta_png.parent.mkdir(parents=True, exist_ok=True)
    img.transpose(Image.Transpose.ROTATE_270).save(ruta_png)
    colores = len(img.getcolors(maxcolors=1 << 20) or [])
    return colores, ruta_png.stat().st_size


def cerrar(jsonl: Path, salida: Path, metas: list[str]) -> dict[str, Any]:
    """Junta las lineas JSON en barrido.json, results.csv e index.txt."""
    filas = [json.loads(linea) for linea in jsonl.read_text().splitlines() if linea.strip()]
    salida.mkdir(parents=True, exist_ok=True)

    # Por app: el peor resultado de sus rondas manda (una corrida fallida no se descarta).
    por_app: dict[str, dict[str, Any]] = {}
    for fila in filas:
        app = fila.get("app", "?")
        actual = por_app.get(app)
        if actual is None or GRAVEDAD[fila["resultado"]] > GRAVEDAD[actual["resultado"]]:
            por_app[app] = fila

    rondas = sorted({f["ronda"] for f in filas})
    meta = dict(m.split("=", 1) for m in metas if "=" in m)

    documento = {
        "capa": "QEMU_USER",
        "contrato": "iariap/x6200_gui#222",
        "meta": meta,
        "rondas": rondas,
        "apps_solicitadas": len(por_app),
        "resumen": {
            estado: sum(1 for f in por_app.values() if f["resultado"] == estado)
            for estado in ("PASS", "PARTIAL", "FAIL", "BLOCKED")
        },
        "apps": {app: por_app[app] for app in por_app},
        "rondas_detalle": filas,
    }
    (salida / "barrido.json").write_text(json.dumps(documento, indent=2, ensure_ascii=False) + "\n")

    with (salida / "results.csv").open("w", newline="") as fh:
        escritor = csv.writer(fh)
        escritor.writerow(
            ["app", "ronda", "resultado", "app_estado", "colores", "segundos", "captura", "motivo"]
        )
        for fila in filas:
            escritor.writerow(
                [
                    fila.get("app", ""),
                    fila.get("ronda", ""),
                    fila.get("resultado", ""),
                    fila.get("app_estado", ""),
                    fila.get("colores", ""),
                    fila.get("segundos", ""),
                    fila.get("captura", ""),
                    fila.get("motivo", ""),
                ]
            )

    with (salida / "index.txt").open("w") as fh:
        for fila in filas:
            captura = fila.get("captura") or "-"
            fh.write(
                f"{fila.get('app', '?'):12} ronda {fila.get('ronda', '?'):>2}  "
                f"{fila.get('resultado', '?'):7}  {captura}\n"
            )

    return documento


# --- selftest: el reporteador, no la cobertura -------------------------------------------

BASE: dict[str, str] = {
    "contenedor": "si",
    "handshake": "si",
    "app_estado": "Bands",
    "app_esperada": "Bands",
    "app_cerrada": "",
    "home_px": "igual",
    "colores": "812",
    "ronda": "1",
    "segundos": "0.4",
}


def _caso(nombre: str, cambios: dict[str, str], esperado: str) -> list[str]:
    obs = dict(BASE)
    obs.update(cambios)
    obtenido = clasificar(obs)["resultado"]
    if obtenido == esperado:
        return []
    return [f"{nombre}: se esperaba {esperado} y dio {obtenido}"]


def selftest() -> int:
    """Casos deterministas del clasificador. NO es evidencia de cobertura QEMU."""
    fallos: list[str] = []
    fallos += _caso("limpio", {}, "PASS")
    fallos += _caso("sin-handshake", {"handshake": "no"}, "BLOCKED")
    fallos += _caso("sin-contenedor", {"contenedor": "no"}, "BLOCKED")
    fallos += _caso("crash", {"crash": "si", "crash_senal": "CRASHLOG 1"}, "FAIL")
    fallos += _caso("sigsegv", {"sigsegv": "si"}, "FAIL")
    fallos += _caso("tx-no-solicitada", {"tx": "si"}, "FAIL")
    fallos += _caso("timeout", {"timeout": "si"}, "FAIL")
    fallos += _caso("no-abrio", {"app_estado": ""}, "FAIL")
    fallos += _caso("home-no-vuelve", {"app_cerrada": "Bands"}, "FAIL")
    fallos += _caso("home-px-distinto-no-falla", {"home_px": "distinto"}, "PASS")
    fallos += _caso("sin-dibujo", {"colores": "1"}, "FAIL")
    fallos += _caso("pid-cambia", {"pid_antes": "100", "pid_despues": "101"}, "FAIL")
    fallos += _caso("did-cambia", {"did_antes": "d1", "did_despues": "d2"}, "FAIL")
    fallos += _caso("logrev-cambia", {"logrev_antes": "4", "logrev_despues": "5"}, "FAIL")
    fallos += _caso("freq-cambia", {"freq_antes": "14074000", "freq_despues": "14074010"}, "FAIL")
    fallos += _caso("fuga-fds", {"fds_crecidos": "40"}, "FAIL")
    fallos += _caso("fuga-hilos", {"hilos_crecidos": "9"}, "FAIL")
    fallos += _caso(
        "gps-periferico", {"app": "GPS", "app_estado": "GPS", "app_esperada": "GPS"}, "PARTIAL"
    )
    fallos += _caso(
        "gps-con-crash",
        {"app": "GPS", "app_estado": "GPS", "app_esperada": "GPS", "crash": "si"},
        "FAIL",
    )

    if fallos:
        print("selftest FAIL")
        for fallo in fallos:
            print(f"  - {fallo}")
        return 1
    print("selftest OK: 19 casos del reporteador (capa SELFTEST, no cobertura)")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_cap = sub.add_parser("captura")
    p_cap.add_argument("framebuffer")
    p_cap.add_argument("png")

    p_est = sub.add_parser("estado")
    p_est.add_argument("linea")

    p_cla = sub.add_parser("clasificar")
    p_cla.add_argument("obs")

    p_cer = sub.add_parser("cerrar")
    p_cer.add_argument("jsonl")
    p_cer.add_argument("salida")
    p_cer.add_argument("--meta", action="append", default=[])

    sub.add_parser("selftest")

    args = parser.parse_args(argv)

    if args.cmd == "captura":
        colores, tam = capturar(Path(args.framebuffer), Path(args.png))
        print(f"colores={colores} bytes={tam}")
        return 0
    if args.cmd == "estado":
        print(json.dumps(leer_estado(args.linea), ensure_ascii=False))
        return 0
    if args.cmd == "clasificar":
        obs: dict[str, str] = {}
        for linea in Path(args.obs).read_text().splitlines():
            if not linea.strip() or linea.lstrip().startswith("#"):
                continue
            clave, _, valor = linea.partition("=")
            obs[clave.strip()] = valor.strip()
        print(json.dumps(clasificar(obs), ensure_ascii=False))
        return 0
    if args.cmd == "cerrar":
        documento = cerrar(Path(args.jsonl), Path(args.salida), args.meta)
        print(json.dumps(documento["resumen"], ensure_ascii=False))
        return 0
    if args.cmd == "selftest":
        return selftest()
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
