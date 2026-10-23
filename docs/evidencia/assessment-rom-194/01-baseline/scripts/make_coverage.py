#!/usr/bin/env python3
"""Generate coverage.csv for assessment unit #195 (inventory only; no measurements invented).

Columns: cov_id,kind,name,route,test_or_tool,layer,coverage,owner_unit,status,notes
layer:      HOST | QEMU_USER | HARDWARE | NONE
coverage:   REAL | PARTIAL | SYNTHETIC | UNSUPPORTED
status:     PASS | FAIL | SKIP | BLOCKED | PENDING | PENDING_HARDWARE | UNSUPPORTED
"""
import csv
import os
import subprocess

OUT = os.environ.get(
    "OUT",
    "/home/pablo/desa/x6200/assessment-194/wt-assessment-195/docs/evidencia/assessment-rom-194/01-baseline/coverage.csv")
GUI = os.environ.get("GUI", "/home/pablo/desa/x6200/assessment-194/clone-gui")
BR = os.environ.get("BR", "/home/pablo/desa/x6200/assessment-194/clone-buildroot")

rows = []


def add(cid, kind, name, route, test_tool, layer, coverage, owner, status, notes):
    rows.append(dict(cov_id=cid, kind=kind, name=name, route=route, test_or_tool=test_tool,
                     layer=layer, coverage=coverage, owner_unit=owner, status=status, notes=notes))


def src_line(path, pattern, occurrence=0, min_line=1):
    """Return 'path:line' for the n-th (0-based) match of pattern in path, or path."""
    try:
        lines = open(os.path.join(GUI, path), encoding="utf-8", errors="replace").read().splitlines()
    except OSError:
        return path
    hits = [i + 1 for i, l in enumerate(lines) if pattern in l and i + 1 >= min_line]
    if len(hits) > occurrence:
        return f"{path}:{hits[occurrence]}"
    return path


# ---------------------------------------------------------------- apps (24 + alias)
# (name, action enum, implementation file, harness/tests, coverage, owner, status, note)
apps = [
    ("RTTY", "ACTION_APP_RTTY", "src/rtty.c", "manual_apps, app_handlers, buttons page RTTY",
     "PARTIAL", "#197", "PENDING", "Ventana no-dialogo; sin fixture de harness propio"),
    ("Settings", "ACTION_APP_SETTINGS", "src/dialog_settings.c",
     "check_settings_focus, check_settings_grid, fixtures settings-*", "REAL", "#197", "PASS",
     "Grupos/tabs verificados estaticamente y renderizados en harness (fixtures)"),
    ("SWR Scan", "ACTION_APP_SWRSCAN", "src/dialog_swrscan.c",
     "test_swrscan*, swrscan_curve_evidence, fixtures swrscan-*", "REAL", "#196", "PASS",
     "Curva ROE sintetica; barrido real transmite (PENDING_HARDWARE)"),
    ("FT8", "ACTION_APP_FT8", "src/dialog_ft8.c", "test_ft8_qso (CTest BLOCKED), test_ft8_udp",
     "PARTIAL", "#196", "BLOCKED", "CTest ft8/qth no corre: falta ft8lib/constants.h"),
    ("FT4 (alias)", "ACTION_APP_FT8", "src/remote_server.cpp app_names[2]",
     "test_ft8_udp; app_names alias", "PARTIAL", "#196", "PENDING",
     "ALIAS explicito de FT8 (misma accion); no es app separada"),
    ("WSPR", "ACTION_APP_WSPR", "src/dialog_wspr.c", "run_wspr_tests.sh (loopback)", "REAL", "#195",
     "PASS", "158 comprobaciones en imagen full; SKIP con imagen minimal"),
    ("NavTex", "ACTION_APP_NAVTEX", "src/dialog_navtex.c", "run_navtex_tests.sh", "REAL", "#195",
     "PASS", "SITOR-B loopback"),
    ("SSTV", "ACTION_APP_SSTV", "src/dialog_sstv.c", "run_sstv_tests.sh", "REAL", "#195", "PASS",
     "7 modos, correlacion 0.92-0.98"),
    ("WeFax", "ACTION_APP_WEFAX", "src/dialog_wefax.c", "run_wefax_tests.sh", "REAL", "#195", "PASS",
     "300/300 lineas, correlacion 0.995"),
    ("Cluster", "ACTION_APP_CLUSTER", "src/dialog_cluster.c", "test_cluster, test_cluster (native)",
     "REAL", "#196", "PASS", "Logica de spots; red real no medida"),
    ("Field Ops", "ACTION_APP_FIELD", "src/dialog_field.c", "test_field", "REAL", "#196", "PASS",
     "Refs/bandas/RBN/flog; red real no medida"),
    ("JS8", "ACTION_APP_JS8", "src/dialog_js8.c", "run_js8_tests.sh", "REAL", "#195", "PASS",
     "1994 aserciones (suite X6100) con imagen full"),
    ("CW Keyboard", "ACTION_APP_CW_KBD", "src/dialog_cw_kbd.c",
     "test_cw_encoder, test_cw_decoder, test_cw_tune", "REAL", "#197", "PASS",
     "Keyer/decoder host; keying fisico PENDING_HARDWARE"),
    ("CW Trainer", "ACTION_APP_CWTRAINER", "src/dialog_cw_trainer.c", "test_cw_tune (parcial)",
     "PARTIAL", "#197", "PENDING", "Sin fixture de harness propio"),
    ("Bluetooth", "ACTION_APP_BT", "src/dialog_bt.c", "test_bt_devices, fixtures bt-dialog-*",
     "SYNTHETIC", "#197", "PENDING", "Logica de dispositivos; radio BT real PENDING_HARDWARE"),
    ("GPS", "ACTION_APP_GPS", "src/dialog_gps.c", "test_gps_format", "PARTIAL", "#199",
     "PENDING_HARDWARE", "Formato NMEA host; GPS fisico no medido"),
    ("Status", "ACTION_APP_STATUS", "src/dialog_status.c", "manual_apps, app_handlers", "PARTIAL",
     "#197", "PENDING", "Sin fixture propio"),
    ("Recorder", "ACTION_APP_RECORDER", "src/dialog_recorder.c", "test_avi_writer", "PARTIAL",
     "#198", "PENDING", "Contenedor AVI host; audio real PENDING_HARDWARE"),
    ("WiFi", "ACTION_APP_WIFI", "src/dialog_wifi.c", "manual_apps, app_handlers", "PARTIAL",
     "#199", "PENDING_HARDWARE", "WiFi real no medido; radio del owner intocable en #195"),
    ("EQ", "ACTION_APP_EQ", "src/dialog_eq.cpp", "manual_apps, app_handlers", "PARTIAL", "#197",
     "PENDING", "Sin fixture de harness propio"),
    ("Bands", "ACTION_APP_BANDS", "src/dialog_bands.c", "test_band_plan, fixture app-bands",
     "REAL", "#197", "PASS", "Plan de bandas host + fixture"),
    ("QTH", "ACTION_APP_QTH", "src/dialog_qth.c", "test_qth (CTest BLOCKED)", "UNSUPPORTED",
     "#196", "BLOCKED", "CTest no corre: falta ft8lib/constants.h + libasan host"),
    ("Callsign", "ACTION_APP_CALLSIGN", "src/dialog_callsign.c", "manual_apps, app_handlers",
     "PARTIAL", "#197", "PENDING", "Sin fixture propio"),
    ("Propagation", "ACTION_APP_PROPAGATION", "src/dialog_propagation.c", "test_propagation",
     "REAL", "#196", "PASS", "8 fixtures IARU R2, 408 puntos"),
    ("DFL Editor (inline)", "ACTION_APP_DFL_EDITOR", "src/dfl_inline.c",
     "test_dfl_editor, fixtures dfl-editor-*", "REAL", "#197", "PASS",
     "No es app del menu APPS; figura en remoto (app_names[25]) y en barra DFL"),
    ("Home / main screen", "ACTION_NONE", "src/main_screen.c", "fixtures main-*, home-*, check_wcag",
     "REAL", "#197", "PASS", "Pantalla principal, spectrum/waterfall, temas"),
]
for i, (name, action, impl, tests, cov, owner, status, note) in enumerate(apps, 1):
    trail = name.split(" ")[0].upper().replace("(", "").replace(")", "")
    route = src_line("src/main_screen.c", f"case {action}", 0, 237) if action != "ACTION_NONE" else "src/main_screen.c"
    add(f"APP-{i:02d}", "app", name, f"{route}; {impl}", tests, "HOST", cov, owner, status, note)

# ---------------------------------------------------------------- actions (press_action_t)
actions = [
    ("ACTION_NONE", "sin accion"),
    ("ACTION_SCREENSHOT", "screenshot.c"),
    ("ACTION_SCREEN_REC", "screen_rec.c"),
    ("ACTION_RECORDER", "recorder.c on/off"),
    ("ACTION_MUTE", "radio_change_mute"),
    ("ACTION_STEP_UP", "controls_step.c"),
    ("ACTION_STEP_DOWN", "controls_step.c"),
    ("ACTION_VOICE_MODE", "voice_change_mode"),
    ("ACTION_BAT_INFO", "info.c / battery"),
    ("ACTION_NR_TOGGLE", "dsp NR"),
    ("ACTION_NB_TOGGLE", "dsp NB"),
    ("ACTION_APP_RTTY", "app RTTY"),
    ("ACTION_APP_FT8", "app FT8"),
    ("ACTION_APP_WSPR", "app WSPR"),
    ("ACTION_APP_NAVTEX", "app NavTex"),
    ("ACTION_APP_SSTV", "app SSTV"),
    ("ACTION_APP_WEFAX", "app WeFax"),
    ("ACTION_APP_CLUSTER", "app Cluster"),
    ("ACTION_APP_FIELD", "app Field Ops"),
    ("ACTION_APP_JS8", "app JS8"),
    ("ACTION_APP_CW_KBD", "app CW Keyboard"),
    ("ACTION_APP_CWTRAINER", "app CW Trainer"),
    ("ACTION_APP_BT", "app Bluetooth"),
    ("ACTION_APP_STATUS", "app Status"),
    ("ACTION_THEME", "theme change"),
    ("ACTION_APP_SWRSCAN", "app SWR Scan"),
    ("ACTION_APP_SETTINGS", "app Settings"),
    ("ACTION_APP_RECORDER", "app Recorder"),
    ("ACTION_APP_QTH", "app QTH Grid"),
    ("ACTION_APP_CALLSIGN", "app Callsign"),
    ("ACTION_APP_WIFI", "app WiFi"),
    ("ACTION_APP_EQ", "app EQ"),
    ("ACTION_APP_GPS", "app GPS"),
    ("ACTION_ATT_TOGGLE", "ATT"),
    ("ACTION_PRE_TOGGLE", "PRE"),
    ("ACTION_ATU_TOGGLE", "ATU on/off"),
    ("ACTION_ATU_TUNE", "ATU tune (transmite)"),
    ("ACTION_WF_MODE", "WF mode (no ofrecida: no portada)"),
    ("ACTION_APP_BANDS", "app Bands"),
    ("ACTION_ATUNE", "Auto Tune v2 (issue #190)"),
    ("ACTION_APP_PROPAGATION", "app Propagation"),
    ("ACTION_APP_DFL_EDITOR", "editor DFL inline"),
]
for i, (act, what) in enumerate(actions, 1):
    route = src_line("src/params/params.h", act, 0)
    known = act not in ("ACTION_NONE", "ACTION_WF_MODE")
    add(f"ACT-{i:02d}", "action", act, route, "tests/check_actions.py (handler de cada accion ofrecida)",
        "HOST", "PARTIAL" if known else "UNSUPPORTED", "#197",
        "PASS" if known else "UNSUPPORTED",
        f"{what}; check_actions solo verifica que exista handler, no el efecto" if known
        else f"{what}; no ofrecida al operador (no portada)")

# ---------------------------------------------------------------- settings groups + items
groups = [
    ("Operation", ["Freq acceleration", "Accel threshold", "Bluetooth", "Cluster column",
                   "wfview server", "Transverter 1..4"]),
    ("Audio", ["Line-in, Line-out", "Play,Rec gain", "Audio enhance", "Speaker mode", "Voice mode",
               "Voice rate, pitch, volume"]),
    ("Spectrum", ["Spectrum auto min, max", "Waterfall auto min, max", "Waterfall line, zoom",
                  "Waterfall smooth scroll", "Waterfall battery mode"]),
    ("Clock", ["Day, Month, Year", "Hour, Min, Sec", "Clock view",
               "Timeout Clock, Power, TX"]),
    ("Display", ["Timeout, Brightness", "Buttons brightness", "S-meter needle", "Band plan"]),
    ("Appearance", ["Theme", "Font", "Visual profile"]),
    ("Actions", ["Long press gen/app/key/msg/dfn/dfl", "HMIC action"]),
    ("Info", ["Mag Freq, Info, ALC"]),
]
gid = 0
iid = 0
for gname, items in groups:
    gid += 1
    add(f"SET-G{gid}", "settings_group", gname,
        "src/dialog_settings.c::construct_cb (" + gname + ")",
        "check_settings_focus, check_settings_grid, fixtures settings-*", "HOST", "PARTIAL",
        "#197", "PASS", "Navegacion/foco y grilla verificados; interaccion completa es de #197")
    for it in items:
        iid += 1
        add(f"SET-I{iid:02d}", "settings_item", it,
            "src/dialog_settings.c make_*/" + it, "fixtures settings-* (render)",
            "HOST", "PARTIAL", "#197", "PENDING",
            "Render cubierto por fixture; efecto en runtime (params/radio) sin medicion")

# ---------------------------------------------------------------- button pages / physical input
pages = [
    ("pagina VOL 1 (VOL/SQL/RFG/PWR)", "buttons_page_vol_1"),
    ("pagina VOL 2 (mic/comp/monitor)", "page_vol_2"),
    ("pagina VOL 3 (voz)", "page_vol_3"),
    ("pagina MFK 1 (AGC/ATT/PRE/COMP)", "page_mfk_1"),
    ("pagina MFK 2 (spectrum min/max/FFT/beta)", "page_mfk_2"),
    ("pagina MFK 3 (fill/peak/hold/speed)", "page_mfk_3"),
    ("pagina MFK 4 (charger/ant/RIT/XIT)", "page_mfk_4"),
    ("pagina MEM 1 (mem 1-4)", "page_mem_1"),
    ("pagina MEM 2 (mem 5-8)", "page_mem_2"),
    ("paginas APP 1-6 (24 apps)", "page_app_1, page_app_2, page_app_3, page_app_4, page_app_5, page_app_6"),
    ("paginas KEY 1-2 (keyer)", "page_key_1, page_key_2"),
    ("paginas CW decoder 1-2", "page_cw_decoder_1, page_cw_decoder_2"),
    ("paginas DFN 1-3 (DNF/NB/NR)", "page_dfn_1, page_dfn_2, page_dfn_3"),
    ("pagina RTTY", "buttons_page_rtty"),
    ("paginas DFL A-F (favoritos/ajustar/grafico/mas/confirmar/asignar)", "buttons_page_dfl_1, buttons_page_dfl_expanded, page_dfl_fav, page_dfl_more, page_dfl_confirm, page_dfl_assign"),
    ("paginas mensajes CW / voz", "dialog_msg_cw.c, dialog_msg_voice.c"),
]
for i, (name, impl) in enumerate(pages, 1):
    add(f"BTN-P{i:02d}", "buttons_page", name, f"src/buttons.cpp::{impl}",
        "test_dfl_* (native), fixtures dfl-presets-*", "HOST", "PARTIAL", "#197", "PENDING",
        "Barra inferior; pulsacion fisica no accionable en host")

inputs = [
    ("F1..F5 pulsacion corta", "keypad.c/buttons.c", "PARTIAL"),
    ("F1..F5 long press (acciones configurables)", "buttons.cpp long_action", "PARTIAL"),
    ("Touch barra inferior (CLICKED/LONG_PRESSED)", "buttons.cpp touch_hold; fixtures dfl-presets-touch*", "REAL"),
    ("Rotary 0 VFO (adaptive_tuning/rotary_queue)", "src/adaptive_tuning.c, rotary_queue.c; test_adaptive_tuning, test_rotary_queue", "REAL"),
    ("Rotary 1 VOL", "src/vol.c, controls_encoder.c", "PARTIAL"),
    ("Rotary 2 MFK", "src/mfk.cpp", "PARTIAL"),
    ("Rotary 3 MFK interno", "src/rotary.c, mfk_inner", "PARTIAL"),
    ("Teclado fisico / hkey", "src/hkey.c, keyboard.c; test_hkey, test_keyboard", "REAL"),
    ("Touch pantalla (LVGL indev)", "src/mouse.c, input_dev.c", "PARTIAL"),
    ("PTT / microfono", "radio/A33 GPIO", "UNSUPPORTED"),
]
for i, (name, impl, cov) in enumerate(inputs, 1):
    add(f"IN-{i:02d}", "physical_input", name, impl,
        "tests/native (logica) / harness (eventos LVGL)" if cov != "UNSUPPORTED" else "ninguno",
        "HOST" if cov != "UNSUPPORTED" else "HARDWARE", cov, "#197" if cov != "UNSUPPORTED" else "#199",
        "PENDING" if cov != "UNSUPPORTED" else "PENDING_HARDWARE",
        "Mecanica real del equipo no accionable sin hardware" if cov != "REAL" else "Eventos LVGL inyectados por harness")

# ---------------------------------------------------------------- subsystems and services
subs = [
    # (name, route, tool, layer, coverage, owner, status, note)
    ("RX path / sintonia", "src/radio.c, controls.c, cfg/mode.c", "ninguno", "HARDWARE", "UNSUPPORTED", "#199",
     "PENDING_HARDWARE", "RX real solo en fase fisica #199"),
    ("TX / PTT / keyer RF", "src/radio.c, cw.c", "ninguno (prohibido transmitir)", "HARDWARE", "UNSUPPORTED",
     "#199", "PENDING_HARDWARE", "Requiere autorizacion/instrumentacion; fuera de #195"),
    ("Filtros / DFL / editor inline", "src/dfl_*.c, buttons.cpp", "test_dfl_editor, test_dfl_presets, test_dfl_rotary, fixtures dfl-*",
     "HOST", "REAL", "#197", "PASS", "Cobertura nativa + harness extensa"),
    ("Presets DFL / memoria de filtros", "src/dfl_presets_store.c, sql", "test_dfl_presets", "HOST", "REAL", "#197",
     "PASS", "Persistencia SQLite de presets"),
    ("Memoria de canales / VFO / split", "src/cfg/memory.c, mem_load/save", "test_cfg_fills (parcial)", "HOST", "PARTIAL",
     "#196", "PENDING", "Sin test de round-trip de memoria"),
    ("RIT / XIT", "src/buttons.cpp btn_rit/btn_xit, radio", "ninguno", "HOST", "UNSUPPORTED", "#197",
     "PENDING", "Sin test; efecto en radio no medido"),
    ("Lock freq/mode/AB", "src/main_screen.c main_screen_lock_*", "ninguno", "HOST", "UNSUPPORTED", "#197",
     "PENDING", "Sin test"),
    ("CB 27 MHz", "src/cb.c, params cb_*", "test_cb", "HOST", "REAL", "#196", "PASS",
     "Reglas/interlock host"),
    ("CW decoder / encoder / QSK", "src/cw_decoder.c, cw_encoder.c", "test_cw_decoder, test_cw_encoder", "HOST",
     "REAL", "#197", "PASS", "Decodificador y encoder"),
    ("Auto Tune v2 (CW/CWR)", "src/autotune.c, cw_autotune.c, autotune_ui.c", "test_autotune, test_cw_autotune, check_autotune_v2, fixtures autotune-*",
     "HOST", "REAL", "#197", "PASS", "Alcance RX verificado por QA previo en radio (historico)"),
    ("DSP / audio enhancement", "src/dsp.cpp, audio_enhancement.c", "test_audio_enhancement", "HOST", "REAL",
     "#198", "PASS", "Perfiles de audio host; audio real PENDING_HARDWARE"),
    ("Voz / espeak / voice_text", "src/voice.c, voice_text.c", "test_voice", "HOST", "REAL", "#198", "PASS",
     "Textos y modo; sintesis real no medida"),
    ("Modos digitales (FT8/FT4/JS8/WSPR/SSTV/WeFax/NavTex)", "src/ft8/, js8/, wspr/, sstv/, wefax/, navtex/",
     "run_*_tests.sh, test_ft8_udp", "HOST", "REAL", "#195", "PASS",
     "Codecs loopback; operacion en aire PENDING_HARDWARE"),
    ("CAT (serial/USB)", "src/cat.cpp, btcat/", "ninguno", "HARDWARE", "UNSUPPORTED", "#199",
     "PENDING_HARDWARE", "Requiere equipo/adaptador"),
    ("USB devices", "src/usb_devices.cpp", "ninguno", "HARDWARE", "UNSUPPORTED", "#199", "PENDING_HARDWARE",
     "Depende de hardware"),
    ("Control remoto (socket)", "src/remote_server.cpp", "tests/native/test_remote.cpp", "HOST", "REAL",
     "#196", "PASS", "Protocolo/socket host; sin HW"),
    ("wfview server", "src/wfview_server.c", "test_wfview", "HOST", "REAL", "#196", "PASS",
     "UDP real entre procesos host"),
    ("Web configurator (19 paginas)", "br2_external/package/x6100-webserver", "tests/run_pages_test.py (SKIP: sin checkout/volumen)",
     "HOST", "UNSUPPORTED", "#196", "SKIP", "No se uso el volumen compartido x6200-build-output"),
    ("WiFi / wpa_supplicant", "src/wifi.cpp, overlay wpa_supplicant", "ninguno", "HARDWARE", "UNSUPPORTED",
     "#199", "PENDING_HARDWARE", "Red de la radio intocable en #195"),
    ("Bluetooth / btaudio", "src/btaudio.c, bt_devices.c, S39/S94", "test_bt_devices", "HOST", "SYNTHETIC",
     "#197", "PENDING", "Logica; stack BT real PENDING_HARDWARE"),
    ("GPS", "src/gps.c, gps_format.c", "test_gps_format", "HOST", "PARTIAL", "#199", "PENDING_HARDWARE",
     "Formato NMEA; GPS fisico no medido"),
    ("Cluster / spots", "src/cluster/, dialog_cluster.c", "test_cluster", "HOST", "REAL", "#196", "PASS",
     "Parser/logica; red real no medida"),
    ("Recorder / AVI / screen rec", "src/recorder.c, avi_writer.c, screen_rec.c", "test_avi_writer", "HOST",
     "PARTIAL", "#198", "PENDING", "Contenedor AVI; captura real de audio/video PENDING_HARDWARE"),
    ("QSO log / ADIF", "src/qso_log.c, adif.c", "ninguno", "HOST", "UNSUPPORTED", "#196", "PENDING",
     "Sin test dedicado"),
    ("SQLite / migraciones / params", "src/cfg/db.cpp, src/params/db.c, migrations.c", "test_migrations_cb, test_sql_policy",
     "HOST", "REAL", "#196", "PASS", "Migraciones y politica SQL"),
    ("Boot / init / S20gui / daemon", "rootfs/etc/init.d/S20gui, rcS, x6200_daemon", "boot-rootfs.yml (prune), ninguno de arranque",
     "QEMU_USER", "UNSUPPORTED", "#196", "PENDING", "Arranque real no ejecutado en #195"),
    ("Watchdog / last breath / crashlog", "src/lastbreath.c, crashlog_modules.c", "test_lastbreath, test_crashlog_modules",
     "HOST", "REAL", "#196", "PASS", "Modulos host"),
    ("Apagado / reinicio", "rootfs, power", "ninguno", "HARDWARE", "UNSUPPORTED", "#196", "PENDING_HARDWARE",
     "Cortes de energia prohibidos en #195"),
    ("Update / S02update / tgz", "br2_external/.../S02update, tools/release/", "tools/release/test_update.sh (requiere rootfs+sudo)",
     "QEMU_USER", "PARTIAL", "#196", "BLOCKED", "No ejecutado en #195 (requiere rootfs y sudo)"),
    ("DATA partition / S01create_data / backup", "br2_external overlay S01create_data, S04restore_backup_config",
     "ninguno", "QEMU_USER", "UNSUPPORTED", "#196", "PENDING", "Sin prueba de particion en #195"),
    ("Temas (6 JSON) / WCAG", "rootfs/usr/share/x6200/themes", "test_theme, check_wcag", "HOST", "REAL",
     "#197", "PASS", "Aurora/Cathode/Redshift/Sprint/Sunlit/Trail; AA verificado"),
    ("Band plan / bandas", "src/band_plan*.c, sql/bands_*.csv", "test_band_plan", "HOST", "REAL", "#197",
     "PASS", "Datos IARU y busqueda"),
    ("Propagacion / IARU R2", "src/propagation*.c", "test_propagation", "HOST", "REAL", "#196", "PASS",
     "Oraculo IARU R2"),
    ("Spectrum / waterfall", "src/spectrum.c, waterfall.c", "test_spectrum (TSan host omitido), check_waterfall_incremental",
     "HOST", "PARTIAL", "#198", "PENDING", "TSan no disponible en host; benchmark en #198"),
    ("Backlight / clock de potencia", "src/backlight.c, clock.c", "test_backlight, test_clock", "HOST", "REAL",
     "#198", "PASS", "Politicas de brillo/reloj"),
    ("Bateria / visual policy / ECO", "src/visual_policy.c, params", "test_visual_policy, fixtures *-eco", "HOST",
     "REAL", "#198", "PASS", "Perfiles; consumo real PENDING_HARDWARE"),
    ("Update checker", "src/update_check.c", "test_update_check", "HOST", "REAL", "#196", "PASS",
     "Comparacion de version"),
    ("Screenshot", "src/screenshot.c", "ninguno", "HARDWARE", "UNSUPPORTED", "#197", "PENDING_HARDWARE",
     "Requiere framebuffer real"),
    ("SWR scan / ROE", "src/swr_session.c, roe_curve.c, dialog_swrscan.c", "test_swrscan*, swrscan_curve_evidence",
     "HOST", "SYNTHETIC", "#196", "PASS", "Modelo de curva; barrido real transmite"),
    ("ATU (toggle/tune)", "src/cfg/atu.c, radio", "ninguno", "HARDWARE", "UNSUPPORTED", "#199",
     "PENDING_HARDWARE", "Tune transmite; fuera de alcance"),
    ("Field ops / POTA/SOTA", "src/field/", "test_field", "HOST", "REAL", "#196", "PASS",
     "Refs, bandas, log; red real no medida"),
    ("QTH / grid", "src/qth/", "test_qth (CTest BLOCKED)", "HOST", "UNSUPPORTED", "#196", "BLOCKED",
     "Dependencia ft8lib/constants.h + libasan"),
    ("Teclado / hkey / rollover", "src/keyboard.c, hkey.c, kbd_*.c", "test_keyboard, test_hkey, test_kbd_caps, test_kbd_rollover?",
     "HOST", "REAL", "#197", "PASS", "Mapeo de teclas y rollover"),
    ("Encoder / rotary queue", "src/rotary.c, rotary_queue.c, controls_encoder.c", "test_rotary_queue, fixtures dfl-*-rotary0",
     "HOST", "REAL", "#197", "PASS", "Cola de detents host"),
    ("Gestion de energia (idle/wakeups)", "src/main.c, scheduler.cpp", "natural_evidence.py, wakeup_evidence.py (requieren harness/banco)",
     "HOST", "SYNTHETIC", "#198", "PENDING", "Requiere banco/harness; #198"),
    ("Identidad de build / fuente-md5 / gate #86", "tools/fuente-md5.sh, tools/verifica-firmware-port.py",
     "check_build_identity.py, test_verifica_firmware_port.py (pytest), gate #86", "HOST", "REAL", "#195",
     "PENDING", "Ejecutado en #195 sobre artefactos de esta corrida"),
    ("Paridad host<->CI (sin lvgl)", "tests/native/run_native_tests.sh --ci-parity", "run_all_tests.sh host_ci_parity",
     "HOST", "REAL", "#195", "PASS", "138 comprobaciones, 0 fallos"),
    ("Documentacion / auditoria de pruebas", "docs/PRUEBAS.md, tests/audita-pruebas.py", "audita-pruebas.py, check_manual_apps.py",
     "HOST", "REAL", "#199", "PASS", "Estructura y evidencia citada; revision final en #199"),
    ("Build ROM / rootfs / sdcard.img", "Buildroot d084e287 + GUI c888308e", ".github/workflows/main.yml (referencia)",
     "HOST", "REAL", "#195", "PENDING", "Receta reproducida en sandbox privado; resultado en este informe"),
]
for i, (name, route, tool, layer, cov, owner, status, note) in enumerate(subs, 1):
    add(f"SYS-{i:02d}", "subsystem", name, route, tool, layer, cov, owner, status, note)

# ---------------------------------------------------------------- screens that are not in the APPS menu
# Dialogs opened by keys/long-press/messages, not by main_screen_start_app(): sin fixture de
# render propio, solo pares de color WCAG y (para freq) la logica compartida de teclado.
screens = [
    ("dialog_freq (entrada de frecuencia/memoria)", "src/dialog_freq.c; abierto por tecla FREQ/MEM",
     "check_wcag.py (pares de color), contrato de teclado compartido", "PARTIAL", "#197", "PENDING",
     "Sin fixture de render; no hay test de la ventana"),
    ("dialog_msg_cw (editor de mensajes CW)", "src/dialog_msg_cw.c; paginas mensajes CW",
     "check_wcag.py (pares de color); test_cw_encoder (codec, no ventana)", "PARTIAL", "#197", "PENDING",
     "Sin fixture de render; edicion real no instrumentada"),
    ("dialog_msg_voice (mensajes de voz)", "src/dialog_msg_voice.c; pagina voz",
     "check_wcag.py (pares de color); test_voice (TTS host)", "PARTIAL", "#197", "PENDING",
     "Sin fixture de render; reproduccion real no instrumentada"),
]
for i, (name, route, tool, cov, owner, status, note) in enumerate(screens, 1):
    add(f"SCR-{i:02d}", "screen", name, route, tool, "HOST", cov, owner, status, note)

# ---------------------------------------------------------------- rootfs / init services
# Todo rootfs/ + overlay del Buildroot: cada script de arranque/servicio tiene su fila. La
# verificacion posible en #195 es estatica (contenido en el artefacto); ejecutar el arranque es
# de #196 (QEMU/banco) o #199 (hardware).
services = [
    ("rcS (arranque de scripts init.d)", "rootfs/etc/init.d/rcS", "inspeccion del artefacto", "UNSUPPORTED",
     "#196", "PENDING", "Arranque real no ejecutado en #195 (sin kernel/init)"),
    ("S01boot_time (hora de arranque)", "rootfs/etc/init.d/S01boot_time", "inspeccion del artefacto",
     "UNSUPPORTED", "#196", "PENDING", "Requiere arranque; no ejecutado"),
    ("S20gui (arranque del GUI/daemon)", "rootfs/etc/init.d/S20gui", "inspeccion del artefacto",
     "UNSUPPORTED", "#196", "PENDING", "Requiere arranque; no ejecutado"),
    ("S39btpairings (restaura emparejamientos BT)", "rootfs/etc/init.d/S39btpairings",
     "inspeccion del artefacto", "UNSUPPORTED", "#196", "PENDING", "Requiere BT real; #199"),
    ("S94btcat (puente BT CAT)", "rootfs/etc/init.d/S94btcat; rootfs/etc/default/btcat",
     "inspeccion del artefacto", "UNSUPPORTED", "#196", "PENDING", "Requiere BT/CAT reales; #199"),
    ("S01create_data (crea/monta DATA)", "br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S01create_data",
     "inspeccion del artefacto (chequeo 'DATA ya existe')", "PARTIAL", "#196", "PENDING",
     "Contenido verificable en rootfs; creacion real de particion no ejecutada"),
    ("S02update (update desde /mnt/*.tgz)", "br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update",
     "inspeccion del artefacto", "UNSUPPORTED", "#196", "PENDING", "Update real no ejecutado ni autorizado"),
    ("btaudio-ctl (control de audio BT)", "rootfs/usr/sbin/btaudio-ctl", "inspeccion del artefacto",
     "UNSUPPORTED", "#199", "PENDING_HARDWARE", "Requiere audio BT real"),
    ("btcat-ctl / btcat-setup (CAT por BT)", "rootfs/usr/sbin/btcat-ctl, btcat-setup; rootfs/etc/default/btcat",
     "inspeccion del artefacto", "UNSUPPORTED", "#199", "PENDING_HARDWARE", "Requiere CAT/BT reales"),
    ("x6200-field (helper de Field Ops)", "rootfs/usr/sbin/x6200-field", "inspeccion del artefacto",
     "PARTIAL", "#196", "PENDING", "Logica cubierta por test_field en host; ejecucion en target no"),
    ("x6200-pota-spots (descarga de spots POTA/SOTA)", "rootfs/usr/sbin/x6200-pota-spots",
     "inspeccion del artefacto", "UNSUPPORTED", "#199", "PENDING_HARDWARE", "Requiere red real"),
    ("x6200_daemon (daemon de la GUI)", "rootfs/usr/sbin/x6200_daemon", "inspeccion del artefacto",
     "UNSUPPORTED", "#196", "PENDING", "Arranque real no ejecutado en #195"),
    ("S03tune_cache (sysctl vm.*)", "br2_external/.../init.d/S03tune_cache", "inspeccion del artefacto",
     "UNSUPPORTED", "#196", "PENDING", "Requiere arranque; no ejecutado"),
    ("S04restore_backup_config (restaura/backup /mnt/backups)", "br2_external/.../init.d/S04restore_backup_config",
     "inspeccion del artefacto", "UNSUPPORTED", "#196", "PENDING", "Toca DATA/backup; solo #196 con copia privada"),
    ("S11radio_daemon (DNS + BASE board /dev/ttyS1)", "br2_external/.../init.d/S11radio_daemon",
     "inspeccion del artefacto", "UNSUPPORTED", "#199", "PENDING_HARDWARE", "Requiere BASE board real"),
    ("S50alsa (alsactl restore)", "br2_external/.../init.d/S50alsa", "inspeccion del artefacto",
     "UNSUPPORTED", "#199", "PENDING_HARDWARE", "Requiere codec de audio real"),
    ("S50pulseaudio (pulseaudio --system)", "br2_external/.../init.d/S50pulseaudio", "inspeccion del artefacto",
     "UNSUPPORTED", "#199", "PENDING_HARDWARE", "Requiere audio real"),
    ("S96webserver (x6100_webserver --db /mnt/params.db)", "br2_external/.../init.d/S96webserver",
     "inspeccion del artefacto", "UNSUPPORTED", "#199", "SKIP", "Webserver omitido en #195 (volumen compartido)"),
]
for i, (name, route, tool, cov, owner, status, note) in enumerate(services, 1):
    add(f"SRV-{i:02d}", "service", name, route, tool, "HOST", cov, owner, status, note)

# ---------------------------------------------------------------- Buildroot packages / config / overlay
pkgs = [
    ("x6200-gui (override local al candidato)", "br2_external/package/x6200-gui/x6200_gui.mk",
     "build privado con X6200_GUI_OVERRIDE_SRCDIR; fuente-md5 en el artefacto", "PARTIAL", "#195", "PENDING",
     "El paquete vendor apunta a v0.1.0; el override es imprescindible"),
    ("ft8lib (headers+lib para FT8/QTH)", "br2_external/package/ft8lib/ft8lib.mk", "build ARM; CTest la requiere",
     "PARTIAL", "#196", "PENDING", "Su ausencia en host bloquea CTest (S4-CTEST)"),
    ("aether-x6200-control (control de radio)", "br2_external/package/aether-x6200-control/",
     "build ARM", "UNSUPPORTED", "#196", "PENDING", "Sin instrumento de control en #195"),
    ("x6100-webserver (webserver)", "br2_external/package/x6100-webserver/", "build ARM; WS no ejecutado",
     "UNSUPPORTED", "#199", "SKIP", "Requiere checkout web + python3 de target; no se adjudica a la GUI"),
    ("rhvoice (TTS de voz)", "br2_external/package/rhvoice/", "build ARM; test_voice (host)",
     "PARTIAL", "#197", "PENDING", "TTS real en target no ejecutado"),
    ("rtw88 / rtl8723du (WiFi)", "br2_external/package/rtw88/", "build ARM", "UNSUPPORTED", "#199",
     "PENDING_HARDWARE", "WiFi real solo en fase fisica"),
    ("python-bottle-sqlite (dep. webserver)", "br2_external/package/python-bottle-sqlite/", "build ARM",
     "UNSUPPORTED", "#199", "SKIP", "Dependencia del webserver omitido"),
]
for i, (name, route, tool, cov, owner, status, note) in enumerate(pkgs, 1):
    add(f"PKG-{i:02d}", "build_package", name, route, tool, "HOST", cov, owner, status, note)

bld = [
    ("X6200_defconfig", "br2_external/configs/X6200_defconfig", "br_config.sh; .config del build privado",
     "PARTIAL", "#195", "PASS", "Generado en el build privado (warning de override LIQUID_DSP registrado)"),
    ("genimage.cfg (BOOT + rootfs, sin DATA)", "br2_external/board/X6200/genimage.cfg",
     "inspeccion del artefacto", "REAL", "#196", "PENDING", "DATA no viene precargada; la crea S01create_data"),
    ("post-build.sh", "br2_external/board/X6200/post-build.sh", "inspeccion del artefacto",
     "PARTIAL", "#196", "PENDING", "Se aplica al rootfs; verificacion en el artefacto"),
    ("rootfs-overlay (fstab, init.d)", "br2_external/board/X6200/linux/rootfs-overlay/",
     "inspeccion del artefacto (fstab ext4 / DATA)", "REAL", "#196", "PENDING",
     "Contenido verificado en el artefacto de esta corrida"),
]
for i, (name, route, tool, cov, owner, status, note) in enumerate(bld, 1):
    add(f"BLD-{i:02d}", "build_config", name, route, tool, "HOST", cov, owner, status, note)

# ---------------------------------------------------------------- CI / build / toolchain inventory
extra = [
    ("CI ab-testing.yml (functional/contracts/visual/performance)", ".github/workflows/ab-testing.yml",
     "PR paths tools/**; ultima corrida del candidato: PR #193 head 08d604d9 run 39649140152 success",
     "HOST", "REAL", "#195", "PASS", "No hay corrida sobre el merge c888308e (push no dispara ab-testing)"),
    ("CI boot-rootfs.yml (prune)", ".github/workflows/boot-rootfs.yml",
     "push/PR paths buildroot/**, CMakeLists.txt, docs/IMAGE_ROOTFS_PRUNE.md",
     "HOST", "PARTIAL", "#196", "PENDING", "En el tip nuevo 50bf8ab0 la corrida 39653588113 fallo (#200, fuera del candidato)"),
    ("CI main.yml (imagen completa)", ".github/workflows/main.yml",
     "solo tags/workflow_dispatch; usa repo upstream por defecto",
     "HOST", "PARTIAL", "#195", "PENDING", "Un verde de CI no prueba el par congelado; se reproduce localmente"),
    ("Version de toolchain host", "gcc 16.2.1, cmake 4.3.0, python 3.14.7, uv 0.9.18, docker 29.8.2",
     "registrado en logs/step1-toolchain.log", "HOST", "REAL", "#195", "PASS", "ninja y fakeroot ausentes en host; sanitizers ausentes"),
    ("Imagen docker x6200-buildroot:latest", "sha256:b3e7601313d64284645a5224c60fb40f39ea58e0bfc68f42fd9bb0dad13a1189",
     "contenedor privado assessment195-build (usuario 1000)", "HOST", "REAL", "#195", "PASS",
     "No se uso el volumen compartido x6200-build-output"),
    ("Imagen docker x6200-uiharness-full:latest", "sha256:33fc1ed113e31978c600df82375628fe9d979d5c3408b4e16a42f43ae2f9312c",
     "codecs + sanitizers gcc 13", "HOST", "REAL", "#195", "PASS", "Usada solo como complemento de la baseline"),
]
for i, (name, route, tool, layer, cov, owner, status, note) in enumerate(extra, 1):
    add(f"ENV-{i:02d}", "ci_environment", name, route, tool, layer, cov, owner, status, note)

# ---------------------------------------------------------------- test inventory (auto)
def owner_for_test(path):
    p = path.lower()
    if "performance" in p or "bench" in p or "natural" in p or "wakeup" in p:
        return "#198"
    if "visual" in p or "contract" in p or "theme" in p or "typography" in p or "layout" in p or "nontext" in p or "contrast" in p:
        return "#197"
    if "verifica_firmware" in p or "build_identity" in p:
        return "#195"
    if p.startswith("tools/ab_testing/tests/test_e2e") or "e2e" in p:
        return "#196"
    if p.startswith("tools/ab_testing/tests/test_cli") or "git_scaffolding" in p or "known_issues" in p or "evaluator" in p or "scenarios" in p:
        return "#196"
    return "#195"

tracked = subprocess.check_output(["git", "-C", GUI, "ls-files", "tests", "tools/ab_testing/tests"],
                                  text=True).splitlines()
def is_test_source(p):
    """Prueba ejecutable versionada: incluye las de tests/ raiz (CTest) y tests/waterfall/."""
    b = os.path.basename(p)
    if p.startswith("tests/waterfall/"):
        return b.startswith("test_") and b.endswith(".c")
    if p.startswith("tests/native/"):
        return (b.startswith("test_") and b.endswith((".c", ".cpp"))) or b.endswith(".sh")
    if p.startswith("tests/"):
        if p.endswith((".py", ".sh")):
            return True
        return p.count("/") == 1 and b.startswith("test_") and b.endswith(".cpp")
    return p.startswith("tools/ab_testing/tests/") and p.endswith(".py")


executables = [p for p in tracked if is_test_source(p)]
ti = 0
for p in executables:
    ti += 1
    base = os.path.basename(p)
    owner = {"tests/test_ft8_qso.cpp": "#196", "tests/test_qth.cpp": "#196",
             "tests/waterfall/test_waterfall.c": "#198",
             "tests/native/test_js8_suite.cpp": "#195", "tests/native/test_wspr.c": "#195",
             "tests/native/test_sstv_loopback.cpp": "#195", "tests/native/test_wefax_loopback.cpp": "#195",
             "tests/native/test_navtex_loopback.cpp": "#195"}.get(p, owner_for_test(p))
    override = {
        "tests/test_ft8_qso.cpp": ("CTest (run_tests.sh)", "HOST", "UNSUPPORTED", "BLOCKED",
                                   "CTest no corre: falta ft8lib/constants.h + libasan; ver S4-CTEST"),
        "tests/test_qth.cpp": ("CTest (run_tests.sh)", "HOST", "UNSUPPORTED", "BLOCKED",
                               "CTest no corre: falta ft8lib/constants.h + libasan; ver S4-CTEST"),
        "tests/waterfall/test_waterfall.c": ("check_waterfall_incremental.sh (waterfall_test)",
                                             "HOST", "PARTIAL", "PENDING",
                                             "No ejecutado en #195 (requiere imagen/harness); #198"),
        "tests/native/test_js8_suite.cpp": ("run_js8_tests.sh (imagen full)", "HOST", "REAL", "PASS",
                                            "1994 aserciones en 88 casos"),
        "tests/native/test_wspr.c": ("run_wspr_tests.sh (imagen full)", "HOST", "REAL", "PASS",
                                     "158 comprobaciones"),
        "tests/native/test_sstv_loopback.cpp": ("run_sstv_tests.sh (imagen full)", "HOST", "REAL", "PASS",
                                                "7 modos, all OK"),
        "tests/native/test_wefax_loopback.cpp": ("run_wefax_tests.sh (imagen full)", "HOST", "REAL", "PASS",
                                                 "300/300 lineas"),
        "tests/native/test_navtex_loopback.cpp": ("run_navtex_tests.sh", "HOST", "REAL", "PASS", "all OK"),
        "tests/native/test_spectrum.c": ("run_native_tests.sh (spectrum)", "HOST", "PARTIAL", "PASS",
                                         "TSan omitido en host; TSan cubierto en contenedor (S4B-SANITIZERS)"),
        "tools/ab_testing/tests/test_e2e.py": ("pytest -m 'not e2e'", "HOST", "PARTIAL", "SKIP",
                                               "deseleccionado por marcador e2e (docker+git); dueño #196"),
    }
    if p in override:
        suite, layer, cov, status, note_t = override[p]
    elif p.startswith("tests/native/test_"):
        suite, layer, cov, status = "run_native_tests.sh (suite nativa)", "HOST", "REAL", "PASS"
    elif p.startswith("tests/native/run_"):
        suite, layer, cov, status = "suite de codec en imagen full", "HOST", "REAL", "PASS"
    elif p.startswith("tools/ab_testing/tests/"):
        suite, layer, cov, status = "uv run pytest -m 'not e2e'", "HOST", "REAL", "PASS"
    elif p.startswith("tests/check_") or p in ("tests/audita-pruebas.py",):
        suite, layer, cov, status = "run_all_tests.sh (checks estaticos/host)", "HOST", "REAL", "PASS"
    elif p.startswith("tests/"):
        suite, layer, cov, status = "run_all_tests.sh / manual", "HOST", "PARTIAL", "PENDING"
    else:
        suite, layer, cov, status = "n/a", "HOST", "PARTIAL", "PENDING"
    note = note_t if p in override else "Inventario de pruebas existentes; no se crean pruebas nuevas"
    add(f"TST-{ti:03d}", "test", base, p, suite, layer, cov, owner, status, note)

with open(OUT, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=["cov_id", "kind", "name", "route", "test_or_tool", "layer",
                                     "coverage", "owner_unit", "status", "notes"])
    w.writeheader()
    w.writerows(rows)

print(f"wrote {OUT}: {len(rows)} rows")
from collections import Counter
print("kind:", dict(Counter(r['kind'] for r in rows)))
print("coverage:", dict(Counter(r['coverage'] for r in rows)))
print("status:", dict(Counter(r['status'] for r in rows)))
print("owner:", dict(Counter(r['owner_unit'] for r in rows)))
