#ifndef OPERATION_FW_STATION_CONFIG_H
#define OPERATION_FW_STATION_CONFIG_H

/* H1M4W4R1
 * Copy the example to fw_station_config_local.h and define its values there.
 * The local file is ignored by Git, so Wi-Fi credentials are never committed. */
#if __has_include("operation/fw_station_config_local.h")
#include "operation/fw_station_config_local.h"
#endif

#ifndef FW_WIFI_SSID
#define FW_WIFI_SSID ""
#endif

#ifndef FW_WIFI_PASSWORD
#define FW_WIFI_PASSWORD ""
#endif

/* Optional text fragment used to reduce the downloaded list, e.g. "Dworzec". */
#ifndef FW_STOP_QUERY
#define FW_STOP_QUERY ""
#endif

#ifndef FW_ENABLE_DEBUG
#define FW_ENABLE_DEBUG 1
#endif

#endif /* OPERATION_FW_STATION_CONFIG_H */
