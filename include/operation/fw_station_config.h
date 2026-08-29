#ifndef OPERATION_FW_STATION_CONFIG_H
#define OPERATION_FW_STATION_CONFIG_H

/* H1M4W4R1
 * Copy secrets.h.example to secrets.h for compile-time configuration. The
 * ignored file is optional because sys_platform also reads /config.json from
 * the board's SD card. */
#if __has_include("secrets.h")
#include "secrets.h"
#endif

/* Compatibility for existing ignored local Wi-Fi files. New installations
 * should use secrets.h or /config.json. */
#if __has_include("operation/fw_station_config_local.h")
#include "operation/fw_station_config_local.h"
#endif

#ifndef SECRETS_WIFI_SSID
#ifdef FW_WIFI_SSID
#define SECRETS_WIFI_SSID FW_WIFI_SSID
#else
#define SECRETS_WIFI_SSID ""
#endif
#endif

#ifndef SECRETS_WIFI_PASSWORD
#ifdef FW_WIFI_PASSWORD
#define SECRETS_WIFI_PASSWORD FW_WIFI_PASSWORD
#else
#define SECRETS_WIFI_PASSWORD ""
#endif
#endif

#ifndef SECRETS_PROVIDER_URL
#define SECRETS_PROVIDER_URL ""
#endif

#ifndef SECRETS_NTP_SERVER
#define SECRETS_NTP_SERVER ""
#endif

#ifndef FW_ENABLE_DEBUG
#define FW_ENABLE_DEBUG 1
#endif

#endif /* OPERATION_FW_STATION_CONFIG_H */
