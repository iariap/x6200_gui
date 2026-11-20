/*
 * x6200_route_arbiter — proceso arbitro (branch-local, DEFAULT OFF).
 * En produccion real solo se construye con -DX6200_LINUX_DSP_ROUTE=ON y se
 * lanza desde el supervisor; en QA/host usa un bus mock y --self-check.
 * No se instala ni arranca por defecto; no toca I2C sin transporte real.
 */
#include <stdio.h>
#include <string.h>
#include "route_arbiter.h"
#include "route_ipc.h"

static uint32_t mock_writes;
static int mock_write(void *ctx, uint32_t value) { (void)ctx; mock_writes++; (void)value; return 0; }

static int self_check(void) {
    route_bus_t bus = { .write_reg12 = mock_write, .ctx = NULL };
    route_arbiter_t *a = route_arbiter_create(&bus, 0);
    if (a == NULL) return 1;
    int fails = 0;
    if (route_arbiter_boot(a) != ROUTE_ERR_OK || route_arbiter_state(a) != ROUTE_STATE_BASE) fails++;
    if (route_arbiter_set_enabled(a, 10, true) == ROUTE_ERR_OK) fails++; /* no auto-enable */
    if (route_arbiter_client_heartbeat(a, 20, 1, 0, true) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_pcm_submit(a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_set_enabled(a, 30, true) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_state(a) != ROUTE_STATE_RX_LINUX) fails++;
    if (route_arbiter_tx_prepare(a, 40) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_state(a) != ROUTE_STATE_DEGRADED_BASE) fails++;
    route_arbiter_destroy(a);
    printf("route-arbiter self-check: %d fails, writes=%u\n", fails, mock_writes);
    return fails ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-check") == 0) return self_check();
    /* Produccion: el transporte real y el lanzamiento por supervisor quedan
     * fuera de este PR (default OFF). Este binario no abre I2C sin --transport. */
    fprintf(stderr, "x6200_route_arbiter: branch-local, DEFAULT OFF; use --self-check\n");
    return 2;
}
