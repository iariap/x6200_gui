#!/usr/bin/env python3
"""Chequea que el manual tenga una seccion por cada aplicacion del firmware.

Las apps salen del dispatcher de la GUI (`ACTION_APP_*` en `src/main_screen.c`, que es la lista
completa: el menu de apps, las acciones de Ajustes y el control remoto llegan todos ahi). Para cada
una se busca su seccion en `docs/manual/MANUAL.md`. Si una app nueva no tiene seccion, el chequeo
falla y dice cual.

Uso:  python3 tests/check_manual_apps.py
"""

import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
MAIN = RAIZ / "src" / "main_screen.c"
MANUAL = RAIZ / "docs" / "manual" / "MANUAL.md"

# Como se llama cada app en el manual (el dispatcher usa el nombre corto del codigo).
NOMBRE_EN_EL_MANUAL = {
    "ACTION_APP_FT8": "FT8",
    "ACTION_APP_JS8": "JS8",
    "ACTION_APP_WSPR": "WSPR",
    "ACTION_APP_SSTV": "SSTV",
    "ACTION_APP_WEFAX": "WeFax",
    "ACTION_APP_NAVTEX": "NavTex",
    "ACTION_APP_RTTY": "RTTY",
    "ACTION_APP_CW_KBD": "CW Keyboard",
    "ACTION_APP_CWTRAINER": "CW Trainer",
    "ACTION_APP_CLUSTER": "Cl\u00faster",
    "ACTION_APP_GPS": "GPS",
    "ACTION_APP_FIELD": "Field Ops",
    "ACTION_APP_BANDS": "Bandas",
    "ACTION_APP_RECORDER": "Recorder",
    "ACTION_APP_EQ": "EQ",
    "ACTION_APP_STATUS": "Status",
    "ACTION_APP_WIFI": "WiFi",
    "ACTION_APP_BT": "Bluetooth",
    "ACTION_APP_SWRSCAN": "SWR Scan",
    "ACTION_APP_CALLSIGN": "Callsign",
    "ACTION_APP_QTH": "QTH",
    "ACTION_APP_PROPAGATION": "Propagation",
    # El editor DFL (issue #173): se abre por la tecla larga DFL o por esta accion.
    # Estaba sin nombre mapeado, asi que el chequeo lo listaba como "revisar" y no
    # verificaba su seccion del manual; se documenta en §6.22 (issue #222).
    "ACTION_APP_DFL_EDITOR": "DFL editor",
    # App Logs (subida de QSO a QRZ/eQSL/Clublog): el feature la dejo sin seccion y
    # sin mapear, asi que este chequeo la imprimia como "revisar" y seguia en verde.
    # Se documenta en §6.25.
    "ACTION_APP_LOGS": "Logs",
    "ACTION_APP_SETTINGS": "Ajustes",
}


def apps_del_firmware():
    src = MAIN.read_text(encoding="utf-8", errors="ignore")
    return sorted(set(re.findall(r"ACTION_APP_[A-Z0-9_]+", src)))


def main():
    apps = apps_del_firmware()
    manual = MANUAL.read_text(encoding="utf-8", errors="ignore")
    secciones = re.findall(r"^### 6\.\d+ (.+)$", manual, re.M)  # las apps estan en la seccion 6
    titulos = list(secciones)

    faltan, desconocidas = [], []
    for app in apps:
        nombre = NOMBRE_EN_EL_MANUAL.get(app)
        if nombre is None:
            desconocidas.append(app)
            continue
        if not any(nombre.lower() in t.lower() for t in titulos):
            faltan.append(f"{app} -> '{nombre}'")

    print(f"  apps en el firmware: {len(apps)} | secciones de apps en el manual: {len(secciones)}")
    if desconocidas:
        print(f"  apps sin nombre mapeado (revisar el chequeo): {', '.join(desconocidas)}")
    if faltan:
        print("  SIN SECCION en el manual:")
        for f in faltan:
            print(f"    - {f}")
        return 1
    print(f"  todas las apps tienen su seccion ({len(apps)} apps)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
