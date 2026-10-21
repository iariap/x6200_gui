# Animador de arranque neo-retro (issue #192): un ejecutable nativo chico, sin
# LVGL, sin radio y sin dependencias, que dibuja en /dev/fb0 durante el arranque
# hasta que la GUI le pide la cesion (src/boot_splash.c).
#
# Se agrega desde src/CMakeLists.txt (add_subdirectory) y el install lo declara
# el CMakeLists.txt raiz, junto al resto de los binarios en sbin. Tambien se
# puede configurar solo (project + install) para validar el cross-compile del
# target con el toolchainfile.cmake de Buildroot, sin arrastrar el arbol de la
# GUI: `cmake -S src/bootanim -B <dir> -DCMAKE_TOOLCHAIN_FILE=<tc>`.
cmake_minimum_required(VERSION 3.23)

set(X6200_BOOTANIM_STANDALONE OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(x6200_bootanim C)
    set(X6200_BOOTANIM_STANDALONE ON)
endif()

# El sello de las fuentes (issue #165) lo calcula el CMakeLists raiz; aca se
# compila el mismo valor en el binario para poder saber que codigo corre.
if(NOT DEFINED X6200_FUENTE_MD5)
    set(X6200_FUENTE_MD5 "desconocido")
endif()

add_executable(x6200_bootanim
    main.c
    renderer.c
    fb.c
)

target_compile_options(x6200_bootanim PRIVATE -std=c11 -Wall -Wextra -Werror)
target_compile_definitions(x6200_bootanim PRIVATE X6200_FUENTE_MD5="${X6200_FUENTE_MD5}")

# En el proyecto de la GUI el install lo declara el CMakeLists.txt raiz (junto
# al de x6200_gui y btcat); configurado solo, se declara aca.
if(X6200_BOOTANIM_STANDALONE)
    install(TARGETS x6200_bootanim DESTINATION sbin)
endif()
