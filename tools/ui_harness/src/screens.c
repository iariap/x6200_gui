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
#include "dialog_dfl.h"
#include "dialog_settings.h"
#include "events.h"
#include "keyboard.h"
#include "main.h"
#include "main_screen.h"
#include "meter.h"
#include "params/params.h"
#include "rtty.h"
#include "signal_gen.h"
#include "spectrum.h"
#include "styles.h"
#include "theme.h"
#include "visual_policy.h"

#include "cfg/cfg.h"
#include "cfg/mode.h"

#include "autotune_ui.h"
#include "swr_backend.h"
#include "cw_tune_ui.h"
#include "virtual_time.h"
#include "waterfall.h"

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

/* Como label_on_screen, pero calza cuando el texto visible TERMINA en `suffix`. La
 * fila de frecuencia lleva el color del tema como markup de recolor de LVGL
 * (`#RRGGBB 27.185`), asi que el match exacto no la ve; el sufijo si (issue #169). */
static bool label_suffix_on_screen(lv_obj_t *obj, const char *suffix)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        const char *obj_text = lv_label_get_text(obj);

        if (obj_text != NULL && visible_on_screen(obj)) {
            size_t n = strlen(obj_text), m = strlen(suffix);

            if (m <= n && strcmp(obj_text + (n - m), suffix) == 0) {
                return true;
            }
        }
    }

    uint32_t children = lv_obj_get_child_cnt(obj);

    for (uint32_t i = 0; i < children; i++) {
        if (label_suffix_on_screen(lv_obj_get_child(obj, (int32_t) i), suffix)) {
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

/* El widget de CW tune (src/cw_tune_ui.c) entre los hijos directos de la pantalla:
 * el objeto base de 139x40, que son la WIDTH y la HEIGHT que el modulo se da
 * (N_BLOCKS * (BLOCK_W + SPACING) + SPACING y HEIGHT, src/cw_tune_ui.c). El modulo
 * guarda su objeto en una variable estatica sin accesor, asi que el fixture lo
 * identifica por su geometria; se exige exactamente uno, para que un cambio de
 * tamano (o un objeto gemelo) corte la corrida en vez de medir otra cosa.
 * Devuelve NULL si no hay exactamente un candidato. */
static lv_obj_t *cw_tune_widget(lv_obj_t *screen)
{
    lv_obj_t *found    = NULL;
    uint32_t  matches  = 0;
    uint32_t  children = lv_obj_get_child_cnt(screen);

    for (uint32_t i = 0; i < children; i++) {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t) i);

        if (lv_obj_check_type(child, &lv_obj_class) && lv_obj_get_width(child) == 139
            && lv_obj_get_height(child) == 40) {
            found = child;
            matches++;
        }
    }

    return (matches == 1) ? found : NULL;
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

/* Utilidades de los fixtures del editor DFL (definidas mas abajo, junto a
 * scenario_settle): se usan en scenario_build. */
static void dfl_press_ui(dfl_ui_id_t id);
static void dfl_drag_ui(int32_t local_x);
static void dfl_rotary_ui(int32_t diff);
static void dfl_esc_ui(void);
static void settle_dfl_editor(const scenario_t *scenario);

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
        || !strcmp(scenario->screen, "settings-audio")
        || !strcmp(scenario->screen, "settings-grupo")) {
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

    if (!strcmp(scenario->screen, "cw-tune-default") || !strcmp(scenario->screen, "cw-tune-tuner")
        || !strcmp(scenario->screen, "cw-tune-ssb") || !strcmp(scenario->screen, "cw-autotune-off")
        || !strcmp(scenario->screen, "cw-autotune-on")) {
        /* El widget de CW tune (issue #122): la pantalla principal real con la pagina CW
         * de la barra cargada. La ruta es la del usuario: la tecla KEY carga
         * `buttons_group_key` (src/main_screen.c, KEYPAD_KEY) y el boton-pagina cicla
         * hasta `page_cw_decoder_1` (src/buttons.cpp), la pagina "(CW 1:2)" que lleva
         * Decoder / Tuner / Dec SN R. `cw-tune-tuner` pulsa ademas el boton "Tuner" en
         * scenario_settle, por el mismo camino que el usuario (button_action_cb ->
         * controls_toggle_cw_tuner -> cfg.cw_tune). `cw-tune-ssb` no carga esa pagina:
         * es el control de la otra mitad de la regla de visibilidad (fuera de CW/CWR el
         * widget no se muestra), asi que solo arma la pantalla principal. */
        mock_keyboard_group_up();
        build_main(scenario);

        if (strcmp(scenario->screen, "cw-tune-ssb")) {
            buttons_load_page_group(buttons_group_key);
            buttons_press(0, false);    /* (KEY 1:2) -> (KEY 2:2) */
            buttons_press(0, false);    /* (KEY 2:2) -> (CW 1:2)  */

            /* Los fixtures de AUTO (issue #139) siguen un paso mas, a la pagina
             * (CW 2:2), que es donde vive el boton "Auto" junto a Peak/Noise
             * beta: la ruta es la del usuario (boton-pagina), no un atajo. */
            if (!strcmp(scenario->screen, "cw-autotune-off") || !strcmp(scenario->screen, "cw-autotune-on")) {
                buttons_press(0, false);    /* (CW 1:2) -> (CW 2:2)  */
            }
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

    if (!strcmp(scenario->screen, "dfl-editor") || !strcmp(scenario->screen, "dfl-editor-tx")
        || !strcmp(scenario->screen, "dfl-editor-interaction")) {
        /* Editor DFL (issue #173, PR 2): la ventana REAL, abierta por el camino
         * del firmware main_screen_action(ACTION_APP_DFL_EDITOR) ->
         * dialog_construct(dialog_dfl) (src/main_screen.c -> src/dialog.c ->
         * src/dialog_dfl.c). El fixture distingue USB-D/LSB-D/temas/TX por sus
         * claves; `dfl-editor-interaction` ademas empuja pulsaciones y un
         * arrastre por los callbacks reales. El grupo de teclado sube primero
         * porque el dialogo lo usa si existe. */
        mock_keyboard_group_up();
        build_main(scenario);
        main_screen_action(ACTION_APP_DFL_EDITOR);

        if (!strcmp(scenario->screen, "dfl-editor-interaction")) {
            dfl_rotary_ui(1);                   /* LOW (default): 225 -> 230 */
            dfl_rotary_ui(1);                   /* LOW 230 -> 240 */
            dfl_press_ui(DFL_UI_HIGH_ROW);      /* la fila tocada SELECCIONA HIGH */
            dfl_rotary_ui(1);                   /* HIGH 3525 -> 3550 (paso 50, cuantiza) */
            dfl_press_ui(DFL_UI_BW_ROW);        /* selecciona BW */
            dfl_rotary_ui(1);                   /* BW 3300 -> 3320 (paso 20, legal) */
            dfl_drag_ui(300);                   /* arrastra HIGH -> 7000 y lo selecciona */
            dfl_drag_ui(500);                   /* fuera del grafico: HIGH se recorta a MAX=10000 */
            dfl_rotary_ui(1);                   /* HIGH 10050 > MAX: la guarda rechaza, queda 10000 */
            dfl_rotary_ui(1);
        }

        return true;
    }

    if (!strcmp(scenario->screen, "dfl-editor-cycles")) {
        /* 30 ciclos abrir/cerrar del editor con cambios de modo y frecuencia
         * entre medio, y los tres caminos de cierre (ESC fisico, "Cerrar DFL"
         * tactil y cierre directo). Queda abierto al final: el arnes captura el
         * ultimo estado y comprueba que los Subjects/labels no quedaron
         * desfasados (settle_dfl_editor). */
        mock_keyboard_group_up();
        build_main(scenario);

        for (int i = 0; i < 30; i++) {
            main_screen_action(ACTION_APP_DFL_EDITOR);
            subject_set_int(cfg_cur.mode, (i % 2) ? x6200_mode_lsb_dig : x6200_mode_usb_dig);
            subject_set_int(cfg_cur.fg_freq, 14074000 + i * 1000);

            switch (i % 3) {
                case 0:
                    dfl_esc_ui();
                    break;
                case 1:
                    buttons_press(4, false);    /* quinta celda: "Cerrar DFL" */
                    break;
                default:
                    dialog_destruct();
                    break;
            }
        }

        subject_set_int(cfg_cur.mode, x6200_mode_usb_dig);
        main_screen_action(ACTION_APP_DFL_EDITOR);
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

/* Linea central del waterfall (issue #140). El issue reporta una linea de largo
 * cero; en el trunk no se reproduce (medido en el banco: 3 px de punta a punta),
 * pero el invariante es geometrico, exacto y barato de codificar, y es lo que
 * evita que vuelva: con `waterfall_center_line` encendido la linea existe, no esta
 * oculta, mide el alto del waterfall mas su propio trazo, esta centrada
 * horizontalmente y es el ultimo hijo del waterfall (se dibuja sobre la paleta);
 * con el parametro apagado no se ve. Se mide sobre los objetos vivos de cada
 * render, no sobre pixeles, y el arnes corta aca en vez de capturar una pantalla
 * sin linea y dar verde. */
static void settle_waterfall_center_line(const scenario_t *scenario)
{
    lv_obj_t *line = waterfall_middle_line();

    if (!params.waterfall_center_line.x) {
        if (line != NULL && !lv_obj_has_flag(line, LV_OBJ_FLAG_HIDDEN)) {
            fprintf(stderr,
                    "%s: la linea central del waterfall se ve con el parametro apagado "
                    "(src/waterfall.c, draw_middle_line)\n",
                    scenario->name);
            exit(1);
        }
        return;
    }

    if (line == NULL) {
        fprintf(stderr,
                "%s: no hay linea central del waterfall con el parametro encendido "
                "(src/waterfall.c, draw_middle_line)\n",
                scenario->name);
        exit(1);
    }

    lv_obj_t *wf = lv_obj_get_parent(line);

    /* El estilo no es decoracion: es el fix del owner del 2026-10-18. La linea
     * tiene que salir con el ancho del tema (3 px por default, 1..8 si el archivo
     * lo pide), a opacidad plena y con el rol `text` del tema, que es el que mide
     * >= 3:1 contra el fondo del waterfall en los seis temas (docs/THEMES.md,
     * tools/ab_testing/quality/nontext.yaml). Un cambio de color, ancho u opacidad
     * que vuelva a dejar la linea invisible tiene que romper aca, no en la radio. */
    lv_coord_t ancho = lv_obj_get_style_line_width(line, LV_PART_MAIN);

    if (ancho != theme_shape.waterfall_signal_width) {
        fprintf(stderr,
                "%s: la linea central mide %d px y el tema pide %d ("
                "src/waterfall.c, waterfall_init)\n",
                scenario->name, (int) ancho, (int) theme_shape.waterfall_signal_width);
        exit(1);
    }

    lv_opa_t opa = lv_obj_get_style_line_opa(line, LV_PART_MAIN);

    if (opa != LV_OPA_COVER) {
        fprintf(stderr,
                "%s: la linea central sale con opacidad %d, no LV_OPA_COVER: se lava "
                "contra el waterfall (src/waterfall.c, waterfall_init)\n",
                scenario->name, (int) opa);
        exit(1);
    }

    lv_color_t color = lv_obj_get_style_line_color(line, LV_PART_MAIN);
    lv_color_t esperado = lv_color_hex(theme_hex(TC_text));

    if (color.full != esperado.full) {
        fprintf(stderr,
                "%s: la linea central no usa el rol text del tema (#%06x vs #%06x): "
                "no alcanza el 3:1 contra el fondo del waterfall (src/waterfall.c)\n",
                scenario->name, (unsigned) color.full, (unsigned) esperado.full);
        exit(1);
    }

    if (lv_obj_has_flag(line, LV_OBJ_FLAG_HIDDEN)) {
        fprintf(stderr,
                "%s: la linea central del waterfall esta oculta con el parametro encendido "
                "(src/waterfall.c, middle_line_cb)\n",
                scenario->name);
        exit(1);
    }

    /* El objeto linea mide, por su tamano propio, el punto mas lejano de sus
     * puntos mas el ancho del trazo (lv_line, LV_EVENT_GET_SELF_SIZE). Si los
     * puntos siguen en (0,0)/(0,0) mide solo el trazo, no el waterfall: ese es
     * exactamente el defecto del issue #140. */
    lv_coord_t trazo = lv_obj_get_style_line_width(line, LV_PART_MAIN);
    lv_coord_t alto = lv_obj_get_height(wf);

    if (lv_obj_get_height(line) != alto + trazo) {
        fprintf(stderr,
                "%s: la linea central mide %d px y el waterfall %d (+%d de trazo): largo cero "
                "o alto viejo (issue #140, src/waterfall.c draw_middle_line)\n",
                scenario->name, (int) lv_obj_get_height(line), (int) alto, (int) trazo);
        exit(1);
    }

    lv_area_t linea, wf_area;

    lv_obj_get_coords(line, &linea);
    lv_obj_get_coords(wf, &wf_area);

    int centro_linea = linea.x1 + (linea.x2 - linea.x1 + 1) / 2;
    int centro_wf    = wf_area.x1 + (wf_area.x2 - wf_area.x1 + 1) / 2;

    if (centro_linea - centro_wf > 1 || centro_wf - centro_linea > 1) {
        fprintf(stderr,
                "%s: la linea central quedo en x=%d y el centro del waterfall es x=%d "
                "(src/waterfall.c, draw_middle_line)\n",
                scenario->name, centro_linea, centro_wf);
        exit(1);
    }

    if (lv_obj_get_child(wf, lv_obj_get_child_cnt(wf) - 1) != line) {
        fprintf(stderr,
                "%s: la linea central no es el ultimo hijo del waterfall (se dibuja debajo "
                "de la paleta: no se veria)\n",
                scenario->name);
        exit(1);
    }
}

/* Ticks del ancho del filtro RX en el borde superior de la cascada (issue #173,
 * PR 1). El invariante es geometrico: dos objetos linea hijos del waterfall,
 * sobre su borde de arriba, en las x que da el MISMO spectrum_filter_band() que
 * usa el firmware (asi el arnes no puede estar de acuerdo con una formula
 * distinta a la que dibuja). Si la banda no es visible, los dos ticks tienen que
 * estar ocultos. */
static void settle_waterfall_filter_ticks(const scenario_t *scenario)
{
    lv_obj_t *t0 = waterfall_filter_tick(0);
    lv_obj_t *t1 = waterfall_filter_tick(1);

    if (t0 == NULL || t1 == NULL) {
        fprintf(stderr,
                "%s: no hay ticks del filtro RX (issue #173, src/waterfall.c "
                "draw_filter_ticks)\n",
                scenario->name);
        exit(1);
    }

    /* Los ticks son hijos de la IMAGEN del waterfall (su superficie de pixeles),
     * que a su vez es hija del objeto del waterfall. La geometria se mide contra
     * la imagen, que es donde caen los pixeles de la cascada. */
    lv_obj_t *surface = lv_obj_get_parent(t0);
    lv_obj_t *wf      = (surface != NULL) ? lv_obj_get_parent(surface) : NULL;

    if (surface == NULL || wf == NULL || lv_obj_get_parent(t1) != surface) {
        fprintf(stderr, "%s: los ticks del filtro no son hijos de la imagen del waterfall "
                "(src/waterfall.c)\n",
                scenario->name);
        exit(1);
    }

    spectrum_filter_band_t band = spectrum_filter_band(subject_get_int(cfg_cur.filter.real.from),
                                                       subject_get_int(cfg_cur.filter.real.to),
                                                       subject_get_int(cfg_cur.fft_width),
                                                       lv_obj_get_width(surface));

    if (!band.visible) {
        if (!lv_obj_has_flag(t0, LV_OBJ_FLAG_HIDDEN) || !lv_obj_has_flag(t1, LV_OBJ_FLAG_HIDDEN)) {
            fprintf(stderr, "%s: ticks del filtro visibles sin banda (src/waterfall.c)\n",
                    scenario->name);
            exit(1);
        }
        return;
    }

    if (lv_obj_has_flag(t0, LV_OBJ_FLAG_HIDDEN) || lv_obj_has_flag(t1, LV_OBJ_FLAG_HIDDEN)) {
        fprintf(stderr, "%s: ticks del filtro ocultos con la banda visible (src/waterfall.c)\n",
                scenario->name);
        exit(1);
    }

    lv_area_t a0, a1, sa;

    lv_obj_get_coords(t0, &a0);
    lv_obj_get_coords(t1, &a1);
    lv_obj_get_coords(surface, &sa);

    int x0 = a0.x1 - sa.x1;
    int x1 = a1.x1 - sa.x1;

    if (x0 != band.left || x1 != band.right) {
        fprintf(stderr,
                "%s: ticks del filtro en x=%d/%d, esperado %d/%d (issue #173, "
                "src/waterfall.c draw_filter_ticks)\n",
                scenario->name, x0, x1, band.left, band.right);
        exit(1);
    }

    if (a0.y1 != sa.y1 || a1.y1 != sa.y1) {
        fprintf(stderr, "%s: los ticks del filtro no estan en el borde superior de la cascada "
                "(y=%d/%d vs %d)\n",
                scenario->name, a0.y1, a1.y1, sa.y1);
        exit(1);
    }
}

/* --- Editor DFL (issue #173, PR 2): utilidades del fixture -------------------
 * El arnes no tiene touch ni teclado fisico: para probar la INTERACCION por los
 * callbacks reales del dialogo, el fixture ubica un control por su user_data
 * (dialog_dfl.h) y le manda el evento, y empuja un punto sintetico por el hook
 * dialog_dfl_pointer_hook para el arrastre de los handles. */
static lv_obj_t *find_by_user_data(lv_obj_t *root, intptr_t data)
{
    if (root == NULL) {
        return NULL;
    }

    if ((intptr_t) lv_obj_get_user_data(root) == data) {
        return root;
    }

    uint32_t n = lv_obj_get_child_cnt(root);

    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *found = find_by_user_data(lv_obj_get_child(root, (int32_t) i), data);

        if (found) {
            return found;
        }
    }

    return NULL;
}

static void dfl_press_ui(dfl_ui_id_t id)
{
    lv_obj_t *b = find_by_user_data(lv_scr_act(), (intptr_t) id);

    if (!b) {
        fprintf(stderr, "dfl-editor: no encontre el control %d en el arbol\n", (int) id);
        exit(1);
    }

    lv_event_send(b, LV_EVENT_CLICKED, NULL);
}

static lv_point_t dfl_test_point;

static bool dfl_test_pointer(lv_point_t *out)
{
    *out = dfl_test_point;
    return true;
}

static void dfl_drag_ui(int32_t local_x)
{
    lv_obj_t *g = find_by_user_data(lv_scr_act(), (intptr_t) DFL_UI_GRAPH);

    if (!g) {
        fprintf(stderr, "dfl-editor: no encontre el grafico en el arbol\n");
        exit(1);
    }

    /* El arnes empuja el evento antes de que corra el lazo de LVGL: forzar el
     * layout para que el grafico tenga sus coordenadas reales (en la radio el
     * usuario siempre arrastra con la pantalla ya dibujada). */
    lv_obj_update_layout(lv_scr_act());

    dialog_dfl_pointer_hook = dfl_test_pointer;
    dfl_test_point.x        = g->coords.x1 + local_x;
    dfl_test_point.y        = g->coords.y1 + 10;

    lv_event_send(g, LV_EVENT_PRESSED, NULL);
    lv_event_send(g, LV_EVENT_PRESSING, NULL);
    lv_event_send(g, LV_EVENT_RELEASED, NULL);

    dialog_dfl_pointer_hook = NULL;
}

static void dfl_esc_ui(void)
{
    uint32_t key = LV_KEY_ESC;

    lv_event_send(dialog_dfl->obj, LV_EVENT_KEY, &key);
}

/* El arnes no tiene encoder fisico: empuja el paso por el MISMO despachador del
 * firmware, src/dialog.c::dialog_rotary(), al que main_screen_rotary_enhanced_cb
 * llama con enhanced->raw_diff (el camino real de la perilla). */
static void dfl_rotary_ui(int32_t diff)
{
    dialog_rotary(diff);
}

/* Lo que cada fixture del editor *significa*: el arnes corta si no lo ve, en vez
 * de capturar otra pantalla y reportar verde. */
static void settle_dfl_editor(const scenario_t *scenario)
{
    if (strncmp(scenario->screen, "dfl-editor", 10) != 0) {
        return;
    }

    if (!label_on_screen(lv_scr_act(), "DFL - Digital Filter")) {
        fprintf(stderr, "%s: falta el encabezado del editor DFL\n", scenario->name);
        exit(1);
    }
    if (!label_on_screen(lv_scr_act(), "LOW") || !label_on_screen(lv_scr_act(), "HIGH")
        || !label_on_screen(lv_scr_act(), "BW")) {
        fprintf(stderr, "%s: faltan los controles LOW/HIGH/BW\n", scenario->name);
        exit(1);
    }
    if (!label_on_screen(lv_scr_act(), "Esquema de banda pasante")) {
        fprintf(stderr, "%s: falta la etiqueta 'Esquema de banda pasante'\n", scenario->name);
        exit(1);
    }

    char buf[32];

    snprintf(buf, sizeof(buf), "%d Hz", subject_get_int(cfg_cur.filter.low));
    if (!label_on_screen(lv_scr_act(), buf)) {
        fprintf(stderr, "%s: el label de LOW no muestra '%s'\n", scenario->name, buf);
        exit(1);
    }

    snprintf(buf, sizeof(buf), "%d Hz", subject_get_int(cfg_cur.filter.high));
    if (!label_on_screen(lv_scr_act(), buf)) {
        fprintf(stderr, "%s: el label de HIGH no muestra '%s'\n", scenario->name, buf);
        exit(1);
    }

    if (!strcmp(scenario->screen, "dfl-editor-interaction")) {
        /* Rueda sobre LOW (default, paso 10): 225 +10 +10 -> 240. La fila HIGH
         * tocada la selecciona; rueda (paso 50): 3525 -> 3550. La fila BW tocada
         * la selecciona; rueda (paso 20): 3300 -> 3320 (el arnes deriva el BW del
         * par del fixture; el par LOW/HIGH resultante es legal). Despues el
         * arrastre de HIGH lo lleva a 7000 (x=300) y el segundo arrastre fuera del
         * grafico (x=500) lo recorta a MAX=10000; la rueda sobre HIGH (10050) la
         * rechaza la guarda: queda 10000, sin valor optimista. El arrastre
         * SELECCIONA HIGH, asi que la rueda sigue con el. */
        if (subject_get_int(cfg_cur.filter.low) != 240) {
            fprintf(stderr, "%s: LOW esperado 240, quedo %d\n", scenario->name,
                    subject_get_int(cfg_cur.filter.low));
            exit(1);
        }
        if (subject_get_int(cfg_cur.filter.high) != MAX_FILTER_FREQ) {
            fprintf(stderr, "%s: HIGH esperado %d (el rechazo no debe cambiar el valor), quedo %d\n",
                    scenario->name, MAX_FILTER_FREQ, subject_get_int(cfg_cur.filter.high));
            exit(1);
        }
        if (subject_get_int(cfg_cur.filter.bw) != 3320) {
            fprintf(stderr, "%s: BW esperado 3320 (paso de la rueda), quedo %d\n", scenario->name,
                    subject_get_int(cfg_cur.filter.bw));
            exit(1);
        }

        snprintf(buf, sizeof(buf), "%d Hz", subject_get_int(cfg_cur.filter.bw));
        if (!label_on_screen(lv_scr_act(), buf)) {
            fprintf(stderr, "%s: el label de BW no muestra '%s'\n", scenario->name, buf);
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "dfl-editor-tx")) {
        /* Durante TX los controles de edicion quedan deshabilitados y el modo no
         * cambio; el estado RX (LOW/HIGH/BW) tampoco. */
        if (subject_get_int(cfg_cur.filter.low) != 225
            || subject_get_int(cfg_cur.filter.high) != 3525) {
            fprintf(stderr, "%s: TX no debe alterar el filtro RX (low=%d high=%d)\n", scenario->name,
                    subject_get_int(cfg_cur.filter.low), subject_get_int(cfg_cur.filter.high));
            exit(1);
        }
    }
}

void scenario_settle(const scenario_t *scenario)
{
    settle_waterfall_center_line(scenario);
    settle_waterfall_filter_ticks(scenario);
    settle_dfl_editor(scenario);

    /* CB 27 MHz (issues #146/#169): lo que cada fixture significa. La celda VFO de
     * la barra tiene que mostrar el canal, no el rotulo VFO-A/SPL-B; y en la fila
     * de frecuencia (issue #169) el label izquierdo lleva la frecuencia del grid
     * con su color de tema (`#RRGGBB 27.185`), que el match exacto no ve. Si
     * falta cualquiera de los dos el arnes corta en vez de capturar otra cosa y
     * reportar verde. El recorte del texto (no solo la visibilidad) se mide aparte
     * con --inspect (text_truncated / text_natural), que es lo que usa la
     * evidencia de los seis temas. */
    if (!strcmp(scenario->name, "home-cb")) {
        if (!label_on_screen(lv_scr_act(), "CH 19") || !label_suffix_on_screen(lv_scr_act(), "27.185")) {
            fprintf(stderr, "home-cb: la fila no muestra 27.185 / CH 19 / CB C (¿se perdio el modo CB?)\n");
            exit(1);
        }
    }
    if (!strcmp(scenario->name, "home-cb-offchannel")) {
        if (!label_on_screen(lv_scr_act(), "CH --") || !label_suffix_on_screen(lv_scr_act(), "27.000")) {
            fprintf(stderr, "home-cb-offchannel: la fila no muestra 27.000 / CH -- en 27.000 MHz\n");
            exit(1);
        }
    }
    if (!strcmp(scenario->name, "home-cb-cathode") && !label_suffix_on_screen(lv_scr_act(), "27.185")) {
        fprintf(stderr, "home-cb-cathode: la fila no muestra 27.185 con la familia normal en Cathode\n");
        exit(1);
    }
    if (!strcmp(scenario->name, "home-cb-a") && !label_suffix_on_screen(lv_scr_act(), "26.065")) {
        fprintf(stderr, "home-cb-a: la fila no muestra 26.065 / CH 1 / CB A\n");
        exit(1);
    }
    if (!strcmp(scenario->name, "home-cb-d") && !label_suffix_on_screen(lv_scr_act(), "27.855")) {
        fprintf(stderr, "home-cb-d: la fila no muestra 27.855 / CH 40 / CB D\n");
        exit(1);
    }

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

    if (!strcmp(scenario->screen, "settings-grupo")) {
        /* Las 8 paginas (grupos) de Ajustes por el mismo camino que el usuario: el tab
         * de la barra de abajo llama a settings_goto() (scroll + foco). La tabla es la
         * de src/dialog_settings.c (tabs_pagina_1/tabs_pagina_2, el orden del issue
         * #109): la pagina 1 tiene los indices 0..3 y el 4 es "More >"; la pagina 2
         * tiene los indices 0..3 (Display, Appearance, Actions, Info). Cada grupo lleva
         * una etiqueta TESTIGO de una de sus filas: el arnes corta si el tab no esta en
         * la barra o si la fila no quedo en pantalla, asi que un reordenamiento de los
         * tabs no puede dar una corrida verde capturando otra pagina. */
        static const struct {
            const char *tab;
            bool        pagina2;
            uint8_t     idx;
            const char *fila;
        } grupos[] = {
            { "Operation",  false, 0, "Freq acceleration" },
            { "Audio",      false, 1, "Play,Rec gain" },
            { "Spectrum",   false, 2, "Waterfall battery mode" },
            { "Clock",      false, 3, "Day, Month, Year" },
            { "Display",    true,  0, "S-meter needle" },
            { "Appearance", true,  1, "Theme" },
            { "Actions",    true,  2, "GEN long press" },
            { "Info",       true,  3, "Mag Freq, Info, ALC" },
        };
        const size_t n = sizeof(grupos) / sizeof(grupos[0]);
        const char  *fila = NULL;
        bool         pagina2 = false;
        uint8_t      idx = 0;

        for (size_t i = 0; i < n; i++) {
            if (!strcmp(grupos[i].tab, scenario->settings_grupo)) {
                pagina2 = grupos[i].pagina2;
                idx     = grupos[i].idx;
                fila    = grupos[i].fila;
                break;
            }
        }

        if (fila == NULL) {
            fprintf(stderr,
                    "settings-grupo: '%s' no es uno de los 8 grupos de Ajustes (Operation, "
                    "Audio, Spectrum, Clock, Display, Appearance, Actions, Info)\n",
                    scenario->settings_grupo);
            exit(1);
        }

        if (pagina2) {
            buttons_press(4, false);    /* page 1, index 4: "More >" */
        }

        if (!label_on_screen(lv_scr_act(), scenario->settings_grupo)) {
            fprintf(stderr,
                    "settings-grupo: no hay tab '%s' en la barra de abajo (¿se reordenaron los "
                    "tabs?)\n", scenario->settings_grupo);
            exit(1);
        }

        buttons_press(idx, false);

        if (!label_on_screen(lv_scr_act(), fila)) {
            fprintf(stderr,
                    "settings-grupo: la fila testigo '%s' del grupo '%s' no quedo en pantalla "
                    "despues del tab (¿se movio el grupo?)\n", fila, scenario->settings_grupo);
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

    if (!strcmp(scenario->screen, "cw-tune-default") || !strcmp(scenario->screen, "cw-tune-tuner")
        || !strcmp(scenario->screen, "cw-tune-ssb") || !strcmp(scenario->screen, "cw-autotune-off")
        || !strcmp(scenario->screen, "cw-autotune-on")) {
        /* El widget de CW tune (src/cw_tune_ui.c) entre los hijos de la pantalla: sin el,
         * la fixture no prueba nada del widget. */
        lv_obj_t *widget = cw_tune_widget(lv_scr_act());

        if (widget == NULL) {
            fprintf(stderr,
                    "%s: no encontre el widget de CW tune (139x40) entre los hijos de la "
                    "pantalla (¿cambio su tamano en src/cw_tune_ui.c?)\n",
                    scenario->name);
            exit(1);
        }

        /* El widget es un INDICADOR de lectura, no un control: no tiene eventos de entrada (solo
         * DRAW_MAIN y DELETE, src/cw_tune_ui.c) y su objeto no puede quedar clicable. Ademas de ser
         * lo correcto, es lo que lo mantiene fuera del contrato de touch target de 44 px: el
         * criterio audita los objetos clicables que pintan (_is_control,
         * tools/ab_testing/src/x6200_ab/layout_checks.py) y el widget mide 139x40. Si alguien le
         * devuelve los flags de entrada, el arnes corta aca en vez de dejar que el contrato lo lea
         * como un control de 40 px de alto. */
        if (lv_obj_has_flag(widget, LV_OBJ_FLAG_CLICKABLE)
            || lv_obj_has_flag(widget, LV_OBJ_FLAG_SCROLLABLE)) {
            fprintf(stderr,
                    "%s: el widget de CW tune quedo con flags de entrada (CLICKABLE/SCROLLABLE) "
                    "(es un indicador de lectura: sin esto el contrato lo audita como un control "
                    "de 139x40, por debajo del target de 44 px)\n",
                    scenario->name);
            exit(1);
        }

        /* La regla de visibilidad nueva (issue #122): el widget es la herramienta de sintonia
         * de CW, asi que se muestra en CW/CWR y en ningun otro modo — el parametro `cw_tune`
         * (el boton "Tuner" de la pagina CW) ya NO decide. Se codifican las dos mitades, sobre
         * el estado real del objeto (el flag HIDDEN) y sobre el accesor que el gate del feed
         * del tono consulta (src/cw.cpp): si el accesor no coincide con el objeto, el feed
         * correria cuando el widget no se ve (o al reves) y el arnes corta aca. */
        if (!strcmp(scenario->screen, "cw-tune-ssb")) {
            /* Fuera de CW/CWR: el widget existe pero NO se muestra, con el parametro en
             * cualquiera de sus valores. Es la mitad que el fixture `cw-tune-default` no
             * puede cubrir (ahi el modo es CW). */
            if (!lv_obj_has_flag(widget, LV_OBJ_FLAG_HIDDEN)) {
                fprintf(stderr,
                        "%s: el widget de CW tune esta VISIBLE fuera de CW/CWR "
                        "(¿la regla de visibilidad de update_visibility dejo de mirar el modo?)\n",
                        scenario->name);
                exit(1);
            }
            if (cw_tune_ui_visible()) {
                fprintf(stderr,
                        "%s: cw_tune_ui_visible() dice visible con el widget oculto "
                        "(¿el accesor no sigue al flag del objeto?)\n",
                        scenario->name);
                exit(1);
            }
        } else {
            bool autotune_page = !strcmp(scenario->screen, "cw-autotune-off")
                                 || !strcmp(scenario->screen, "cw-autotune-on");
            bool autotune_on   = !strcmp(scenario->screen, "cw-autotune-on");

            /* El boton-pagina es posicional: codificar que la pagina que quedo en la barra es
             * la que el fixture dice. Un reordenamiento de las paginas KEY corta la corrida
             * en vez de medir otra pagina y reportar verde. Los fixtures de AUTO miden la
             * (CW 2:2); los de #122, la (CW 1:2). */
            if (!label_on_screen(lv_scr_act(), autotune_page ? "(CW 2:2)" : "(CW 1:2)")) {
                fprintf(stderr,
                        "%s: la barra no quedo en la pagina '%s' despues de las "
                        "pulsaciones del boton-pagina (¿se reordenaron las paginas KEY?)\n",
                        scenario->name, autotune_page ? "(CW 2:2)" : "(CW 1:2)");
                exit(1);
            }

            if (autotune_page) {
                /* Lo que el fixture de AUTO *significa*: en (CW 2:2) estan Peak/Noise beta
                 * y el boton "Auto", que arranca Off (cfg.cw_decoder_auto=0, src/cfg/cfg.c).
                 * El fixture `cw-autotune-on` lo pulsa por el camino del usuario
                 * (button_action_cb -> controls_toggle_cw_decoder_auto) y el rotulo tiene
                 * que pasar a On. Peak/Noise conservan los valores del firmware: el arnes no
                 * compila src/cw.cpp, que es el unico que los mueve con AUTO encendido. */
                if (!label_on_screen(lv_scr_act(), "Peak beta:\n0.10")
                    || !label_on_screen(lv_scr_act(), "Noise beta:\n0.80")) {
                    fprintf(stderr,
                            "%s: faltan los botones 'Peak beta' / 'Noise beta' de la pagina "
                            "(CW 2:2) (¿se movieron de pagina?)\n",
                            scenario->name);
                    exit(1);
                }
                if (subject_get_int(cfg.cw_decoder_auto.val) != 0) {
                    fprintf(stderr,
                            "%s: cfg.cw_decoder_auto arranco en %d y no en 0 (¿cambio el "
                            "defecto?): la fixture dejaria de probar el estado apagado\n",
                            scenario->name, (int) subject_get_int(cfg.cw_decoder_auto.val));
                    exit(1);
                }
                if (!label_on_screen(lv_scr_act(), "Auto:\nOff")) {
                    fprintf(stderr,
                            "%s: el boton 'Auto' no esta en la barra leyendo Off "
                            "(¿se movio de la pagina CW 2:2 o cambio su rotulo?)\n",
                            scenario->name);
                    exit(1);
                }
                if (autotune_on) {
                    /* Indice 3 de (CW 2:2): pagina, Peak, Noise, Auto. */
                    buttons_press(3, false);

                    if (subject_get_int(cfg.cw_decoder_auto.val) != 1) {
                        fprintf(stderr,
                                "%s: pulsar 'Auto' dejo cfg.cw_decoder_auto en %d y no en 1 "
                                "(¿el boton no llega a controls_toggle_cw_decoder_auto?)\n",
                                scenario->name, (int) subject_get_int(cfg.cw_decoder_auto.val));
                        exit(1);
                    }
                    if (!label_on_screen(lv_scr_act(), "Auto:\nOn")) {
                        fprintf(stderr,
                                "%s: el boton 'Auto' no lee On despues de pulsarlo "
                                "(¿el rotulo no sigue al subject?)\n",
                                scenario->name);
                        exit(1);
                    }
                }
            } else {
                /* Y que el boton "Tuner" siga en la barra: su rotulo sale del subject real
                 * (src/buttons.cpp, cw_tuner_label_getter). El parametro ya no gatea el widget
                 * (issue #122), pero el boton sigue existiendo. */
                if (!label_on_screen(lv_scr_act(), "Tuner:\nOff")) {
                    fprintf(stderr,
                            "%s: el boton 'Tuner' no esta en la barra con el valor de arranque "
                            "(¿se movio de la pagina CW o cambio su rotulo?)\n",
                            scenario->name);
                    exit(1);
                }
            }

            /* La pagina CW de la barra no depende del decodificador CW: se llega con
             * cw_decoder=0 (el valor de arranque, src/cfg/cfg.c:332-353), que es el estado
             * del reporte del issue #122. Codifica la refutacion de la hipotesis 1 del issue
             * ("la pagina solo se alcanza con el decodificador encendido"): la ruta de la
             * tecla KEY y el boton-pagina no consultan cw_decoder (src/main_screen.c:773,
             * src/buttons.cpp:443-444,526). Si el mock arrancara con cw_decoder=1 esta
             * fixture dejaria de probar lo que dice. */
            if (subject_get_int(cfg.cw_decoder.val) != 0) {
                fprintf(stderr,
                        "%s: cfg.cw_decoder arranco en %d y no en 0 (¿cambio el defecto del "
                        "mock?): la fixture dejaria de probar que la pagina CW se alcanza con el "
                        "decodificador apagado\n",
                        scenario->name, (int) subject_get_int(cfg.cw_decoder.val));
                exit(1);
            }

            /* Lo que esta fixture *significa*: en modo CW el widget se muestra con el
             * parametro `cw_tune` en su valor de arranque (0, src/cfg/cfg.c:349) y sin tocar
             * el boton. Es la respuesta al issue #122 ("el widget de Tune no aparece en la
             * banda CW"): la causa era el gate por `cw_tune` y ya no existe. Codificado sobre
             * el estado real (el parametro en 0 + el flag del objeto): si alguien vuelve a
             * gatear la visibilidad por `cw_tune`, el arnes corta en vez de capturar un
             * widget oculto y reportar verde. */
            if (subject_get_int(cfg.cw_tune.val) != 0) {
                fprintf(stderr,
                        "%s: cfg.cw_tune arranco en %d y no en 0 (¿cambio el "
                        "defecto?): la fixture dejaria de probar que el widget no depende de "
                        "ese parametro\n",
                        scenario->name, (int) subject_get_int(cfg.cw_tune.val));
                exit(1);
            }
            if (lv_obj_has_flag(widget, LV_OBJ_FLAG_HIDDEN)) {
                fprintf(stderr,
                        "%s: el widget de CW tune esta OCULTO en modo CW con "
                        "cw_tune=0 (issue #122: la visibilidad no puede depender de cw_tune)\n",
                        scenario->name);
                exit(1);
            }
            if (!cw_tune_ui_visible()) {
                fprintf(stderr,
                        "%s: cw_tune_ui_visible() dice oculto con el widget visible "
                        "(¿el accesor no sigue al flag del objeto?)\n",
                        scenario->name);
                exit(1);
            }

            if (!strcmp(scenario->screen, "cw-tune-tuner")) {
                /* Pulsar el "Tuner" (indice 2 de la pagina (CW 1:2)) por el camino del usuario:
                 * button_action_cb -> controls_toggle_cw_tuner -> cfg.cw_tune (src/buttons.cpp,
                 * src/controls.cpp). El parametro ya no gatea el widget, pero el boton tiene que
                 * seguir funcionando (es el control que el owner dejo en su lugar). */
                buttons_press(2, false);

                if (subject_get_int(cfg.cw_tune.val) != 1) {
                    fprintf(stderr,
                            "%s: pulsar 'Tuner' dejo cfg.cw_tune en %d y no en 1 "
                            "(¿el boton no llega a controls_toggle_cw_tuner?)\n",
                            scenario->name, (int) subject_get_int(cfg.cw_tune.val));
                    exit(1);
                }
                if (!label_on_screen(lv_scr_act(), "Tuner:\nOn")) {
                    fprintf(stderr,
                            "%s: el boton 'Tuner' no lee 'On' despues de pulsarlo "
                            "(¿el rotulo no sigue al subject?)\n",
                            scenario->name);
                    exit(1);
                }
                if (lv_obj_has_flag(widget, LV_OBJ_FLAG_HIDDEN)) {
                    fprintf(stderr,
                            "%s: el widget de CW tune quedo OCULTO despues de pulsar "
                            "'Tuner' en modo CW\n",
                            scenario->name);
                    exit(1);
                }

                /* El bloque activo lo pinta la senal de tono que en la radio empuja el DSP
                 * (src/cw.cpp, update_peak_freq -> cw_tune_set_freq). El banco no tiene DSP, asi
                 * que el fixture la sintetiza: un tono de 8 Hz corre el bloque activo al verde y
                 * deja el capture como lo ve el usuario con senal. La funcion que corre es la real
                 * (src/cw_tune_ui.c). */
                cw_tune_set_freq(8.0f);
            }
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
