#pragma once
#include "route_common.h"
#include "route_arbiter.h"

#define ROUTE_IPC_MAGIC   0x52584450u  /* 'RXDP' */
#define ROUTE_IPC_VERSION 1u
#define ROUTE_IPC_TOKEN_LEN 16u

typedef enum {
    ROUTE_IPC_REQ_APPLY_SHARED = 1,
    ROUTE_IPC_REQ_HEARTBEAT,
    ROUTE_IPC_REQ_ENABLE,
    ROUTE_IPC_REQ_DISABLE,
    ROUTE_IPC_REQ_TX_PREPARE,
    ROUTE_IPC_REQ_ATU_PREPARE,
    ROUTE_IPC_REQ_STATUS,
    ROUTE_IPC_REQ_REFRESH,
    ROUTE_IPC_REQ_PCM_READY,   /* QA: PCM post-Speex vivo (value=block_id) */
    ROUTE_IPC_REQ_SHUTDOWN,
} route_ipc_req_t;

typedef struct __attribute__((__packed__)) {
    uint32_t magic, version, type;
    uint8_t  token[ROUTE_IPC_TOKEN_LEN];
    uint32_t pid, starttime;
    uint64_t mono_ms;
    uint32_t seq;
    uint32_t value;
    uint32_t lease_ms;
    int32_t  err;
    uint32_t state;
    uint32_t stats[6];  /* writes,restores,bus_errors,faults,overflows,denied */
    uint32_t block[ROUTE_SNAPSHOT_REGS];  /* sólo REFRESH: regs 0..53 del snapshot */
    uint8_t  pad[8];
} route_ipc_msg_t;

size_t route_ipc_encode(const route_ipc_msg_t *m, void *buf, size_t cap);
int route_ipc_decode(const void *buf, size_t len, route_ipc_msg_t *m);

typedef struct route_ipc_server route_ipc_server_t;
route_ipc_server_t *route_ipc_server_create(route_arbiter_t *arb, const uint8_t token[ROUTE_IPC_TOKEN_LEN]);
void route_ipc_server_destroy(route_ipc_server_t *s);
/* Socketpair/attached fd para tests y para el proceso daemon. */
int route_ipc_server_attach(route_ipc_server_t *s, int fd);
/* Procesa como máximo un mensaje (poll timeout). 1 = procesado, 0 = timeout, <0 error. */
int route_ipc_server_serve(route_ipc_server_t *s, int timeout_ms);
void route_ipc_server_stop(route_ipc_server_t *s);
bool route_ipc_server_has_client(const route_ipc_server_t *s);
uint32_t route_ipc_server_last_seq(const route_ipc_server_t *s);

typedef struct {
    int fd;
    uint8_t token[ROUTE_IPC_TOKEN_LEN];
    uint32_t pid, starttime;
    uint32_t seq;
} route_ipc_client_t;

int route_ipc_client_attach(route_ipc_client_t *c, int fd, const uint8_t token[ROUTE_IPC_TOKEN_LEN]);
/* Envía request y espera respuesta con timeout (nunca bloqueo indefinido). */
int route_ipc_client_request(route_ipc_client_t *c, uint32_t type, uint32_t value, uint32_t lease_ms,
                             uint64_t now_ms, route_ipc_msg_t *resp, int timeout_ms);
/* Envía REFRESH con el bloque de 54 regs (218 B) y espera respuesta con timeout. */
int route_ipc_client_refresh(route_ipc_client_t *c, const uint32_t regs[ROUTE_SNAPSHOT_REGS],
                             uint64_t now_ms, route_ipc_msg_t *resp, int timeout_ms);
