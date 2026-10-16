#!/usr/bin/env python3
#
# Gate fail-closed: comprueba que un archivo es el FIRMWARE DEL PORT (x6200_gui)
# y no otro binario. Nace del issue #86: construir la ROM desde el `main` del
# remoto (fork/main = e1476ec, la linea original, cientos de commits atras)
# termina con exit 0 y deja un `x6200_gui` de 776.884 bytes que NO es el firmware;
# si alguien lo despliega, la radio queda sin GUI.
#
#   tools/verifica-firmware-port.py <archivo> [--min-bytes N]
#   tools/verifica-firmware-port.py --tar <rootfs.tar>   # valida el binario adentro
#   tools/verifica-firmware-port.py --config             # imprime las senales calibradas
#
# Senales (todas tienen que pasar; fail-closed = ante la duda, NO es el firmware):
#
#   1. ELF de la ISA del objetivo: ELF32, little-endian, EM_ARM (40), tipo
#      ET_EXEC o ET_DYN. Un binario de otra arquitectura o un no-ELF se rechaza.
#   2. Tamano >= PISO_BYTES. Calibrado con los binarios REALES del issue #86:
#        port  (sin stripped): 13.737.784 B  (volumen: qa-backup-antes-de-main-d9ccb7d4)
#        port  (stripped):     13.248.936 B  (usr/sbin/x6200_gui dentro de rootfs.tar)
#        main  (fork/main):       776.884 B  <- el que NO es el firmware
#      El piso (8.000.000 B) queda a ~60% del port mas chico y ~10x por encima del
#      de main: rechaza el de main con margen y no molesta al port.
#      NO se usa PIE vs non-PIE: medido con `file`/`readelf -h`, el binario del PORT
#      TAMBIEN es PIE (ET_DYN) — esa senal del issue #86 es FALSA (ver la tabla).
#   3. Marcas del port: cadenas de features que SOLO tiene el port (ausentes en el
#      arbol de main). Se exige MARCAS_REQUERIDAS de MARCAS_PORT:
#        /tmp/x6200_remote.sock   src/remote_server.cpp:92 (socket de control)
#        x6200-pota-spots         src/cluster/client.c:47
#        repos/iariap/x6200_gui   src/update_check.c:17
#      Las tres sobreviven al `strip` (medido sobre qa-backup-cb2da072).
#
# Uso desde los scripts de deploy/banco: si sale != 0, NO se despliega ni se corre.
#
# SPDX-License-Identifier: GPL-2.0-or-later
"""Gate fail-closed de firmware del port (ver el encabezado)."""

from __future__ import annotations

import argparse
import json
import struct
import sys
import tarfile
from pathlib import Path

# --- calibracion (issue #86, medido el 2026-10-15) --------------------------
PISO_BYTES = 8_000_000
TAMANO_PORT_STRIPPED = 13_248_936
TAMANO_PORT = 13_737_784
TAMANO_MAIN = 776_884

MARCAS_PORT = (
    b"/tmp/x6200_remote.sock",
    b"x6200-pota-spots",
    b"repos/iariap/x6200_gui",
)
MARCAS_REQUERIDAS = 2

ELF_MAGIC = b"\x7fELF"
ELFCLASS32 = 1
ELFDATA2LSB = 1
ET_EXEC = 2
ET_DYN = 3
EM_ARM = 40

BIN_EN_IMAGEN = "usr/sbin/x6200_gui"

OK = 0
NO_ES_FIRMWARE = 1
ERROR_DE_USO = 2


def revisar_elf(datos: bytes) -> str | None:
    """Devuelve el motivo del rechazo si `datos` no es un ELF32 ARM ejecutable."""
    if len(datos) < 52:
        return f"no es un ELF (solo {len(datos)} bytes, un ELF32 tiene >= 52)"
    if datos[:4] != ELF_MAGIC:
        return "no empieza con la magia ELF (\\x7fELF)"
    if datos[4] != ELFCLASS32:
        return f"no es ELF32 (EI_CLASS={datos[4]})"
    if datos[5] != ELFDATA2LSB:
        return f"no es little-endian (EI_DATA={datos[5]})"
    e_type, e_machine = struct.unpack_from("<HH", datos, 16)
    if e_machine != EM_ARM:
        return f"no es ARM (e_machine={e_machine}, se espera {EM_ARM})"
    if e_type not in (ET_EXEC, ET_DYN):
        return f"no es un ejecutable (e_type={e_type})"
    return None


def marcas_presentes(datos: bytes) -> list[bytes]:
    return [m for m in MARCAS_PORT if m in datos]


def verificar(datos: bytes, min_bytes: int) -> list[str]:
    """Lista de motivos por los que `datos` NO es el firmware del port (vacia = OK)."""
    motivos: list[str] = []
    problema_elf = revisar_elf(datos)
    if problema_elf is not None:
        motivos.append(problema_elf)
    if len(datos) < min_bytes:
        motivos.append(
            f"pesa {len(datos)} B, por debajo del piso {min_bytes} B "
            f"(el port pesa {TAMANO_PORT} B; el de main, {TAMANO_MAIN} B)"
        )
    presentes = marcas_presentes(datos)
    if len(presentes) < MARCAS_REQUERIDAS:
        faltan = [m.decode() for m in MARCAS_PORT if m not in presentes]
        motivos.append(
            f"tiene {len(presentes)} de {len(MARCAS_PORT)} marcas del port "
            f"(se piden {MARCAS_REQUERIDAS}); faltan: {', '.join(faltan)}"
        )
    return motivos


def leer_de_tar(archivo: Path) -> bytes:
    """El contenido de usr/sbin/x6200_gui dentro de una rootfs.tar."""
    with tarfile.open(archivo, "r:*") as tar:
        for miembro in tar.getmembers():
            if miembro.name.endswith(BIN_EN_IMAGEN) and miembro.isfile():
                extraido = tar.extractfile(miembro)
                if extraido is None:
                    break
                return extraido.read()
    raise LookupError(f"la imagen {archivo} no trae {BIN_EN_IMAGEN}")


def config() -> dict[str, object]:
    return {
        "min_bytes": PISO_BYTES,
        "marcas": [m.decode() for m in MARCAS_PORT],
        "marcas_requeridas": MARCAS_REQUERIDAS,
        "tamano_port": TAMANO_PORT,
        "tamano_port_stripped": TAMANO_PORT_STRIPPED,
        "tamano_main": TAMANO_MAIN,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Gate fail-closed: el archivo es el firmware del port (x6200_gui).",
    )
    parser.add_argument("archivo", nargs="?", help="binario a validar, o el .tar con --tar")
    parser.add_argument(
        "--tar",
        action="store_true",
        help=f"tratar `archivo` como una imagen y validar {BIN_EN_IMAGEN} adentro",
    )
    parser.add_argument(
        "--min-bytes",
        type=int,
        default=PISO_BYTES,
        help=f"piso de tamano en bytes (por defecto {PISO_BYTES})",
    )
    parser.add_argument(
        "--config",
        action="store_true",
        help="imprimir las senales calibradas (JSON) y salir",
    )
    args = parser.parse_args(argv)

    if args.config:
        print(json.dumps(config(), indent=2))
        return OK

    if not args.archivo:
        parser.error("falta el archivo a validar")

    archivo = Path(args.archivo)
    if not archivo.is_file():
        print(f"ERROR: {archivo} no existe o no es un archivo", file=sys.stderr)
        return ERROR_DE_USO

    try:
        datos = leer_de_tar(archivo) if args.tar else archivo.read_bytes()
    except (OSError, tarfile.TarError, LookupError) as exc:
        print(f"ERROR: no se pudo leer {archivo}: {exc}", file=sys.stderr)
        return ERROR_DE_USO

    motivos = verificar(datos, args.min_bytes)
    if motivos:
        print(f"ERROR: {archivo} NO es el firmware del port (x6200_gui):", file=sys.stderr)
        for motivo in motivos:
            print(f"  - {motivo}", file=sys.stderr)
        print(
            "  un build desde el `main` del remoto (issue #86) sale con exit 0 y deja "
            "un binario que NO sirve: no desplegarlo ni correrlo.",
            file=sys.stderr,
        )
        return NO_ES_FIRMWARE

    presentes = marcas_presentes(datos)
    print(
        f"OK: {archivo} es el firmware del port "
        f"(ELF32 ARM, {len(datos)} B, {len(presentes)}/{len(MARCAS_PORT)} marcas)"
    )
    return OK


if __name__ == "__main__":
    sys.exit(main())
