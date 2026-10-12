/* Runtime font choice: the code keeps using &sony_N; for Roboto/DejaVu/Arial the contents of each
 * sony_N are replaced by the alt font before the UI is built. The alt fonts are ASCII only
 * (lv_font_conv --bpp 4 --no-compress --range 32-126 --format lvgl, size two steps below
 * Sony's; Arial: tools/font_gen.py, about 0.8 x Sony's size so two-line button labels fit);
 * icon glyphs come from the untouched Sony copy through the fallback. */
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "font_select.h"

#define SIZES(X) X(14) X(16) X(18) X(20) X(22) X(24) X(26) X(28) X(30) \
                 X(32) X(34) X(36) X(38) X(40) X(42) X(44) X(60)

#define DECL(n) extern lv_font_t sony_##n; extern const lv_font_t roboto_##n, dejavu_##n, arial_##n;
SIZES(DECL)

#define SONY(n) &sony_##n,
#define ROBOTO(n) &roboto_##n,
#define DEJAVU(n) &dejavu_##n,
#define ARIAL(n) &arial_##n,
static lv_font_t *const sony[] = { SIZES(SONY) };
static const lv_font_t *const roboto[] = { SIZES(ROBOTO) };
static const lv_font_t *const dejavu[] = { SIZES(DEJAVU) };
static const lv_font_t *const arial[] = { SIZES(ARIAL) };
#define N_FONTS (sizeof(sony) / sizeof(sony[0]))

static lv_font_t orig[N_FONTS];
static bool applied;

int buttons_h = 54;  /* = BUTTONS_H_MIN */

void fonts_apply(uint8_t choice) {
    if (applied || (choice != FONT_ROBOTO && choice != FONT_DEJAVU && choice != FONT_ARIAL)) return;
    applied = true;

    /* La copia de la fuente alternativa dentro de sony_N (que es lo que hace que el resto del
     * código siga usando &sony_N) se probó y funciona con Roboto, pero con DejaVu y Arial
     * termina escribiendo en una dirección inválida y mata la GUI en el arranque: el equipo se
     * queda en el logo. Hasta encontrar la causa, esas dos se quedan con la fuente Sony, que es
     * la que dibuja bien. */
    if (choice != FONT_ROBOTO) {
        LV_LOG_WARN("La fuente alternativa %s no se aplica todavía (se usa la del equipo); "
                    "con ella la GUI moría al arrancar",
                    choice == FONT_DEJAVU ? "DejaVu" : "Arial");
        return;
    }

    const lv_font_t *const *alt = roboto;

    for (unsigned i = 0; i < N_FONTS; i++) {
        orig[i] = *sony[i];
        *sony[i] = *alt[i];
        sony[i]->fallback = &orig[i];
    }
}

/* The sony_N `steps` sizes smaller (sony_60 stays): for screens that VISION's scale makes too big */
const lv_font_t *font_down(const lv_font_t *f, int steps) {
    for (int i = 0; i + 1 < (int)N_FONTS; i++)
        if (sony[i] == f) return sony[i - steps > 0 ? i - steps : 0];
    return f;
}

/* VISION: every sony_N drawn `steps` sizes bigger (sony_20 as sony_26 with 3), up to sony_44; sony_60 stays.
 * Call once, after fonts_apply() and before the UI is built */
void fonts_scale(int steps) {
    static lv_font_t was[N_FONTS];

    for (unsigned i = 0; i < N_FONTS; i++) was[i] = *sony[i];
    for (unsigned i = 0; i + 1 < N_FONTS; i++) {
        unsigned to = i + steps < N_FONTS - 1 ? i + steps : N_FONTS - 2;
        *sony[i]    = was[to];
    }
}
