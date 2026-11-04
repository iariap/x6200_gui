/* Modelo de referencia OFFLINE — ver route_arbiter_model.h. Sin E/S real. */
#define _POSIX_C_SOURCE 200809L
#include "route_arbiter_model.h"
#include <stdlib.h>
#include <string.h>

struct ra {
    ra_backend_t be;
    ra_state_t state;
    ra_truth_t truth;
    uint32_t shadow;              /* valor aplicado por el arbitro (sombra autoritativa) */
    bool initialized, enabled, gui_alive, generated, pcm_ok;
    uint32_t hb_value; bool hb_write_ok;
    uint64_t hb_ms;
    uint64_t restore_deadline_ms;
    bool restore_pending;
    ra_stats_t stats;
    uint32_t writes;
    uint32_t ring_count;
    uint64_t last_block_id;
};

static uint32_t only_bit(uint32_t base, bool on) {
    return on ? (base | RA_REG_VOICE_REC) : (base & ~RA_REG_VOICE_REC);
}

static int bus_write(ra_t *ra, uint32_t value) {
    ra->writes++;
    ra->stats.writes++;
    if (ra->be.write_reg12 == NULL) { ra->stats.bus_errors++; return -1; }
    int r = ra->be.write_reg12(ra->be.ctx, value);
    if (r != 0) { ra->stats.bus_errors++; return -1; }
    ra->shadow = value;
    return 0;
}

ra_t *ra_create(const ra_backend_t *be, uint64_t now_ms) {
    (void)now_ms;
    ra_t *ra = calloc(1, sizeof(*ra));
    if (ra == NULL) return NULL;
    if (be != NULL) ra->be = *be;
    ra->state = RA_BASE; ra->truth = RA_TRUTH_UNKNOWN;
    ra->initialized = false; ra->enabled = false; ra->gui_alive = false;
    ra->pcm_ok = false; ra->restore_pending = false;
    return ra;
}
void ra_destroy(ra_t *ra) { free(ra); }

bool ra_boot_init(ra_t *ra) {
    /* Arranque: BASE inicial, sin auto-enable y SIN habilitar hasta confirmar
     * la escritura BASE (sombra del arbitro). No usa readback del MCU. */
    if (ra == NULL) return false;
    ra->enabled = false; ra->state = RA_BASE; ra->truth = RA_TRUTH_UNKNOWN;
    ra->shadow = 0;
    if (bus_write(ra, 0u) != 0) { ra->state = RA_FAULT; ra->stats.fault_count++; return false; }
    ra->truth = RA_TRUTH_ARBITER_SHADOW;
    ra->initialized = true;
    return true;
}

bool ra_gui_heartbeat(ra_t *ra, uint64_t now_ms, uint32_t last_written_reg12, bool last_write_ok) {
    if (ra == NULL || !ra->initialized) return false;
    ra->gui_alive = true;
    ra->hb_ms = now_ms;
    ra->hb_value = last_written_reg12;
    ra->hb_write_ok = last_write_ok;
    return true;
}

void ra_gui_exit(ra_t *ra) { if (ra != NULL) ra->gui_alive = false; }

static bool gui_fresh(const ra_t *ra, uint64_t now) {
    return ra->gui_alive && ra->hb_ms != 0 && now >= ra->hb_ms && (now - ra->hb_ms) <= RA_LEASE_MS;
}

static ra_event_t degrade_restore(ra_t *ra, uint64_t now_ms, ra_event_t why) {
    ra->stats.denied++;
    if (ra->state != RA_RX_LINUX && ra->state != RA_READY) return why;
    if (bus_write(ra, only_bit(ra->shadow, false)) != 0) {
        ra->state = RA_FAULT; ra->stats.fault_count++;
        return RA_EV_FAULT;
    }
    ra->stats.restores++;
    ra->stats.last_restore_ms = now_ms;
    if (ra->hb_ms != 0 && now_ms >= ra->hb_ms) {
        uint64_t lat = now_ms - ra->hb_ms;
        ra->stats.last_restore_latency_ms = lat;
        if (lat > ra->stats.max_restore_ms) ra->stats.max_restore_ms = lat;
    }
    ra->state = RA_DEGRADED_BASE;
    return RA_EV_RESTORED;
}

bool ra_set_enabled(ra_t *ra, bool enabled, uint64_t now_ms) {
    if (ra == NULL || !ra->initialized) return false;
    if (!enabled) { ra->enabled = false; degrade_restore(ra, now_ms, RA_EV_OK); ra->state = RA_BASE; return true; }
    /* Reglas de entrada: nunca auto-enable, nunca con estado desconocido. */
    if (ra->truth != RA_TRUTH_ARBITER_SHADOW) { ra->stats.denied++; return false; }
    if (!gui_fresh(ra, now_ms)) { ra->stats.denied++; return false; }
    if (!ra->hb_write_ok) { ra->stats.denied++; return false; }
    if (!ra->pcm_ok) { ra->stats.denied++; return false; }
    if (ra->generated) { ra->stats.denied++; return false; }
    if (ra->state == RA_FAULT) { ra->stats.denied++; return false; }
    if ((ra->hb_value & RA_REG_TX_MASK) != 0 || (ra->shadow & RA_REG_TX_MASK) != 0) {
        ra->stats.denied++; return false;
    }
    /* Verdad de arbitro: la GUI viva es el unico escritor y su ultimo valor con
     * ioctl OK es la base autoritativa; se adopta y solo se cambia el bit 0x8. */
    ra->shadow = ra->hb_value;
    if (bus_write(ra, only_bit(ra->shadow, true)) != 0) {
        ra->state = RA_FAULT; ra->stats.fault_count++; return false;
    }
    ra->enabled = true;
    ra->state = (ra->pcm_ok) ? RA_RX_LINUX : RA_READY;
    return true;
}

bool ra_tx_prepare(ra_t *ra, uint64_t now_ms) {
    if (ra == NULL) return false;
    if (ra->state == RA_RX_LINUX || ra->state == RA_READY) {
        /* TX/ATU: el arbitro deshabilita RX Linux ANTES de permitir PTT. */
        if (bus_write(ra, only_bit(ra->shadow, false)) != 0) {
            ra->state = RA_FAULT; ra->stats.fault_count++; return false;
        }
        ra->stats.tx_disables++;
        ra->state = RA_DEGRADED_BASE;
        ra->enabled = false;
    }
    (void)now_ms;
    return ra->state != RA_FAULT;
}

bool ra_atu_prepare(ra_t *ra, uint64_t now_ms) { return ra_tx_prepare(ra, now_ms); }

void ra_set_generated_audio(ra_t *ra, bool active) {
    if (ra == NULL) return;
    ra->generated = active;
    if (active && ra->state == RA_RX_LINUX) {
        degrade_restore(ra, ra->hb_ms, RA_EV_RESTORED);
    }
}

bool ra_pcm_submit(ra_t *ra, size_t samples, uint32_t rate, bool already_processed,
                   uint64_t block_id, uint64_t now_ms) {
    (void)now_ms;
    if (ra == NULL) return false;
    if (samples == 0 || samples > RA_BLOCK_SAMPLES) { ra->stats.underruns++; ra->pcm_ok = false; return false; }
    if (rate != RA_PCM_RATE) { ra->pcm_ok = false; return false; }
    /* El router consume PCM POST-SpeexDSP exactamente una vez. RAW o repetido se rechaza. */
    if (!already_processed) { ra->pcm_ok = false; return false; }
    if (block_id != 0 && block_id == ra->last_block_id) {
        ra->stats.double_process++; ra->pcm_ok = false; return false;
    }
    if (block_id != 0) ra->last_block_id = block_id;
    if (ra->ring_count == RA_RING_BLOCKS) { ra->stats.overflows++; ra->stats.dropped_blocks++; ra->ring_count--; }
    ra->ring_count++;
    ra->stats.pcm_blocks++;
    ra->pcm_ok = true;
    return true;
}

ra_event_t ra_sink_lost(ra_t *ra, uint64_t now_ms) { return degrade_restore(ra, now_ms, RA_EV_RESTORED); }
ra_event_t ra_pcm_stale(ra_t *ra, uint64_t now_ms) { ra->pcm_ok = false; return degrade_restore(ra, now_ms, RA_EV_RESTORED); }

ra_event_t ra_tick(ra_t *ra, uint64_t now_ms) {
    if (ra == NULL) return RA_EV_FAULT;
    if (ra->state == RA_RX_LINUX || ra->state == RA_READY) {
        bool lease_expired = (!ra->gui_alive) ||
            !gui_fresh(ra, now_ms);
        if (lease_expired) {
            /* Restore independiente: el arbitro sobrevive a la GUI y escribe BASE. */
            if (bus_write(ra, only_bit(ra->shadow, false)) != 0) {
                ra->state = RA_FAULT; ra->stats.fault_count++;
                ra->stats.last_restore_ms = now_ms;
                return RA_EV_FAULT;
            }
            ra->stats.restores++;
            ra->stats.last_restore_ms = now_ms;
            if (ra->hb_ms != 0 && now_ms >= ra->hb_ms) {
                uint64_t lat = now_ms - ra->hb_ms;
                ra->stats.last_restore_latency_ms = lat;
                if (lat > ra->stats.max_restore_ms) ra->stats.max_restore_ms = lat;
            }
            ra->enabled = false;
            ra->state = RA_DEGRADED_BASE;
            return RA_EV_RESTORED;
        }
    }
    return RA_EV_OK;
}

ra_state_t ra_state(const ra_t *ra) { return ra ? ra->state : RA_FAULT; }
ra_truth_t ra_truth(const ra_t *ra) { return ra ? ra->truth : RA_TRUTH_UNKNOWN; }
uint32_t ra_shadow(const ra_t *ra) { return ra ? ra->shadow : 0; }
const ra_stats_t *ra_stats(const ra_t *ra) { return ra ? &ra->stats : NULL; }
uint32_t ra_successful_writes(const ra_t *ra) { return ra ? ra->writes : 0; }
const char *ra_state_name(ra_state_t s) {
    switch (s) {
        case RA_BASE: return "BASE";
        case RA_READY: return "READY";
        case RA_RX_LINUX: return "RX_LINUX";
        case RA_DEGRADED_BASE: return "DEGRADED_BASE";
        case RA_FAULT: return "FAULT";
        default: return "?";
    }
}
