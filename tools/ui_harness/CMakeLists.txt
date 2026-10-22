# Host UI harness: compiles the real X6200 GUI sources for the PC with the
# repository's stock LVGL (submodule lvgl/, 8.3.11) and renders them headless
# to a PNG. See README.md.
#
# No external checkout is used: LVGL comes from the submodule and the sources
# from this repository. Build with the x6200-uiharness Docker image (also
# defined here) or any host that has libpng, libnm and sqlite3 headers.
cmake_minimum_required(VERSION 3.23)
# C++ is needed by the GUI's own C++ sources (buttons.cpp, mfk.cpp,
# controls.cpp) and by cfg/subjects.cpp; GCC accepts their designated
# initialisers as an extension, exactly as the ROM build does (the firmware
# sets no C++ standard either).
project(ui_harness C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# tools/ui_harness -> repository root
get_filename_component(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)

# --- LVGL from the submodule ------------------------------------------------
add_subdirectory(${REPO}/lvgl lvgl)

# Only the library is needed; skip the bundled examples/demos.
set_target_properties(lvgl_examples lvgl_demos PROPERTIES EXCLUDE_FROM_ALL ON)

# --- revision for the JSON --------------------------------------------------
# Pass -DHARNESS_REVISION=<sha> when building where git cannot see the
# worktree (e.g. inside the Docker image, whose mount hides .git).
set(HARNESS_REVISION "" CACHE STRING "Git revision embedded in the harness JSON")
if(NOT HARNESS_REVISION)
    execute_process(
        COMMAND git -C ${REPO} rev-parse HEAD
        OUTPUT_VARIABLE HARNESS_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT HARNESS_REVISION)
    set(HARNESS_REVISION "unknown")
endif()

# --- el modo del tick de la ROM ---------------------------------------------
# La ROM decide con LV_TICK_CUSTOM (lv_conf.h de la raiz) si el tick de LVGL sale
# de un hilo propio (`usleep` + `lv_tick_inc`) o del reloj monotono. El perfil de
# wakeups del arnes tiene que declarar el MISMO modo, y
# tests/wakeup_evidence.py lo cruza contra esa misma fuente: el arnes lee el
# lv_conf.h de la ROM en build time (no tiene uno propio para el tick: el suyo
# queda en LV_TICK_CUSTOM 0 porque avanza LVGL con `lv_tick_inc` desde el reloj
# virtual, tools/ui_harness/src/virtual_time.c).
file(READ ${REPO}/lv_conf.h ROM_LV_CONF)
if(ROM_LV_CONF MATCHES "#define[ \t]+LV_TICK_CUSTOM[ \t]+1")
    set(HARNESS_ROM_TICK_CUSTOM 1)
else()
    set(HARNESS_ROM_TICK_CUSTOM 0)
endif()

# --- el periodo de refresco de la ROM ---------------------------------------
# Un frame natural es un periodo de refresco de LVGL, y los dos modos que miden
# ese lazo (natural_bench.c y wakeup_bench.c) tienen que correr con el periodo
# que configura la ROM (lv_conf.h: LV_DISP_DEF_REFR_PERIOD). El arnes es un
# binario de host con su propio lv_conf.h, que no define ese valor, asi que LVGL
# le queda en su default (30 ms) y la medicion no seria la de la ROM. El valor
# sale de la misma fuente que LV_TICK_CUSTOM, en build time: no puede driftear
# del lv_conf.h de la ROM. Sin la definicion (build suelto) queda el default de
# LVGL, y los gates (tests/natural_evidence.py, tests/check_harness_wakeups.sh)
# leen el numero de lv_conf.h y lo cruzan con el JSON.
if(ROM_LV_CONF MATCHES "#define[ \t]+LV_DISP_DEF_REFR_PERIOD[ \t]+([0-9]+)")
    set(HARNESS_ROM_REFR_PERIOD ${CMAKE_MATCH_1})
else()
    set(HARNESS_ROM_REFR_PERIOD 30)   # el default de LVGL: la ROM no lo define
endif()

# --- theme images: styles.c points at A:/dev/shm on the radio ---------------
file(READ ${REPO}/src/styles.c STYLES_HOST)
string(REPLACE "A:/dev/shm/" "A:${REPO}/" STYLES_HOST "${STYLES_HOST}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/styles_host.c "${STYLES_HOST}")

file(GLOB FONTS ${REPO}/src/fonts/*.c)

file(GLOB HARNESS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB MOCK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/mocks/*.c ${CMAKE_CURRENT_SOURCE_DIR}/mocks/*.cpp)

# --- real firmware components ----------------------------------------------
# The main screen and the visual components it places (spectrum, waterfall,
# frequency row, band plan, buttons, panels, S-meter, info, clock). Their
# radio/DSP/hardware dependencies are the explicit mocks under mocks/.
set(GUI_SRC
    ${REPO}/src/main_screen.c
    ${REPO}/src/spectrum.c
    ${REPO}/src/waterfall.c
    ${REPO}/src/tx_info.c
    ${REPO}/src/band_plan.c
    ${REPO}/src/band_plan_data.c
    ${REPO}/src/pannel.c
    ${REPO}/src/vol.c
    ${REPO}/src/msg.c
    ${REPO}/src/msg_tiny.c
    ${REPO}/src/adaptive_tuning.c
    # CB 27 MHz (issue #146): el modulo puro del dial por canal y la etiqueta
    # "CH nn" de la celda VFO. main_screen.c e info.c lo llaman tal cual; las
    # bandas CB que dibuja la franja salen del catalogo de mocks/mock_cfg.c.
    ${REPO}/src/cb.c
    # El asistente de sintonia fina CW/CWR (issue #190, "Auto Tune v2"): el
    # nucleo puro y su coordinador LVGL. El coordinador corre sobre el main screen
    # real (mueve cfg_cur.fg_freq y lee la instantanea de CW), y los fixtures
    # `autotune*` lo disparan por el boton de APP 6 sobre el reloj virtual. La
    # senal de CW entra por inyeccion SOLO EN TESTS
    # (mocks/mock_cw_tune.c): src/cw.cpp no se compila aca.
    ${REPO}/src/autotune.c
    ${REPO}/src/autotune_ui.c
    ${REPO}/src/cw_tune_ui.c
    # La politica visual de bateria (UI-08 incr. 1): modulo puro que todavia no
    # llama nadie; se compila aca para que el -Wall del arnes lo cubra desde ya.
    ${REPO}/src/visual_policy.c
    # El modulo real del backlight (issue #13, UI-07 remanente): ahi vive la
    # ruta de pantalla apagada que el banco mide. backlight_switch() apaga la
    # invalidacion del display (lv_disp_enable_invalidation) y `--display-off`
    # la recorre por la tecla POWER del propio main screen
    # (src/main_screen.c -> src/backlight.c). Lo que el host sustituye son las
    # escrituras a sysfs (los fds quedan -1, el modulo ya es no-op) y el GPIO
    # del SDK (mocks/mock_ui_deps.c, third_party/).
    ${REPO}/src/backlight.c
    # The shared dialog machinery, the real Ajustes window and the real Bluetooth
    # window: the `settings-*` fixtures open Ajustes over the real main screen and
    # `bt-dialog` opens the Bluetooth app (UI-002/UI-005). The rest of the app
    # dialogs stay inert mocks (see mocks/mock_main_screen.c). `bt_devices.c` is the
    # dialog's pure parser (no UI, no LVGL); the script surface is mocked
    # (mocks/mock_btaudio.c, mocks/mock_ui_deps.c).
    ${REPO}/src/dialog.c
    ${REPO}/src/dialog_settings.c
    ${REPO}/src/dialog_bt.c
    ${REPO}/src/bt_devices.c
    # Editor DFL inline (issue #179): el panel REAL del editor de filtro, que los
    # fixtures `dfl-editor-*` abren por el camino del firmware
    # (main_screen_action(ACTION_APP_DFL_EDITOR) -> dfl_inline_expand()).
    # El modelo puro que consume vive en src/dfl_model.h.
    ${REPO}/src/dfl_inline.c
    # La aplicacion por los setters vigentes (sin LVGL): la comparte el arrastre
    # del grafico y la rueda.
    ${REPO}/src/dfl_rotary.c
    # Presets del DFL (issue #183): el catalogo/memorias/favoritos y la banda
    # contextual. El modelo se compila aca contra el doble en memoria
    # (mocks/mock_dfl_store.c); el SQL real lo prueba el test nativo.
    ${REPO}/src/dfl_presets.c
    ${REPO}/src/dfl_context.c
    # El barrido de ROE (issue #125): la ventana REAL, su motor puro de sesion y
    # el resample del chart. Los fixtures `swrscan-*` la abren por el camino del
    # firmware (main_screen_start_app(ACTION_APP_SWRSCAN)) y la manejan con el
    # backend fake de src/swr_backend.c sobre el reloj virtual; el lado de la
    # radio es mocks/mock_radio.c (contadores, sin RF). El gate de transmision
    # multipasada se compila ENCENDIDO en este binario de host (ver abajo) para
    # poder capturar las pasadas 3 y 5: el binario de la ROM lo deja apagado.
    ${REPO}/src/dialog_swrscan.c
    ${REPO}/src/swr_session.c
    ${REPO}/src/roe_curve.c
)

# The GUI's own C++ (button bar, MFK encoder state, controls, DSP helpers) and
# the real Subject/observer implementation from cfg/ (the C++ template).
# cfg/settings_manager.cpp is the remote control's mirror of the configuration
# (the ported X6100 SettingsManager): the harness compiles it so the mirror's
# wiring is exercised by the fixtures' boot -the "vol": 0 with vol=8 in
# params.db of issue #30 was its Params pointing at subjects that did not exist
# yet- and mock_cfg.c codifies the boot order (see its cfg_sm_init_live() call).
set(GUI_CXX_SRC
    ${REPO}/src/buttons.cpp
    ${REPO}/src/mfk.cpp
    ${REPO}/src/controls.cpp
    ${REPO}/src/cfg/subjects.cpp
    ${REPO}/src/cfg/settings_manager.cpp
)

add_executable(ui_harness
    ${HARNESS_SRC}
    ${MOCK_SRC}
    # Real firmware components
    ${CMAKE_CURRENT_BINARY_DIR}/styles_host.c
    ${REPO}/src/theme.c
    ${REPO}/src/cJSON.c
    ${REPO}/src/clock.c
    ${REPO}/src/info.c
    ${REPO}/src/meter.c
    ${REPO}/src/band_info.c
    ${GUI_SRC}
    ${GUI_CXX_SRC}
    ${FONTS}
)

# M_PI and the `case a ... b:` ranges the firmware uses need GNU extensions.
# -Wall lints the harness sources and the firmware sources alike; the firmware
# warnings it surfaces are upstream (the ROM compiles the same code, and -Wformat
# is on by default in GCC), never introduced by the harness code.
target_compile_options(ui_harness PRIVATE -D_GNU_SOURCE -Wall)

# The rendered state must not depend on how long the process has run, so the
# wall clock is another service the harness owns: `--wrap=time` rewrites every
# `time()` call of the linked firmware sources to `__wrap_time` (defined in
# mocks/mock_ui_deps.c), which returns a fixed epoch. The RTC/`time()` on the
# radio is replaced the same way `get_time()` is replaced by the virtual clock;
# nothing else in the linked objects calls `time()` (the power clock only does it
# in its CLOCK_TIME branch, which no fixture reaches).
#
# `--wrap=system` is the same idea for the Bluetooth app (UI-005): `src/dialog_bt.c`
# runs `btaudio-ctl` with system() on a worker thread, and the host must run
# nothing. `__wrap_system` (mocks/mock_btaudio.c) is a no-op that documents it; the
# state the dialog reads is seeded explicitly by the fixture.
target_link_options(ui_harness PRIVATE -Wl,--wrap=time -Wl,--wrap=system)

# Una llamada sin declaracion (C99 la acepta como `int`) devuelve un puntero
# truncado: es un fallo silencioso que el compilador avisa y el link suele dejar
# pasar. En este arnes ya costo un segfault (un `#include` que faltaba), asi que
# el aviso es un error.
target_compile_options(ui_harness PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)

target_compile_definitions(ui_harness PRIVATE
    LV_CONF_INCLUDE_SIMPLE=1
    HARNESS_REVISION="${HARNESS_REVISION}"
    HARNESS_ROM_TICK_CUSTOM=${HARNESS_ROM_TICK_CUSTOM}
    HARNESS_ROM_REFR_PERIOD=${HARNESS_ROM_REFR_PERIOD}
    # The themes the ROM ships (the six JSON files, docs/THEMES.md), read from this
    # repository instead of /usr/share: theme.c looks in THEMES_DIR_SHIPPED and
    # the harness sets it to this path before theme_init (harness_main.c). Same
    # files the image installs; nothing is read from the environment.
    HARNESS_THEMES_DIR_SHIPPED="${REPO}/rootfs/usr/share/x6200/themes"
    # Los datasets del barrido de ROE (issue #125), los mismos archivos que consume
    # el test nativo (tests/native/swrscan_fixtures.c): un solo origen de datos
    # para las dos superficies. La ruta la fija el CMake, no el entorno.
    HARNESS_SWR_FIXTURES_DIR="${REPO}/tests/fixtures/swrscan"
    # El gate de transmision multipasada (src/dialog_swrscan.h) se compila
    # ENCENDIDO **solo en este binario de host**, que no tiene radio: los
    # contadores de mocks/mock_radio.c son la unica "transmision" y el binario no
    # linkea ninguna libreria de RF. Hace falta encendido para poder capturar las
    # pasadas 3 y 5 (con el gate apagado la ventana rechaza la sesion y el fixture
    # capturaria una pantalla en Error). El binario de la ROM NO pasa este define
    # (src/CMakeLists.txt no lo tiene), asi que ahi el gate sigue apagado: el
    # test nativo test_swrscan_multipass_gate lo fija, y la cobertura de los
    # fixtures `swrscan-*` se declara PARTIAL por esto (docs/UI_HARNESS.md).
    SWRSCAN_MULTIPASS_TX_ENABLED=1)

target_include_directories(ui_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}          # lv_conf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src      # harness headers
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks    # mock headers
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party   # vendored SDK headers
    ${REPO}                              # lvgl/lvgl.h
    ${REPO}/src                          # styles.h, params/, cfg/, fonts/
    ${REPO}/third-party/utf8)

# lv_conf.h lives with the harness, not in the repository root.
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE=1)

find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NM REQUIRED libnm)

target_include_directories(ui_harness PRIVATE ${NM_INCLUDE_DIRS})
target_link_libraries(ui_harness PRIVATE lvgl PNG::PNG ${NM_LIBRARIES} m pthread)

# --- Draw-buffer / software-rotation probe (issue #147) ---------------------
# Links the same stock LVGL 8.3.11 and models the firmware's display exactly
# (480x800, sw_rotate, ROT_90) to prove that a partial draw buffer renders the
# same physical framebuffer. It is a measurement instrument: not linked into
# ui_harness, not shipped, no radio/network/GPU. See probe/disp_buf_probe.c and
# tests/check_drawbuf_partial.sh.
add_executable(disp_buf_probe ${CMAKE_CURRENT_SOURCE_DIR}/probe/disp_buf_probe.c)
target_compile_options(disp_buf_probe PRIVATE -D_GNU_SOURCE -Wall -Wextra
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)
target_include_directories(disp_buf_probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # harness lv_conf.h
    ${REPO})                      # lvgl/lvgl.h
target_link_libraries(disp_buf_probe PRIVATE lvgl m)

# --- Waterfall image format probe (issue #148, OPT-2, option B) --------------
# Measures the LVGL 8.3.11 draw cost of a 32-bit TRUE_COLOR image against an
# INDEXED_8BIT + palette one, same geometry and content: the data the option-B
# comparison of the experiment rests on. A measurement instrument, not linked
# into ui_harness. Run: wf_format_probe [reps].
add_executable(wf_format_probe ${CMAKE_CURRENT_SOURCE_DIR}/probe/wf_format_probe.c)
target_compile_options(wf_format_probe PRIVATE -D_GNU_SOURCE -Wall
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)
target_include_directories(wf_format_probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # harness lv_conf.h
    ${REPO})                      # lvgl/lvgl.h
target_compile_definitions(wf_format_probe PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
target_link_libraries(wf_format_probe PRIVATE lvgl m)

# --- Waterfall incremental rasterization fixtures (issue #148, OPT-2) --------
# Compiles the REAL src/waterfall.c against the stock LVGL 8.3.11 and a small
# stub layer (tests/waterfall/stubs/) that replaces the radio: params,
# cfg_cur/subjects, theme, palette, backlight and the scheduler. The source is
# staged next to the stub headers so the quoted `#include "theme.h"` of
# waterfall.c resolves to the stub and not to src/. Gate:
# tests/check_waterfall_incremental.sh. It is a measurement instrument, not
# linked into ui_harness (its fixtures use the new OPT-2 accessors, which do not
# exist in the baseline revision the A/B builds).
set(WF_TEST_STAGE ${CMAKE_CURRENT_BINARY_DIR}/waterfall_stage)
file(COPY ${REPO}/src/waterfall.c ${REPO}/src/waterfall.h DESTINATION ${WF_TEST_STAGE})
# spectrum.h aporta spectrum_filter_band() inline (issue #173, PR 1): los ticks
# del filtro del waterfall la comparten con el espectro sin linkear su modulo.
file(COPY ${REPO}/src/spectrum.h DESTINATION ${WF_TEST_STAGE})
file(COPY ${REPO}/tests/waterfall/stubs/ DESTINATION ${WF_TEST_STAGE})

add_executable(waterfall_test
    ${WF_TEST_STAGE}/waterfall.c
    ${REPO}/tests/waterfall/test_waterfall.c
    ${REPO}/tests/waterfall/stubs.c)
target_compile_options(waterfall_test PRIVATE -D_GNU_SOURCE -Wall
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)
target_include_directories(waterfall_test PRIVATE
    ${WF_TEST_STAGE}              # staged waterfall.h + stub headers
    ${CMAKE_CURRENT_SOURCE_DIR}   # harness lv_conf.h
    ${REPO})                      # lvgl/lvgl.h
target_compile_definitions(waterfall_test PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
# --wrap=lv_img_buf_alloc: el fixture de reserva fallida necesita forzar NULL
# sin tocar el codigo de produccion (tests/waterfall/stubs.c).
target_link_options(waterfall_test PRIVATE -Wl,--wrap=lv_img_buf_alloc)
target_link_libraries(waterfall_test PRIVATE lvgl m)
