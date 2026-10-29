#!/usr/bin/env python3
"""Pruebas deterministas de `tests/swrscan_curve_evidence.py` (issue #132).

Corre el script sobre las capturas ya commiteadas del dataset de aceptacion y
asevera la clasificacion que imprime y el exit code:

  0 = todas las comprobaciones de forma pasan
  1 = falla de forma (incluye "domo" y "rama medida")
  2 = uso / IO

No usa red, radio ni Docker: solo PIL sobre PNGs del repo. Se puede correr
suelto (`python3 tests/test_swrscan_curve_evidence.py`) o con pytest.

Casos (los del issue #132):

* radio real, tema Redshift (1 argumento): `01`, `02` y `03` -> U, exit 0.
  El `01` tiene que dar los numeros medidos del issue: valle y=398 en x=255,
  bordes subiendo 94 / 168 px. Antes del #132 el script ni corria con 1
  argumento (exit 2) y con 2 argumentos no veia la tinta de Redshift.
* domo del owner (binario viejo): se mide como domo (exit 1), no como
  "no encontre la curva".
* arnes (`03-harness-domo-valle` + `04-harness-valle-real`): PASS, sin
  regresion, tanto sueltos como en el modo de 2 argumentos.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

RAIZ = Path(__file__).resolve().parent.parent
SCRIPT = RAIZ / "tests" / "swrscan_curve_evidence.py"

RADIO = RAIZ / "docs" / "evidencia" / "2026-10-17-qa-radio-swr125"
BANCO = RAIZ / "docs" / "evidencia" / "2026-10-17-swr125-direction-banco"
VIEJO = RAIZ / "docs" / "evidencia" / "2026-10-15-validacion-arreglos"
TEMAS = RAIZ / "rootfs" / "usr" / "share" / "x6200" / "themes"

RADIO_01 = RADIO / "01-swrscan-50k-dial7000-U.png"
RADIO_02 = RADIO / "02-swrscan-50k-dial7000-U-2da.png"
RADIO_03 = RADIO / "03-swrscan-200k-log-dial7016-U.png"
RADIO_FUERA = RADIO / "05-swrscan-50k-dial7020-fuera-resonancia.png"
DOMO_OWNER = VIEJO / "26-roe-multipasada-final.png"
ARNES_DOMO = BANCO / "03-harness-domo-valle.png"
ARNES_VALLE = BANCO / "04-harness-valle-real.png"


def corre(*args: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *(str(a) for a in args)],
        capture_output=True,
        text=True,
        check=False,
    )


def salida(r: subprocess.CompletedProcess[str]) -> str:
    return f"stdout:\n{r.stdout}\nstderr:\n{r.stderr}"


def test_radio_u_1_arg() -> None:
    """La captura real del #130 con 1 argumento: U y los numeros del issue."""
    r = corre(RADIO_01)

    assert r.returncode == 0, salida(r)
    assert "forma: U" in r.stdout, salida(r)
    assert "tinta: RGB(255,179,160)" in r.stdout, salida(r)
    assert "valle: y=398 en x=255" in r.stdout, salida(r)
    assert "sube 94 px" in r.stdout, salida(r)
    assert "sube 168 px" in r.stdout, salida(r)


def test_radio_u_2da_y_log() -> None:
    """Las otras dos capturas de radio (2da pasada y span 200k/Log) tambien U."""
    for png, xv in ((RADIO_02, 232), (RADIO_03, 238)):
        r = corre(png)

        assert r.returncode == 0, salida(r)
        assert "forma: U" in r.stdout, salida(r)
        assert f"valle: y=398 en x={xv}" in r.stdout, salida(r)


def test_domo_del_owner_se_reporta_como_domo() -> None:
    """El domo viejo del owner: medido y clasificado domo, no 'no encontre'.

    Con la curva = dato medido, una captura con forma de domo se reporta domo y
    sale 0: el modo de una captura no impone estetica (la relacion dato<->dibujo la
    verifica el modo de dos capturas sobre un render fresco). Lo que se asegura aca
    es que la medicion lo ve: la base en los BORDES (x=10) y la forma clasificada.
    """
    r = corre(DOMO_OWNER)

    assert r.returncode == 0, salida(r)
    assert "forma: domo" in r.stdout, salida(r)
    assert "no encontre la curva" not in r.stdout, salida(r)
    assert "valle: y=398 en x=10" in r.stdout, salida(r)


def test_arnes_capturas_medidas() -> None:
    """El arnes del #125: las dos capturas commiteadas se miden (U, tinta del arnes).

    Son las del 2026-10-17, cuando la politica dibujaba el domo del owner como
    valle; hoy el domo se dibuja domo. Por eso aca solo se verifica que la
    medicion las lee (tinta RGB(248,238,255) y traza U); el gate VIVO de la
    relacion dato<->dibujo es tests/check_harness_swrscan_curve.sh, que
    re-renderiza las fixtures swrscan-owner-40m* y exige domo->domo y valle->valle
    (necesita Docker, por eso no corre en este test).
    """
    for png in (ARNES_DOMO, ARNES_VALLE):
        r = corre(png)

        assert r.returncode == 0, salida(r)
        assert "forma: U" in r.stdout, salida(r)
        assert "tinta: RGB(248,238,255)" in r.stdout, salida(r)


def test_dos_argumentos_domo_y_valle() -> None:
    """Modo de 2 argumentos: el domo medido se dibuja domo y el valle, valle."""
    # El domo del owner (captura de radio) + el valle real de la radio: las dos
    # formas que el dato manda. Es el caso que fallaba en el issue #132 ("no
    # encontre la curva" con 2 argumentos sobre la captura de Redshift).
    r = corre(DOMO_OWNER, RADIO_01)
    assert r.returncode == 0, salida(r)
    assert "forma: domo" in r.stdout, salida(r)
    assert "forma: U" in r.stdout, salida(r)
    assert "no encontre la curva" not in r.stdout, salida(r)

    # El mismo domo contra el valle del arnes: la relacion se mide en los dos.
    r = corre(DOMO_OWNER, ARNES_VALLE)
    assert r.returncode == 0, salida(r)
    assert "PASS" in r.stdout, salida(r)


def test_rama_fuera_de_resonancia() -> None:
    """Con el dial fuera de la resonancia el minimo cae en el borde: se reporta rama.

    Es un dato legitimo del barrido (no una estetica): el modo de una captura lo
    mide, lo clasifica y sale 0; lo que no puede pasar es confundirlo con "no
    encontre la curva".
    """
    r = corre(RADIO_FUERA)

    assert r.returncode == 0, salida(r)
    assert "forma: rama" in r.stdout, salida(r)
    assert "no encontre la curva" not in r.stdout, salida(r)


def test_uso_y_io() -> None:
    """Uso incorrecto o archivo que no existe: exit 2."""
    assert corre().returncode == 2
    assert corre(RADIO_01, RADIO_02, RADIO_03).returncode == 2
    assert corre(RAIZ / "no-existe.png").returncode == 2
    assert corre("--ink", "no-es-un-color", RADIO_01).returncode == 2


def _rgb(hexa: str) -> tuple[int, int, int]:
    valor = hexa.lstrip("#")
    return tuple(int(valor[i : i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]


def _png_sintetico(tema: Path, destino: Path) -> tuple[int, int, int]:
    """Chart sintetico de un tema: fondo/panel, grilla, etiquetas y una U.

    Las etiquetas de los ejes usan la MISMA tinta que la curva: es el falso
    positivo que tiene que descartar la seleccion de trazos largos.
    """
    datos = json.loads(tema.read_text(encoding="utf-8"))["colors"]
    fondo = _rgb(datos["bg"])
    panel = _rgb(datos["panel"])
    tinta = _rgb(datos["text"])
    grilla = (170, 170, 170)

    im = Image.new("RGB", (800, 480), fondo)
    dibujo = ImageDraw.Draw(im)
    dibujo.rectangle((0, 69, 799, 398), fill=panel)

    for y in (110, 180, 250, 320):
        dibujo.line((0, y, 799, y), fill=grilla, width=2)
    for x in (200, 400, 600):
        dibujo.line((x, 69, x, 398), fill=grilla, width=2)

    # Etiquetas cortas (los ejes reales) y una larga: el ancho solo no alcanza
    # para confundirlas con la curva, tienen que ser altas.
    for x, y, ancho in (
        (12, 100, 34),
        (12, 180, 34),
        (12, 260, 34),
        (12, 340, 34),
        (120, 372, 34),
        (330, 372, 34),
        (540, 372, 130),
    ):
        dibujo.rectangle((x, y, x + ancho, y + 13), fill=tinta)

    # U: base abajo en el centro, puntas arriba en los bordes.
    anterior = None
    for x in range(10, 791):
        y = max(69, min(398, int(398 - 250 * ((x - 400) / 390) ** 2)))
        if anterior is not None:
            dibujo.line((anterior, (x, y)), fill=tinta, width=4)
        anterior = (x, y)

    im.save(destino)
    return tinta


def test_deteccion_en_los_seis_temas() -> None:
    """La tinta se detecta por tema y las etiquetas de los ejes no son la curva."""
    temas = sorted(TEMAS.glob("*.json"))
    assert len(temas) >= 6, f"se esperaban al menos los 6 temas de la ROM, hay {len(temas)}"

    with tempfile.TemporaryDirectory() as tmp:
        for tema in temas:
            png = Path(tmp) / f"{tema.stem}.png"
            tinta = _png_sintetico(tema, png)
            r = corre(png)

            assert r.returncode == 0, f"{tema.stem}: {salida(r)}"
            assert f"tinta: RGB({tinta[0]},{tinta[1]},{tinta[2]})" in r.stdout, salida(r)
            assert "forma: U" in r.stdout, salida(r)


def main() -> int:
    pruebas = [
        (nombre, fn)
        for nombre, fn in sorted(globals().items())
        if nombre.startswith("test_") and callable(fn)
    ]
    fallos = 0

    for nombre, fn in pruebas:
        try:
            fn()
        except AssertionError as exc:
            fallos += 1
            print(f"FAIL {nombre}: {exc}")

    if fallos:
        print(f"FAIL: {fallos} de {len(pruebas)} pruebas")
        return 1

    print(f"PASS: {len(pruebas)} pruebas de swrscan_curve_evidence")
    return 0


if __name__ == "__main__":
    sys.exit(main())
