#include "recovery_evidence_model.h"
#include <stddef.h>

recovery_evidence_t recovery_evidence_level(const recovery_scenario_t *s) {
    if (s == NULL) return EV_NONE;
    if (s->external_readback) return EV_PHYSICAL_OBSERVED;
    if (s->acoustic_observed) return EV_AUDIBLE_OBSERVED;
    if (s->transport_accepted) return EV_TRANSPORT_ACCEPTED;
    if (s->arbiter_alive || s->independent_watchdog) return EV_COMMAND_SENT;
    return EV_NONE;
}
recovery_verdict_t recovery_classify(const recovery_scenario_t *s) {
    if (s == NULL) return REC_UNSAFE;
    /* Invariantes de seguridad: nunca segundo writer, nunca reactivar sin evidencia,
     * nunca pisar bits compartidos. */
    if (s->second_writer || !s->tx_bits_preserved || s->reactivate_after_unknown) return REC_UNSAFE;
    if (s->external_readback || s->acoustic_observed) return REC_PHYSICAL_CONFIRMED;
    if (s->arbiter_alive && s->shadow_valid && s->transport_accepted) return REC_LOGICAL_BASE_ONLY;
    /* Watchdog software dependiente del proceso/transporte caído NO es recuperación
     * independiente; sin árbitro/transporte no hay escritura segura => bloqueado. */
    return REC_BLOCKED_PHYSICAL;
}
const char *recovery_evidence_name(recovery_evidence_t e) {
    switch (e) {
        case EV_NONE: return "NONE";
        case EV_LOGICAL_REQUESTED: return "LOGICAL_REQUESTED";
        case EV_COMMAND_SENT: return "COMMAND_SENT";
        case EV_TRANSPORT_ACCEPTED: return "TRANSPORT_ACCEPTED";
        case EV_PHYSICAL_OBSERVED: return "PHYSICAL_OBSERVED";
        case EV_AUDIBLE_OBSERVED: return "AUDIBLE_OBSERVED";
    }
    return "?";
}
const char *recovery_verdict_name(recovery_verdict_t v) {
    switch (v) {
        case REC_UNSAFE: return "UNSAFE";
        case REC_BLOCKED_PHYSICAL: return "BLOCKED_PHYSICAL_RECOVERY";
        case REC_LOGICAL_BASE_ONLY: return "LOGICAL_BASE_ONLY";
        case REC_PHYSICAL_CONFIRMED: return "PHYSICAL_CONFIRMED";
    }
    return "?";
}
