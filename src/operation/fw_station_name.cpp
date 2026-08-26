#include "operation/fw_station_name.h"

#include <ctype.h>
#include <string.h>

/* H1M4W4R1 */
void fw_station_name_normalize(
    const char *source_name,
    char *destination_name,
    const size_t destination_name_size)
{
    size_t source_index = 0u;
    size_t destination_index = 0u;

    if ((source_name == nullptr) || (destination_name == nullptr) ||
        (destination_name_size == 0u))
    {
        return;
    }

    while ((source_name[source_index] != '\0') &&
           (source_name[source_index] != '(') &&
           (source_name[source_index] != '[') &&
           (destination_index + 1u < destination_name_size))
    {
        destination_name[destination_index] = source_name[source_index];
        ++source_index;
        ++destination_index;
    }

    while ((destination_index > 0u) &&
           isspace(static_cast<unsigned char>(destination_name[destination_index - 1u])))
    {
        --destination_index;
    }

    destination_name[destination_index] = '\0';
}
