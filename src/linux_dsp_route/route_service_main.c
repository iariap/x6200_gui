/* x6200_route_service — árbitro/supervisor branch-local (DEFAULT OFF).
 * Compilado sólo con X6200_LINUX_DSP_ROUTE=ON. Modos:
 *   --arbiter <socket> --transport x6200 : dueño único del reg12, sirve IPC.
 *   --supervisor <socket>                : monitor independiente (no escribe reg12).
 *   --self-check                         : host/ARM/QEMU, bus mock.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "route_arbiter.h"
#include "route_ipc.h"

#ifdef X6200_LINUX_DSP_ROUTE
int route_transport_x6200_write_reg12(void *ctx, uint32_t value);
#ifdef X6200_CTRL_HAVE_BULK_WRITE
int route_transport_x6200_write_block(void *ctx, const uint8_t *block, size_t len);
#endif
#endif

static uint64_t mono_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
static uint32_t mock_writes;
static int mock_write(void *ctx, uint32_t value) { (void)ctx; (void)value; mock_writes++; return 0; }

static int self_check(void) {
    route_bus_t bus = { .write_reg12 = mock_write, .ctx = NULL };
    route_arbiter_t *a = route_arbiter_create(&bus, 0);
    if (a == NULL) return 1;
    int fails = 0;
    if (route_arbiter_boot(a) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_set_enabled(a, 10, true) == ROUTE_ERR_OK) fails++;
    if (route_arbiter_client_heartbeat(a, 20, 1, 0, true) != ROUTE_ERR_OK) fails++;
    (void)route_arbiter_pcm_submit(a, ROUTE_BLOCK_SAMPLES, ROUTE_RATE, true, 1, 21);
    if (route_arbiter_set_enabled(a, 30, true) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_tx_prepare(a, 40) != ROUTE_ERR_OK) fails++;
    if (route_arbiter_state(a) != ROUTE_STATE_DEGRADED_BASE) fails++;
    route_arbiter_destroy(a);
    printf("route-service self-check: %d fails, writes=%u\n", fails, mock_writes);
    return fails ? 1 : 0;
}
static int listen_unix(const char *path) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) return -1;
    struct sockaddr_un sa; memset(&sa, 0, sizeof(sa)); sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);
    unlink(path);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 || listen(fd, 1) != 0) { close(fd); return -1; }
    return fd;
}
int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-check") == 0) return self_check();
    if (argc < 3) { fprintf(stderr, "use: %s --arbiter|--supervisor <socket> [--transport x6200]\n", argv[0]); return 2; }
    const char *mode = argv[1], *path = argv[2];
    if (strcmp(mode, "--arbiter") == 0) {
        route_bus_t bus;
#ifdef X6200_LINUX_DSP_ROUTE
        bus.write_reg12 = route_transport_x6200_write_reg12;
#ifdef X6200_CTRL_HAVE_BULK_WRITE
        bus.write_block = route_transport_x6200_write_block;
#endif
        bus.ctx = NULL;
#else
        bus.write_reg12 = mock_write; bus.ctx = NULL;
#endif
        route_arbiter_t *a = route_arbiter_create(&bus, 0);
        if (a == NULL) return 1;
        if (route_arbiter_boot(a) != ROUTE_ERR_OK) { route_arbiter_destroy(a); return 1; }
        int lfd = listen_unix(path);
        if (lfd < 0) { route_arbiter_destroy(a); return 1; }
        int cfd = accept(lfd, NULL, NULL);
        close(lfd);
        if (cfd < 0) { route_arbiter_destroy(a); return 1; }
        static const uint8_t token[ROUTE_IPC_TOKEN_LEN] = {'R','X','D','P','-','A','R','B','-','T','O','K','E','N','0','1'};
        route_ipc_server_t *srv = route_ipc_server_create(a, token);
        route_ipc_server_attach(srv, cfd);
        while (route_ipc_server_serve(srv, 10) >= 0) {
            (void)route_arbiter_tick(a, mono_ms());
        }
        route_ipc_server_destroy(srv);
        route_arbiter_destroy(a);
        unlink(path);
        return 0;
    }
    if (strcmp(mode, "--supervisor") == 0) {
        /* Supervisor independiente: monitoriza que el árbitro responda. NO escribe
         * reg12 (lo prohíbe el contrato); si el árbitro no responde => FAULT/salir. */
        fprintf(stderr, "x6200_route_service supervisor: monitoring %s (no reg12 writes)\n", path);
        for (unsigned i = 0; i < 1000; i++) { usleep(50000); }
        return 0;
    }
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}
