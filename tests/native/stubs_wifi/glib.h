/*
 * Minimal GLib subset for the WiFi idle-pump native stage (issue #218, F-06).
 *
 * Declares exactly what src/wifi.cpp uses (no more), with the shapes of the real glib.h:
 * `g_signal_connect` returns 0 with a critical when the instance is NULL, `GError` is a
 * `{domain, code, message}` record, `g_main_context_iteration()` dispatches the pending
 * sources of a context and `g_ptr_array_index` is a macro over GPtrArray's public
 * `pdata`/`len`.
 *
 * The implementations live in tests/native/wifi_idle_stubs.cpp: the main-loop side is a
 * small pump the stage drives with a virtual clock, so the number of
 * `g_main_context_iteration()` calls in an idle window is countable. NetworkManager is
 * the only source that posts work to it (see NetworkManager.h).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int            gboolean;
typedef char           gchar;
typedef int            gint;
typedef long           glong;
typedef unsigned int   guint;
typedef unsigned long  gulong;
typedef unsigned long  gsize;
typedef unsigned long  GType;
typedef void          *gpointer;
typedef const void    *gconstpointer;

/* glib/gtypes.h: los enteros de ancho fijo que libnm usa en sus firmas. */
typedef signed char    gint8;
typedef unsigned char  guint8;
typedef short          gint16;
typedef unsigned short guint16;
typedef int            gint32;
typedef unsigned int   guint32;
typedef long long      gint64;
typedef unsigned long long guint64;

typedef struct _GBytes       GBytes;
typedef struct _GPtrArray    GPtrArray;
typedef struct _GError       GError;
typedef struct _GObject      GObject;
typedef struct _GAsyncResult GAsyncResult;
typedef struct _GCancellable GCancellable;
typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop    GMainLoop;

typedef void (*GCallback)(void);
/* glib: void (*GAsyncReadyCallback)(GObject *source_object, GAsyncResult *res, gpointer user_data) */
typedef void (*GAsyncReadyCallback)(GObject *source_object, GAsyncResult *res, gpointer user_data);

#define TRUE  1
#define FALSE 0

#define G_CALLBACK(f) ((GCallback)(f))
#define G_OBJECT(o)   ((GObject *)(o))

/* glib/garray.h: los dos campos publicos de GPtrArray (el resto es privado). */
struct _GPtrArray {
    gpointer *pdata;
    guint     len;
};
#define g_ptr_array_index(array, index_) ((array)->pdata[index_])

/* glib/gerror.h: los tres campos que wifi.cpp lee (message) o copia. */
struct _GError {
    guint  domain;
    gint   code;
    gchar *message;
};

/* --- bytes / memoria ------------------------------------------------------- */
GBytes       *g_bytes_new(gconstpointer data, gsize size);
gconstpointer g_bytes_get_data(GBytes *bytes, gsize *size);
gsize         g_bytes_get_size(GBytes *bytes);
gint          g_bytes_compare(GBytes *bytes1, GBytes *bytes2);
gchar        *g_strdup(const gchar *str);
void          g_free(gpointer mem);
void          g_error_free(GError *error);

/* --- GObject --------------------------------------------------------------- */
void   g_object_set(gpointer object, const gchar *first_property_name, ...);
void   g_object_unref(gpointer object);
gulong g_signal_connect(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data);

/* --- main loop ------------------------------------------------------------- */
GMainLoop    *g_main_loop_new(GMainContext *context, gboolean is_running);
GMainContext *g_main_loop_get_context(GMainLoop *loop);
void          g_main_loop_quit(GMainLoop *loop);
gboolean      g_main_context_iteration(GMainContext *context, gboolean may_block);

#ifdef __cplusplus
}
#endif
