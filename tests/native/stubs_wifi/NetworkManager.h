/*
 * Minimal NetworkManager subset for the WiFi idle-pump native stage (issue #218, F-06).
 *
 * Declares exactly what src/wifi.cpp references (no more): the types, the enums it
 * compares or switches on, the setting key strings and the functions. The enum values and
 * the key strings are copied from the host's libnm (/usr/include/libnm/nm-dbus-interface.h
 * and nm-setting-*.h), so the fixture cannot drift into "always equal" behaviour: the
 * `switch (new_state)` of the module walks the same numbers as on the radio.
 *
 * The implementations (tests/native/wifi_idle_stubs.cpp) are a two-scenario double: a
 * client with no wlan0 device (the "sin adaptador NM" case the contract asks for) and a
 * client with one, which is enough to drive the real scan/disconnect/state paths without
 * a daemon. The device object is opaque to the module: wifi.cpp only ever passes it back
 * to libnm, so the fixture hands it a pointer of its own.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "glib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- tipos (nm-core-types.h / nm-*.h) -------------------------------------- */
typedef struct _NMClient             NMClient;
typedef struct _NMDevice             NMDevice;
typedef struct _NMDeviceWifi         NMDeviceWifi;
typedef struct _NMAccessPoint        NMAccessPoint;
typedef struct _NMActiveConnection   NMActiveConnection;
typedef struct _NMConnection         NMConnection;
typedef struct _NMRemoteConnection   NMRemoteConnection;
typedef struct _NMIPConfig           NMIPConfig;
typedef struct _NMIPAddress          NMIPAddress;
typedef struct _NMSetting            NMSetting;
typedef struct _NMSettingConnection  NMSettingConnection;
typedef struct _NMSettingWireless    NMSettingWireless;
typedef struct _NMSettingWirelessSecurity NMSettingWirelessSecurity;
typedef struct _NMSettingIP4Config   NMSettingIP4Config;
typedef struct _NMSettingIP6Config   NMSettingIP6Config;

#define NM_CLIENT(c)            ((NMClient *)(c))
#define NM_DEVICE(d)            ((NMDevice *)(d))
#define NM_DEVICE_WIFI(d)       ((NMDeviceWifi *)(d))
#define NM_ACCESS_POINT(a)      ((NMAccessPoint *)(a))
#define NM_CONNECTION(c)        ((NMConnection *)(c))
#define NM_REMOTE_CONNECTION(c) ((NMRemoteConnection *)(c))
#define NM_SETTING(s)           ((NMSetting *)(s))
#define NM_TYPE_SETTING_WIRELESS_SECURITY (nm_setting_wireless_security_get_type())

/* --- enums (valores de libnm/nm-dbus-interface.h) -------------------------- */
typedef enum {
    NM_DEVICE_STATE_UNKNOWN      = 0,
    NM_DEVICE_STATE_UNMANAGED    = 10,
    NM_DEVICE_STATE_UNAVAILABLE  = 20,
    NM_DEVICE_STATE_DISCONNECTED = 30,
    NM_DEVICE_STATE_PREPARE      = 40,
    NM_DEVICE_STATE_CONFIG       = 50,
    NM_DEVICE_STATE_NEED_AUTH    = 60,
    NM_DEVICE_STATE_IP_CONFIG    = 70,
    NM_DEVICE_STATE_IP_CHECK     = 80,
    NM_DEVICE_STATE_SECONDARIES  = 90,
    NM_DEVICE_STATE_ACTIVATED    = 100,
    NM_DEVICE_STATE_DEACTIVATING = 110,
    NM_DEVICE_STATE_FAILED       = 120,
} NMDeviceState;

typedef enum {
    NM_DEVICE_STATE_REASON_NONE      = 0,
    NM_DEVICE_STATE_REASON_NO_SECRETS = 7,
} NMDeviceStateReason;

typedef enum {
    NM_802_11_MODE_UNKNOWN = 0,
    NM_802_11_MODE_ADHOC   = 1,
    NM_802_11_MODE_INFRA   = 2,
    NM_802_11_MODE_AP      = 3,
    NM_802_11_MODE_MESH    = 4,
} NM80211Mode;

typedef enum {
    NM_802_11_AP_FLAGS_NONE = 0x00000000,
} NM80211ApFlags;

typedef enum {
    NM_802_11_AP_SEC_KEY_MGMT_PSK = 0x00000100,
} NM80211ApSecurityFlags;

/* --- claves de settings (nm-setting-*.h) ----------------------------------- */
#define NM_SETTING_CONNECTION_UUID                  "uuid"
#define NM_SETTING_CONNECTION_ID                    "id"
#define NM_SETTING_CONNECTION_TYPE                  "type"
#define NM_SETTING_CONNECTION_AUTOCONNECT           "autoconnect"
#define NM_SETTING_CONNECTION_AUTOCONNECT_RETRIES   "autoconnect-retries"
#define NM_SETTING_CONNECTION_INTERFACE_NAME        "interface-name"
#define NM_SETTING_WIRELESS_SETTING_NAME            "802-11-wireless"
#define NM_SETTING_WIRELESS_SSID                    "ssid"
#define NM_SETTING_WIRELESS_MODE                    "mode"
#define NM_SETTING_WIRELESS_MODE_INFRA              "infrastructure"
#define NM_SETTING_WIRELESS_SECURITY_AUTH_ALG       "auth-alg"
#define NM_SETTING_WIRELESS_SECURITY_KEY_MGMT       "key-mgmt"
#define NM_SETTING_WIRELESS_SECURITY_PSK            "psk"
#define NM_SETTING_IP_CONFIG_METHOD                 "method"
#define NM_SETTING_IP4_CONFIG_METHOD_AUTO           "auto"

/* --- NMClient -------------------------------------------------------------- */
NMClient *nm_client_new(GCancellable *cancellable, GError **error);
NMDevice *nm_client_get_device_by_iface(NMClient *client, const char *iface);
NMRemoteConnection *nm_client_get_connection_by_id(NMClient *client, const char *id);

/* --- NMDevice -------------------------------------------------------------- */
const char         *nm_device_get_iface(NMDevice *device);
guint               nm_device_get_state(NMDevice *device);
NMActiveConnection *nm_device_get_active_connection(NMDevice *device);
NMIPConfig         *nm_device_get_ip4_config(NMDevice *device);
void                nm_device_disconnect_async(NMDevice *device, GCancellable *cancellable,
                                               GAsyncReadyCallback callback, gpointer user_data);
gboolean            nm_device_disconnect_finish(NMDevice *device, GAsyncResult *result, GError **error);

/* --- NMDeviceWifi ---------------------------------------------------------- */
gint64  nm_device_wifi_get_last_scan(NMDeviceWifi *device);
void    nm_device_wifi_request_scan_async(NMDeviceWifi *device, GCancellable *cancellable,
                                          GAsyncReadyCallback callback, gpointer user_data);
gboolean nm_device_wifi_request_scan_finish(NMDeviceWifi *device, GAsyncResult *result, GError **error);
const GPtrArray *nm_device_wifi_get_access_points(NMDeviceWifi *device);
NMAccessPoint   *nm_device_wifi_get_active_access_point(NMDeviceWifi *device);

/* --- NMAccessPoint --------------------------------------------------------- */
GBytes       *nm_access_point_get_ssid(NMAccessPoint *ap);
NM80211ApFlags nm_access_point_get_flags(NMAccessPoint *ap);
guint32       nm_access_point_get_wpa_flags(NMAccessPoint *ap);
guint32       nm_access_point_get_rsn_flags(NMAccessPoint *ap);
const char   *nm_access_point_get_bssid(NMAccessPoint *ap);
NM80211Mode   nm_access_point_get_mode(NMAccessPoint *ap);
guint8        nm_access_point_get_strength(NMAccessPoint *ap);

/* --- connections / settings ------------------------------------------------ */
NMConnection       *nm_simple_connection_new(void);
NMConnection       *nm_simple_connection_new_clone(NMConnection *connection);
NMSetting          *nm_setting_connection_new(void);
NMSetting          *nm_setting_wireless_new(void);
NMSetting          *nm_setting_wireless_security_new(void);
NMSetting          *nm_setting_ip4_config_new(void);
NMSetting          *nm_setting_ip6_config_new(void);
GType               nm_setting_wireless_security_get_type(void);
void                nm_connection_add_setting(NMConnection *connection, NMSetting *setting);
void                nm_connection_remove_setting(NMConnection *connection, GType setting_type);
void                nm_connection_replace_settings_from_connection(NMConnection *connection,
                                                                  NMConnection *new_connection);
const char         *nm_connection_get_path(NMConnection *connection);
const char         *nm_connection_get_id(NMConnection *connection);
const char         *nm_connection_get_uuid(NMConnection *connection);
const char         *nm_active_connection_get_id(NMActiveConnection *connection);

void     nm_client_add_connection_async(NMClient *client, NMConnection *connection, gboolean save_to_disk,
                                        GCancellable *cancellable, GAsyncReadyCallback callback,
                                        gpointer user_data);
NMRemoteConnection *nm_client_add_connection_finish(NMClient *client, GAsyncResult *result, GError **error);
void     nm_client_add_and_activate_connection_async(NMClient *client, NMConnection *partial,
                                                     NMDevice *device, const char *specific_object,
                                                     GCancellable *cancellable, GAsyncReadyCallback callback,
                                                     gpointer user_data);
NMActiveConnection *nm_client_add_and_activate_connection_finish(NMClient *client, GAsyncResult *result,
                                                                GError **error);
void     nm_client_activate_connection_async(NMClient *client, NMConnection *connection, NMDevice *device,
                                             const char *specific_object, GCancellable *cancellable,
                                             GAsyncReadyCallback callback, gpointer user_data);
NMActiveConnection *nm_client_activate_connection_finish(NMClient *client, GAsyncResult *result, GError **error);

void     nm_remote_connection_commit_changes_async(NMRemoteConnection *connection, gboolean save_to_disk,
                                                   GCancellable *cancellable, GAsyncReadyCallback callback,
                                                   gpointer user_data);
gboolean nm_remote_connection_commit_changes_finish(NMRemoteConnection *connection, GAsyncResult *result,
                                                    GError **error);
void     nm_remote_connection_delete_async(NMRemoteConnection *connection, GCancellable *cancellable,
                                           GAsyncReadyCallback callback, gpointer user_data);
gboolean nm_remote_connection_delete_finish(NMRemoteConnection *connection, GAsyncResult *result, GError **error);

/* --- IP config ------------------------------------------------------------- */
const char *nm_ip_config_get_gateway(NMIPConfig *config);
GPtrArray  *nm_ip_config_get_addresses(NMIPConfig *config);
const char *nm_ip_address_get_address(NMIPAddress *address);

/* --- utils ----------------------------------------------------------------- */
char    *nm_utils_uuid_generate(void);
char    *nm_utils_ssid_to_utf8(const guint8 *ssid, gsize len);
gboolean nm_utils_wpa_psk_valid(const char *psk);

#ifdef __cplusplus
}
#endif
