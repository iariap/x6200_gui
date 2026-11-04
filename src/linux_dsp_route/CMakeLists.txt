# Infraestructura branch-local QA-LINUX-DSP-INTEGRATION (#362/#364).
# DEFAULT OFF: no se compila ni se instala salvo -DX6200_LINUX_DSP_ROUTE=ON.
add_library(x6200_linux_dsp_route STATIC
    route_shadow.c route_arbiter.c route_ipc.c route_supervisor.c)
target_include_directories(x6200_linux_dsp_route PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(x6200_linux_dsp_route PUBLIC pthread)
add_executable(x6200_route_arbiter route_daemon_main.c)
target_link_libraries(x6200_route_arbiter PRIVATE x6200_linux_dsp_route pthread)
