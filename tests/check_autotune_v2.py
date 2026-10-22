#!/usr/bin/env python3
"""Comprueba el contrato estatico de Auto Tune (issues #190 CW/CWR y #205 multimodo).

Lo que el arnes de UI y el banco no pueden ver se fija aca, sobre el fuente:

1. Los textos de UX exactos, en ingles: los de CW/CWR de #190 y los multimodo de
   #205, seccion 7. El texto viejo "Auto Tune: CW/CWR only" ya NO existe (el modo
   se rechaza ahora con "mode unsupported" y AM/SAM y USB/LSB corren).
2. La entrada exacta de Settings > Actions (`{ .label = " Auto Tune ",
   .action = ACTION_ATUNE }`) y que ninguna entrada PREEXISTENTE de
   `long_action_items[]` haya desaparecido o cambiado de accion.
3. Que `ACTION_ATUNE` siga valiendo 129 (no renumerar ids persistidos).
4. Que los defaults de las pulsaciones largas y de MFK no se hayan movido.
5. Que el nucleo CW (`src/autotune.c`) siga siendo puro: sin LVGL, sin radio, sin
   `cfg`/`params` y sin el S-meter del barrido v1.
6. Que la capa de UI sea estrictamente RX: sin `radio_set_ptt`, sin tocar el
   pitch (`cfg.key_tone`) ni el RIT (`params.rit`), y sin leer el S-meter.
7. (#205) Que el nucleo CW de #190 sea BYTE A BYTE el mismo: sus tres archivos
   estan pineados por md5. Si un cambio futuro tiene que tocar el nucleo CW, el
   pin se actualiza en el MISMO PR con la razon, y la suite de 169 comprobaciones
   (tests/native/test_autotune.c) sigue siendo el juez del comportamiento.
8. (#205) Que el modulo de politicas espectrales sea puro y sin FFT ni reloj
   propios, que el snapshot del DSP sea el del contrato (800 bins que interpolan
   512) y que el publicador viva en `dsp_samples()`.
9. (#205) Que el coordinador tenga UNA sola via de escritura del dial, que la
   use a traves de la propiedad (`autotune_owns`), que invalide la instantanea
   antes de escribir y que los modos digitales/FM caigan en el rechazo.
10. (#205) Que los fixtures de la familia existan (el digital reemplaza al
   `autotune-notcw`, y AM/USB/LSB estan representados) y que la lista de
   escenarios del banco los reconozca (un fixture sin rama seria un verde falso).

Por que un chequeo estatico y no un test de ejecucion: los mensajes pasan por
`event_send()`, que en el arnes es un no-op documentado (la barra no se dibuja),
y las prohibiciones (no tocar TX/pitch/RIT/S-meter) son ausencias de llamadas,
que ningun fixture puede observar.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import hashlib
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
ROOT = Path(__file__).resolve().parent.parent
fallos: list[str] = []

AUTOTUNE_C = SRC / "autotune.c"
AUTOTUNE_H = SRC / "autotune.h"
AUTOTUNE_UI_C = SRC / "autotune_ui.c"
AUTOTUNE_SPECTRUM_C = SRC / "autotune_spectrum.c"
AUTOTUNE_SPECTRUM_H = SRC / "autotune_spectrum.h"
DSP_H = SRC / "dsp.h"
DSP_CPP = SRC / "dsp.cpp"
SRC_CMAKE = SRC / "CMakeLists.txt"
SETTINGS_C = SRC / "dialog_settings.c"
PARAMS_H = SRC / "params" / "params.h"
PARAMS_C = SRC / "params" / "params.c"
TEST_AUTOTUNE = ROOT / "tests" / "native" / "test_autotune.c"
FIXTURES = ROOT / "tools" / "ui_harness" / "fixtures"
HARNESS_SCREENS = ROOT / "tools" / "ui_harness" / "src" / "screens.c"

# Los textos EXACTOS del contrato. CW/CWR (#190): los que siguen existiendo, sin
# un caracter de diferencia. Multimodo (#205, seccion 7): los nuevos.
MENSAJES_CW_EXACTOS = [
    "Auto Tune: tuning...",
    "Auto Tune: tuned",
    "Auto Tune: no reliable adjustment",
    "Auto Tune: cancelled",
    "Auto Tune: RX only",
    "Auto Tune: frequency locked",
    "Auto Tune: disable RIT",
    "Auto Tune: close current app",
]

MENSAJES_MULTIMODO_EXACTOS = [
    "Auto Tune: carrier centered",
    "Auto Tune: carrier centered - press again to undo",
    "Auto Tune: no reliable signal",
    "Auto Tune: suggested %+d Hz - press again",
    "Auto Tune: applied - press again to undo",
    "Auto Tune: restored",
    "Auto Tune: mode unsupported",
    "Auto Tune: disable split",
]

# El texto del rechazo viejo: con AM/SAM y USB/LSB soportados ya no describe el
# contrato. Dejarlo seria una trampa para el operador.
TEXTO_VIEJO_CW_ONLY = "Auto Tune: CW/CWR only"

# Núcleo CW de #190, byte a byte (base 00a653e7 = tip de lu2dsx/x6200-rom al
# implementar #205). Ver el punto 7 de la cabecera.
PIN_CW_MD5 = {
    "src/autotune.c": "30814c17da9761b73bcce6423343ffcf",
    "src/autotune.h": "e5e62ebee115502599e15cd6a7372c7a",
    "tests/native/test_autotune.c": "3c223be5ebef84012f4061c585ea2794",
}

# Entradas preexistentes de `long_action_items[]` que NO pueden desaparecer ni
# cambiar de accion (el orden no importa: se permite agregar, no quitar).
ENTRADAS_PREEXISTENTES = [
    (" None ", "ACTION_NONE"),
    (" Screenshot ", "ACTION_SCREENSHOT"),
    (" Screen rec ", "ACTION_SCREEN_REC"),
    (" Recorder on/off ", "ACTION_RECORDER"),
    (" Mute ", "ACTION_MUTE"),
    (" Voice mode ", "ACTION_VOICE_MODE"),
    (" ATT ", "ACTION_ATT_TOGGLE"),
    (" PRE ", "ACTION_PRE_TOGGLE"),
    (" ATU ", "ACTION_ATU_TOGGLE"),
    (" ATU tune ", "ACTION_ATU_TUNE"),
    (" APP Bands ", "ACTION_APP_BANDS"),
    (" Battery info ", "ACTION_BAT_INFO"),
    (" APP RTTY ", "ACTION_APP_RTTY"),
    (" APP Callsign", "ACTION_APP_CALLSIGN"),
    (" APP WiFi", "ACTION_APP_WIFI"),
    (" APP EQ", "ACTION_APP_EQ"),
    (" APP DFL editor", "ACTION_APP_DFL_EDITOR"),
    (" APP FT8 ", "ACTION_APP_FT8"),
    (" APP SWR Scan ", "ACTION_APP_SWRSCAN"),
    (" APP Settings", "ACTION_APP_SETTINGS"),
    (" APP Recorder", "ACTION_APP_RECORDER"),
    (" QTH Grid", "ACTION_APP_QTH"),
    (" APP WSPR", "ACTION_APP_WSPR"),
    (" APP NavTex", "ACTION_APP_NAVTEX"),
    (" APP SSTV", "ACTION_APP_SSTV"),
    (" APP WeFax", "ACTION_APP_WEFAX"),
    (" APP JS8", "ACTION_APP_JS8"),
    (" APP Cluster", "ACTION_APP_CLUSTER"),
    (" APP Field Ops", "ACTION_APP_FIELD"),
    (" APP GPS", "ACTION_APP_GPS"),
    (" APP CW Keyboard", "ACTION_APP_CW_KBD"),
    (" APP CW Trainer", "ACTION_APP_CWTRAINER"),
    (" APP Bluetooth", "ACTION_APP_BT"),
    (" APP Status", "ACTION_APP_STATUS"),
    (" APP Propagation", "ACTION_APP_PROPAGATION"),
    (" Theme", "ACTION_THEME"),
]

# Defaults de las pulsaciones largas y de MFK (src/params/params.c): Auto Tune no
# cambia ninguno (el chequeo solo exige que ninguno pase a ACTION_ATUNE).
CLAVES_DE_DEFAULTS = (
    "long_gen",
    "long_app",
    "long_key",
    "long_msg",
    "long_dfn",
    "long_dfl",
    "long_f1",
    "long_f2",
    "press_f1",
    "press_f2",
)

# Los fixtures de la familia y la lista de escenarios que el banco reconoce. Un
# fixture nuevo sin rama en screens.c no se ejecutaria nunca (verde falso).
FIXTURES_ESPERADOS = (
    "autotune.scn",
    "autotune-progress.scn",
    "autotune-found.scn",
    "autotune-digital.scn",
    "autotune-am.scn",
    "autotune-usb.scn",
    "autotune-lsb.scn",
    "autotune-usb-applied.scn",
    "autotune-usb-timeout.scn",
)
FIXTURES_PROHIBIDOS = ("autotune-notcw.scn",)


def sin_comentarios(texto: str) -> str:
    """El fuente sin comentarios de bloque y de linea, respetando literales."""
    salida = []
    i = 0
    n = len(texto)
    while i < n:
        c = texto[i]
        if c in "\"'":
            comilla = c
            salida.append(c)
            i += 1
            while i < n:
                salida.append(texto[i])
                if texto[i] == "\\":
                    i += 1
                    if i < n:
                        salida.append(texto[i])
                elif texto[i] == comilla:
                    break
                i += 1
            i += 1
        elif texto.startswith("/*", i):
            fin = texto.find("*/", i + 2)
            i = n if fin < 0 else fin + 2
            salida.append(" ")
        elif texto.startswith("//", i):
            fin = texto.find("\n", i)
            i = n if fin < 0 else fin
        else:
            salida.append(c)
            i += 1
    return "".join(salida)


def valor_de_action(texto: str, nombre: str) -> int | None:
    """El valor entero de un enumerador de press_action_t (params.h)."""
    m = re.search(r"typedef enum\s*\{(.*?)\}\s*press_action_t\s*;", sin_comentarios(texto), re.S)
    if not m:
        return None
    valor = -1
    for item in m.group(1).split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            nom, val = (x.strip() for x in item.split("=", 1))
            if not val.lstrip("-").isdigit():
                return None
            valor = int(val)
        else:
            nom = item
            valor += 1
        if nom == nombre:
            return valor
    return None


def entradas_long_action(texto: str) -> list[tuple[str, str]]:
    """Los pares (label, action) del array long_action_items[], en orden."""
    san = sin_comentarios(texto)
    bloque = re.search(r"long_action_items\[\]\s*=\s*\{(.*?)\n\};", san, re.S)
    if not bloque:
        return []
    pares = []
    for m in re.finditer(r'\.label\s*=\s*"([^"]*)"\s*,\s*\.action\s*=\s*(ACTION_[A-Z0-9_]+)', bloque.group(1)):
        pares.append((m.group(1), m.group(2)))
    return pares


def sin_lvgl(texto: str, archivo: str) -> None:
    """Un modulo puro: nada de LVGL, radio, cfg/params, Subjects, S-meter, msg."""
    for prohibido in ("lvgl", "radio.h", "cfg/", "params/", "meter", "subject", "msg.h",
                      "dsp.h", "liquid"):
        if re.search(rf'#include\s*"[^"]*{re.escape(prohibido)}[^"]*"', texto):
            fallos.append(f"{archivo} incluye algo de {prohibido!r}: tiene que seguir puro")


def main() -> int:
    autotune_ui = AUTOTUNE_UI_C.read_text(errors="surrogateescape")
    autotune = AUTOTUNE_C.read_text(errors="surrogateescape")
    spectrum = AUTOTUNE_SPECTRUM_C.read_text(errors="surrogateescape")
    spectrum_h = AUTOTUNE_SPECTRUM_H.read_text(errors="surrogateescape")
    dsp_h = DSP_H.read_text(errors="surrogateescape")
    dsp_cpp = DSP_CPP.read_text(errors="surrogateescape")
    src_cmake = SRC_CMAKE.read_text(errors="surrogateescape")
    settings = SETTINGS_C.read_text(errors="surrogateescape")
    params_h = PARAMS_H.read_text(errors="surrogateescape")
    params_c = PARAMS_C.read_text(errors="surrogateescape")

    # 1. textos exactos (CW que sobreviven + multimodo) y el viejo, fuera.
    for texto in MENSAJES_CW_EXACTOS + MENSAJES_MULTIMODO_EXACTOS:
        if f'"{texto}"' not in autotune_ui:
            fallos.append(f'falta el texto exacto "{texto}" en src/autotune_ui.c')
    if f'"{TEXTO_VIEJO_CW_ONLY}"' in autotune_ui:
        fallos.append(
            f'src/autotune_ui.c todavia ofrece "{TEXTO_VIEJO_CW_ONLY}": con AM/SAM y USB/LSB '
            "soportados, el rechazo por modo es \"Auto Tune: mode unsupported\""
        )

    # 2. Settings > Actions
    esperada = '{ .label = " Auto Tune ", .action = ACTION_ATUNE }'
    if esperada not in settings:
        fallos.append(f"falta la entrada exacta `{esperada}` en src/dialog_settings.c")

    pares = entradas_long_action(settings)
    if (" Auto Tune ", "ACTION_ATUNE") not in pares:
        fallos.append("long_action_items[] no ofrece Auto Tune con ACTION_ATUNE")
    for label, accion in ENTRADAS_PREEXISTENTES:
        if (label, accion) not in pares:
            fallos.append(f"long_action_items[] perdio la entrada preexistente {label!r} -> {accion}")

    # 3. id persistido
    valor = valor_de_action(params_h, "ACTION_ATUNE")
    if valor != 129:
        fallos.append(f"ACTION_ATUNE vale {valor} y tiene que seguir siendo 129")

    # 4. defaults de pulsaciones largas y MFK
    defaults = {m.group(1): m.group(2) for m in re.finditer(
        r"\.(long_gen|long_app|long_key|long_msg|long_dfn|long_dfl|long_f1|long_f2|press_f1|press_f2)"
        r"\s*=\s*(ACTION_[A-Z0-9_]+)", sin_comentarios(params_c))}
    faltantes = [c for c in CLAVES_DE_DEFAULTS if c not in defaults]
    if faltantes:
        fallos.append(f"no pude leer los defaults de pulsaciones: {faltantes}")
    for clave, accion in defaults.items():
        if accion == "ACTION_ATUNE":
            fallos.append(f"el default {clave} quedo en ACTION_ATUNE: Auto Tune no es un default")

    # 5. el nucleo CW sigue puro
    sin_lvgl(autotune, "src/autotune.c")
    for prohibido in ("meter_get_raw_db", "rssi", "RSSI", "autotune_plan", "autotune_probe_freq"):
        if prohibido in autotune:
            fallos.append(f"src/autotune.c todavia menciona {prohibido!r} (el barrido v1 tiene que estar fuera)")

    # 6. la UI es RX puro y no toca pitch/RIT/S-meter
    if "radio_set_ptt" in autotune_ui:
        fallos.append("src/autotune_ui.c llama radio_set_ptt (Auto Tune es estrictamente RX)")
    if re.search(r'subject_set_int\(\s*cfg\.key_tone', autotune_ui):
        fallos.append("src/autotune_ui.c escribe cfg.key_tone (el pitch manual no se toca)")
    if re.search(r"params\.rit\s*=", autotune_ui):
        fallos.append("src/autotune_ui.c escribe params.rit (el RIT no se toca)")
    if re.search(r"\bmeter_[a-z_]*\s*\(", autotune_ui):
        fallos.append("src/autotune_ui.c sigue usando el S-meter (issue #190, seccion 1)")

    # 7. el nucleo CW de #190, byte a byte.
    for rel, pin in PIN_CW_MD5.items():
        archivo = ROOT / rel
        if not archivo.is_file():
            fallos.append(f"falta {rel}")
            continue
        real = hashlib.md5(archivo.read_bytes()).hexdigest()
        if real != pin:
            fallos.append(
                f"{rel} cambio (md5 {real} != {pin}): el nucleo CW de #190 no se toca; si el "
                "cambio es deliberado, actualiza el pin en el mismo PR y explica por que"
            )

    # 8. modulo espectral puro, snapshot del contrato y publicador en el DSP.
    sin_lvgl(spectrum, "src/autotune_spectrum.c")
    for prohibido in ("get_time", "meter_get_raw_db", "rssi", "RSSI"):
        if prohibido in spectrum:
            fallos.append(
                f"src/autotune_spectrum.c menciona {prohibido!r}: la politica no tiene reloj ni "
                "S-meter propios (todo entra por la instantanea y el now_ms del llamador)"
            )
    if "#define AUTOTUNE_SPECTRUM_BINS      800" not in spectrum_h:
        fallos.append("src/autotune_spectrum.h no declara los 800 bins del frame")
    if "#define AUTOTUNE_SPECTRUM_CENTER    400" not in spectrum_h:
        fallos.append("src/autotune_spectrum.h no declara el centro en 400")
    if "AUTOTUNE_SPECTRUM_PHYSICAL_BINS 512" not in spectrum_h:
        fallos.append(
            "src/autotune_spectrum.h no declara los 512 bins FISICOS que el frame interpola: sin "
            "eso se prometeria una resolucion (800) que el equipo no tiene"
        )
    if not re.search(r"float\s+db\[AUTOTUNE_SPECTRUM_BINS\]", spectrum_h):
        fallos.append("el snapshot no lleva float db[800]")
    if "#include \"autotune_spectrum.h\"" not in dsp_h:
        fallos.append("src/dsp.h no reexporta la instantanea (src/autotune_spectrum.h)")
    for firma in ("bool dsp_autotune_get_spectrum(dsp_autotune_spectrum_t *out);",
                  "void dsp_autotune_invalidate(void);"):
        if firma not in dsp_h:
            fallos.append(f"src/dsp.h no declara `{firma}`")
    if not re.search(r"dsp_autotune_publish\(.*\);\s*\n\s*// Decrease beta", dsp_cpp):
        fallos.append(
            "src/dsp.cpp no publica la instantanea dentro de dsp_samples() justo despues de la "
            "PSD promediada (misma fuente espectral, sin FFT nueva)"
        )
    if "autotune_spectrum.c" not in src_cmake:
        fallos.append("src/CMakeLists.txt no compila src/autotune_spectrum.c (la ROM no lo tendria)")

    # 9. coordinador: UNA via de escritura, propiedad, invalidacion y modos.
    escrituras = [m.start() for m in re.finditer(r"subject_set_int\(\s*cfg_cur\.fg_freq", autotune_ui)]
    if len(escrituras) != 1:
        fallos.append(
            f"src/autotune_ui.c escribe cfg_cur.fg_freq {len(escrituras)} veces: el dial solo se "
            "toca por la unica via que pregunta la propiedad primero"
        )
    else:
        cuerpo = re.search(r"static bool dial_write_own\(.*?\n\}", autotune_ui, re.S)
        if cuerpo is None or not (cuerpo.start() <= escrituras[0] <= cuerpo.end()):
            fallos.append("la escritura de cfg_cur.fg_freq no esta dentro de dial_write_own()")
    if "autotune_owns(&at, &now)" not in autotune_ui:
        fallos.append("src/autotune_ui.c no consulta autotune_owns() antes de escribir")
    if "dsp_autotune_invalidate()" not in autotune_ui:
        fallos.append(
            "src/autotune_ui.c no invalida la instantanea espectral en la escritura propia "
            "(issue #205, seccion 4: ninguna trama anterior puede mezclarse)"
        )
    if "dsp_autotune_get_spectrum(" not in autotune_ui:
        fallos.append("src/autotune_ui.c no lee la instantanea espectral del DSP")
    for prohibido in ("x6200_mode_usb_dig", "x6200_mode_lsb_dig", "x6200_mode_nfm",
                      "x6200_mode_wfm"):
        if prohibido in autotune_ui:
            fallos.append(
                f"src/autotune_ui.c nombra {prohibido!r}: el digital y FM no tienen Auto Tune "
                "(el default de la estrategia es el rechazo)"
            )
    if "AUTOTUNE_STRATEGY_NONE" not in autotune_ui:
        fallos.append("src/autotune_ui.c no tiene la estrategia de rechazo (AUTOTUNE_STRATEGY_NONE)")

    # 10. fixtures de la familia y lista de escenarios del banco.
    for nombre in FIXTURES_ESPERADOS:
        if not (FIXTURES / nombre).is_file():
            fallos.append(f"falta el fixture tools/ui_harness/fixtures/{nombre}")
    for nombre in FIXTURES_PROHIBIDOS:
        if (FIXTURES / nombre).is_file():
            fallos.append(
                f"{nombre} sigue existiendo: AM ya esta soportado, asi que su premisa "
                "(\"Auto Tune: CW/CWR only\") es falsa; el rechazo vigente es el del modo digital"
            )
    if HARNESS_SCREENS.is_file():
        screens = HARNESS_SCREENS.read_text(errors="surrogateescape")
        for nombre in ("autotune-digital", "autotune-am", "autotune-usb", "autotune-lsb",
                       "autotune-usb-applied", "autotune-usb-timeout"):
            if screens.count(f'"{nombre}"') < 2:
                fallos.append(
                    f'el banco (tools/ui_harness/src/screens.c) no arma y comprueba "{nombre}" '
                    "en las dos fases (build + settle): un fixture sin rama no se ejecutaria"
                )

    print(
        f"  textos: {len(MENSAJES_CW_EXACTOS)} CW + {len(MENSAJES_MULTIMODO_EXACTOS)} multimodo | "
        f"long_action_items: {len(pares)} entradas | ACTION_ATUNE={valor} | "
        f"pins CW: {len(PIN_CW_MD5)} | fixtures: {len(FIXTURES_ESPERADOS)}"
    )

    if fallos:
        for f in fallos:
            print("  FALLO:", f)
        return 1

    print("  contrato estatico de Auto Tune (CW v2 + multimodo) OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
