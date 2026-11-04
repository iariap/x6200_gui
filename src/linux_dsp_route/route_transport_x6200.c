/* Adaptador al transporte EXISTENTE de X6200Control (sólo feature ON).
 * No crea un I2C nuevo ni cambia framing: usa x6200_control_cmd() con el mismo
 * enum/registro; el árbitro calcula el valor completo preservando bits. */
#ifdef X6200_LINUX_DSP_ROUTE
#include "route_common.h"
#include <aether_radio/x6200_control/api.h>
#include <aether_radio/x6200_control/low/control.h>

int route_transport_x6200_write_reg12(void *ctx, uint32_t value) {
    (void)ctx;
    /* Semántica existente: x6200_control_cmd escribe sombra y envía el comando. */
    return x6200_control_cmd(x6200_sple_atue_trx, value) ? 0 : -1;
}
#endif
