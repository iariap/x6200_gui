/* Adaptador al transporte EXISTENTE de X6200Control (sólo feature ON).
 * No crea un I2C nuevo ni cambia framing: usa x6200_control_cmd() con el mismo
 * enum/registro; el árbitro calcula el valor completo preservando bits. */
#ifdef X6200_LINUX_DSP_ROUTE
#include "route_common.h"
#include <aether_radio/x6200_control/api.h>
#include <aether_radio/x6200_control/low/control.h>

#ifdef X6200_CTRL_HAVE_BULK_WRITE
/* API condicional del parche X6200Control (218 B: addr BE + 54 u32 LE).
 * Sin el parche instalado, this symbol no existe y refresh queda deshabilitado. */
int route_transport_x6200_write_block(void *ctx, const uint8_t *block, size_t len) {
    (void)ctx;
    if (block == NULL || len != ROUTE_SNAPSHOT_BYTES) return -1;
    return x6200_control_bulk_write(block, len) ? 0 : -1;
}
#endif

int route_transport_x6200_write_reg12(void *ctx, uint32_t value) {
    (void)ctx;
    /* Semántica existente: x6200_control_cmd escribe sombra y envía el comando. */
    return x6200_control_cmd(x6200_sple_atue_trx, value) ? 0 : -1;
}
#endif
