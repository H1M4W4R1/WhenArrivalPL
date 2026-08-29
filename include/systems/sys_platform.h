#ifndef SYSTEMS_SYS_PLATFORM_H
#define SYSTEMS_SYS_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/driver_http_client.h"
#include "ui/ui_display.h"

/* H1M4W4R1
 * This contract isolates board SDKs. A Waveshare implementation provides the
 * same functions in its own source file and never needs an M5 header. */
void sys_platform_initialize(void);
ui_display_t *sys_platform_display(void);
driver_http_client_t *sys_platform_http_client(void);
const char *sys_platform_provider_url(void);
bool sys_platform_has_full_keyboard(void);
bool sys_platform_is_touched(void);
int16_t sys_platform_touch_x(void);
int16_t sys_platform_touch_y(void);
bool sys_platform_network_is_ready(void);
int16_t sys_platform_network_rssi_dbm(void);
void sys_platform_debug_log(const char *message);
uint32_t sys_platform_millis(void);
uint32_t sys_platform_epoch_s(void);
void *sys_platform_allocate_psram(size_t size);

#endif /* SYSTEMS_SYS_PLATFORM_H */
