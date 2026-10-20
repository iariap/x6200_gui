#!/usr/bin/env python3
"""Comprueba que la identidad del build este fundada y sea verificable (issue #165).

El md5 del binario NO distingue commits: dos fuentes distintas pueden dar el mismo md5 (y el
mismo tamano) si Buildroot no repone la copia del repo. Medido en la corrida: el commit A
(1c599025) y el B (5f3c52fe, 7 archivos) dieron el MISMO `bd28b95d4426` compilando con el
`compila-gui.sh` viejo, porque el `rsync -au` de Buildroot (pkg-generic.mk) saltea un archivo
modificado si en la copia tiene un timestamp igual o mas nuevo; el archivo NUEVO (ui_set.h) si
se copiaba, y el QA miraba justo ese.

Este chequeo fija las dos mitades del arreglo, que no se ven en un render ni en el banco:

1. `tools/compila-gui.sh` repone la copia con CONTENIDO (`rsync -a --checksum`, sin `-u`) antes
   de compilar, y deja el sello `.fuente.md5` en la copia (lo que lee CMake).
2. El sello viaja al binario: `CMakeLists.txt` lo lee/calcula, `src/CMakeLists.txt` lo define en
   el target y `src/main.c` lo loguea al arrancar (una linea, al log de la GUI y a boot.log).
3. Los dos usan el MISMO algoritmo (`tools/fuente-md5.sh`, rutas relativas) para que el sello del
   build y el de la radio se puedan comparar. `tools/captura-banco.sh` tambien lo usa.

Se lee el texto de los archivos (la prosa explica el porque, el codigo es lo que se mira). El
hash de `fuente-md5.sh` se corre de verdad: dos veces sobre el repo tiene que dar lo mismo, y no
puede depender del directorio actual.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
fallos = []


def leer(rel: str) -> str:
    p = RAIZ / rel
    if not p.is_file():
        fallos.append(f"falta {rel}")
        return ""
    return p.read_text(errors="surrogateescape")


# --- 1. el hash compartido, reproducible y sin depender del cwd --------------------------------
script = RAIZ / "tools" / "fuente-md5.sh"
if not script.is_file():
    fallos.append("falta tools/fuente-md5.sh (el hash de las fuentes que comparten compila-gui y CMake)")
else:
    h1 = subprocess.run(["sh", str(script)], capture_output=True, text=True)
    h2 = subprocess.run(["sh", str(script), str(RAIZ)], capture_output=True, text=True)
    # desde otro cwd: el hash NO tiene que cambiar (rutas relativas al repo)
    h3 = subprocess.run(["sh", str(script)], capture_output=True, text=True, cwd="/tmp")
    salida = h1.stdout.strip()
    if h1.returncode != 0 or len(salida) != 32:
        fallos.append(f"tools/fuente-md5.sh no devolvio un md5 de 32 hex (rc={h1.returncode}, {salida!r})")
    if not (salida == h2.stdout.strip() == h3.stdout.strip()):
        fallos.append(
            "tools/fuente-md5.sh no es estable: cambia con el cwd o con el argumento "
            f"({h1.stdout.strip()!r}, {h2.stdout.strip()!r}, {h3.stdout.strip()!r})"
        )

# --- 2. compila-gui repone la copia por contenido y deja el sello ------------------------------
compila = leer("tools/compila-gui.sh")
if compila:
    if "fuente-md5.sh" not in compila:
        fallos.append("tools/compila-gui.sh no usa tools/fuente-md5.sh (tendria su propio hash)")
    if "--checksum" not in compila:
        fallos.append(
            "tools/compila-gui.sh no hace `rsync -a --checksum` antes de compilar: el `rsync -au` "
            "de Buildroot puede saltear los archivos modificados y compilar codigo viejo (issue #165)"
        )
    if ".fuente.md5" not in compila:
        fallos.append("tools/compila-gui.sh no deja el sello .fuente.md5 en la copia del buildroot")

# --- 3. el sello llega al binario --------------------------------------------------------------
cmake = leer("CMakeLists.txt")
if cmake and "X6200_FUENTE_MD5" not in cmake:
    fallos.append("CMakeLists.txt no define X6200_FUENTE_MD5 (la identidad no llega al binario)")
src_cmake = leer("src/CMakeLists.txt")
if src_cmake and not (
    "target_compile_definitions" in src_cmake and "X6200_FUENTE_MD5" in src_cmake
):
    fallos.append("src/CMakeLists.txt no aplica X6200_FUENTE_MD5 al target x6200_gui")

# --- 4. main.c lo loguea (una linea, al log y a boot.log) --------------------------------------
main = leer("src/main.c")
if main:
    if "X6200_FUENTE_MD5" not in main:
        fallos.append("src/main.c no usa X6200_FUENTE_MD5: la radio no puede decir que codigo corre")
    if "boot_identity" not in main:
        fallos.append("src/main.c no tiene boot_identity(): la identidad no se loguea al arrancar")
    if "/mnt/app_logs/boot.log" not in main:
        fallos.append("src/main.c no escribe la identidad en /mnt/app_logs/boot.log")

# --- 5. el banco usa el mismo hash (para poder comparar) ---------------------------------------
banco = leer("tools/captura-banco.sh")
if banco and "fuente-md5.sh" not in banco:
    fallos.append("tools/captura-banco.sh no usa tools/fuente-md5.sh: el hash del banco no seria comparable")

if fallos:
    for f in fallos:
        print(f"  FALLO: {f}")
    sys.exit(1)

print("  fuente-md5.sh: estable y sin depender del cwd")
print("  compila-gui.sh: repone la copia con --checksum y deja el sello")
print("  CMakeLists + src/main.c: el sello se embebe y se loguea al arrancar")
print("  captura-banco.sh: usa el mismo hash")
