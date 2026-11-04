/* QA-LINUX-DSP-RECOVERY-HARDEN — failover/restore/lifecycle offline. */
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "route_arbiter.h"
#include "route_supervisor.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
typedef struct { int writes; int fail; uint32_t log[64]; } bus_t;
static int bw(void *ctx, uint32_t v) { bus_t *b = ctx; if (b->fail) { b->fail = 0; return -1; } if (b->writes < 64) b->log[b->writes++] = v; return 0; }

static void boot_ready(route_arbiter_t *a) {
    CHECK(route_arbiter_boot(a) == ROUTE_ERR_OK);
    CHECK(route_arbiter_client_heartbeat(a, 100, 1, 0, true) == ROUTE_ERR_OK);
    CHECK(route_arbiter_pcm_submit(a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 101) == ROUTE_ERR_OK);
    CHECK(route_arbiter_set_enabled(a, 120, true) == ROUTE_ERR_OK);
    CHECK(route_arbiter_state(a) == ROUTE_STATE_RX_LINUX);
}

static void test_arbiter_lost_and_shutdown(void) {
    bus_t b; memset(&b, 0, sizeof(b));
    route_bus_t bus = { .write_reg12 = bw, .ctx = &b };
    route_arbiter_t *a = route_arbiter_create(&bus, 0);
    CHECK(a != NULL);
    boot_ready(a);
    int before = b.writes;
    route_arbiter_mark_unavailable(a);
    CHECK(route_arbiter_available(a) == false);
    CHECK(route_arbiter_state(a) == ROUTE_STATE_FAULT);
    CHECK(b.writes == before);                       /* no escritura ciega */
    CHECK(route_shadow_valid(route_arbiter_shadow(a)) == false);
    CHECK(route_arbiter_set_enabled(a, 130, true) != ROUTE_ERR_OK);
    CHECK(route_arbiter_tick(a, 140) == ROUTE_EV_FAULT);
    CHECK(route_arbiter_physical_confirmed(a) == false);
    /* disponibilidad no implica auto-recovery */
    route_arbiter_mark_available(a);
    CHECK(route_arbiter_available(a) == true);
    CHECK(route_arbiter_set_enabled(a, 150, true) != ROUTE_ERR_OK);
    CHECK(route_arbiter_boot(a) == ROUTE_ERR_OK);     /* BASE safe, no RX automático */
    CHECK(route_arbiter_state(a) == ROUTE_STATE_BASE);
    CHECK(route_arbiter_set_enabled(a, 300, true) != ROUTE_ERR_OK);  /* lease vencido: no RX */
    route_arbiter_destroy(a);

    /* shutdown seguro escribe BASE una vez, invalida sombra y no confirma físico */
    memset(&b, 0, sizeof(b));
    a = route_arbiter_create(&bus, 0); boot_ready(a);
    before = b.writes;
    CHECK(route_arbiter_shutdown_safe(a, 200) == ROUTE_ERR_OK);
    CHECK(b.writes == before + 1);
    CHECK((b.log[b.writes-1] & ROUTE_REG_VOICE_REC) == 0);
    CHECK(route_arbiter_state(a) == ROUTE_STATE_DEGRADED_BASE);
    CHECK(route_shadow_valid(route_arbiter_shadow(a)) == false);
    CHECK(route_arbiter_shutdown_done(a) == true);
    CHECK(route_arbiter_physical_confirmed(a) == false);
    CHECK(route_arbiter_set_enabled(a, 210, true) != ROUTE_ERR_OK);
    route_arbiter_destroy(a);
}
static void test_preconditions_never_rx(void) {
    bus_t b; memset(&b, 0, sizeof(b));
    route_bus_t bus = { .write_reg12 = bw, .ctx = &b };
    route_arbiter_t *a = route_arbiter_create(&bus, 0);
    CHECK(route_arbiter_boot(a) == ROUTE_ERR_OK);
    CHECK(route_arbiter_set_enabled(a, 1, true) != ROUTE_ERR_OK);    /* sin hb/pcm */
    CHECK(route_arbiter_client_heartbeat(a, 2, 1, 0, true) == ROUTE_ERR_OK);
    CHECK(route_arbiter_set_enabled(a, 3, true) != ROUTE_ERR_OK);    /* sin pcm */
    CHECK(route_arbiter_pcm_submit(a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 4) == ROUTE_ERR_OK);
    route_arbiter_set_generated_audio(a, true);
    CHECK(route_arbiter_set_enabled(a, 5, true) != ROUTE_ERR_OK);    /* generado */
    route_arbiter_set_generated_audio(a, false);
    route_arbiter_set_sink_ok(a, false);
    CHECK(route_arbiter_set_enabled(a, 6, true) != ROUTE_ERR_OK);    /* sink */
    route_arbiter_set_sink_ok(a, true);
    CHECK(route_arbiter_set_enabled(a, 7, true) == ROUTE_ERR_OK);   /* sólo con todo */
    CHECK(route_arbiter_state(a) == ROUTE_STATE_RX_LINUX);
    /* heartbeat repetido (seq no monótona) no renueva lease */
    CHECK(route_arbiter_client_heartbeat(a, 8, 1, 0, true) == ROUTE_ERR_STALE);
    route_arbiter_destroy(a);
}
static void test_supervisor_failover(void) {
    route_supervisor_t s; route_supervisor_init(&s, 100, 250);
    route_supervisor_gui_heartbeat(&s, 1000);
    route_supervisor_arbiter_status(&s, 1000, true, ROUTE_STATE_RX_LINUX);
    CHECK(route_supervisor_tick(&s, 1050) == ROUTE_SUP_NONE);
    CHECK(route_supervisor_tick(&s, 1200) == ROUTE_SUP_REQUEST_RESTORE);
    route_supervisor_note_restored(&s, 1230, ROUTE_STATE_DEGRADED_BASE);
    CHECK(route_supervisor_physical_recovery_blocked(&s) == false);
    CHECK(route_supervisor_tick(&s, 1240) == ROUTE_SUP_NONE);
    route_supervisor_arbiter_lost(&s);
    CHECK(route_supervisor_physical_recovery_blocked(&s) == true);
    CHECK(route_supervisor_tick(&s, 1250) == ROUTE_SUP_FAULT);
    CHECK(s.arbiter_failures >= 1);
    /* restaurado lógico limpia el block (sigue sin confirmación física) */
    route_supervisor_note_restored(&s, 1260, ROUTE_STATE_BASE);
    CHECK(route_supervisor_physical_recovery_blocked(&s) == false);
    CHECK(route_supervisor_tick(&s, 1270) == ROUTE_SUP_NONE);
}
int main(void) {
    test_arbiter_lost_and_shutdown();
    test_preconditions_never_rx();
    test_supervisor_failover();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
