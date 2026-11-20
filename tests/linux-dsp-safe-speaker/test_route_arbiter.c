/* QA-LINUX-DSP-ROUTE — harness determinista del modelo de arbitro (sin hardware). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "route_arbiter_model.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

typedef struct {
    int fail_next;
    uint32_t log[64];
    int n;
} fake_t;

static int fake_write(void *ctx, uint32_t value) {
    fake_t *f = (fake_t *)ctx;
    if (f->fail_next) { f->fail_next = 0; return -1; }
    if (f->n < (int)(sizeof(f->log)/sizeof(f->log[0]))) f->log[f->n++] = value;
    return 0;
}
static fake_t f;
static ra_backend_t be;

static ra_t *mk(void) {
    memset(&f, 0, sizeof(f));
    be.write_reg12 = fake_write; be.ctx = &f;
    return ra_create(&be, 0);
}

/* helper: boot + heartbeat + un PCM post-Speex + enable */
static ra_t *armed(uint64_t t_enable) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_gui_heartbeat(ra, 5, 0x100000u, true));
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 1, 6));
    CHECK(ra_set_enabled(ra, true, t_enable));
    return ra;
}

static void test_boot_base_no_auto_enable(void) {
    ra_t *ra = mk();
    CHECK(ra_state(ra) == RA_BASE);
    CHECK(ra_truth(ra) == RA_TRUTH_UNKNOWN);
    CHECK(ra_set_enabled(ra, true, 10) == false);   /* sin boot/heartbeat/pcm */
    CHECK(ra_state(ra) == RA_BASE);
    ra_destroy(ra);
}

static void test_boot_then_base_no_auto_enable(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_state(ra) == RA_BASE);
    CHECK(ra_truth(ra) == RA_TRUTH_ARBITER_SHADOW);
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) == 0);
    CHECK(f.n == 1);                     /* única escritura: BASE inicial */
    CHECK(ra_set_enabled(ra, true, 10) == false);  /* no heartbeat/pcm */
    CHECK(ra_state(ra) == RA_BASE);
    ra_destroy(ra);
}

static void test_enable_requires_heartbeat_and_pcm(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_set_enabled(ra, true, 10) == false);
    CHECK(ra_gui_heartbeat(ra, 5, 0, true));
    CHECK(ra_set_enabled(ra, true, 10) == false);   /* falta PCM */
    CHECK(ra_state(ra) == RA_BASE);
    ra_destroy(ra);
}

static void test_enable_sets_only_voice_bit(void) {
    ra_t *ra = armed(20);
    CHECK(ra_state(ra) == RA_RX_LINUX);
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) != 0);
    CHECK((ra_shadow(ra) & ~RA_REG_VOICE_REC) == 0x100000u);  /* bits compartidos intactos */
    CHECK(f.log[f.n-1] == (0x100000u | RA_REG_VOICE_REC));
    ra_destroy(ra);
}

static void test_enable_rejects_tx_bits(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_gui_heartbeat(ra, 5, RA_REG_IPTT | 0x100000u, true));  /* PTT activo */
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 1, 6));
    CHECK(ra_set_enabled(ra, true, 20) == false);
    CHECK(ra_state(ra) != RA_RX_LINUX);
    ra_destroy(ra);
}

static void test_heartbeat_write_failure_blocks_enable(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_gui_heartbeat(ra, 5, 0, false));    /* última escritura GUI falló */
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 1, 6));
    CHECK(ra_set_enabled(ra, true, 20) == false);
    ra_destroy(ra);
}

static void test_tx_prepare_disables_first(void) {
    ra_t *ra = armed(20);
    int before = f.n;
    CHECK(ra_tx_prepare(ra, 30));
    CHECK(f.n == before + 1);
    CHECK(f.log[f.n-1] == 0x100000u);            /* BASE, sin voice_rec */
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    CHECK(ra_stats(ra)->tx_disables == 1);
    ra_destroy(ra);
}

static void test_tx_prepare_bus_failure_fault(void) {
    ra_t *ra = armed(20);
    f.fail_next = 1;
    CHECK(ra_tx_prepare(ra, 30) == false);
    CHECK(ra_state(ra) == RA_FAULT);
    CHECK(ra_set_enabled(ra, true, 40) == false);   /* FAULT nunca habilita */
    ra_destroy(ra);
}

static void test_atu_prepare_same_order(void) {
    ra_t *ra = armed(20);
    CHECK(ra_atu_prepare(ra, 30));
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) == 0);
    ra_destroy(ra);
}

static void test_restore_on_heartbeat_loss(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_gui_heartbeat(ra, 100, 0x100000u, true));
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 1, 101));
    CHECK(ra_set_enabled(ra, true, 110));
    CHECK(ra_state(ra) == RA_RX_LINUX);
    uint64_t hb = 100, restored_at = 0;
    for (uint64_t t = 120; t <= 260; t += 10) {
        ra_event_t ev = ra_tick(ra, t);
        if (ev == RA_EV_RESTORED || ev == RA_EV_FAULT) { restored_at = t; break; }
    }
    CHECK(restored_at != 0);
    CHECK(restored_at - hb <= RA_RESTORE_BUDGET_MS);   /* <=250 ms desde el último heartbeat */
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) == 0);
    CHECK((ra_shadow(ra) & ~RA_REG_VOICE_REC) == 0x100000u);
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    CHECK(ra_stats(ra)->restores == 1);
    printf("restore_latency_ms=%llu (budget=%u)\n",
           (unsigned long long)ra_stats(ra)->last_restore_latency_ms, RA_RESTORE_BUDGET_MS);
    ra_destroy(ra);
}

static void test_gui_exit_restore(void) {
    ra_t *ra = armed(20);
    ra_gui_exit(ra);
    CHECK(ra_tick(ra, 30) == RA_EV_RESTORED);
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) == 0);
    ra_destroy(ra);
}

static void test_generated_audio_restores(void) {
    ra_t *ra = armed(20);
    ra_set_generated_audio(ra, true);
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) == 0);
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    CHECK(ra_set_enabled(ra, true, 40) == false);   /* generado activo: no se habilita */
    ra_destroy(ra);
}

static void test_sink_lost_restores(void) {
    ra_t *ra = armed(20);
    CHECK(ra_sink_lost(ra, 25) == RA_EV_RESTORED);
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    ra_destroy(ra);
}

static void test_pcm_stale_restores(void) {
    ra_t *ra = armed(20);
    CHECK(ra_pcm_stale(ra, 25) == RA_EV_RESTORED);
    CHECK(ra_state(ra) == RA_DEGRADED_BASE);
    ra_destroy(ra);
}

static void test_pcm_format_and_stage_rejected(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_gui_heartbeat(ra, 5, 0, true));
    CHECK(ra_pcm_submit(ra, 4410, 48000, true, 1, 6) == false);   /* rate incorrecto */
    CHECK(ra_pcm_submit(ra, 4410, 44100, false, 2, 6) == false);  /* RAW: no post-Speex */
    CHECK(ra_set_enabled(ra, true, 20) == false);
    ra_destroy(ra);
}

static void test_pcm_double_block_rejected(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    CHECK(ra_gui_heartbeat(ra, 5, 0, true));
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 7, 6));
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 7, 7) == false);   /* mismo bloque: doble */
    CHECK(ra_stats(ra)->double_process == 1);
    ra_destroy(ra);
}

static void test_pcm_overflow_bounded(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    for (uint64_t id = 1; id <= 7; id++) CHECK(ra_pcm_submit(ra, 4410, 44100, true, id, id));
    CHECK(ra_stats(ra)->overflows == 3);          /* ring 4: 7-4 */
    CHECK(ra_stats(ra)->dropped_blocks == 3);
    ra_destroy(ra);
}

static void test_restore_bus_failure_fault(void) {
    ra_t *ra = armed(20);
    f.fail_next = 1;
    ra_gui_exit(ra);
    CHECK(ra_tick(ra, 30) == RA_EV_FAULT);
    CHECK(ra_state(ra) == RA_FAULT);
    CHECK(ra_set_enabled(ra, true, 40) == false);
    ra_destroy(ra);
}

static void test_boot_bus_failure_fault(void) {
    ra_t *ra = mk();
    f.fail_next = 1;
    CHECK(ra_boot_init(ra) == false);
    CHECK(ra_state(ra) == RA_FAULT);
    CHECK(ra_truth(ra) == RA_TRUTH_UNKNOWN);
    ra_destroy(ra);
}

static void test_heartbeat_does_not_write(void) {
    ra_t *ra = mk();
    CHECK(ra_boot_init(ra));
    int n = f.n;
    for (int i = 0; i < 20; i++) CHECK(ra_gui_heartbeat(ra, 10 + i, 0x1234u, true));
    CHECK(f.n == n);                               /* sólo el árbitro escribe */
    ra_destroy(ra);
}

static void test_disable_and_reenable(void) {
    ra_t *ra = armed(20);
    CHECK(ra_set_enabled(ra, false, 30));
    CHECK(ra_state(ra) == RA_BASE);
    CHECK((ra_shadow(ra) & RA_REG_VOICE_REC) == 0);
    CHECK(ra_gui_heartbeat(ra, 40, 0x100000u, true));
    CHECK(ra_pcm_submit(ra, 4410, 44100, true, 2, 41));
    CHECK(ra_set_enabled(ra, true, 50));
    CHECK(ra_state(ra) == RA_RX_LINUX);
    ra_destroy(ra);
}

int main(void) {
    test_boot_base_no_auto_enable();
    test_boot_then_base_no_auto_enable();
    test_enable_requires_heartbeat_and_pcm();
    test_enable_sets_only_voice_bit();
    test_enable_rejects_tx_bits();
    test_heartbeat_write_failure_blocks_enable();
    test_tx_prepare_disables_first();
    test_tx_prepare_bus_failure_fault();
    test_atu_prepare_same_order();
    test_restore_on_heartbeat_loss();
    test_gui_exit_restore();
    test_generated_audio_restores();
    test_sink_lost_restores();
    test_pcm_stale_restores();
    test_pcm_format_and_stage_rejected();
    test_pcm_double_block_rejected();
    test_pcm_overflow_bounded();
    test_restore_bus_failure_fault();
    test_boot_bus_failure_fault();
    test_heartbeat_does_not_write();
    test_disable_and_reenable();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
