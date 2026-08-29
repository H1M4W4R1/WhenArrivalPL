#ifndef OPERATION_FW_LOCAL_API_SOURCE_H
#define OPERATION_FW_LOCAL_API_SOURCE_H

#include "drivers/driver_http_client.h"
#include "operation/fw_transit_source.h"

/* H1M4W4R1
 * Adapter for the local IOT Open API. The API normalizes all city feeds, so
 * this source never contacts external transit services directly. */
class fw_local_api_source_t final : public fw_transit_source_t
{
public:
    fw_local_api_source_t(driver_http_client_t *http_client, const char *provider_url);

    const char *name() const override;
    void set_provider(const char *provider_slug, const char *provider_name);
    fw_result_t find_stops(const char *query, fw_stop_list_t *stops) override;
    fw_result_t get_departures(
        const char *stop_name,
        size_t requested_count,
        fw_departure_list_t *departures) override;

private:
    driver_http_client_t *_http_client;
    const char *_provider_url;
    const char *_provider_slug;
    const char *_provider_name;
};

#endif /* OPERATION_FW_LOCAL_API_SOURCE_H */
