#ifndef OPERATION_FW_CITY_CATALOGUE_H
#define OPERATION_FW_CITY_CATALOGUE_H

#include <stddef.h>

/* H1M4W4R1 */
typedef enum
{
    fw_city_warsaw = 0,
    fw_city_lodz,
    fw_city_gdansk,
    fw_city_wroclaw,
    fw_city_poznan
} fw_city_t;

typedef enum
{
    fw_feed_warsaw_api = 0,
    fw_feed_gtfs_realtime,
    fw_feed_tristar_json,
    fw_feed_gtfs_static
} fw_feed_kind_t;

typedef struct
{
    fw_city_t city;
    const char *name;
    fw_feed_kind_t feed_kind;
    const char *official_url;
    bool requires_api_key;
} fw_city_config_t;

const fw_city_config_t *fw_city_catalogue_get(size_t *city_count);

#endif /* OPERATION_FW_CITY_CATALOGUE_H */
