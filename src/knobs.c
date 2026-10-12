/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - encoder state for the apps that draw the knobs (see
 *  knobs.h). Names come from our vol/mfk mode enums; the values are read from
 *  the same cfg parameters those encoders change.
 */

#include "knobs.h"

#include "cfg/cfg.h"
#include "mfk.h"
#include "vol.h"

#include <stdio.h>

/* Names of our vol_mode_t (vol.h) */
static const char *const vol_names[VOL_LAST] = {
    [VOL_VOL]          = "Volume",
    [VOL_SQL]          = "Squelch",
    [VOL_RFG]          = "RF gain",
    [VOL_FILTER_LOW]   = "Filter low",
    [VOL_FILTER_HIGH]  = "Filter high",
    [VOL_PWR]          = "Power",
    [VOL_HMIC]         = "H-Mic",
    [VOL_MIC]          = "Mic",
    [VOL_IMIC]         = "I-Mic",
    [VOL_MONI]         = "Monitor",
    [VOL_SPMODE]       = "Span",
    [VOL_VOICE_LANG]   = "Voice language",
    [VOL_VOICE_RATE]   = "Voice rate",
    [VOL_VOICE_PITCH]  = "Voice pitch",
    [VOL_VOICE_VOLUME] = "Voice volume",
    [VOL_FILTER_BW]    = "Filter width",
};

/* Names of the MFK modes that have a plain name; the rest fall back to "MFK" */
static const char *mfk_name(mfk_mode_t mode) {
    switch (mode) {
        case MFK_MIN_LEVEL:       return "Min level";
        case MFK_MAX_LEVEL:       return "Max level";
        case MFK_AGC:             return "AGC";
        case MFK_AGC_HANG:        return "AGC hang";
        case MFK_AGC_KNEE:        return "AGC knee";
        case MFK_AGC_SLOPE:       return "AGC slope";
        case MFK_NB:              return "NB";
        case MFK_NB_LEVEL:        return "NB level";
        case MFK_NB_WIDTH:        return "NB width";
        case MFK_NR:              return "NR";
        case MFK_NR_LEVEL:        return "NR level";
        case MFK_DNF:             return "DNF";
        case MFK_DNF_CENTER:      return "DNF center";
        case MFK_DNF_WIDTH:       return "DNF width";
        case MFK_KEY_SPEED:       return "Key speed";
        case MFK_KEY_TONE:        return "Key tone";
        case MFK_KEY_VOL:         return "Key volume";
        case MFK_ATT:             return "ATT";
        case MFK_PRE:             return "PRE";
        case MFK_COMP:            return "COMP";
        case MFK_ANT:             return "Antenna";
        case MFK_RIT:             return "RIT";
        case MFK_XIT:             return "XIT";
        case MFK_CHARGER:         return "Charger";
        case MFK_SPECTRUM_BETA:   return "Spectrum";
        case MFK_FFT_DECIM:       return "FFT decimation";
        default:                  return "MFK";
    }
}

bool knobs_visible(void) {
    return true;
}

bool knobs_info(bool vol, const char **name, char *val, size_t size, float *level, bool *edit) {
    int32_t v = 0;

    if (vol) {
        vol_mode_t mode = vol_get_mode();

        *name  = mode < VOL_LAST ? vol_names[mode] : "Volume";
        *edit  = true;                          /* the volume knobs always change their value */

        switch (mode) {
            case VOL_VOL: v = subject_get_int(cfg.vol.val); *level = (float) v / 20.0f; break;
            case VOL_SQL: v = subject_get_int(cfg.sql.val); *level = (float) v / 20.0f; break;
            case VOL_PWR: v = subject_get_int(cfg.pwr.val); *level = -1.0f; break;
            default:      v = 0; *level = -1.0f; break;   /* no scale known here */
        }
    } else {
        mfk_mode_t mode = mfk_get_mode();

        *name = mfk_name(mode);
        *edit = mfk_state == MFK_STATE_EDIT;
        v     = 0;
        *level = -1.0f;
    }

    snprintf(val, size, "%d", (int) v);

    return true;
}
