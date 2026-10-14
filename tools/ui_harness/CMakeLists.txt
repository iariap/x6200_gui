# Host UI harness: compiles the real X6200 GUI sources for the PC with the
# repository's stock LVGL (submodule lvgl/, 8.3.11) and renders them headless
# to a PNG. See README.md.
#
# No external checkout is used: LVGL comes from the submodule and the sources
# from this repository. Build with the x6200-uiharness Docker image (also
# defined here) or any host that has libpng, libnm and sqlite3 headers.
cmake_minimum_required(VERSION 3.23)
project(ui_harness C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
file(GLOB MOCK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/mocks/*.c)

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
    ${FONTS}
)

# M_PI and the `case a ... b:` ranges the firmware uses need GNU extensions.
target_compile_options(ui_harness PRIVATE -D_GNU_SOURCE -Wall)

target_compile_definitions(ui_harness PRIVATE
    LV_CONF_INCLUDE_SIMPLE=1
    HARNESS_REVISION="${HARNESS_REVISION}")

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
