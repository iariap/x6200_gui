#ifdef X6200_LINUX_DSP_ROUTE
#include "route_hooks.h"
#include "route_wiring.h"
static route_wiring_t *g_wiring;   /* NULL: sin activación QA; hooks no-op */
void x6200_route_audio_submit(const int16_t *samples, size_t count, bool already_processed) {
    if (g_wiring != NULL) route_wiring_submit_pcm(g_wiring, samples, count, already_processed);
}
void x6200_route_audio_generated(bool active) {
    if (g_wiring != NULL) route_wiring_set_generated(g_wiring, active);
}
void x6200_route_radio_tx(bool tx) {
    if (g_wiring != NULL) (void)route_wiring_set_tx(g_wiring, tx);
}
#endif
