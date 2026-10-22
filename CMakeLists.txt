cmake_minimum_required(VERSION 3.23)

project(x6200_gui)

# Identidad de las FUENTES embebida en el binario (issue #165). Es el mismo hash
# que tools/compila-gui.sh deja en el sello .fuente.md5 del volumen y que calcula
# tools/fuente-md5.sh; src/main.c lo loguea al arrancar para que se pueda saber
# que codigo corre la radio SIN depender del md5 del binario (que no distingue
# commits: dos fuentes distintas pueden dar el mismo binario, o al reves).
#
# De donde sale, en orden: el archivo .fuente.md5 que compila-gui.sh escribe en la
# copia justo antes de compilar (autoritativo), la variable de entorno
# X6200_FUENTE_MD5 (por si el archivo no llego), o el mismo script calculado sobre
# la copia (build a mano / CI). Rutas relativas: el repo y la copia dan el mismo.
set(X6200_FUENTE_MD5 "desconocido")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.fuente.md5")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.fuente.md5" _x6200_fuente)
    string(STRIP "${_x6200_fuente}" X6200_FUENTE_MD5)
elseif(DEFINED ENV{X6200_FUENTE_MD5} AND NOT "$ENV{X6200_FUENTE_MD5}" STREQUAL "")
    set(X6200_FUENTE_MD5 "$ENV{X6200_FUENTE_MD5}")
else()
    execute_process(
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/fuente-md5.sh" "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE X6200_FUENTE_MD5
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT X6200_FUENTE_MD5)
        set(X6200_FUENTE_MD5 "desconocido")
    endif()
endif()
message(STATUS "x6200_gui: fuente md5 ${X6200_FUENTE_MD5}")

include_directories(.)
include_directories(third-party/rapidxml)
include_directories(third-party/utf8)

if(NOT ENABLE_TESTING)
add_compile_options(-mtune=cortex-a7 -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4)
endif()

add_subdirectory(lvgl)

if(ENABLE_TESTING)
        enable_testing()
        add_subdirectory(src/ft8)
        add_subdirectory(src/qth)
        add_subdirectory(tests)
else()
        add_subdirectory(third-party/js8core)
        add_subdirectory(src)
        add_subdirectory(btcat)
        add_subdirectory(lv_drivers)
        add_subdirectory(sql)
        install(TARGETS ${PROJECT_NAME} btcat x6200_bootanim DESTINATION sbin)
        install(DIRECTORY rootfs/
                DESTINATION /
                USE_SOURCE_PERMISSIONS
        )
        # El rootfs del vendor (el origen que Buildroot cachea para este paquete)
        # trae /etc/init.d/S95gui, que arranca el mismo x6200_daemon que el S20gui
        # del port: dos GUIs dibujan sobre /dev/fb0 (flicker + doble CPU, issue
        # #89). El target de Buildroot es incremental y no limpia lo que un
        # paquete dejo de instalar, asi que ese S95gui sobrevive a cada build; el
        # port declara lo prohibido en buildroot/prune-vendor-rootfs.sh y lo saca
        # en cada instalacion (ver docs/IMAGE_ROOTFS_PRUNE.md).
        install(CODE "
            execute_process(
                COMMAND sh \"${CMAKE_CURRENT_SOURCE_DIR}/buildroot/prune-vendor-rootfs.sh\" \"\$ENV{DESTDIR}\"
                RESULT_VARIABLE _x6200_prune_rc
            )
            if(NOT _x6200_prune_rc EQUAL 0)
                message(FATAL_ERROR \"prune-vendor-rootfs.sh fallo (rc=\${_x6200_prune_rc})\")
            endif()
        ")
        install(DIRECTORY images DESTINATION share/x6200)
endif()


