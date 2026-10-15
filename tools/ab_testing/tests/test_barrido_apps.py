"""Unit tests of the bench app sweep (`tools/barrido-apps-banco.sh` + `tools/barrido_apps.py`).

These do not touch docker, the GUI or the radio: they exercise the data half of the sweep — the
verdict per app, the framebuffer read (a uniform one is "nothing was drawn") and the JSON the
driver writes — with fake observations and a fake framebuffer, the same way the shell driver calls
it. The bench itself is exercised by `tools/barrido-apps-banco.sh` and its JSON is the evidence.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOL = REPO_ROOT / "tools" / "barrido_apps.py"

ANCHO, ALTO = 480, 800


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(TOOL), *args], capture_output=True, text=True, check=False
    )


def _obs(tmp_path: Path, nombre: str, **kv: str) -> Path:
    ruta = tmp_path / nombre
    ruta.write_text("".join(f"{clave}={valor}\n" for clave, valor in kv.items()))
    return ruta


def _fb(tonos: int) -> bytes:
    """Framebuffer 480x800x32 BGRA con `tonos` colores distintos (1 = uniforme)."""
    datos = bytearray()
    for y in range(ALTO):
        banda = min(y * tonos // ALTO, tonos - 1) if tonos > 1 else 0
        for _ in range(ANCHO):
            datos += bytes([banda, 0, banda, 255])  # B, G, R, A
    return bytes(datos)


def test_captura_de_una_pantalla_uniforme_y_de_una_dibujada(tmp_path: Path) -> None:
    uniforme = tmp_path / "uniforme.fb"
    uniforme.write_bytes(_fb(1))
    dibujada = tmp_path / "dibujada.fb"
    dibujada.write_bytes(_fb(8))

    r = _run("captura", str(uniforme), str(tmp_path / "uniforme.png"))
    assert r.returncode == 0, r.stderr
    assert "colores=1" in r.stdout
    assert (tmp_path / "uniforme.png").stat().st_size > 0

    r = _run("captura", str(dibujada), str(tmp_path / "dibujada.png"))
    assert r.returncode == 0, r.stderr
    assert "colores=8" in r.stdout


def test_captura_no_inventa_una_foto_si_el_framebuffer_no_esta(tmp_path: Path) -> None:
    r = _run("captura", str(tmp_path / "no-existe.fb"), str(tmp_path / "x.png"))
    assert r.returncode != 0
    assert not (tmp_path / "x.png").exists()


def test_huella_distingue_dos_framebuffers(tmp_path: Path) -> None:
    a = tmp_path / "a.fb"
    a.write_bytes(_fb(1))
    b = tmp_path / "b.fb"
    b.write_bytes(_fb(2))

    ha = _run("huella", str(a))
    hb = _run("huella", str(b))
    assert ha.returncode == 0 and hb.returncode == 0
    assert ha.stdout.strip() != hb.stdout.strip()


@pytest.mark.parametrize(
    ("observacion", "resultado"),
    [
        # Una app se declara viva solo con las cuatro senales: proceso, handshake, dibujo y
        # el estado diciendo que la app abierta es la que se pidio.
        ({"contenedor": "si", "handshake": "si", "colores": "300", "app_estado": "Status"}, "viva"),
        # El proceso murio (el contenedor se fue): muerta aunque el resto parezca bien.
        ({"contenedor": "no", "handshake": "no", "colores": "300", "app_estado": ""}, "muerta"),
        # El proceso vive pero el socket no contesta: muerta, no viva.
        ({"contenedor": "si", "handshake": "no", "colores": "300", "app_estado": ""}, "muerta"),
        # La pantalla quedo uniforme: la GUI no dibujo (aunque conteste).
        (
            {"contenedor": "si", "handshake": "si", "colores": "1", "app_estado": "Status"},
            "sin-dibujo",
        ),
        # Un crash medido en el log manda sobre un handshake que todavia contesta.
        ({"contenedor": "si", "handshake": "si", "colores": "300", "crash": "si"}, "muerta"),
        # La pantalla de inicio dibuja, pero la app pedida no es la que quedo abierta.
        ({"contenedor": "si", "handshake": "si", "colores": "300", "app_estado": ""}, "no-abrio"),
        (
            {"contenedor": "si", "handshake": "si", "colores": "300", "app_estado": "Cluster"},
            "no-abrio",
        ),
    ],
)
def test_veredicto_por_app(tmp_path: Path, observacion: dict[str, str], resultado: str) -> None:
    r = _run("clasificar", str(_obs(tmp_path, "obs.txt", app="Status", intento="1", **observacion)))
    assert r.returncode == (0 if resultado == "viva" else 1), r.stderr
    assert json.loads(r.stdout)["resultado"] == resultado


def _intento(tmp_path: Path, nombre: str, app: str, intento: str, **kv: str) -> dict:
    r = _run("clasificar", str(_obs(tmp_path, nombre, app=app, intento=intento, **kv)))
    return json.loads(r.stdout)


def test_cerrar_junta_intentos_y_el_peor_manda(tmp_path: Path) -> None:
    viva = _intento(
        tmp_path,
        "1.txt",
        "FT8",
        "1",
        contenedor="si",
        handshake="si",
        colores="200",
        app_estado="FT8",
    )
    muerta = _intento(tmp_path, "2.txt", "FT8", "2", contenedor="no", handshake="no", colores="0")
    otra = _intento(
        tmp_path,
        "3.txt",
        "JS8",
        "1",
        contenedor="si",
        handshake="si",
        colores="200",
        app_estado="JS8",
    )

    jsonl = tmp_path / "intentos.jsonl"
    jsonl.write_text("\n".join(json.dumps(x) for x in (viva, muerta, otra)) + "\n")
    salida = tmp_path / "barrido.json"

    r = _run("cerrar", str(jsonl), str(salida), "--meta", "commit=abc")
    assert r.returncode == 0, r.stderr

    documento = json.loads(salida.read_text())
    assert documento["meta"]["commit"] == "abc"
    assert documento["resumen"] == {"apps": 2, "intentos": 3, "vivas": 1, "falladas": ["FT8"]}
    ft8 = documento["apps"][0]
    assert ft8["app"] == "FT8"
    assert ft8["resultado"] == "muerta"
    assert len(ft8["intentos"]) == 2
    assert "2 apps" in r.stdout
