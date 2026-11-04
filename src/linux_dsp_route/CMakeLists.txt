# Infraestructura branch-local QA-LINUX-DSP-INTEGRATION (#362/#364).
# DEFAULT OFF: no se compila ni se instala salvo -DX6200_LINUX_DSP_ROUTE=ON.
add_library(x6200_linux_dsp_route STATIC
    route_shadow.c route_arbiter.c route_ipc.c route_supervisor.c
    route_client_worker.c route_wiring.c route_hooks.c route_transport_x6200.c
    route_proxy.c route_snapshot.c)
target_compile_definitions(x6200_linux_dsp_route PUBLIC X6200_LINUX_DSP_ROUTE=1)
if(X6200_CTRL_HAVE_BULK_WRITE)
    target_compile_definitions(x6200_linux_dsp_route PUBLIC X6200_CTRL_HAVE_BULK_WRITE=1)
endif()
target_include_directories(x6200_linux_dsp_route PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(x6200_linux_dsp_route PUBLIC pthread)
add_executable(x6200_route_arbiter route_daemon_main.c)
target_link_libraries(x6200_route_arbiter PRIVATE x6200_linux_dsp_route pthread)
add_executable(x6200_route_service route_service_main.c)
target_link_libraries(x6200_route_service PRIVATE x6200_linux_dsp_route pthread)
if(TARGET aether_x6200_control)
    target_link_libraries(x6200_route_service PRIVATE aether_x6200_control)
else()
    find_library(AETHER_X6200_CONTROL_LIB NAMES aether_x6200_control
                 PATHS "${CMAKE_SYSROOT}/usr/lib" "${CMAKE_SYSROOT}/usr/lib64" NO_DEFAULT_PATH)
    if(AETHER_X6200_CONTROL_LIB)
        target_link_libraries(x6200_route_service PRIVATE ${AETHER_X6200_CONTROL_LIB})
    endif()
endif()
install(TARGETS x6200_route_service DESTINATION sbin)
