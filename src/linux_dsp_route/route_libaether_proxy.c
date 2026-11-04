/* Interposición de libaether (sólo X6200_LINUX_DSP_ROUTE=ON).
 * En producción default OFF este archivo no se compila. */
#ifdef X6200_LINUX_DSP_ROUTE
#define _GNU_SOURCE
#define ROUTE_REFRESH_LEASE_MS 1000u
#include "route_libaether_proxy.h"
#include "route_snapshot.h"
#include "route_ipc.h"
#ifdef X6200_CTRL_HAVE_BULK_WRITE
#include <aether_radio/x6200_control/low/control.h>
#endif
#include <dlfcn.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <aether_radio/x6200_control/api.h>
#include <aether_radio/x6200_control/low/control.h>

static route_proxy_t *g_proxy;
static route_err_t g_last_err = ROUTE_ERR_OK;
static route_lib_proxy_refresh_status_t g_refresh_status;

void route_libaether_proxy_set(route_proxy_t *proxy) { g_proxy = proxy; }
route_proxy_t *route_libaether_proxy_get(void) { return g_proxy; }
route_err_t route_libaether_proxy_last_error(void) { return g_last_err; }
void route_libaether_proxy_refresh_status(route_lib_proxy_refresh_status_t *out) {
    if (out != NULL) *out = g_refresh_status;
}
static bool active(void) { return g_proxy != NULL && route_proxy_is_enabled(g_proxy); }
static void note(route_err_t e) { if (e != ROUTE_ERR_OK) g_last_err = e; }
/* Con el parche X6200Control, mantener la sombra de libaether coherente con las
 * escrituras proxied (sin escritura I2C). Sin parche, no-op (weak). */
extern void x6200_control_shadow_update(uint32_t cmd, uint32_t arg) __attribute__((weak));
static void sync_shadow12(void) {
    if (x6200_control_shadow_update == NULL || g_proxy == NULL) return;
    uint32_t v = 0;
    if (route_proxy_get_reg12(g_proxy, &v) == ROUTE_ERR_OK) x6200_control_shadow_update(12, v);
}

#ifdef ROUTE_LIB_PROXY_TEST
/* En tests, los "originales" son stubs inyectados (sin dlsym). */
extern void route_test_orig_record(bool);
extern void route_test_orig_ptt(bool);
extern void route_test_orig_atu_tune(bool);
extern void route_test_orig_atu_set(bool);
extern void route_test_orig_modem(bool);
extern void route_test_orig_swrscan(bool);
extern void route_test_orig_split(bool);
extern void route_test_orig_poweroff(void);
extern void route_test_orig_calibration(bool);
extern bool route_test_orig_cmd(int, uint32_t);
extern void route_test_orig_idle(void);
extern void route_test_orig_host_cmd(int);
#define ORIG_RECORD()              route_test_orig_record
#define ORIG_PTT()                 route_test_orig_ptt
#define ORIG_ATU_TUNE()            route_test_orig_atu_tune
#define ORIG_ATU_SET()             route_test_orig_atu_set
#define ORIG_MODEM()               route_test_orig_modem
#define ORIG_SWRSCAN()             route_test_orig_swrscan
#define ORIG_SPLIT()               route_test_orig_split
#define ORIG_POWEROFF()            route_test_orig_poweroff()
#define ORIG_CALIBRATION()         route_test_orig_calibration
#define ORIG_CMD()                 route_test_orig_cmd
#define ORIG_IDLE()                route_test_orig_idle
#define ORIG_HOST_CMD()            route_test_orig_host_cmd
#else
typedef void (*fn_void_bool)(bool);
typedef void (*fn_void)(void);
typedef bool (*fn_bool_cmd)(int, uint32_t);
typedef void (*fn_void_int)(int);
static void *next(const char *name) { return dlsym(RTLD_NEXT, name); }
static void call_void_bool(const char *name, bool on) { fn_void_bool f = (fn_void_bool)next(name); if (f) f(on); }
static void call_void(const char *name) { fn_void f = (fn_void)next(name); if (f) f(); }
static bool call_cmd(const char *name, int cmd, uint32_t arg) { fn_bool_cmd f = (fn_bool_cmd)next(name); return f ? f(cmd, arg) : false; }
static void call_void_int(const char *name, int v) { fn_void_int f = (fn_void_int)next(name); if (f) f(v); }
#define ORIG_RECORD()              call_void_bool("x6200_control_record_set", false) /* no usado */
#define ORIG_PTT()                 call_void_bool("x6200_control_ptt_set", false)
#define ORIG_ATU_TUNE()            call_void_bool("x6200_control_atu_tune", false)
#define ORIG_ATU_SET()             call_void_bool("x6200_control_atu_set", false)
#define ORIG_MODEM()               call_void_bool("x6200_control_modem_set", false)
#define ORIG_SWRSCAN()             call_void_bool("x6200_control_swrscan_set", false)
#define ORIG_SPLIT()               call_void_bool("x6200_control_split_set", false)
#define ORIG_POWEROFF()            call_void("x6200_control_poweroff")
#define ORIG_CALIBRATION()         call_void_bool("x6200_control_calibration_set", false)
#define ORIG_CMD()                 call_cmd("x6200_control_cmd", 0, 0)
#define ORIG_IDLE()                call_void("x6200_control_idle")
#define ORIG_HOST_CMD()            call_void_int("x6200_control_host_cmd", 0)
#endif

/* Helpers con el valor original en tests: en TEST llamamos el stub con `on`. */
#ifdef ROUTE_LIB_PROXY_TEST
#define ORIG_RECORD_BOOL(on)       route_test_orig_record(on)
#define ORIG_PTT_BOOL(on)          route_test_orig_ptt(on)
#define ORIG_ATU_TUNE_BOOL(on)     route_test_orig_atu_tune(on)
#define ORIG_ATU_SET_BOOL(on)      route_test_orig_atu_set(on)
#define ORIG_MODEM_BOOL(on)        route_test_orig_modem(on)
#define ORIG_SWRSCAN_BOOL(on)      route_test_orig_swrscan(on)
#define ORIG_SPLIT_BOOL(on)        route_test_orig_split(on)
#define ORIG_CALIB_BOOL(on)        route_test_orig_calibration(on)
#define ORIG_CMD_BOOL(c,a)         route_test_orig_cmd(c,a)
#define ORIG_IDLE_VOID()           route_test_orig_idle()
#define ORIG_HOST_CMD_VOID(c)      route_test_orig_host_cmd(c)
#else
#define ORIG_RECORD_BOOL(on)       call_void_bool("x6200_control_record_set", on)
#define ORIG_PTT_BOOL(on)          call_void_bool("x6200_control_ptt_set", on)
#define ORIG_ATU_TUNE_BOOL(on)     call_void_bool("x6200_control_atu_tune", on)
#define ORIG_ATU_SET_BOOL(on)      call_void_bool("x6200_control_atu_set", on)
#define ORIG_MODEM_BOOL(on)        call_void_bool("x6200_control_modem_set", on)
#define ORIG_SWRSCAN_BOOL(on)      call_void_bool("x6200_control_swrscan_set", on)
#define ORIG_SPLIT_BOOL(on)        call_void_bool("x6200_control_split_set", on)
#define ORIG_CALIB_BOOL(on)        call_void_bool("x6200_control_calibration_set", on)
#define ORIG_CMD_BOOL(c,a)         call_cmd("x6200_control_cmd", c, a)
#define ORIG_IDLE_VOID()           call_void("x6200_control_idle")
#define ORIG_HOST_CMD_VOID(c)      call_void_int("x6200_control_host_cmd", c)
#endif

void x6200_control_record_set(bool on) {
    if (!active()) { ORIG_RECORD_BOOL(on); return; }
    note(route_proxy_record_set(g_proxy, on));  /* OWNERSHIP: nunca toca 0x8 */
}
void x6200_control_ptt_set(bool on) { if (!active()) { ORIG_PTT_BOOL(on); return; } route_err_t e = route_proxy_ptt_set(g_proxy, on); note(e); if (e == ROUTE_ERR_OK) sync_shadow12(); }
void x6200_control_atu_tune(bool on) {
    if (!active()) { ORIG_ATU_TUNE_BOOL(on); return; }
    note(route_proxy_atu_tune(g_proxy, on));
    ORIG_HOST_CMD_VOID(on ? 0x8004 : 0x8003);   /* host cmd no toca reg12 */
}
void x6200_control_atu_set(bool on) { if (!active()) { ORIG_ATU_SET_BOOL(on); return; } note(route_proxy_atu_set(g_proxy, on)); }
void x6200_control_modem_set(bool on) { if (!active()) { ORIG_MODEM_BOOL(on); return; } note(route_proxy_modem_set(g_proxy, on)); }
void x6200_control_swrscan_set(bool on) { if (!active()) { ORIG_SWRSCAN_BOOL(on); return; } note(route_proxy_swrscan_set(g_proxy, on)); }
void x6200_control_split_set(bool on) { if (!active()) { ORIG_SPLIT_BOOL(on); return; } note(route_proxy_split_set(g_proxy, on)); }
void x6200_control_poweroff(void) {
    if (!active()) { ORIG_POWEROFF(); return; }
    note(route_proxy_poweroff(g_proxy));
    /* El kill(1,SIGUSR2) original queda suprimido en QA: no apagar la radio desde el agente. */
}
void x6200_control_calibration_set(bool on) {
    if (!active()) { ORIG_CALIB_BOOL(on); return; }
    note(route_proxy_calibration_set(g_proxy, on));
}
bool x6200_control_cmd(x6200_cmd_enum_t cmd, uint32_t arg) {
    if (!active()) return ORIG_CMD_BOOL((int)cmd, arg);
    if (cmd == x6200_sple_atue_trx) {
        if ((arg & ROUTE_REG_VOICE_REC) != 0) { g_last_err = ROUTE_ERR_OWNERSHIP; return false; }
        route_err_t e = route_proxy_write_reg12(g_proxy, arg);
        note(e);
        if (e == ROUTE_ERR_OK) sync_shadow12();
        return e == ROUTE_ERR_OK;
    }
    return ORIG_CMD_BOOL((int)cmd, arg);
}
void x6200_control_idle(void) {
    if (!active()) { ORIG_IDLE_VOID(); return; }
#ifdef X6200_CTRL_HAVE_BULK_WRITE
    /* Sombra autoritativa: libaether recibe sync_shadow12() tras cada escritura
     * proxied; el bloque se envía por el árbitro (bulk 218 B). */
    if (g_refresh_status.pending) g_refresh_status.coalesced++;
    g_refresh_status.pending = true;
    g_refresh_status.attempts++;
    typedef uint32_t (*real_get_t)(int cmd);
    real_get_t get = (real_get_t)dlsym(RTLD_DEFAULT, "x6200_control_get");
    if (get != NULL) {
        route_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        for (unsigned i = 0; i < ROUTE_SNAPSHOT_REGS; i++) snap.regs[i] = get((int)i);
        snap.valid = true;
        route_err_t e = route_proxy_refresh_block(g_proxy, snap.regs);
        note(e);
        if (e == ROUTE_ERR_OK) { g_refresh_status.blocked = 0; g_refresh_status.pending = false; }
        else { g_refresh_status.failures++; g_refresh_status.blocked++; g_refresh_status.last_error = e; g_refresh_status.pending = true; }
        return;
    }
    g_last_err = ROUTE_ERR_UNSUPPORTED;
    g_refresh_status.attempts++; g_refresh_status.failures++; g_refresh_status.blocked++;
    g_refresh_status.last_error = ROUTE_ERR_UNSUPPORTED; g_refresh_status.pending = true;
#else
    /* Sin API bulk parcheada: no se reescribe directo ni se hacen 54 writes. */
    g_last_err = ROUTE_ERR_UNSUPPORTED;
    g_refresh_status.attempts++; g_refresh_status.failures++; g_refresh_status.blocked++;
    g_refresh_status.last_error = ROUTE_ERR_UNSUPPORTED; g_refresh_status.pending = true;
#endif
}
#endif
