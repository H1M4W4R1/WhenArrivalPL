#ifndef OPERATION_FW_TRANSIT_SOURCE_H
#define OPERATION_FW_TRANSIT_SOURCE_H

#include "operation/fw_result.h"
#include "operation/fw_transit_types.h"

/* H1M4W4R1 */
class fw_transit_source_t
{
public:
    virtual ~fw_transit_source_t() = default;

    virtual const char *name() const = 0;
    virtual fw_result_t find_stops(const char *query, fw_stop_list_t *stops) = 0;
    virtual fw_result_t get_departures(
        const char *stop_id,
        fw_departure_list_t *departures) = 0;
};

#endif /* OPERATION_FW_TRANSIT_SOURCE_H */
