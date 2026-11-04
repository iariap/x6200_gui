#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Sink de reproduccion post-SpeexDSP. El worker es el UNICO que llama open/write/close;
 * nunca el callback de captura. Produccion: adaptador PulseAudio; tests: fake. */
typedef struct {
    void *ctx;
    bool (*open)(void *ctx, uint32_t rate);
    int  (*write)(void *ctx, const int16_t *samples, size_t count); /* 0 ok, 1 sin espacio, <0 error */
    void (*close)(void *ctx);
} route_sink_t;
