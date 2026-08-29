#ifndef OPERATION_FW_CITY_CATALOGUE_H
#define OPERATION_FW_CITY_CATALOGUE_H

#include <stddef.h>

#include "drivers/driver_http_client.h"
#include "operation/fw_result.h"

/* H1M4W4R1 */
static const size_t fw_city_capacity = 32u;
static const size_t fw_city_name_max_length = 56u;
static const size_t fw_city_provider_slug_max_length = 32u;

typedef struct
{
    char name[fw_city_name_max_length];
    char provider_slug[fw_city_provider_slug_max_length];
} fw_city_config_t;

typedef struct
{
    fw_city_config_t items[fw_city_capacity];
    size_t count;
} fw_city_list_t;

/* Downloads configured providers from GET /status. The optional city field is
 * used when provided; current servers expose only slug, which is displayed. */
fw_result_t fw_city_catalogue_load(
    driver_http_client_t *http_client,
    const char *provider_url,
    fw_city_list_t *cities);

#endif /* OPERATION_FW_CITY_CATALOGUE_H */
