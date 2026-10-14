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
    ${REPO}/src/cw_tune_ui.c
    # La politica visual de bateria (UI-08 incr. 1): modulo puro que todavia no
    # llama nadie; se compila aca para que el -Wall del arnes lo cubra desde ya.
    ${REPO}/src/visual_policy.c
    # The shared dialog machinery and the real About window: the fixture
    # `dialog-about` opens it over the real main screen. The rest of the app
    # dialogs stay inert mocks (see mocks/mock_main_screen.c).
    ${REPO}/src/dialog.c
    ${REPO}/src/dialog_about.c
)

# The GUI's own C++ (button bar, MFK encoder state, controls, DSP helpers) and
# the real Subject/observer implementation from cfg/ (the C++ template).
set(GUI_CXX_SRC
    ${REPO}/src/buttons.cpp
    ${REPO}/src/mfk.cpp
    ${REPO}/src/controls.cpp
    ${REPO}/src/cfg/subjects.cpp
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

target_compile_definitions(ui_harness PRIVATE
    LV_CONF_INCLUDE_SIMPLE=1
    HARNESS_REVISION="${HARNESS_REVISION}"
    # The themes the ROM ships (the JSON files + the RS folder), read from this
    # repository instead of /usr/share: theme.c looks in THEMES_DIR_SHIPPED and
    # the harness sets it to this path before theme_init (harness_main.c). Same
    # files the image installs; nothing is read from the environment.
    HARNESS_THEMES_DIR_SHIPPED="${REPO}/rootfs/usr/share/x6200/themes")

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
