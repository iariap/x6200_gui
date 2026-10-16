"""Tests del gate fail-closed de firmware (`tools/verifica-firmware-port.py`, issue #86).

No tocan docker, la GUI ni la radio: le pasan al script ELF falsos con las senales que el gate
mide (ISA, tamano, marcas del port) y comprueban el veredicto. La calibracion (el piso cae entre
el binario de `main` y el del port) se verifica contra los numeros REALES del issue #86.
"""

from __future__ import annotations

import json
import struct
import subprocess
import sys
import tarfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOL = REPO_ROOT / "tools" / "verifica-firmware-port.py"

# Los numeros reales del issue #86 (medidos el 2026-10-15).
TAMANO_MAIN = 776_884
TAMANO_PORT_STRIPPED = 13_248_936
TAMANO_PORT = 13_737_784

MARCAS = (b"/tmp/x6200_remote.sock", b"x6200-pota-spots", b"repos/iariap/x6200_gui")


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(TOOL), *args], capture_output=True, text=True, check=False
    )


def _elf32_arm(tamano: int, *, marcas: tuple[bytes, ...] = (), e_type: int = 3) -> bytes:
    """Un ELF32 ARM (ET_DYN por defecto) del tamano pedido, con las marcas dadas adentro."""
    cabecera = b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    cabecera += struct.pack("<HH", e_type, 40)  # e_type, e_machine = EM_ARM
    cuerpo = bytearray(cabecera.ljust(52, b"\x00"))
    for marca in marcas:
        cuerpo += marca + b"\x00"
    if len(cuerpo) < tamano:
        cuerpo += b"\x00" * (tamano - len(cuerpo))
    return bytes(cuerpo)


def test_config_calibra_el_piso_entre_main_y_el_port() -> None:
    r = _run("--config")
    assert r.returncode == 0, r.stderr
    cfg = json.loads(r.stdout)
    assert cfg["tamano_main"] == TAMANO_MAIN
    assert cfg["tamano_port"] == TAMANO_PORT
    assert cfg["tamano_port_stripped"] == TAMANO_PORT_STRIPPED
    # El piso tiene que rechazar el binario de main y aceptar hasta el port mas chico (stripped).
    assert TAMANO_MAIN < cfg["min_bytes"] <= TAMANO_PORT_STRIPPED
    assert set(MARCAS) <= {marca.encode() for marca in cfg["marcas"]}


def test_acepta_el_firmware_del_port(tmp_path: Path) -> None:
    bueno = tmp_path / "x6200_gui"
    bueno.write_bytes(_elf32_arm(4096, marcas=MARCAS))
    r = _run(str(bueno), "--min-bytes", "1024")
    assert r.returncode == 0, r.stderr
    assert "firmware del port" in r.stdout


def test_rechaza_el_binario_de_main_776k(tmp_path: Path) -> None:
    """El caso del issue: ELF ARM PIE de 776.884 B, sin ninguna marca del port."""
    malo = tmp_path / "x6200_gui-main"
    malo.write_bytes(_elf32_arm(TAMANO_MAIN))
    r = _run(str(malo))
    assert r.returncode != 0
    assert "NO es el firmware del port" in r.stderr
    assert str(TAMANO_MAIN) in r.stderr


def test_rechaza_un_archivo_que_no_es_elf(tmp_path: Path) -> None:
    texto = tmp_path / "no-elf.bin"
    texto.write_bytes(b"esto no es un ELF" * 100)
    r = _run(str(texto), "--min-bytes", "1")
    assert r.returncode != 0
    assert "ELF" in r.stderr


def test_rechaza_una_arquitectura_que_no_es_arm(tmp_path: Path) -> None:
    x86 = tmp_path / "x86.bin"
    datos = bytearray(_elf32_arm(4096, marcas=MARCAS))
    struct.pack_into("<H", datos, 18, 62)  # e_machine = EM_X86_64
    x86.write_bytes(bytes(datos))
    r = _run(str(x86), "--min-bytes", "1024")
    assert r.returncode != 0
    assert "ARM" in r.stderr


def test_rechaza_pocas_marcas_del_port(tmp_path: Path) -> None:
    una = tmp_path / "una-marca"
    una.write_bytes(_elf32_arm(4096, marcas=MARCAS[:1]))
    r = _run(str(una), "--min-bytes", "1024")
    assert r.returncode != 0
    assert "marcas del port" in r.stderr


def test_rechaza_por_debajo_del_piso(tmp_path: Path) -> None:
    chico = tmp_path / "chico"
    chico.write_bytes(_elf32_arm(4096, marcas=MARCAS))
    r = _run(str(chico), "--min-bytes", str(TAMANO_PORT_STRIPPED))
    assert r.returncode != 0
    assert "piso" in r.stderr


def _imagen(tmp_path: Path, nombre: str, contenido: bytes) -> Path:
    """Un rootfs.tar con `contenido` como usr/sbin/x6200_gui."""
    binario = tmp_path / nombre
    binario.write_bytes(contenido)
    tar = tmp_path / f"{nombre}.tar"
    with tarfile.open(tar, "w") as tf:
        tf.add(binario, arcname="usr/sbin/x6200_gui")
    return tar


def test_valida_el_binario_adentro_de_la_imagen(tmp_path: Path) -> None:
    buena = _imagen(tmp_path, "buena", _elf32_arm(4096, marcas=MARCAS))
    r = _run("--tar", str(buena), "--min-bytes", "1024")
    assert r.returncode == 0, r.stderr

    mala = _imagen(tmp_path, "mala", _elf32_arm(TAMANO_MAIN))
    r = _run("--tar", str(mala))
    assert r.returncode != 0
    assert "NO es el firmware del port" in r.stderr


def test_una_imagen_sin_el_binario_no_pasa(tmp_path: Path) -> None:
    vacia = tmp_path / "vacia.tar"
    with tarfile.open(vacia, "w"):
        pass
    r = _run("--tar", str(vacia))
    assert r.returncode != 0
