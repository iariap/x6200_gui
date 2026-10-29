add_executable(${PROJECT_NAME})

# El sello de las fuentes que compila este binario (issue #165): src/main.c lo
# loguea al arrancar. Solo al target de la GUI, para que un cambio de hash no
# recompile lvgl ni los otros targets.
target_compile_definitions(${PROJECT_NAME} PRIVATE X6200_FUENTE_MD5="${X6200_FUENTE_MD5}")

target_sources(${PROJECT_NAME} PUBLIC
    main.c main_screen.c
    boot_splash.c
    styles.c spectrum.c radio.c dsp.cpp util.cpp
    waterfall.c rotary.c rotary_queue.c keyboard.c encoder.c kbd_caps.c bt_state.c btaudio.c
    events.c msg.c msg_tiny.c keypad.c
    crashlog_modules.c
    lastbreath.c
    hkey.c clock.c info.c
    meter.c band_info.c tx_info.c
    audio.c mfk.cpp cw.cpp cw_decoder.c cw_autotune.c pannel.c
    rtty.c screenshot.c backlight.c cat.cpp
    dialog.c dialog_settings.c dialog_swrscan.c roe_curve.c swr_session.c
    propagation.c propagation_data.c dialog_propagation.c upload_conf.c dialog_logs.c
    dialog_ft8.c dialog_freq.c dialog_msg_cw.c
    ft8_udp.c
    dialog_msg_voice.c dialog_recorder.c dialog_qth.c dialog_gps.c gps_format.c dialog_callsign.c
    textarea_window.c cw_encoder.c buttons.cpp vol.c recorder.c
    cw_tune_ui.c adif.c qso_log.c scheduler.cpp
    update_check.c band_plan.c band_plan_data.c
    screen_rec.c avi_writer.c wfview_server.c
    dialog_wifi.c wifi.cpp controls_encoder.c controls_step.c voice.c voice_text.c controls.cpp usb_devices.cpp
    dialog_eq.cpp
    dfl_inline.c
    dfl_rotary.c
    dfl_presets.c
    dfl_presets_store.c
    dfl_context.c
    adaptive_tuning.c
    autotune.c autotune_ui.c autotune_spectrum.c
    visual_policy.c
    sql_policy.c
    audio_enhancement.c
    wspr/wspr.c wspr/wsprnet.c
    theme.c cJSON.c app_frame.c station.c tx_level.c
    tx_player.c dialog_wspr.c cluster/spots.c gps.c
    freq_list.c dialog_navtex.c navtex/decoder.cpp navtex/navtex_rx.cpp navtex/fftfilt.cxx
    dialog_sstv.c sstv/decoder.c sstv/encoder.c sstv/linesync.c sstv/preview.c sstv/vis.c
    dialog_wefax.c wefax/decoder.c wefax/encoder.c
    cluster/client.c cluster/spots.c dialog_cluster.c fonts/font_select.c
    kbd_rollover.c knobs.c remote_server.cpp cfg/db.cpp cfg/settings_manager.cpp cfg/cxx_bridge.c dialog_field.c field/refs.c field/flog.c field/rbn.c field/bands.c dialog_bands.c
    dialog_js8.c js8_map.c dialog_cw_kbd.c panel.cpp dialog_cw_trainer.c bt_devices.c dialog_bt.c dxcc.c flags.c input_dev.c gpio_safe.c cluster_column.c spot_marks.c dialog_status.c
    cb.c
    rx_audio_mux_guard.c
)

# A-prime (#367): helper guardian independiente de la GUI. Se compila con el mismo
# toolchain de la ROM y se instala en sbin; NO lo lanza x6200_daemon todavia (el
# guardian queda OFF hasta que #363 vuelva a A_PASS). Ver
# docs/evidencia/2026-10-29-rx-audio-a-prime/guardian-contract.md.
add_executable(x6200_audio_guard rx_audio_mux_guard.c rx_audio_mux_guard_main.c)

add_subdirectory(fonts)
add_subdirectory(ft8)
# Dibujante temprano del arranque (issue #192): ejecutable aparte, sin LVGL ni
# dependencias, que la GUI detiene antes de su primer dibujo (src/boot_splash.c).
add_subdirectory(bootanim)
add_subdirectory(widgets)
add_subdirectory(params)
add_subdirectory(qth)
add_subdirectory(wspr)
add_subdirectory(js8)
add_subdirectory(cfg)

include_directories(utf8)
include_directories(${CMAKE_SYSROOT}/usr/include/ft8lib/)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    libnm dbus-glib-1 sqlite3 libpng sndfile libudev libpulse libpulse-simple alsa speexdsp
)

# The DRM/KMS display path (lv_drivers/display/drm.c) needs libdrm
pkg_check_modules(drm REQUIRED IMPORTED_TARGET libdrm)


target_link_libraries(${PROJECT_NAME} PRIVATE
PkgConfig::deps
PkgConfig::drm
FT8 QTH
Threads::Threads
lvgl lvgl::drivers
aether_x6200_control
liquid
ft8
jpeg
)

# target_compile_options(${PROJECT_NAME} PRIVATE -g -fno-omit-frame-pointer -fasynchronous-unwind-tables)
# target_link_options(${PROJECT_NAME} PRIVATE -g -rdynamic)

# target_compile_options(${PROJECT_NAME} PRIVATE -g -fsanitize=address -fsanitize=undefined  -fno-omit-frame-pointer -fno-sanitize-recover)
# target_link_options(${PROJECT_NAME} PRIVATE -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover -static-libasan -static-libubsan)
