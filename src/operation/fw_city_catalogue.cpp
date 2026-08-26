#include "operation/fw_city_catalogue.h"

/* H1M4W4R1 */
static const fw_city_config_t city_configs[] =
{
    {fw_city_warsaw, "Warszawa", fw_feed_warsaw_api, "https://dane.um.warszawa.pl/pl/catalogue?category=Transport&category_name=Transport", true},
    {fw_city_lodz, "Łódź", fw_feed_gtfs_realtime, "https://otwarte.miasto.lodz.pl/transport_komunikacja/", false},
    {fw_city_gdansk, "Gdańsk", fw_feed_tristar_json, "https://ckan2.multimediagdansk.pl/departures?stopId={stopId}", false},
    {fw_city_wroclaw, "Wrocław", fw_feed_gtfs_static, "https://www.wroclaw.pl/open-data/87b09b32-f076-4475-8ec9-6020ed1f9ac0/OtwartyWroclaw_rozklad_jazdy_GTFS.zip", false},
    {fw_city_poznan, "Poznań", fw_feed_gtfs_static, "https://www.ztm.poznan.pl/pl/dla-deweloperow/getGTFSFile", false}
};

const fw_city_config_t *fw_city_catalogue_get(size_t *const city_count)
{
    if (city_count != nullptr)
    {
        *city_count = sizeof(city_configs) / sizeof(city_configs[0]);
    }

    return city_configs;
}
