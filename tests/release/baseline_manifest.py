#!/usr/bin/env python3
"""Manifiesto de identidad y baseline reproducible del ROM X6200 (issue #222, epica #216).

El dossier #194 dejo dos huecos de evidencia que este manifiesto cierra: F-13 (no hay
corrida de CI sobre el SHA candidato exacto) y la falta de un baseline reproducible con
la identidad completa del par GUI + Buildroot y de los artefactos. El manifiesto es
**solo lectura**: no compila, no despliega, no toca la radio ni recursos de otras
corridas. Cuando algo no esta disponible lo declara `PENDING` con la razon, nunca lo
omite (un campo vacio se lee como "0" y eso seria una mentira).

Campos del contrato (issue #222 §7): `GUI_SHA`, `Buildroot_SHA`, `LVGL_gitlink`,
`fuente-md5`, GUI `md5`/`sha256`, hashes de `sdcard`/`rootfs`, `QEMU_CPU`,
imagen/contenedor y `seed`. Los hashes de artefacto se toman de los archivos que se le
pasen; los que no esten quedan `PENDING` con su ruta esperada.

Uso:
    tests/release/baseline_manifest.py                    # JSON del arbol actual
    tests/release/baseline_manifest.py --txt              # lo mismo, para leer
    tests/release/baseline_manifest.py --verify           # corre 2x y compara (reproducible)
    WT=<worktree> BR=<clon Buildroot> OUT=<salida Buildroot> \\
        ARTEFACTOS=<dir con sdcard.img/rootfs.tar> \\
        tests/release/baseline_manifest.py > baseline.json

`--verify` es la parte "reproducible" del contrato: corre el manifiesto dos veces sobre
el mismo arbol y falla (exit 1) si cambia algun campo determinista. Un manifiesto que
cambia entre dos corridas iguales no sirve como baseline.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Any

RAIZ = Path(__file__).resolve().parent.parent.parent

# Campos que NO tienen que cambiar entre dos corridas sobre el mismo arbol. La fecha
# queda afuera a proposito (es lo unico volatil del manifiesto).
VOLATILES = {"fecha_utc"}


def sh(comando: list[str], cwd: Path | None = None) -> str:
    try:
        salida = subprocess.run(comando, cwd=cwd, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return salida.stdout.strip() if salida.returncode == 0 else ""


def sh_pipe(comando: str, cwd: Path | None = None) -> str:
    """Un pipeline de shell, para lo que necesita `strings | grep`."""
    return sh(["sh", "-c", comando], cwd=cwd)


def sello_embebido(binario: Path) -> str:
    """El `fuente-md5` que el binario embebe al arrancar (una linea de 32 hex).

    CMake define `X6200_FUENTE_MD5` con el sello de `tools/fuente-md5.sh`, asi que el
    literal queda como una cadena propia de 32 hex dentro del ELF (issue #165). Un
    binario anterior a #165 no la tiene: se declara PENDING, no se inventa.
    """
    sello = sh_pipe(f"strings -a {shlex.quote(str(binario))} | grep -xE '[0-9a-f]{{32}}' | head -1")
    return sello or "PENDING: el binario no tiene sello de fuente de 32 hex (build sin #165)"


def hash_archivo(ruta: Path, algoritmo: str) -> str:
    if not ruta.is_file():
        return ""
    h = hashlib.new(algoritmo)
    with ruta.open("rb") as fh:
        for bloque in iter(lambda: fh.read(1 << 20), b""):
            h.update(bloque)
    return h.hexdigest()


def identidad_gui(wt: Path) -> dict[str, Any]:
    fuente = sh(["sh", str(wt / "tools" / "fuente-md5.sh"), str(wt)])
    sucio = sh(["git", "-C", str(wt), "status", "--porcelain", "--", "src", "lvgl/src"])
    gitlink = sh(["git", "-C", str(wt), "ls-tree", "HEAD", "lvgl"]).split()
    return {
        "sha": sh(["git", "-C", str(wt), "rev-parse", "HEAD"]) or "PENDING: no es un checkout git",
        "rama": sh(["git", "-C", str(wt), "rev-parse", "--abbrev-ref", "HEAD"]) or "PENDING",
        "fuente_md5": fuente or "PENDING: falta tools/fuente-md5.sh",
        "archivos_sucios_en_src": len(sucio.splitlines()),
        "lvgl_gitlink": gitlink[2] if len(gitlink) > 2 else "PENDING",
        "lvgl_describe": sh(["git", "-C", str(wt / "lvgl"), "describe", "--tags"]) or "PENDING",
    }


def identidad_buildroot(br: Path | None) -> dict[str, Any]:
    if br is None or not br.is_dir():
        return {"estado": "PENDING: sin clon de Buildroot (pasar BR=<clon>)"}
    gitlink = sh(["git", "-C", str(br), "ls-tree", "HEAD", "buildroot"]).split()
    return {
        "sha": sh(["git", "-C", str(br), "rev-parse", "HEAD"]) or "PENDING",
        "rama": sh(["git", "-C", str(br), "rev-parse", "--abbrev-ref", "HEAD"]) or "PENDING",
        "archivos_sucios": len(sh(["git", "-C", str(br), "status", "--porcelain"]).splitlines()),
        "buildroot_gitlink": gitlink[2] if len(gitlink) > 2 else "PENDING",
    }


def artefactos(out: Path | None, binario: Path | None, art: Path | None) -> dict[str, Any]:
    """Hashes del binario de la GUI y de la imagen. Lo que falta queda PENDING con la ruta."""
    datos: dict[str, Any] = {}

    gui = binario
    if gui is None and out is not None:
        candidato = out / "target" / "usr" / "sbin" / "x6200_gui"
        gui = candidato if candidato.is_file() else None
    if gui is not None and gui.is_file():
        datos["gui"] = {
            "ruta": str(gui),
            "bytes": gui.stat().st_size,
            "md5": hash_archivo(gui, "md5"),
            "sha256": hash_archivo(gui, "sha256"),
            "sello_embebido": sello_embebido(gui),
        }
    else:
        datos["gui"] = {
            "estado": "PENDING: sin binario de la GUI (pasar OUT=<salida Buildroot> o BIN=<ruta>)"
        }

    if art is not None and art.is_dir():
        for nombre in ("sdcard.img", "rootfs.tar", "rootfs.ext4", "boot.vfat"):
            ruta = art / nombre
            if ruta.is_file():
                datos[nombre] = {
                    "ruta": str(ruta),
                    "bytes": ruta.stat().st_size,
                    "sha256": hash_archivo(ruta, "sha256"),
                }
            else:
                datos[nombre] = {"estado": f"PENDING: no existe {ruta}"}
    else:
        datos["imagenes"] = {
            "estado": "PENDING: sin directorio de artefactos (pasar ARTEFACTOS=<dir>)"
        }
    return datos


def capa_qemu() -> dict[str, Any]:
    cpu = os.environ.get("QEMU_CPU", "cortex-a7")
    imagen = os.environ.get("IMAGEN", "x6200-buildroot")
    contenedor = os.environ.get("CONT", "x6200-next222-qemu")
    seed = os.environ.get("SEED", "")
    return {
        "qemu_cpu": cpu,
        "imagen": imagen,
        "contenedor": contenedor,
        "volumen": os.environ.get("VOL", "x6200-next222-build"),
        "seed": seed
        or "PENDING: sin seed de la corrida (las fixtures del arnes lo fijan por escenario)",
    }


def inventario(wt: Path) -> dict[str, Any]:
    """Dos conteos baratos y deterministas que anclan el baseline al arbol real."""
    fixtures = sorted((wt / "tools" / "ui_harness" / "fixtures").glob("*.scn"))
    inertes = wt / "tests" / "qemu" / "dialog-coverage.csv"
    filas = 0
    if inertes.is_file():
        filas = sum(
            1
            for linea in inertes.read_text().splitlines()
            if (
                linea.strip()
                and not linea.lstrip().startswith("#")
                and not linea.startswith("dialog,")
            )
        )
    return {
        "fixtures_host": len(fixtures),
        "dialogos_inertes": filas,
        "capturas_qemu_contrato": "3 rondas x 26 apps (tests/qemu/run_apps_qemu.sh)",
    }


def manifiesto() -> dict[str, Any]:
    wt = Path(os.environ.get("WT", RAIZ)).resolve()
    br_env = os.environ.get("BR", "")
    out_env = os.environ.get("OUT", "")
    art_env = os.environ.get("ARTEFACTOS", "")
    bin_env = os.environ.get("BIN", "")
    return {
        "schema": 1,
        "contrato": "iariap/x6200_gui#222 (epica #216)",
        "fecha_utc": sh(["date", "-u", "+%Y-%m-%dT%H:%M:%SZ"]) or "PENDING",
        "gui": identidad_gui(wt),
        "buildroot": identidad_buildroot(Path(br_env).resolve() if br_env else None),
        "artefactos": artefactos(
            Path(out_env).resolve() if out_env else None,
            Path(bin_env).resolve() if bin_env else None,
            Path(art_env).resolve() if art_env else None,
        ),
        "capa_qemu": capa_qemu(),
        "inventario": inventario(wt),
        "capas": {
            "HOST": "arnes de UI en x86 (sintetico)",
            "QEMU_USER": "binario ARM real en chroot privado; kernel del host",
            "HARDWARE": "PENDING_HARDWARE (pipeline fisico separado)",
        },
    }


def determinista(documento: dict[str, Any]) -> dict[str, Any]:
    return {k: v for k, v in documento.items() if k not in VOLATILES}


def txt(documento: dict[str, Any]) -> str:
    lineas = [f"== baseline X6200 ({documento['fecha_utc']}) =="]
    gui = documento["gui"]
    lineas += [
        f"GUI_SHA          {gui['sha']}  ({gui['rama']})",
        f"fuente-md5       {gui['fuente_md5']}  "
        f"({gui['archivos_sucios_en_src']} archivos sucios en src/)",
        f"LVGL_gitlink     {gui['lvgl_gitlink']}  ({gui['lvgl_describe']})",
    ]
    br = documento["buildroot"]
    lineas.append(
        f"Buildroot_SHA    {br.get('sha', br.get('estado', 'PENDING'))}"
        + (f"  ({br.get('rama')})" if br.get("rama") else "")
    )
    art = documento["artefactos"]
    if "gui" in art:
        g = art["gui"]
        if "estado" in g:
            lineas.append(f"GUI binario      {g['estado']}")
        else:
            lineas.append(
                f"GUI binario      {g.get('md5', '')} / {g.get('sha256', '')}  "
                f"({g.get('bytes', '')} B)"
            )
    for nombre in ("sdcard.img", "rootfs.tar", "rootfs.ext4", "boot.vfat"):
        if nombre in art:
            d = art[nombre]
            lineas.append(f"{nombre:16} {d.get('sha256', d.get('estado', ''))}")
    if "imagenes" in art:
        lineas.append(f"imagenes         {art['imagenes']['estado']}")
    q = documento["capa_qemu"]
    lineas.append(
        f"QEMU_CPU         {q['qemu_cpu']}   imagen {q['imagen']}   contenedor {q['contenedor']}"
    )
    lineas.append(f"seed             {q['seed']}")
    inv = documento["inventario"]
    lineas.append(
        f"inventario       {inv['fixtures_host']} fixtures de host, "
        f"{inv['dialogos_inertes']} dialogos inertes, "
        f"{inv['capturas_qemu_contrato']}"
    )
    return "\n".join(lineas)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--txt", action="store_true", help="salida legible en vez de JSON")
    parser.add_argument(
        "--verify", action="store_true", help="corre 2x y compara los campos deterministas"
    )
    args = parser.parse_args(argv)

    documento = manifiesto()

    if args.verify:
        segundo = determinista(manifiesto())
        if determinista(documento) != segundo:
            print(
                "FALLA: el manifiesto no es reproducible "
                "(dos corridas sobre el mismo arbol difieren)"
            )
            for clave in sorted(determinista(documento)):
                if determinista(documento)[clave] != segundo[clave]:
                    print(f"  - {clave}: {determinista(documento)[clave]!r} != {segundo[clave]!r}")
            return 1
        print("  OK: el manifiesto es reproducible (dos corridas sobre el mismo arbol coinciden)")
        return 0

    print(txt(documento) if args.txt else json.dumps(documento, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
