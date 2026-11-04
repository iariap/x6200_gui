#ifdef X6200_LINUX_DSP_ROUTE
#include "route_hooks.h"
#include "route_qa_client.h"
void x6200_route_audio_init(void) { (void)x6200_route_qa_start_default(); }
void x6200_route_audio_submit(const int16_t *samples, size_t count, bool already_processed) { x6200_route_qa_submit_pcm(samples, count, already_processed); }
void x6200_route_audio_generated(bool active) { x6200_route_qa_generated(active); }
void x6200_route_radio_tx(bool tx) { x6200_route_qa_tx(tx); }
#endif
