#ifndef OPERATION_CITIES_FW_GDANSK_SOURCE_H
#define OPERATION_CITIES_FW_GDANSK_SOURCE_H

#include "drivers/driver_http_client.h"
#include "operation/fw_transit_source.h"

/* H1M4W4R1 */
class fw_gdansk_source_t final : public fw_transit_source_t
{
public:
    explicit fw_gdansk_source_t(driver_http_client_t *http_client);

    const char *name() const override;
    fw_result_t find_stops(const char *query, fw_stop_list_t *stops) override;
    fw_result_t get_departures(
        const char *stop_id,
        fw_departure_list_t *departures) override;

private:
    driver_http_client_t *_http_client;
};

#endif /* OPERATION_CITIES_FW_GDANSK_SOURCE_H */
