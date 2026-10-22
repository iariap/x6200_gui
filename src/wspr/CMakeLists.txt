# WSPR decoder engine (K1JT's wsprd, vendored): float FFT + math only.
add_library(wsprd STATIC
    wsprd/fano.c
    wsprd/nhash.c
    wsprd/tab.c
    wsprd/wsprd.c
    wsprd/wsprd_utils.c
    wsprd/wsprsim_utils.c
)
target_include_directories(wsprd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/wsprd)
target_compile_options(wsprd PRIVATE -w)
target_link_libraries(wsprd PUBLIC fftw3f m)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wsprd)
target_link_libraries(${PROJECT_NAME} PRIVATE wsprd)
