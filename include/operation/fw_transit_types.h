#ifndef OPERATION_FW_TRANSIT_TYPES_H
#define OPERATION_FW_TRANSIT_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* H1M4W4R1 */
static const size_t fw_stop_name_max_length = 56u;
static const size_t fw_stop_id_max_length = 32u;
static const size_t fw_route_name_max_length = 16u;
static const size_t fw_headsign_max_length = 56u;
static const size_t fw_departure_capacity = 12u;
static const size_t fw_stop_capacity = 24u;

typedef struct
{
    char id[fw_stop_id_max_length];
    char name[fw_stop_name_max_length];
} fw_stop_t;

typedef struct
{
    char route_name[fw_route_name_max_length];
    char headsign[fw_headsign_max_length];
    /* Seconds since midnight. The TRISTAR calendar year is not reliable
     * for a live board, so departures are compared by time of day. */
    uint32_t departure_time_s;
    int32_t delay_s;
    bool is_realtime;
} fw_departure_t;

typedef struct
{
    fw_departure_t items[fw_departure_capacity];
    size_t count;
} fw_departure_list_t;

typedef struct
{
    fw_stop_t items[fw_stop_capacity];
    size_t count;
} fw_stop_list_t;

#endif /* OPERATION_FW_TRANSIT_TYPES_H */
