add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
    main.c main_screen.c
    styles.c spectrum.c radio.c dsp.cpp util.cpp
    waterfall.c rotary.c keyboard.c encoder.c
    events.c msg.c msg_tiny.c keypad.c
    hkey.c clock.c info.c
    meter.c band_info.c tx_info.c
    audio.c mfk.cpp cw.cpp cw_decoder.c pannel.c
    rtty.c screenshot.c backlight.c cat.cpp
    dialog.c dialog_settings.c dialog_swrscan.c
    dialog_ft8.c dialog_freq.c dialog_msg_cw.c
    ft8_udp.c
    dialog_msg_voice.c dialog_recorder.c dialog_qth.c dialog_callsign.c
    textarea_window.c cw_encoder.c buttons.cpp vol.c recorder.c
    cw_tune_ui.c adif.c qso_log.c scheduler.cpp
    update_check.c band_plan.c band_plan_data.c
    screen_rec.c avi_writer.c wfview_server.c
    dialog_wifi.c wifi.cpp controls.cpp usb_devices.cpp
    dialog_eq.cpp dialog_about.c
    adaptive_tuning.c
    audio_enhancement.c
    wspr/wspr.c wspr/wsprnet.c
    theme.c cJSON.c app_frame.c freq_strip.c station.c tx_level.c
    tx_player.c dialog_wspr.c cluster/spots.c gps.c
    freq_list.c dialog_navtex.c navtex/decoder.cpp navtex/navtex_rx.cpp navtex/fftfilt.cxx
    dialog_sstv.c sstv/decoder.c sstv/encoder.c sstv/linesync.c sstv/preview.c sstv/vis.c
    dialog_wefax.c wefax/decoder.c wefax/encoder.c
    cluster/client.c cluster/spots.c dialog_cluster.c fonts/font_select.c
    kbd_rollover.c knobs.c remote_server.cpp cfg/db.cpp cfg/settings_manager.cpp cfg/cxx_bridge.c dialog_field.c field/refs.c field/flog.c field/rbn.c field/bands.c dialog_bands.c
    dialog_js8.c js8_map.c dialog_cw_kbd.c panel.cpp dialog_cw_trainer.c bt_devices.c dialog_bt.c
)

add_subdirectory(fonts)
add_subdirectory(ft8)
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


target_link_libraries(${PROJECT_NAME} PRIVATE
PkgConfig::deps
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
