cmake_minimum_required(VERSION 3.23)

project(x6200_gui)

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
        install(TARGETS ${PROJECT_NAME} btcat DESTINATION sbin)
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


