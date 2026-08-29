#include "operation/fw_city_catalogue.h"

/* H1M4W4R1 */
static const fw_city_config_t city_configs[] =
{
    {fw_city_warsaw, "Warszawa", "warsaw"},
    {fw_city_lodz, "Łódź", "lodz"},
    {fw_city_gdansk, "Gdańsk", "gdansk"},
    {fw_city_wroclaw, "Wrocław", "wroclaw"},
    {fw_city_poznan, "Poznań", "poznan"}
};

const fw_city_config_t *fw_city_catalogue_get(size_t *const city_count)
{
    if (city_count != nullptr)
    {
        *city_count = sizeof(city_configs) / sizeof(city_configs[0]);
    }

    return city_configs;
}
