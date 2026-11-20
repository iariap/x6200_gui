#pragma once
#include <stdbool.h>
/* Modelo OFFLINE de evidencia de recuperación a BASE. No escribe hardware.
 * Separa: pedido lógico / comando / transporte aceptado / cambio físico / BASE audible. */
typedef enum {
    EV_NONE = 0,
    EV_LOGICAL_REQUESTED,     /* software decidió restaurar */
    EV_COMMAND_SENT,          /* se invocó la API/IOCTL */
    EV_TRANSPORT_ACCEPTED,    /* bus/ACK aceptó la escritura (no prueba efecto) */
    EV_PHYSICAL_OBSERVED,     /* observación física independiente (no disponible sin readback) */
    EV_AUDIBLE_OBSERVED       /* evidencia acústica externa de BASE (no readback) */
} recovery_evidence_t;

typedef enum {
    REC_UNSAFE = 0,
    REC_BLOCKED_PHYSICAL,
    REC_LOGICAL_BASE_ONLY,    /* aceptado lógicamente; físico NO confirmado */
    REC_PHYSICAL_CONFIRMED    /* sólo con evidencia física/acústica independiente */
} recovery_verdict_t;

typedef struct {
    bool arbiter_alive;            /* proceso árbitro responde */
    bool independent_watchdog;     /* watchdog NO depende del proceso/transporte caído */
    bool transport_accepted;       /* escritura BASE aceptada por el transporte */
    bool shadow_valid;
    bool second_writer;            /* sería competencia I2C: prohibido */
    bool tx_bits_preserved;        /* PTT/ATU/modem no se pisan */
    bool reactivate_after_unknown; /* re-habilitar sin evidencia: prohibido */
    bool acoustic_observed;        /* grabación externa confirma BASE audible */
    bool external_readback;        /* readback MCU/estado MUX (no disponible hoy) */
    bool physically_unavailable;   /* fallo físico del árbitro/transporte */
} recovery_scenario_t;

recovery_evidence_t recovery_evidence_level(const recovery_scenario_t *s);
recovery_verdict_t recovery_classify(const recovery_scenario_t *s);
const char *recovery_evidence_name(recovery_evidence_t e);
const char *recovery_verdict_name(recovery_verdict_t v);
