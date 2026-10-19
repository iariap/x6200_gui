/* Stub for src/params/params.h: press_action_t and the FT8 protocol src/remote_server.cpp uses. */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FTX_PROTOCOL_FT4,
    FTX_PROTOCOL_FT8
} ftx_protocol_t;

typedef enum {
    ACTION_NONE = 0,
    ACTION_SCREENSHOT,
    ACTION_SCREEN_REC,
    ACTION_RECORDER,
    ACTION_MUTE,
    ACTION_STEP_UP,
    ACTION_STEP_DOWN,
    ACTION_VOICE_MODE,
    ACTION_BAT_INFO,
    ACTION_NR_TOGGLE,
    ACTION_NB_TOGGLE,

    ACTION_APP_RTTY = 100,
    ACTION_APP_FT8,
    ACTION_APP_WSPR,
    ACTION_APP_NAVTEX,
    ACTION_APP_SSTV,
    ACTION_APP_WEFAX,
    ACTION_APP_CLUSTER,
    ACTION_APP_FIELD,
    ACTION_APP_JS8,
    ACTION_APP_CW_KBD,
    ACTION_APP_CWTRAINER,
    ACTION_APP_BT,
    ACTION_APP_STATUS,
    ACTION_THEME,
    ACTION_APP_SWRSCAN,
    ACTION_APP_SETTINGS,
    ACTION_APP_RECORDER,
    ACTION_APP_QTH,
    ACTION_APP_CALLSIGN,
    ACTION_APP_WIFI,
    ACTION_APP_EQ,

    ACTION_APP_GPS = 122,
    ACTION_ATT_TOGGLE,
    ACTION_PRE_TOGGLE,
    ACTION_ATU_TOGGLE,
    ACTION_ATU_TUNE,
    ACTION_WF_MODE,
    ACTION_APP_BANDS,
    ACTION_ATUNE,
    ACTION_APP_PROPAGATION,
} press_action_t;

typedef struct {
    ftx_protocol_t ft8_protocol;
} params_t;

#ifdef __cplusplus
extern "C" {
#endif

extern params_t params;

#ifdef __cplusplus
}
#endif
