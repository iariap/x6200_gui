#ifdef X6200_LINUX_DSP_ROUTE
#include "route_pulse_sink.h"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { pa_simple *pa; } pulse_ctx_t;
static bool p_open(void *ctx, uint32_t rate) {
    pulse_ctx_t *p = ctx;
    pa_sample_spec ss = { .format = PA_SAMPLE_S16NE, .rate = rate, .channels = 1 };
    int err = 0;
    p->pa = pa_simple_new(NULL, "x6200-linux-dsp-qa", PA_STREAM_PLAYBACK, NULL,
                          "rx-linux-dsp", &ss, NULL, NULL, &err);
    if (p->pa == NULL) { fprintf(stderr, "x6200 QA sink: pa_simple_new fallo: %s\n", pa_strerror(err)); return false; }
    return true;
}
static int p_write(void *ctx, const int16_t *samples, size_t count) {
    pulse_ctx_t *p = ctx; int err = 0;
    if (p->pa == NULL) return -1;
    if (pa_simple_write(p->pa, samples, count * sizeof(int16_t), &err) < 0) {
        fprintf(stderr, "x6200 QA sink: pa_simple_write fallo: %s\n", pa_strerror(err));
        return -1;
    }
    return 0;
}
static void p_close(void *ctx) {
    pulse_ctx_t *p = ctx;
    if (p->pa != NULL) { pa_simple_free(p->pa); p->pa = NULL; }
}
route_sink_t route_pulse_sink_create(void) {
    route_sink_t s = {0};
    pulse_ctx_t *p = calloc(1, sizeof(*p));
    if (p == NULL) return s;
    s.ctx = p; s.open = p_open; s.write = p_write; s.close = p_close;
    return s;
}
void route_pulse_sink_destroy(route_sink_t *sink) {
    if (sink == NULL) return;
    if (sink->ctx != NULL) { p_close(sink->ctx); free(sink->ctx); sink->ctx = NULL; }
}
#endif
