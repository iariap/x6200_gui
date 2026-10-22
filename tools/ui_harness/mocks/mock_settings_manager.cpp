/*
 *  Boot-order check for the remote control's mirror of the configuration
 *  (src/cfg/settings_manager.cpp), for the host UI harness.
 *
 *  The mirror is the ported X6100 SettingsManager: the objects the remote
 *  server (src/remote_server.cpp) reads and writes the radio through. Its
 *  members are bound to *subjects* that the configuration load creates, and the
 *  mirror itself is a C++ global, so its constructor runs before main() -before
 *  any item of cfg exists-. The firmware redoes that mapping once the load is
 *  done (cfg_sm_init_live(), the last step of src/cfg/cfg.c); the harness runs
 *  the same last step here, from mock_cfg_init().
 *
 *  Why this is a check and not a comment: with the mapping left pointing at NULL
 *  every *read* answers the neutral value and every *write* goes nowhere, in
 *  silence. On the radio that is exactly `{"cmd":"get"}` answering `"vol": 0`
 *  with vol=8 in params.db (issue #30), and `{"cmd":"vol","val":8}` changing
 *  nothing. A harness run that has the mirror unwired must not go green
 *  capturing something else, so it stops the run here instead.
 *
 *  It is C++ because the mirror's header is (it has no extern "C" guards, the
 *  same reason src/cfg/cxx_bridge.c exists); mock_harness.h declares the
 *  C-callable entry point.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>

#include "cfg/cfg.h"
#include "cfg/settings_manager.h"
#include "cfg/subjects.h"

/* src/radio.h is a C header without extern "C" guards (like every header of this GUI: it is
 * C), so a C++ caller declares the C symbol it needs, the same way src/cfg/cxx_bridge.c does
 * it the other way round. The definition is mocks/mock_radio.c, next to the simulated radio. */
extern "C" bool radio_check_freq(int32_t freq);

/* Los simbolos que cruzan de C a C++ (mock_harness.h no declara `extern "C"`, igual que los
 * headers del firmware; es el mismo camino que usa src/cfg/cfg.c con cfg_sm_init_live). El
 * puente lo define src/cfg/settings_manager.cpp y lo llama la radio al final de cfg_init();
 * mock_settings_manager_check() lo declara mock_harness.h y la define este archivo. */
extern "C" void cfg_sm_init_live(void);

/* El puente que src/cfg/settings_manager.cpp usa para validar una frecuencia
 * (src/cfg/cxx_bridge.c en el firmware, que ahi envuelve radio_check_freq()): el arnes
 * define el suyo contra el radio simulado, la misma respuesta que daria la radio. */
extern "C" bool x6200_bridge_radio_check_freq(int32_t freq) {
    return radio_check_freq(freq);
}

extern "C" void mock_settings_manager_check(void) {
    /* El ultimo paso de cfg_init() en la radio (src/cfg/cfg.c). */
    cfg_sm_init_live();

    /* Los Param que el control remoto lee y escribe: ninguno puede quedar sin subject. */
    int faltan = cfg_sm.check_live();

    if (faltan != 0) {
        fprintf(stderr,
                "mock_settings_manager: %d parametro(s) del control remoto quedaron sin "
                "subject: el espejo de cfg_sm no se engancho (¿cfg_sm_init_live()?) ...\n",
                faltan);
        exit(1);
    }

    /* Y el estado del fixture tiene que llegar entero: el espejo lee lo mismo que el
     * parametro guarda. `vol` es el del issue #30 (la base dice 8 y el control remoto
     * contestaba 0); `nb` es un item que el arnes no usaba para nada hasta ahora. */
    struct { const char *nombre; int32_t via_espejo; int32_t en_el_estado; } pares[] = {
        {"vol", cfg_sm.p_volume.get(), subject_get_int(cfg.vol.val)},
        {"sql", cfg_sm.p_squelch.get(), subject_get_int(cfg.sql.val)},
        {"nb", cfg_sm.p_nb.get(), subject_get_int(cfg.nb.val)},
        {"nr", cfg_sm.p_nr.get(), subject_get_int(cfg.nr.val)},
        {"band", cfg_sm.p_band_id.get(), subject_get_int(cfg.band_id.val)},
    };

    for (size_t i = 0; i < sizeof(pares) / sizeof(*pares); i++) {
        if (pares[i].via_espejo != pares[i].en_el_estado) {
            fprintf(stderr,
                    "mock_settings_manager: el control remoto ve %s=%i y el estado tiene %i "
                    "(issue #30: el espejo no esta leyendo el parametro)\n",
                    pares[i].nombre, pares[i].via_espejo, pares[i].en_el_estado);
            exit(1);
        }
    }
}
