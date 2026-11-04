/* QA-LINUX-DSP-IDLE-SNAPSHOT — captura/validación del bloque all_cmd (sin hardware). */
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "route_snapshot.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
static uint32_t regs[ROUTE_SNAPSHOT_REGS];
static uint32_t get(void *ctx, int i) { (void)ctx; return (i >= 0 && i < (int)ROUTE_SNAPSHOT_REGS) ? regs[i] : 0; }

static void test_capture_wire(void) {
    for (int i = 0; i < (int)ROUTE_SNAPSHOT_REGS; i++) regs[i] = 0x1000u + (uint32_t)i;
    regs[12] = 0x100000u;
    route_snapshot_t s;
    CHECK(route_snapshot_capture(&s, get, NULL, 7, 42, 1000, 1000));
    CHECK(s.valid); CHECK(s.version == 7); CHECK(s.seq == 42); CHECK(s.addr == 0);
    CHECK(route_snapshot_reg(&s, 12) == 0x100000u);
    CHECK(route_snapshot_reg(&s, 53) == 0x1000u + 53);
    uint8_t wire[ROUTE_SNAPSHOT_BYTES];
    CHECK(route_snapshot_wire(&s, wire));
    CHECK(ROUTE_SNAPSHOT_BYTES == 218);
    CHECK(wire[0] == 0 && wire[1] == 0);
    CHECK(wire[2] == 0x00 && wire[3] == 0x10 && wire[4] == 0x00 && wire[5] == 0x00);
    /* reg12: LE 0x00100000 => bytes 00 00 10 00 en offset 2+48 */
    CHECK(wire[2+48] == 0x00); CHECK(wire[2+49] == 0x00); CHECK(wire[2+50] == 0x10); CHECK(wire[2+51] == 0x00);
}
static void test_authority_lease(void) {
    for (int i = 0; i < (int)ROUTE_SNAPSHOT_REGS; i++) regs[i] = 0;
    regs[12] = 0x100000u;
    route_snapshot_t s;
    CHECK(route_snapshot_capture(&s, get, NULL, 1, 1, 1000, 1000));
    CHECK(route_snapshot_authoritative(&s, get, NULL, 1100, 1));       /* fresco */
    CHECK(!route_snapshot_authoritative(&s, get, NULL, 2101, 1));      /* lease vencido */
    CHECK(!route_snapshot_authoritative(&s, get, NULL, 1100, 2));      /* versión distinta */
    regs[12] = 0x100008u;                                              /* divergente */
    CHECK(!route_snapshot_authoritative(&s, get, NULL, 1100, 1));
    CHECK(!route_snapshot_authoritative(&s, get, NULL, 1100, 1));      /* sigue no autoritativo */
    route_snapshot_t bad;
    CHECK(!route_snapshot_capture(&bad, NULL, NULL, 1, 1, 0, 0));
    CHECK(!route_snapshot_wire(NULL, NULL));
    /* el snapshot no debe mutar los regs del SDK */
    CHECK(regs[12] == 0x100008u);
}
int main(void) {
    test_capture_wire(); test_authority_lease();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
