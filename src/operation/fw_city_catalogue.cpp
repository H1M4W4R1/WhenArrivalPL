#include "operation/fw_city_catalogue.h"

#include <stdio.h>
#include <string.h>

namespace
{
static const size_t status_response_max_length = 4096u;
static char status_response[status_response_max_length];
/* The background network worker is intentionally single-threaded. Keeping its
 * working list static prevents a 2.8 KiB temporary from exhausting its task
 * stack while it parses GET /status. */
static fw_city_list_t parsed_cities;

bool copy_text(char *const destination, const size_t destination_size, const char *const source)
{
    if ((destination == nullptr) || (destination_size == 0u) || (source == nullptr) ||
        (source[0u] == '\0'))
    {
        return false;
    }

    size_t length = 0u;
    while (source[length] != '\0')
    {
        if ((length + 1u) >= destination_size)
        {
            return false;
        }
        destination[length] = source[length];
        ++length;
    }
    destination[length] = '\0';
    return true;
}

bool make_status_url(
    char *const destination,
    const size_t destination_size,
    const char *const provider_url)
{
    if ((destination == nullptr) || (destination_size == 0u) || (provider_url == nullptr) ||
        (provider_url[0u] == '\0'))
    {
        return false;
    }

    const size_t url_length = strlen(provider_url);
    const bool has_trailing_slash = url_length > 0u && provider_url[url_length - 1u] == '/';
    const int written = snprintf(
        destination, destination_size, has_trailing_slash ? "%sstatus" : "%s/status", provider_url);
    return (written >= 0) && (static_cast<size_t>(written) < destination_size);
}

const char *skip_whitespace(const char *value, const char *const limit)
{
    while ((value < limit) && ((*value == ' ') || (*value == '\n') || (*value == '\r') || (*value == '\t')))
    {
        ++value;
    }
    return value;
}

bool read_json_string(
    const char *value,
    const char *const limit,
    char *const destination,
    const size_t destination_size)
{
    if ((value == nullptr) || (destination == nullptr) || (destination_size == 0u) || (value >= limit))
    {
        return false;
    }

    value = skip_whitespace(value, limit);
    if ((value >= limit) || (*value != '"'))
    {
        return false;
    }

    ++value;
    size_t length = 0u;
    while ((value < limit) && (*value != '"'))
    {
        if ((length + 1u) >= destination_size)
        {
            return false;
        }
        if (*value == '\\')
        {
            ++value;
            if (value >= limit)
            {
                return false;
            }
        }
        destination[length++] = *value++;
    }
    if (value >= limit)
    {
        return false;
    }

    destination[length] = '\0';
    return length > 0u;
}

bool read_object_string(
    const char *const object,
    const char *const object_end,
    const char *const key,
    char *const destination,
    const size_t destination_size)
{
    if ((object == nullptr) || (object_end == nullptr) || (key == nullptr) || (object >= object_end))
    {
        return false;
    }

    const char *const key_position = strstr(object, key);
    if ((key_position == nullptr) || (key_position >= object_end))
    {
        return false;
    }

    const char *const value_separator = strchr(key_position + strlen(key), ':');
    if ((value_separator == nullptr) || (value_separator >= object_end))
    {
        return false;
    }

    return read_json_string(value_separator + 1, object_end, destination, destination_size);
}
}

fw_result_t fw_city_catalogue_load(
    driver_http_client_t *const http_client,
    const char *const provider_url,
    fw_city_list_t *const cities)
{
    if ((http_client == nullptr) || (cities == nullptr))
    {
        return fw_result_invalid_argument;
    }

    char status_url[160u];
    if (!make_status_url(status_url, sizeof(status_url), provider_url))
    {
        return fw_result_invalid_argument;
    }

    size_t response_length = 0u;
    const fw_result_t result = http_client->get(
        status_url, status_response, sizeof(status_response), &response_length);
    if (result != fw_result_ok)
    {
        return result;
    }

    parsed_cities = {};
    const char *const response_end = status_response + response_length;
    const char *cursor = skip_whitespace(status_response, response_end);
    if ((cursor >= response_end) || (*cursor != '['))
    {
        return fw_result_parse_error;
    }
    while (cursor < response_end)
    {
        const char *const object = strchr(cursor, '{');
        if ((object == nullptr) || (object >= response_end))
        {
            break;
        }
        const char *const object_end = strchr(object, '}');
        if ((object_end == nullptr) || (object_end >= response_end))
        {
            return fw_result_parse_error;
        }
        if (parsed_cities.count >= fw_city_capacity)
        {
            return fw_result_buffer_too_small;
        }

        fw_city_config_t &item = parsed_cities.items[parsed_cities.count];
        if (!read_object_string(
                object, object_end, "\"slug\"", item.provider_slug, sizeof(item.provider_slug)))
        {
            return fw_result_parse_error;
        }
        if (!read_object_string(object, object_end, "\"city\"", item.name, sizeof(item.name)) &&
            !copy_text(item.name, sizeof(item.name), item.provider_slug))
        {
            return fw_result_parse_error;
        }

        ++parsed_cities.count;
        cursor = object_end + 1;
    }

    *cities = parsed_cities;
    return fw_result_ok;
}
