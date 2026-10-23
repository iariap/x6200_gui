#!/usr/bin/env python3
"""Genera el manifiesto (14 columnas de la epica #269) y las mediciones PIL de la
corrida de huecos (UI AUDIT, rama factory/ui-audit-gaps).

Fuentes:
  - logs/barrido.json      el barrido QEMU del runner del repo (25 apps, 1 ronda)
  - png/*.png              las capturas del arnes de host (dialogos / BT-WiFi / VISION / legacy)
  - png/*.json             el JSON del arnes (cobertura por componente, hash del framebuffer)

Salidas:
  - manifest.csv
  - mediciones/medidas.csv

Determinista y sin red. No toca nada: solo lee PNG/JSON y escribe CSV.
"""

from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops

AQUI = Path(__file__).resolve().parent
RAIZ = AQUI.parent.parent.parent.parent  # repo root
SHA = "e07ad61660643aa10dc45727e920ba6fd6044ba6"
ISSUE = "#269 (gaps)"


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def _lum(c: tuple[int, int, int]) -> float:
    def ch(v: int) -> float:
        s = v / 255.0
        return s / 12.92 if s <= 0.04045 else ((s + 0.055) / 1.055) ** 2.4

    return 0.2126 * ch(c[0]) + 0.7152 * ch(c[1]) + 0.0722 * ch(c[2])


def contraste(a, b) -> float:
    la, lb = _lum(a), _lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def medir(p: Path) -> dict:
    im = Image.open(p)
    w, h = im.size
    rgb = im.convert("RGB")
    # getcolors(>= px count) devuelve TODOS los colores distintos (no None).
    cuenta = rgb.getcolors(maxcolors=w * h + 1)
    cuenta.sort(key=lambda t: -t[0])
    bg = cuenta[0][1]
    fg = next((c for _, c in cuenta if c != bg), bg)
    # Diferencia absoluta contra el fondo: bbox y fraccion de pixeles != fondo sin
    # recorrer pixel a pixel en Python.
    diff = ImageChops.difference(rgb, Image.new("RGB", (w, h), bg))
    bbox = diff.getbbox()
    dcounts = diff.getcolors(maxcolors=w * h + 1)
    cero = next((n for n, c in dcounts if c == (0, 0, 0)), 0)
    return {
        "png": p.name,
        "width": w,
        "height": h,
        "mode": im.mode,
        "colores": len(cuenta),
        "bg_color": f"#{bg[0]:02x}{bg[1]:02x}{bg[2]:02x}",
        "fg_color": f"#{fg[0]:02x}{fg[1]:02x}{fg[2]:02x}",
        "contraste_bg_fg": f"{contraste(bg, fg):.2f}",
        "frac_no_bg": f"{(w * h - cero) / (w * h):.4f}",
        "bbox_x0": bbox[0] if bbox else None,
        "bbox_y0": bbox[1] if bbox else None,
        "bbox_x1": bbox[2] if bbox else None,
        "bbox_y1": bbox[3] if bbox else None,
        "sha256": sha256(p),
    }


def obs(med: dict, extra: str) -> str:
    return (
        f"{extra}; medido (PIL): {med['colores']} colores, fondo {med['bg_color']}, "
        f"contraste fondo/primer plano {med['contraste_bg_fg']}:1, "
        f"{med['frac_no_bg']} px != fondo, bbox contenido "
        f"({med['bbox_x0']},{med['bbox_y0']})-({med['bbox_x1']},{med['bbox_y1']})"
    )


HOST = [
    # (png, screen, state, coverage, source_path, cmd, extra_obs)
    (
        "dialog-freq.png",
        "Frequency entry (dialog_freq)",
        "ventana real abierta por HKEY_FINP (LV_EVENT_KEY al espectro enfocado)",
        "REAL (ventana src/dialog_freq.c; keyer/textarea/store/audio mockeados)",
        "src/dialog_freq.c",
        "./ui_harness_dialogs --scenario fixtures-gaps/dialog-freq.scn --png dialog-freq.png --json dialog-freq.json --inspect",
        "placeholder 'Freq in MHz' centrado; el protocolo remoto NO inyecta la tecla (por eso QEMU lo declara UNSUPPORTED)",
    ),
    (
        "dialog-msg-cw.png",
        "CW messages (dialog_msg_cw)",
        "ventana real abierta por KEYPAD_MSG con la radio en CW (EVENT_KEYPAD)",
        "REAL (ventana src/dialog_msg_cw.c; keyer/textarea/store/audio mockeados)",
        "src/dialog_msg_cw.c",
        "./ui_harness_dialogs --scenario fixtures-gaps/dialog-msg-cw.scn --png dialog-msg-cw.png --json dialog-msg-cw.json --inspect",
        "tabla vacia (store mockeado) + barra (MSG 1:2)/Send/Beacon/Beacon Period; la home se transparenta (dialog_style sin bg)",
    ),
    (
        "dialog-msg-voice.png",
        "Voice messages (dialog_msg_voice)",
        "ventana real abierta por KEYPAD_MSG con la radio en SSB (EVENT_KEYPAD)",
        "REAL (ventana src/dialog_msg_voice.c; keyer/textarea/store/audio-WAV mockeados)",
        "src/dialog_msg_voice.c",
        "./ui_harness_dialogs --scenario fixtures-gaps/dialog-msg-voice.scn --png dialog-msg-voice.png --json dialog-msg-voice.json --inspect",
        "tabla vacia (/mnt/msg ausente) + barra (MSG 1:2)/Send/Beacon/Beacon Period; la home se transparenta (dialog_style sin bg)",
    ),
    (
        "net-bt-off.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: BT apagado (icon_off), WiFi apagado (icon_off)",
        "SYNTHETIC (doble de src/bt_state.c/src/wifi.cpp en mocks/mock_ui_deps.c; render real src/info.c)",
        "src/info.c (bt_state_show/wifi_state_change_cb) + mocks/mock_ui_deps.c",
        "bt_state off; wifi_status off",
        "SYNTHETIC: mide el layout/contraste del estado, NO la funcion real de BT/WiFi",
    ),
    (
        "net-bt-on.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: BT prendido sin auriculares (text_dim), WiFi apagado",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state on; wifi_status off",
        "SYNTHETIC: runa de BT en text_dim",
    ),
    (
        "net-bt-connected.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: BT con auriculares A2DP conectados (text), WiFi apagado",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state connected; wifi_status off",
        "SYNTHETIC: runa de BT en text",
    ),
    (
        "net-wifi-off.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: WiFi apagado (icon_off), BT apagado",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state off; wifi_status off",
        "SYNTHETIC: baseline de la matriz (mismos pixeles que net-bt-off)",
    ),
    (
        "net-wifi-on.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: WiFi prendido sin asociar (text_dim), BT apagado",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state off; wifi_status on",
        "SYNTHETIC: icono de WiFi en text_dim",
    ),
    (
        "net-wifi-connected.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: WiFi conectado (text), BT apagado",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state off; wifi_status connected",
        "SYNTHETIC: icono de WiFi en text",
    ),
    (
        "net-bt-wifi-connected.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: BT conectado Y WiFi conectado (los dos en text)",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state connected; wifi_status connected",
        "SYNTHETIC: combinado, los dos iconos en text",
    ),
    (
        "net-bt-wifi-on.png",
        "home (main_screen) - icono BT/WiFi",
        "barra: BT prendido y WiFi prendido (los dos en text_dim)",
        "SYNTHETIC (doble; render real src/info.c)",
        "src/info.c + mocks/mock_ui_deps.c",
        "bt_state on; wifi_status on",
        "SYNTHETIC: combinado, los dos iconos en text_dim",
    ),
    (
        "vision-home.png",
        "home (main_screen) - VISION forzado",
        "VISION (theme_shape.vision=true; fuentes +3 pasos)",
        "SYNTHETIC (forzado por el arnes; el firmware NO tiene como entrar en VISION)",
        "src/theme.h:196 (theme_shape.vision), src/main.c:628 (fonts_scale), src/info.c",
        "vision true",
        "SYNTHETIC: muestra el layout de VISION; NO que la radio lo pueda seleccionar (ningun tema lo setea)",
    ),
    (
        "bar-legacy-home.png",
        "home (main_screen) - barra legacy 206 px forzada",
        "barra legacy: info/clock de 206 px + bitmaps claros (THEME_LEGACY)",
        "SYNTHETIC (forzado por el arnes; params.theme no lo escribe ninguna UI)",
        "src/styles.c::setup_theme_legacy (info_style 206x61), src/main.c:637 (styles_init(params.theme.x))",
        "bar_theme legacy",
        "SYNTHETIC: muestra la variante legacy (206 px + images/*.bin claros); NO que la radio la pueda elegir",
    ),
]


def main() -> int:
    # --- QEMU: el barrido del runner del repo ---
    barrido = json.loads((AQUI / "logs" / "barrido.json").read_text())
    meta = barrido["meta"]
    filas = []
    medidas = []

    # Cobertura por app: la VENTANA se capturo siempre (el barrido la abrio), pero
    # las funciones que dependen de un periferico/servicio ausente quedan PARTIAL
    # (misma clasificacion que el inventario de C, #272 y que dialog-coverage.csv):
    # sin red no se valida el cluster, sin GNSS no hay fix, sin adaptador no hay
    # BT/WiFi, sin beacons no hay propagation y la subida a WSPRnet no existe.
    parciales = {"WSPR", "Field", "GPS", "Cluster", "BT", "WiFi", "Propagation"}

    for app, a in barrido["apps"].items():
        png = AQUI / "capturas" / f"{app}-r{a['ronda']}.png"
        med = medir(png)
        resultado = a["resultado"]
        cobertura = "PARTIAL" if app in parciales else "REAL"
        cmd = (
            f'docker exec {meta["contenedor"]} chroot /t /tmp/x6200-remote \'{{"cmd":"app","text":"{app}"}}\''
        )
        pasos = a["checks"]
        checks = "; ".join(f"{c['check']}={'ok' if c['ok'] else 'FALLA'}" for c in pasos)
        obs_txt = f"resultado={resultado}; {a.get('motivo', '')}; checks: {checks}"
        filas.append(
            {
                "screen": app,
                "state": f"app-abierta (dwell {meta['dwell_s']} s); ronda {a['ronda']}; app_estado={a['app_estado']}",
                "theme": "Aurora (default)",
                "source_coverage": cobertura,
                "sha": SHA,
                "command": cmd,
                "exit_code": 0,
                "png": f"capturas/{app}-r{a['ronda']}.png",
                "width": med["width"],
                "height": med["height"],
                "issue_id": ISSUE,
                "severity": "P3",
                "source_path": "binario ARM real (bin/usr/sbin/x6200_gui)",
                "observation": obs(med, obs_txt),
            }
        )
        medidas.append({**med, "png": f"capturas/{app}-r{a['ronda']}.png"})

    # --- host: dialogos / BT-WiFi / VISION / legacy ---
    for nombre, screen, state, cov, src, cmd, extra in HOST:
        png = AQUI / "png" / nombre
        med = medir(png)
        filas.append(
            {
                "screen": screen,
                "state": state,
                "theme": "Aurora (default)",
                "source_coverage": cov,
                "sha": SHA,
                "command": cmd,
                "exit_code": 0,
                "png": f"png/{nombre}",
                "width": med["width"],
                "height": med["height"],
                "issue_id": ISSUE,
                "severity": "P3",
                "source_path": src,
                "observation": obs(med, extra),
            }
        )
        medidas.append({**med, "png": f"png/{nombre}"})

    cols = [
        "screen",
        "state",
        "theme",
        "source_coverage",
        "sha",
        "command",
        "exit_code",
        "png",
        "width",
        "height",
        "issue_id",
        "severity",
        "source_path",
        "observation",
    ]
    with (AQUI / "manifest.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(filas)

    mcols = [
        "png",
        "width",
        "height",
        "mode",
        "colores",
        "bg_color",
        "fg_color",
        "contraste_bg_fg",
        "frac_no_bg",
        "bbox_x0",
        "bbox_y0",
        "bbox_x1",
        "bbox_y1",
        "sha256",
    ]
    with (AQUI / "mediciones" / "medidas.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=mcols)
        w.writeheader()
        w.writerows(medidas)

    print(f"manifest.csv: {len(filas)} filas")
    print(f"medidas.csv: {len(medidas)} filas")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
