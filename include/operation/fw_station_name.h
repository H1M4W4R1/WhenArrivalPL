#ifndef OPERATION_FW_STATION_NAME_H
#define OPERATION_FW_STATION_NAME_H

#include <stddef.h>

/* H1M4W4R1 */
void fw_station_name_normalize(
    const char *source_name,
    char *destination_name,
    size_t destination_name_size);

#endif /* OPERATION_FW_STATION_NAME_H */
