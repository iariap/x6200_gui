/*
 *  Screen builders for the host UI harness.
 *
 *  The `main` screen is the firmware's own main_screen() (src/main_screen.c):
 *  it places the real spectrum, waterfall, frequency row, band plan, buttons,
 *  panel, message bars, clock, info, meter and TX info. The radio/DSP state
 *  comes from the explicit mocks; the spectrum/waterfall magnitudes come from
 *  the deterministic generator (src/signal_gen.c), because there is no SDR.
 *
 *  `main-foco` is the same screen with the real keyboard group, so the focus
 *  chain the firmware builds (main_screen registers its spectrum in the group)
 *  can be exercised with lv_group_focus_next().
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario.h"

#include "lvgl/lvgl.h"

#include "buttons.h"
#include "clock.h"
#include "dialog.h"
#include "dialog_settings.h"
#include "events.h"
#include "keyboard.h"
#include "main.h"
#include "main_screen.h"
#include "meter.h"
#include "params/params.h"
#include "rtty.h"
#include "signal_gen.h"
#include "styles.h"
#include "visual_policy.h"

#include "autotune_ui.h"
#include "swr_backend.h"
#include "virtual_time.h"

#include "mock_harness.h"
#include "mock_radio.h"

/* Voltios del fixture -> mV de la politica visual, redondeado: el binario de
 * float daria 11199 para 11.2 V si se truncara. */
static uint16_t volts_to_mv(float volts)
{
    return (uint16_t) (volts * 1000.0f + 0.5f);
}

/* The box the object actually shows on screen: its coordinates intersected with
 * the clipping of its ancestors (a parent without LV_OBJ_FLAG_OVERFLOW_VISIBLE
 * clips its children, exactly as lv_refr.c does) and with the viewport. An object
 * scrolled out of a scroll area has coordinates inside the screen but shows
 * nothing: the intersection is what tells the two apart. */
static bool visible_on_screen(lv_obj_t *obj)
{
    lv_area_t area;

    lv_obj_get_coords(obj, &area);

    for (lv_obj_t *parent = lv_obj_get_parent(obj); parent != NULL;
         parent = lv_obj_get_parent(parent)) {
        if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) {
            return false;
        }
        if (!lv_obj_has_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
            lv_area_t parent_area;

            lv_obj_get_coords(parent, &parent_area);
            if (!_lv_area_intersect(&area, &area, &parent_area)) {
                return false;
            }
        }
    }

    lv_area_t viewport = { 0, 0, lv_disp_get_hor_res(NULL) - 1, lv_disp_get_ver_res(NULL) - 1 };

    return _lv_area_intersect(&area, &area, &viewport);
}

/* Is a label with this exact text visible on screen? Used to codify what a
 * fixture claims to show (see scenario_settle): the harness stops if the row is
 * not visible, instead of capturing another group and reporting a green run. */
static bool label_on_screen(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        const char *obj_text = lv_label_get_text(obj);

        if (obj_text != NULL && strcmp(obj_text, text) == 0 && visible_on_screen(obj)) {
            return true;
        }
    }

    uint32_t children = lv_obj_get_child_cnt(obj);

    for (uint32_t i = 0; i < children; i++) {
        if (label_on_screen(lv_obj_get_child(obj, (int32_t) i), text)) {
            return true;
        }
    }

    return false;
}

/* El borde inferior en pantalla de la etiqueta visible con este texto exacto, o -1 si
 * ninguna calza. Es la metrica con la que el fixture de BT (UI-005) mide que la linea
 * de estado no quede debajo de la fila de botones: el defecto historico era texto
 * tapado por los botones, y "no se ve" no es una frase, es un numero. */
static int label_bottom(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        const char *obj_text = lv_label_get_text(obj);

        if (obj_text != NULL && strcmp(obj_text, text) == 0 && visible_on_screen(obj)) {
            lv_area_t area;

            lv_obj_get_coords(obj, &area);
            return (int) area.y2;
        }
    }

    uint32_t children = lv_obj_get_child_cnt(obj);

    for (uint32_t i = 0; i < children; i++) {
        int bottom = label_bottom(lv_obj_get_child(obj, (int32_t) i), text);

        if (bottom >= 0) {
            return bottom;
        }
    }

    return -1;
}

/* El lv_slider del arbol que esta mostrando este valor. El fixture de audio le da un valor
 * distinto a cada slider de ganancia justamente para poder identificar cual es cual sin leer
 * el texto de su etiqueta (que lo escribe el callback, recien cuando el usuario lo mueve). */
static lv_obj_t *slider_with_value(lv_obj_t *obj, int32_t value)
{
    if (lv_obj_check_type(obj, &lv_slider_class) && lv_slider_get_value(obj) == value) {
        return obj;
    }

    uint32_t children = lv_obj_get_child_cnt(obj);

    for (uint32_t i = 0; i < children; i++) {
        lv_obj_t *hit = slider_with_value(lv_obj_get_child(obj, (int32_t) i), value);

        if (hit != NULL) {
            return hit;
        }
    }

    return NULL;
}

static void build_main(const scenario_t *scenario)
{
    /* Push the scenario's radio state into the subjects before the components
     * read them (the firmware's radio module does this from the driver). */
    mock_radio_apply();

    /* The firmware's real main screen: loads the real components. */
    lv_obj_t *scr = main_screen();

    lv_scr_load(scr);

    /* Values the radio would report that main_screen() itself does not push:
     * the battery the clock draws and the signal the needle points at. */
    clock_update_power(mock_radio.external_v, mock_radio.battery_v,
                       mock_radio.battery_cap, mock_radio.charging);

    /* La politica visual (UI-08 incr. 2 y 3) sale del MISMO estado del pack que
     * el reloj: asi el banco miente el perfil igual que la radio (el ROM lo hace
     * en radio_tick, src/radio.c) y `--bench-natural` mide la cadencia que el
     * modo Ahorro del waterfall impone. Una sola evaluacion, con muestra
     * fresca (age 0): el fixture no tiene reloj. La aplicacion va por las mismas
     * funciones que el ROM usa, incluido el recorte de brillo: en el arnes el
     * backlight es un stub sin senal (mocks/mock_ui_deps.c), asi que el nivel no
     * toca un solo pixel y el banco sigue siendo determinista. */
    visual_policy_t        policy;
    visual_policy_result_t visual;

    /* El override sale del parametro persistido, igual que en el ROM
     * (src/radio.c): con AUTO lo decide la bateria del fixture; con un perfil
     * fijado, ese perfil manda. El arnes no tiene un perfil fijado (mock_params
     * deja VP_PARAM_AUTO), pero el camino es el mismo que el de la radio. */
    visual_policy_init(&policy);
    visual = visual_policy_eval(&policy,
                                volts_to_mv(mock_radio.battery_v),
                                volts_to_mv(mock_radio.external_v),
                                mock_radio.battery_cap, mock_radio.charging,
                                0u, 0u,
                                visual_policy_override_from_param(params.visual_profile.x));
    visual_policy_waterfall_apply(visual.profile, params.waterfall_battery.x);
    visual_policy_spectrum_apply(visual.profile);
    visual_policy_brightness_apply(visual.profile, params.brightness_normal);

    meter_update(mock_radio.smeter_db, 0.0f);

    /* Feed the components the FFT would: deterministic, seed-driven. */
    signal_gen_feed(scenario->seed);
}

bool scenario_build(const scenario_t *scenario)
{
    if (!strcmp(scenario->screen, "main") || !strcmp(scenario->screen, "home-usb-20m")) {
        build_main(scenario);
        return true;
    }

    if (!strcmp(scenario->screen, "main-foco")) {
        /* The real keyboard group has to exist before main_screen() arms its
         * 100 ms timer, so the spectrum ends up registered when it fires. */
        mock_keyboard_group_up();
        build_main(scenario);
        return true;
    }

    if (!strcmp(scenario->screen, "settings-home") || !strcmp(scenario->screen, "settings-wfbat")
        || !strcmp(scenario->screen, "settings-perfil")
        || !strcmp(scenario->screen, "settings-audio")) {
        /* Ajustes on top of the real main screen, opened by the firmware's own
         * path: main_screen_action(ACTION_APP_SETTINGS) -> main_screen_start_app()
         * -> dialog_construct(dialog_settings) (src/main_screen.c -> src/dialog.c
         * -> src/dialog_settings.c). The keyboard group is up first so
         * dialog_item() registers every widget and settings_goto() can move the
         * focus, exactly as in the radio. */
        mock_keyboard_group_up();
        build_main(scenario);
        main_screen_action(ACTION_APP_SETTINGS);
        return true;
    }

    if (!strcmp(scenario->screen, "settings-bands")) {
        /* Ajustes over the main screen, then the real Bands action run from there:
         * main_screen_action(ACTION_APP_BANDS), the dispatcher the Ajustes
         * long-press list (`APP Bands`, src/dialog_settings.c) reaches. The
         * keyboard group is up first because the Bands route goes through
         * dialog_construct()/dialog_init(), the same as every other app.
         *
         * `dialog_bands` is an inert mock on the host (mocks/mock_main_screen.c):
         * the Bands window is not rendered, so this fixture measures the ROUTE
         * (scenario_settle codifies what it must leave behind), never its pixels.
         * The app-menu button the user reported is `app-bands`. */
        mock_keyboard_group_up();
        build_main(scenario);
        main_screen_action(ACTION_APP_SETTINGS);
        main_screen_action(ACTION_APP_BANDS);
        return true;
    }

    if (!strcmp(scenario->screen, "app-bands")) {
        /* The route the owner reported: the APP key loads the app pages
         * (src/main_screen.c, KEYPAD_APP -> buttons_load_page_group(buttons_group_app))
         * and the "Bands" button of page 3 (APP 3:6) is pressed. The page
         * presses are positional and happen in scenario_settle, after the same
         * "is the button I am about to press really Bands?" check the
         * settings-wfbat fixture uses.
         *
         * `dialog_bands` is an inert mock here too: this fixture measures whether
         * the button reaches the Bands path at all (scenario_settle), which is
         * exactly what did not happen. */
        mock_keyboard_group_up();
        build_main(scenario);
        buttons_load_page_group(buttons_group_app);
        buttons_press(0, false);    /* (APP 1:6) -> (APP 2:6) */
        buttons_press(0, false);    /* (APP 2:6) -> (APP 3:6) */
        return true;
    }

    if (!strcmp(scenario->screen, "autotune") || !strcmp(scenario->screen, "autotune-progress")
        || !strcmp(scenario->screen, "autotune-found")) {
        /* El asistente de sintonia fina (issue #100): la pantalla principal real con
         * la pagina APP 6 cargada (radio y red: WiFi / BT / SWR Scan / Auto Tune).
         * El fixture pulsa el boton "Auto Tune" en scenario_settle, por el mismo
         * camino que el usuario (button_action_cb -> main_screen_action(ACTION_ATUNE)),
         * y espera el barrido sobre el reloj virtual. El S-meter del banco es
         * constante (mock_radio.smeter_db), asi que ningun probe supera al arranque y
         * el dial vuelve al punto de partida: eso es lo que captura `autotune`.
         * `autotune-progress` corta el barrido a mitad para capturar el dial en un
         * probe (el progreso visible). */
        mock_keyboard_group_up();
        build_main(scenario);
        buttons_load_page_group(buttons_group_app);
        for (unsigned i = 1; i < 6; i++) {
            buttons_press(0, false);    /* (APP n:6) -> (APP n+1:6) */
        }
        return true;
    }

    if (!strcmp(scenario->screen, "apps-menu")) {
        /* El menú de apps de la pantalla principal (UI-002): la tecla APP carga
         * `buttons_group_app` (src/main_screen.c, KEYPAD_APP) y el botón-página
         * cicla las 6 páginas (src/buttons.cpp, button_next_page_cb). El fixture
         * abre la página que pide `apps_page` desde la 1, pulsando el botón-página
         * las veces que haga falta; scenario_settle codifica que los rótulos de
         * esa página son los que quedaron en la barra. */
        unsigned page = (scenario->apps_page > 0) ? (unsigned) scenario->apps_page : 1u;

        mock_keyboard_group_up();
        build_main(scenario);
        buttons_load_page_group(buttons_group_app);
        for (unsigned i = 1; i < page; i++) {
            buttons_press(0, false);    /* (APP n:6) -> (APP n+1:6) */
        }
        return true;
    }

    if (!strcmp(scenario->screen, "bt-dialog")) {
        /* El diálogo real de Bluetooth (UI-005): la ruta del firmware
         * main_screen_start_app(ACTION_APP_BT) -> dialog_construct(dialog_bt)
         * (src/dialog_bt.c). El host no tiene `btaudio-ctl`, así que el mock
         * escribe el archivo que el script dejaría (/tmp/bt_ui_list.txt) con una
         * lista fija: el diálogo dibuja sus filas reales y su línea de estado. */
        mock_keyboard_group_up();
        build_main(scenario);
        mock_btaudio_seed_files();
        main_screen_start_app(ACTION_APP_BT);
        return true;
    }

    if (!strcmp(scenario->screen, "swrscan")) {
        /* El barrido de ROE real (issue #125) sobre el backend fake. La ruta es la del
         * firmware: main_screen_start_app(ACTION_APP_SWRSCAN) ->
         * dialog_construct(dialog_swrscan) (src/dialog_swrscan.c, compilado aca). El host no
         * tiene radio, asi que el lado de la radio lo hace src/swr_backend.c sobre el reloj
         * virtual: el fixture dice que dataset de tests/fixtures/swrscan/ replayar, cuantas
         * pasadas y hasta donde llegar. El backend fija el span del cfg con el del dataset
         * ANTES de abrir la ventana (construct_cb registra el observer del span), asi que la
         * grilla del dialogo es exactamente la del fixture. El grupo de teclado sube primero
         * porque dialog_construct -> dialog_init -> dialog_item. */
        mock_keyboard_group_up();
        build_main(scenario);

        if (!swr_backend_prepare(scenario)) {
            exit(1);
        }

        main_screen_start_app(ACTION_APP_SWRSCAN);
        return true;
    }

    if (!strcmp(scenario->screen, "app-close-home")) {
        /* menu-hygiene (decisiones 5 y 6): la app que NO es diálogo (RTTY) se abre
         * por la ruta del firmware (main_screen_start_app(ACTION_APP_RTTY)) y el
         * fixture la cierra en scenario_settle por el mismo camino que el control
         * remoto (main_screen_close_app). El grupo de teclado sube primero porque
         * la segunda mitad del fixture abre el Ajustes real encima de RTTY
         * (dialog_construct -> dialog_init -> dialog_item). */
        mock_keyboard_group_up();
        build_main(scenario);
        main_screen_start_app(ACTION_APP_RTTY);
        return true;
    }

    fprintf(stderr, "scenario: unknown screen '%s'\n", scenario->screen);
    return false;
}

/* S-meter sintetico del fixture `autotune-found` (issue #100): un pico en
 * `autotune_peak_hz`, mas fuerte cuanto mas cerca (el mismo modelo que usa el
 * test nativo, con signo de dBm: mas cerca = mas alto). Lo empuja el fixture en
 * cada slice del reloj virtual porque el banco no tiene DSP que actualice el
 * S-meter; el algoritmo que lo consume es el de la radio. */
static int32_t autotune_peak_hz;

static void autotune_found_slice(void *arg)
{
    int32_t d;

    (void) arg;

    d = subject_get_int(cfg_cur.fg_freq) - autotune_peak_hz;
    if (d < 0) {
        d = -d;
    }
    meter_update((int16_t) (-60 - d / 100), 0.0f);
}

void scenario_settle(const scenario_t *scenario)
{
    if (!strcmp(scenario->screen, "main-foco")) {
        /* Move the focus once the timers have run and the group is populated. */
        lv_group_focus_next(keyboard_group);
    }

    if (!strcmp(scenario->screen, "settings-home")) {
        /* El dialogo abre en su PRIMER grupo, que con el orden por utilidad del issue #109 es
         * Operation; esta fixture mide la fila de fecha/hora (grupo Clock), asi que salta ahi
         * como el usuario: `buttons_press(3)` es el tab "Clock" de la pagina 1 y llama a
         * settings_goto() (scroll + foco), que deja la fila Day/Month/Year arriba de la vista
         * igual que cuando Clock era el primer grupo. */
        if (!label_on_screen(lv_scr_act(), "Clock")) {
            fprintf(stderr,
                    "settings-home: no hay tab 'Clock' en la barra de abajo (¿se reordenaron "
                    "los tabs?)\n");
            exit(1);
        }
        buttons_press(3, false);    /* page 1, index 3: tab "Clock" */

        /* Lo que esta fixture *significa*: la fila de fecha (con el reloj deterministico del
         * arnes) esta a la vista. Codificado: si el grupo Clock se mueve, el arnes corta en vez
         * de capturar otra pantalla y reportar verde. */
        if (!label_on_screen(lv_scr_act(), "Day, Month, Year")) {
            fprintf(stderr,
                    "settings-home: la fila 'Day, Month, Year' no quedo en pantalla despues del "
                    "tab Clock (¿se movio el grupo?)\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "settings-wfbat")) {
        /* Jump to the Spectrum/waterfall group the way the user does: `buttons_press(2)` is
         * the page 1 "Spectrum" tab, which calls settings_goto() to scroll the grid and move
         * the focus (src/dialog_settings.c). Runs after the timers so the grid layout is
         * already computed; the callback requests LV_ANIM_OFF, so the resulting scroll is
         * immediate and deterministic. */
        buttons_press(2, false);    /* page 1, index 2: "Spectrum" */

        /* The presses are positional, and what this fixture *means* is that the
         * Spectrum/waterfall group's row is the one on screen now. Codified
         * instead of commented: if the bottom bar's tabs are ever reordered the
         * harness stops here instead of capturing a different group and still
         * printing a green determinism run. */
        if (!label_on_screen(lv_scr_act(), "Waterfall battery mode")) {
            fprintf(stderr,
                    "settings-wfbat: 'Waterfall battery mode' is not on screen after the "
                    "tab presses (did the bottom bar's tabs move?)\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "settings-perfil")) {
        /* La fila nueva vive en el grupo Apariencia (UI-08 incr. 5). Se salta
         * como el usuario: `buttons_press(4)` carga la pagina 2 con "More >" y
         * `buttons_press(1)` es el tab "Appearance", que llama a settings_goto()
         * (scroll + foco). El grupo entra entero en el viewport. */
        buttons_press(4, false);    /* page 1, index 4: "More >"      */
        buttons_press(1, false);    /* page 2, index 1: "Appearance" */

        /* Lo que esta fixture *significa*: el perfil VIGENTE que muestra la
         * etiqueta sale del automatico con la bateria del fixture (11.2 V ->
         * ECO), no del seteado (AUTO). Codificado: si el perfil de la bateria o
         * el nombre de la fila cambian, el arnes corta en vez de capturar otra
         * cosa y reportar verde. */
        if (!label_on_screen(lv_scr_act(), "Visual profile (Eco)")) {
            fprintf(stderr,
                    "settings-perfil: 'Visual profile (Eco)' is not on screen after the "
                    "Apariencia tab press (did the row move or the auto profile change?)\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "settings-audio")) {
        /* La pagina Ajustes -> Audio: el tab "Audio" es el indice 1 de la pagina 1 de la
         * barra de tabs (src/dialog_settings.c, tabs_pagina_1) y llama a settings_goto(),
         * que scrollea la grilla hasta el grupo y le mueve el foco. */
        if (!label_on_screen(lv_scr_act(), "Audio")) {
            fprintf(stderr,
                    "settings-audio: no hay tab 'Audio' en la barra de abajo (¿se reordenaron "
                    "los tabs?)\n");
            exit(1);
        }
        buttons_press(1, false);    /* page 1, index 1: tab "Audio" */

        /* Lo que esta fixture *significa*: la fila "Play,Rec gain" (los dos sliders de
         * ganancia) esta a la vista en el grupo Audio. Codificado: si el grupo o la fila se
         * mueven, el arnes corta en vez de capturar otra cosa y reportar verde. */
        if (!label_on_screen(lv_scr_act(), "Play,Rec gain")) {
            fprintf(stderr,
                    "settings-audio: la fila 'Play,Rec gain' no quedo en pantalla despues del "
                    "tab Audio (¿se movio el grupo?)\n");
            exit(1);
        }

        /* Los dos sliders se ven iguales: se los distingue por el valor que el fixture les
         * dio (play_gain_db 3, rec_gain_db -4). Faltando uno, la premisa del fixture no vale
         * y el arnes corta aca. */
        lv_obj_t *play = slider_with_value(lv_scr_act(), 3);
        lv_obj_t *rec  = slider_with_value(lv_scr_act(), -4);

        if (play == NULL || rec == NULL || play == rec) {
            fprintf(stderr,
                    "settings-audio: no encontre los dos sliders de ganancia por su valor "
                    "(play %s, rec %s): ¿el fixture o la fila cambiaron?\n",
                    play ? "ok" : "falta", rec ? "ok" : "falta");
            exit(1);
        }

        /* El usuario arrastra el slider de Rec gain: LVGL setea el valor y manda
         * LV_EVENT_VALUE_CHANGED, que es exactamente lo que se manda aca. El observable es el
         * setter que corre (mock_audio_last_setter(), mocks/mock_ui_deps.c): cada slider
         * tiene que llegar a SU ganancia. Con el callback de Rec apuntando a la funcion de
         * salida -el copiar/pegar del issue #26- el registro queda en `play` y la corrida
         * para; el fixture nunca sale verde midiendo lo que no es. */
        mock_audio_reset();
        lv_slider_set_value(rec, -7, LV_ANIM_OFF);
        lv_event_send(rec, LV_EVENT_VALUE_CHANGED, NULL);

        if (mock_audio_last_setter() != MOCK_AUDIO_REC || mock_audio_calls(MOCK_AUDIO_REC) != 1
            || mock_audio_calls(MOCK_AUDIO_PLAY) != 0) {
            fprintf(stderr,
                    "settings-audio: arrastrar el slider de Rec gain corrio %s (x%d) en vez de "
                    "audio_set_rec_vol() -> el slider de entrada mueve la salida (issue #26)\n",
                    mock_audio_last_setter() == MOCK_AUDIO_PLAY ? "audio_set_play_vol()"
                                                                : "ningun setter de audio",
                    mock_audio_calls(MOCK_AUDIO_PLAY));
            exit(1);
        }

        /* Y el estado: cada slider guardo en SU parametro. El de Play no se toco. */
        if (params.rec_gain_db_f.x != -7.0f || params.play_gain_db_f.x != 3.0f) {
            fprintf(stderr,
                    "settings-audio: despues de mover Rec gain quedo rec_gain_db_f=%f y "
                    "play_gain_db_f=%f (se esperaba -7 y 3)\n",
                    (double) params.rec_gain_db_f.x, (double) params.play_gain_db_f.x);
            exit(1);
        }

        if (mock_audio_last_db() != -7.0f) {
            fprintf(stderr, "settings-audio: el setter de entrada recibio %f en vez de -7\n",
                    (double) mock_audio_last_db());
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "settings-bands")) {
        /* What this fixture *means* is that the Bands action replaced the window
         * that was open and reached the Bands path — the two things the route
         * change fixes. Codified instead of commented, and checked on the
         * firmware's own state, because `dialog_bands` is an inert mock here: if
         * the action ever goes back to constructing the window on top of the
         * current one (the Ajustes window left running, `current_dialog`
         * overwritten and the Ajustes object orphaned) or reaches no dialog at
         * all, the harness stops here instead of capturing a green run.
         *
         * `dialog_settings` is the real descriptor (src/dialog_settings.c is
         * compiled); `run` stays true until the window's object is deleted. */
        if (dialog_settings->run) {
            fprintf(stderr,
                    "settings-bands: the Bands action left the Ajustes window running "
                    "(it was not closed before the Bands path opened)\n");
            exit(1);
        }
        if (!dialog_is_run()) {
            fprintf(stderr, "settings-bands: the Bands action reached no dialog at all\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "app-bands")) {
        /* The button this fixture presses is positional: codify that it is the
         * "Bands" one before pressing it, so a reordering of the APP pages stops
         * the run instead of pressing another app and still reporting green. */
        if (!label_on_screen(lv_scr_act(), "Bands")) {
            fprintf(stderr,
                    "app-bands: 'Bands' is not in the bottom bar after the page presses "
                    "(did the APP pages move?)\n");
            exit(1);
        }

        buttons_press(4, false);    /* (APP 3:6) index 4: "Bands" */

        /* What this fixture *means*: pressing the app-menu Bands button has to
         * reach the Bands path. With the window itself an inert mock, the
         * firmware's own state is the observable: a dialog has to be running. */
        if (!dialog_is_run()) {
            fprintf(stderr,
                    "app-bands: the app-menu Bands button reached no dialog at all "
                    "(the button did nothing)\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "autotune") || !strcmp(scenario->screen, "autotune-progress")
        || !strcmp(scenario->screen, "autotune-found")) {
        /* El boton que se pulsa es posicional: codificar que es "Auto Tune" antes de
         * pulsarlo, para que reordenar la pagina corte la corrida en vez de pulsar
         * otra app y reportar verde. */
        if (!label_on_screen(lv_scr_act(), "Auto\nTune")) {
            fprintf(stderr,
                    "autotune: 'Auto Tune' no esta en la barra despues de las pulsaciones "
                    "(¿se movio la pagina APP 6?)\n");
            exit(1);
        }

        buttons_press(4, false);    /* (APP 6:6) index 4: "Auto Tune" */

        /* Lo que el fixture *significa*: pulsar el boton arranca el asistente
         * (main_screen_action(ACTION_ATUNE) -> autotune_ui_start). Codificado sobre
         * el estado real del firmware: sin esto el banco capturaria un dial quieto
         * y reportaria verde. */
        if (!autotune_ui_running()) {
            fprintf(stderr,
                    "autotune: el boton 'Auto Tune' no arranco el asistente "
                    "(main_screen_action(ACTION_ATUNE) no llego)\n");
            exit(1);
        }

        if (!strcmp(scenario->screen, "autotune-progress")) {
            /* Cortar a mitad: unos 350 ms dejan el dial en el primer probe
             * (start +/- 500 Hz), el progreso visible. El barrido sigue corriendo;
             * el capture es un instante fijo del reloj virtual. */
            virtual_time_advance(350, 5);

            /* Lo que el fixture *significa*: a mitad del barrido el dial YA se movio
             * del arranque (el progreso visible) y el asistente sigue corriendo. Si
             * alguien saca el retuneo por probe, esto corta en vez de capturar un
             * dial quieto y reportar verde. */
            if (subject_get_int(cfg_cur.fg_freq) == (int32_t) mock_radio.frequency_hz) {
                fprintf(stderr,
                        "autotune-progress: el dial sigue en el arranque a los 350 ms "
                        "(no hay progreso visible)\n");
                exit(1);
            }
            if (!autotune_ui_running()) {
                fprintf(stderr,
                        "autotune-progress: el barrido ya termino a los 350 ms "
                        "(¿se acorto el plan?)\n");
                exit(1);
            }
        } else if (!strcmp(scenario->screen, "autotune-found")) {
            /* El banco no tiene DSP que actualice el S-meter (es constante,
             * mock_radio.smeter_db), asi que este fixture lo empuja por slice con un
             * pico sintetico a `start - 1500 Hz`: el barrido lo encuentra y el dial
             * se queda en el mejor spot. Es la evidencia del camino "spot recogido"
             * con el mismo algoritmo que corre en la radio. */
            autotune_peak_hz = (int32_t) mock_radio.frequency_hz - 1500;

            for (int i = 0; i < 1000 && autotune_ui_running(); i++) {
                virtual_time_advance_step(50, 5, autotune_found_slice, NULL);
            }
            if (autotune_ui_running()) {
                fprintf(stderr, "autotune-found: el barrido no termino (¿timer colgado?)\n");
                exit(1);
            }
            if (subject_get_int(cfg_cur.fg_freq) != autotune_peak_hz) {
                fprintf(stderr,
                        "autotune-found: el dial quedo en %d y no en el pico %d "
                        "(el barrido no encontro el mejor spot)\n",
                        (int) subject_get_int(cfg_cur.fg_freq), (int) autotune_peak_hz);
                exit(1);
            }
            virtual_time_advance(400, 10);
        } else {
            /* Correr el barrido entero sobre el reloj virtual. El tope corta una
             * corrida que no termina (un timer que no se apaga) en vez de colgar el
             * arnes; si el tope se alcanza, el fixture falla en vez de capturar a
             * mitad. */
            for (int i = 0; i < 1000 && autotune_ui_running(); i++) {
                virtual_time_advance(50, 5);
            }
            if (autotune_ui_running()) {
                fprintf(stderr, "autotune: el barrido no termino (¿timer colgado?)\n");
                exit(1);
            }

            /* Lo que el fixture *significa*: con el S-meter constante nadie supera al
             * arranque, asi que el dial tiene que quedar EXACTAMENTE en la frecuencia
             * del fixture (el punto de partida, nunca perdido). */
            if (subject_get_int(cfg_cur.fg_freq) != (int32_t) mock_radio.frequency_hz) {
                fprintf(stderr,
                        "autotune: el dial quedo en %d y no volvio al arranque %d "
                        "(el restore no corrio)\n",
                        (int) subject_get_int(cfg_cur.fg_freq), (int) mock_radio.frequency_hz);
                exit(1);
            }

            /* Dejar asentar el mensaje final (fade-in de 250 ms) antes del capture. */
            virtual_time_advance(400, 10);
        }
    }

    if (!strcmp(scenario->screen, "apps-menu")) {
        /* Lo que esta fixture *significa*: la pagina APP que se pidio es la que quedo
         * en la barra, con SUS apps. Los rotulos se leen de la tabla real
         * (`buttons_group_app`, src/buttons.cpp): si alguien reordena las paginas, el
         * arnes corta aca en vez de capturar otra pagina y reportar verde. */
        unsigned page = (scenario->apps_page > 0) ? (unsigned) scenario->apps_page : 1u;
        buttons_page_t *p = buttons_group_app[page - 1];
        char expected[16];

        snprintf(expected, sizeof(expected), "(APP %u:6)", page);

        if (!label_on_screen(lv_scr_act(), expected)) {
            fprintf(stderr,
                    "apps-menu: el boton-pagina '%s' no quedo en la barra (¿se reordenaron "
                    "las paginas APP?)\n",
                    expected);
            exit(1);
        }

        for (int i = 0; i < BUTTONS; i++) {
            button_item_t *it = p->items[i];

            if (it == NULL || it->type != BTN_TEXT) {
                continue;
            }
            if (!label_on_screen(lv_scr_act(), it->label)) {
                fprintf(stderr,
                        "apps-menu: la app '%s' de la pagina APP %u no quedo en la barra "
                        "(¿se movio de pagina?)\n",
                        it->label, page);
                exit(1);
            }
        }
    }

    if (!strcmp(scenario->screen, "app-close-home")) {
        /* menu-hygiene (decision 5): `{"cmd":"app","text":"off"}` sobre RTTY
         * abierta cierra la app de verdad. El fixture abrio RTTY en el build; aca
         * se la cierra por el mismo camino que usa el control remoto
         * (src/remote_server.cpp -> main_screen_close_app()). Lo que la fixture
         * *significa* son las dos cosas medibles: RTTY quedo OFF (el mock del
         * estado es stateful, mocks/mock_main_screen.c) y la barra volvio al home
         * (la pagina previa a la apertura), no a la de RTTY. */
        main_screen_close_app();

        if (rtty_get_state() != RTTY_OFF) {
            fprintf(stderr,
                    "app-close-home: 'off' no apago RTTY: sigue en %d (menu-hygiene 5)\n",
                    (int) rtty_get_state());
            exit(1);
        }
        if (buttons_get_cur_page() != &buttons_page_vol_1) {
            fprintf(stderr,
                    "app-close-home: cerrar RTTY dejo la barra en la pagina de la app, "
                    "no en el home (menu-hygiene 5)\n");
            exit(1);
        }

        /* menu-hygiene (decision 6): cerrar un dialogo abierto sobre RTTY vuelve al
         * estado pre-apertura de la barra (el home), no a la pagina "pegada" de
         * RTTY. Es la ruta de la repro del #50: RTTY -> dialogo -> cerrar. */
        main_screen_start_app(ACTION_APP_RTTY);
        main_screen_action(ACTION_APP_SETTINGS);
        main_screen_close_app();

        if (rtty_get_state() != RTTY_OFF) {
            fprintf(stderr, "app-close-home: el dialogo no apago RTTY (menu-hygiene 6)\n");
            exit(1);
        }
        if (buttons_get_cur_page() != &buttons_page_vol_1) {
            fprintf(stderr,
                    "app-close-home: cerrar el dialogo dejo la barra en la pagina de RTTY, "
                    "no en el home (menu-hygiene 6)\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "bt-dialog")) {
        /* UI-005: el defecto historico era la linea de estado del dialogo de BT -el
         * texto apoyado en la geometria vieja (DIALOG_H/DIALOG_Y)- tapada por la fila
         * de botones. La fila empieza en 480 - 62 (buttons.cpp, btn_height = 62). El
         * fixture codifica que la linea de estado existe y termina arriba de los
         * botones: si vuelve a caer debajo, el arnes corta en vez de capturar un
         * render tapado y reportar verde. */
        int bottom = label_bottom(lv_scr_act(), "no headphones, RX off");

        if (bottom < 0) {
            fprintf(stderr,
                    "bt-dialog: la linea de estado 'no headphones, RX off' no quedo en "
                    "pantalla (¿cambio el texto o el archivo de la lista?)\n");
            exit(1);
        }
        if (bottom >= 418) {
            fprintf(stderr,
                    "bt-dialog: la linea de estado (y2=%d) cae dentro de la fila de botones "
                    "(y=418): texto tapado por los botones (UI-005)\n",
                    bottom);
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "swrscan")) {
        /* El barrido de ROE corre aca, cuando los timers del dialogo ya corrieron: el
         * backend fake (src/swr_backend.c) pulsa Run —el mismo boton que el usuario— y
         * entrega un paquete del flow por punto sobre el reloj virtual, hasta donde pide el
         * fixture. Sin sleeps y sin RF: la unica "transmision" es el contador del mock
         * (mocks/mock_radio.c). El backend corta la corrida si la sesion no arranco, si el
         * VFO no quedo en la frecuencia del fixture o si el resultado no quedo congelado. */
        swr_backend_run(scenario);
    }
}

/* La ruta de la ROM para apagar el display (`--display-off`): la tecla POWER
 * suelta llega al main screen como en la radio —keypad.c hace
 * lv_event_send(lv_scr_act(), EVENT_KEYPAD, &event) y main_screen.c registra
 * main_screen_keypad_cb para ese evento—, y de ahi backlight_switch() apaga la
 * invalidacion del display (src/backlight.c, el modulo real). Lo que el host
 * sustituye es el teclado, no el camino. */
void scenario_display_off(void)
{
    event_keypad_t keypad;

    keypad.key = KEYPAD_POWER;
    keypad.state = KEYPAD_RELEASE;

    lv_event_send(lv_scr_act(), EVENT_KEYPAD, &keypad);
}
