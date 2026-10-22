/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Presets de filtro del DFL (issue #183; ver src/dfl_presets.h).
 *
 *  Modelo puro (catalogo, cache de memorias y favoritos, validacion por familia)
 *  y aplicacion segura por los setters vigentes. Sin LVGL y sin SQL: la base la
 *  toca src/dfl_presets_store.c.
 */

#include "dfl_presets.h"

#include "dfl_presets_store.h"

#include "cfg/cfg.h"
#include "cfg/mode.h"
#include "cfg/subjects.h"

#include <stdio.h>
#include <string.h>

/* Catalogo FIJO de fabrica (contrato #183, seccion 3). BW = HIGH - LOW y sale de
 * los dos limites; no se guarda. */
typedef struct {
    const char *name;
    int32_t     low;
    int32_t     high;
} dfl_preset_t;

static const dfl_preset_t catalog[DFL_FAMILY_COUNT][DFL_FACTORY_COUNT] = {
    /* SSB (USB/LSB) */
    { { "Wide", 100, 3400 }, { "Normal", 150, 2850 }, { "DX", 300, 2400 } },
    /* DIGI (USB-D/LSB-D) */
    { { "Wide", 100, 3500 }, { "Normal", 50, 2950 }, { "Focus", 1200, 1800 } },
    /* CW (CW/CWR) */
    { { "Wide", -400, 400 }, { "Normal", -250, 250 }, { "DX", -150, 150 } },
    /* AM (AM/SAM) */
    { { "Wide", 0, 6000 }, { "Normal", 0, 4500 }, { "Narrow", 0, 2500 } },
};

typedef struct {
    bool    present;
    int32_t low;
    int32_t high;
} dfl_mem_t;

/* [familia-1][slot-1] */
static dfl_mem_t memories[DFL_FAMILY_COUNT][DFL_MEM_SLOTS];
static int       favorites[DFL_FAMILY_COUNT][DFL_QUICK_SLOTS];

static char      label_buf[DFL_QUICK_SLOTS][28];
static char      mem_label_buf[DFL_MEM_SLOTS][20];
static char      err_buf[96];

/* -------------------------------------------------------------------------- */

static int fam_idx(dfl_family_t family) {
    if (family < DFL_FAMILY_SSB || family > DFL_FAMILY_AM) {
        return -1;
    }
    return (int) family - 1;
}

static bool slot_ok(int slot, int count) {
    return slot >= 1 && slot <= count;
}

dfl_family_t dfl_family_of_mode(int mode) {
    switch ((x6200_mode_t) mode) {
        case x6200_mode_usb:
        case x6200_mode_lsb:
            return DFL_FAMILY_SSB;
        case x6200_mode_usb_dig:
        case x6200_mode_lsb_dig:
            return DFL_FAMILY_DIGI;
        case x6200_mode_cw:
        case x6200_mode_cwr:
            return DFL_FAMILY_CW;
        case x6200_mode_am:
        case x6200_mode_sam:
            return DFL_FAMILY_AM;
        default:
            /* NFM/WFM y los modos desconocidos no tienen catalogo aprobado. */
            return DFL_FAMILY_NONE;
    }
}

const char *dfl_family_name(dfl_family_t family) {
    switch (family) {
        case DFL_FAMILY_SSB:  return "SSB";
        case DFL_FAMILY_DIGI: return "DIGI";
        case DFL_FAMILY_CW:   return "CW";
        case DFL_FAMILY_AM:   return "AM";
        default:              return "?";
    }
}

dfl_family_t dfl_current_family(void) {
    return dfl_family_of_mode((int) subject_get_int(cfg_cur.mode));
}

/* --- Catalogo -------------------------------------------------------------- */

bool dfl_factory_values(dfl_family_t family, int idx, int32_t *low, int32_t *high) {
    int fi = fam_idx(family);

    if (fi < 0 || idx < 0 || idx >= DFL_FACTORY_COUNT) {
        return false;
    }
    *low  = catalog[fi][idx].low;
    *high = catalog[fi][idx].high;
    return true;
}

const char *dfl_factory_name(dfl_family_t family, int idx) {
    int fi = fam_idx(family);

    if (fi < 0 || idx < 0 || idx >= DFL_FACTORY_COUNT) {
        return "--";
    }
    return catalog[fi][idx].name;
}

/* --- Validacion ------------------------------------------------------------ */

bool dfl_values_legal(dfl_family_t family, int32_t low, int32_t high) {
    if (high <= low) {
        return false;
    }
    if (high > MAX_FILTER_FREQ) {
        return false;
    }

    switch (family) {
        case DFL_FAMILY_SSB:
        case DFL_FAMILY_DIGI:
            return low >= 0;
        case DFL_FAMILY_CW:
            return low >= -MAX_FILTER_FREQ;
        case DFL_FAMILY_AM:
            return low == 0;
        default:
            return false;
    }
}

bool dfl_apply_order_low_first(int32_t tgt_low, int32_t tgt_high, int32_t cur_low, int32_t cur_high) {
    (void) tgt_low;
    (void) cur_high;

    /* cfg_mode_set_high_filter() exige val > low (el actual); cfg_mode_set_low_filter()
     * exige val < high (el actual). Subir HIGH primero solo sirve si el HIGH destino
     * ya supera el LOW actual. */
    return !(tgt_high > cur_low);
}

/* --- Modelo / cache -------------------------------------------------------- */

bool dfl_source_values(dfl_family_t family, int source, int32_t *low, int32_t *high) {
    int fi = fam_idx(family);

    if (fi < 0) {
        return false;
    }

    if (source >= DFL_SRC_FACTORY_1 && source <= DFL_SRC_FACTORY_3) {
        return dfl_factory_values(family, source - DFL_SRC_FACTORY_1, low, high);
    }

    if (source == DFL_SRC_MEM_1 || source == DFL_SRC_MEM_2) {
        const dfl_mem_t *m = &memories[fi][source - DFL_SRC_MEM_1];

        if (!m->present) {
            return false;
        }
        *low  = m->low;
        *high = m->high;
        return true;
    }

    return false;
}

const char *dfl_source_name(dfl_family_t family, int source) {
    if (source >= DFL_SRC_FACTORY_1 && source <= DFL_SRC_FACTORY_3) {
        return dfl_factory_name(family, source - DFL_SRC_FACTORY_1);
    }
    switch (source) {
        case DFL_SRC_MEM_1: return "Mem 1";
        case DFL_SRC_MEM_2: return "Mem 2";
        default:            return "--";
    }
}

int dfl_presets_favorite_source(dfl_family_t family, int slot) {
    int fi = fam_idx(family);

    if (fi < 0 || !slot_ok(slot, DFL_QUICK_SLOTS)) {
        return DFL_SRC_NONE;
    }
    return favorites[fi][slot - 1];
}

bool dfl_presets_favorite_values(dfl_family_t family, int slot, int32_t *low, int32_t *high) {
    int source = dfl_presets_favorite_source(family, slot);

    if (source == DFL_SRC_NONE) {
        return false;
    }
    return dfl_source_values(family, source, low, high);
}

const char *dfl_presets_favorite_label(dfl_family_t family, int slot) {
    int32_t low  = 0;
    int32_t high = 0;

    if (!slot_ok(slot, DFL_QUICK_SLOTS)) {
        return "--";
    }
    if (!dfl_presets_favorite_values(family, slot, &low, &high)) {
        return "--";
    }

    snprintf(label_buf[slot - 1], sizeof(label_buf[slot - 1]), "%s\n%d Hz",
             dfl_source_name(family, dfl_presets_favorite_source(family, slot)),
             (int) (high - low));
    return label_buf[slot - 1];
}

bool dfl_presets_memory_present(dfl_family_t family, int slot) {
    int fi = fam_idx(family);

    if (fi < 0 || !slot_ok(slot, DFL_MEM_SLOTS)) {
        return false;
    }
    return memories[fi][slot - 1].present;
}

bool dfl_presets_memory_values(dfl_family_t family, int slot, int32_t *low, int32_t *high) {
    int fi = fam_idx(family);

    if (fi < 0 || !slot_ok(slot, DFL_MEM_SLOTS) || !memories[fi][slot - 1].present) {
        return false;
    }
    *low  = memories[fi][slot - 1].low;
    *high = memories[fi][slot - 1].high;
    return true;
}

const char *dfl_presets_memory_label(dfl_family_t family, int slot) {
    int32_t low  = 0;
    int32_t high = 0;

    if (!slot_ok(slot, DFL_MEM_SLOTS)) {
        return "--";
    }
    if (!dfl_presets_memory_values(family, slot, &low, &high)) {
        snprintf(mem_label_buf[slot - 1], sizeof(mem_label_buf[slot - 1]), "Mem %d\nvacia", slot);
    } else {
        snprintf(mem_label_buf[slot - 1], sizeof(mem_label_buf[slot - 1]), "Mem %d\n%d Hz", slot,
                 (int) (high - low));
    }
    return mem_label_buf[slot - 1];
}

int dfl_matching_slot(dfl_family_t family, int32_t low, int32_t high) {
    for (int slot = 1; slot <= DFL_QUICK_SLOTS; slot++) {
        int32_t l;
        int32_t h;

        if (dfl_presets_favorite_values(family, slot, &l, &h) && l == low && h == high) {
            return slot;
        }
    }
    return -1;
}

/* --- Carga ---------------------------------------------------------------- */

void dfl_presets_reload(void) {
    for (int f = DFL_FAMILY_SSB; f <= DFL_FAMILY_AM; f++) {
        int fi = f - 1;

        for (int s = 1; s <= DFL_MEM_SLOTS; s++) {
            int32_t low  = 0;
            int32_t high = 0;

            memories[fi][s - 1].present = dfl_store_load_memory(f, s, &low, &high);
            memories[fi][s - 1].low     = low;
            memories[fi][s - 1].high    = high;
        }

        for (int q = 1; q <= DFL_QUICK_SLOTS; q++) {
            int32_t loaded = 0;
            int     source = q;   /* default sin fila: el preset de fabrica del slot */

            if (dfl_store_load_favorite(f, q, &loaded) && loaded >= DFL_SRC_FACTORY_1 && loaded <= DFL_SRC_MEM_2) {
                source = loaded;
            }

            /* Fuente de memoria vacia (fila huerfana o DB tocada a mano): fallback
             * seguro al preset de fabrica del mismo slot, sin crash y sin reset
             * global. Se avisa por stdout, que es donde ya escribe migrations.c. */
            if ((source == DFL_SRC_MEM_1 || source == DFL_SRC_MEM_2)
                && !memories[fi][source - DFL_SRC_MEM_1].present) {
                printf("DFL: favorito familia %d slot %d apunta a una memoria vacia; uso el preset %d\n", f, q, q);
                source = q;
            }

            favorites[fi][q - 1] = source;
        }
    }
}

void dfl_presets_init(void) {
    dfl_presets_reload();
}

/* --- Aplicacion ----------------------------------------------------------- */

/* Escribe el par por los dos setters, en el orden que las guardas admiten. No
 * toca cfg_cur.filter.bw: los observers de src/cfg/mode.c lo derivan. */
static void set_pair(int32_t low, int32_t high) {
    int32_t cur_low  = subject_get_int(cfg_cur.filter.low);
    int32_t cur_high = subject_get_int(cfg_cur.filter.high);

    if (dfl_apply_order_low_first(low, high, cur_low, cur_high)) {
        cfg_mode_set_low_filter(low);
        cfg_mode_set_high_filter(high);
    } else {
        cfg_mode_set_high_filter(high);
        cfg_mode_set_low_filter(low);
    }
}

static dfl_apply_result_t apply_pair(dfl_family_t family, int32_t low, int32_t high) {
    if (!dfl_values_legal(family, low, high)) {
        return DFL_APPLY_INVALID_VALUES;
    }

    int32_t prev_low  = subject_get_int(cfg_cur.filter.low);
    int32_t prev_high = subject_get_int(cfg_cur.filter.high);

    if (prev_low == low && prev_high == high) {
        return DFL_APPLY_OK;
    }

    set_pair(low, high);

    /* Relectura obligatoria: solo con el par EXACTO se informa OK. */
    if (subject_get_int(cfg_cur.filter.low) == low && subject_get_int(cfg_cur.filter.high) == high) {
        return DFL_APPLY_OK;
    }

    /* Half-apply: se restaura el par anterior (best effort, misma logica) para no
     * dejar un estado ilegal ni un filtro a medias. */
    set_pair(prev_low, prev_high);
    return DFL_APPLY_FAILED;
}

dfl_apply_result_t dfl_apply_source(dfl_family_t family, int source) {
    int32_t low  = 0;
    int32_t high = 0;

    if (!dfl_source_values(family, source, &low, &high)) {
        return DFL_APPLY_INVALID_SOURCE;
    }
    return apply_pair(family, low, high);
}

dfl_apply_result_t dfl_apply_favorite(int slot) {
    dfl_family_t family = dfl_current_family();

    if (family == DFL_FAMILY_NONE || !slot_ok(slot, DFL_QUICK_SLOTS)) {
        return DFL_APPLY_INVALID_SOURCE;
    }
    return dfl_apply_source(family, dfl_presets_favorite_source(family, slot));
}

dfl_apply_result_t dfl_apply_memory(int slot) {
    dfl_family_t family = dfl_current_family();

    if (family == DFL_FAMILY_NONE || !slot_ok(slot, DFL_MEM_SLOTS)) {
        return DFL_APPLY_INVALID_SOURCE;
    }
    return dfl_apply_source(family, slot == 1 ? DFL_SRC_MEM_1 : DFL_SRC_MEM_2);
}

/* --- Escritura ------------------------------------------------------------ */

static void set_error(const char *text) {
    snprintf(err_buf, sizeof(err_buf), "%s", text ? text : "");
}

const char *dfl_presets_last_error(void) {
    if (err_buf[0] != '\0') {
        return err_buf;
    }
    return dfl_store_last_error();
}

bool dfl_save_memory(dfl_family_t family, int slot, int32_t low, int32_t high) {
    int fi = fam_idx(family);

    if (fi < 0 || !slot_ok(slot, DFL_MEM_SLOTS)) {
        set_error("familia o ranura invalida");
        return false;
    }
    if (!dfl_values_legal(family, low, high)) {
        set_error("valores invalidos para la familia");
        return false;
    }
    /* Limites de la tabla (CHECKs del contrato). */
    if (low < -MAX_FILTER_FREQ || low > 9999 || high < 1 || high > MAX_FILTER_FREQ) {
        set_error("valores fuera de rango");
        return false;
    }

    if (!dfl_store_save_memory(family, slot, low, high)) {
        set_error(dfl_store_last_error());
        return false;
    }

    memories[fi][slot - 1].present = true;
    memories[fi][slot - 1].low     = low;
    memories[fi][slot - 1].high    = high;
    err_buf[0] = '\0';
    return true;
}

bool dfl_assign_favorite(dfl_family_t family, int slot, int source) {
    int fi = fam_idx(family);

    if (fi < 0 || !slot_ok(slot, DFL_QUICK_SLOTS)) {
        set_error("familia o ranura invalida");
        return false;
    }
    if (source < DFL_SRC_FACTORY_1 || source > DFL_SRC_MEM_2) {
        set_error("fuente invalida");
        return false;
    }
    if ((source == DFL_SRC_MEM_1 || source == DFL_SRC_MEM_2)
        && !memories[fi][source - DFL_SRC_MEM_1].present) {
        set_error("memoria vacia");
        return false;
    }

    if (!dfl_store_save_favorite(family, slot, source)) {
        set_error(dfl_store_last_error());
        return false;
    }

    favorites[fi][slot - 1] = source;
    err_buf[0] = '\0';
    return true;
}
