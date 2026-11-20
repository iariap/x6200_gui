#include <stdio.h>
#include "recovery_evidence_model.h"
static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
static recovery_scenario_t base(void) {
    recovery_scenario_t s = {0};
    s.arbiter_alive = true; s.independent_watchdog = true; s.transport_accepted = true;
    s.shadow_valid = true; s.tx_bits_preserved = true;
    return s;
}
int main(void) {
    recovery_scenario_t s = base();
    CHECK(recovery_evidence_level(&s) == EV_TRANSPORT_ACCEPTED);   /* sin observación física */
    CHECK(recovery_classify(&s) == REC_LOGICAL_BASE_ONLY);         /* NO físico */
    /* watchdog dependiente del proceso caído + árbitro muerto => bloqueado, no restore */
    s = base(); s.arbiter_alive = false; s.independent_watchdog = false; s.transport_accepted = false;
    CHECK(recovery_classify(&s) == REC_BLOCKED_PHYSICAL);
    /* watchdog "independiente" sin transporte no alcanza */
    s = base(); s.arbiter_alive = false; s.independent_watchdog = true; s.transport_accepted = false;
    CHECK(recovery_classify(&s) == REC_BLOCKED_PHYSICAL);
    CHECK(recovery_evidence_level(&s) == EV_COMMAND_SENT);
    /* segundo writer => UNSAFE */
    s = base(); s.second_writer = true; CHECK(recovery_classify(&s) == REC_UNSAFE);
    /* bits TX/ATU pisados => UNSAFE */
    s = base(); s.tx_bits_preserved = false; CHECK(recovery_classify(&s) == REC_UNSAFE);
    /* reactivar tras estado desconocido => UNSAFE */
    s = base(); s.reactivate_after_unknown = true; CHECK(recovery_classify(&s) == REC_UNSAFE);
    /* evidencia acústica => audible, NO readback MUX */
    s = base(); s.acoustic_observed = true;
    CHECK(recovery_evidence_level(&s) == EV_AUDIBLE_OBSERVED);
    CHECK(recovery_classify(&s) == REC_PHYSICAL_CONFIRMED);
    /* readback externo (no disponible) => physical observed */
    s = base(); s.external_readback = true;
    CHECK(recovery_evidence_level(&s) == EV_PHYSICAL_OBSERVED);
    /* sin nada => none/bloqueado */
    s = (recovery_scenario_t){0}; s.tx_bits_preserved = true;
    CHECK(recovery_evidence_level(&s) == EV_NONE);
    CHECK(recovery_classify(&s) == REC_BLOCKED_PHYSICAL);
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
