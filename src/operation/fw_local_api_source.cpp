#include "operation/fw_local_api_source.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wundef"
#include <ArduinoJson.h>
#pragma GCC diagnostic pop

#include <stdio.h>
#include <string.h>

namespace
{
static const size_t json_object_max_length = 768u;
static StaticJsonDocument<1024u> json_document;

typedef enum
{
    local_api_parse_stops = 0,
    local_api_parse_departures
} local_api_parse_kind_t;

typedef struct
{
    local_api_parse_kind_t kind;
    fw_stop_list_t *stops;
    fw_departure_list_t *departures;
    bool is_in_string;
    bool is_escape_sequence;
    bool object_overflow;
    uint8_t object_depth;
    size_t object_length;
    char object_buffer[json_object_max_length];
} local_api_parser_t;

/* One foreground transfer uses this shared parser, keeping its buffer out of
 * the firmware task stack. */
static local_api_parser_t parser;

bool copy_text(char *const destination, const size_t destination_size, const char *const source)
{
    if ((destination == nullptr) || (destination_size == 0u) || (source == nullptr))
    {
        return false;
    }

    const int written = snprintf(destination, destination_size, "%s", source);
    return (written >= 0) && (static_cast<size_t>(written) < destination_size);
}

bool is_decimal_digit(const char character)
{
    return (character >= '0') && (character <= '9');
}

bool read_two_digits(const char *const text, uint8_t *const value)
{
    if ((text == nullptr) || (value == nullptr) || !is_decimal_digit(text[0u]) ||
        !is_decimal_digit(text[1u]))
    {
        return false;
    }

    *value = static_cast<uint8_t>((text[0u] - '0') * 10 + (text[1u] - '0'));
    return true;
}

bool read_four_digits(const char *const text, uint16_t *const value)
{
    if ((text == nullptr) || (value == nullptr))
    {
        return false;
    }

    uint16_t parsed_value = 0u;
    for (size_t index = 0u; index < 4u; ++index)
    {
        if (!is_decimal_digit(text[index]))
        {
            return false;
        }
        parsed_value = static_cast<uint16_t>(parsed_value * 10u +
            static_cast<uint16_t>(text[index] - '0'));
    }

    *value = parsed_value;
    return true;
}

bool is_leap_year(const uint16_t year)
{
    return ((year % 4u) == 0u) && (((year % 100u) != 0u) || ((year % 400u) == 0u));
}

uint8_t days_in_month(const uint16_t year, const uint8_t month)
{
    static const uint8_t month_days[] =
    {
        31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
    };
    if ((month == 0u) || (month > 12u))
    {
        return 0u;
    }
    if ((month == 2u) && is_leap_year(year))
    {
        return 29u;
    }
    return month_days[month - 1u];
}

bool parse_iso_time(const char *const text, uint32_t *const departure_time_s)
{
    if ((text == nullptr) || (departure_time_s == nullptr))
    {
        return false;
    }
    const size_t text_length = strlen(text);
    if (text_length < 17u)
    {
        return false;
    }

    uint16_t year = 0u;
    uint8_t month = 0u;
    uint8_t day = 0u;
    uint8_t hour = 0u;
    uint8_t minute = 0u;
    uint8_t second = 0u;
    if (!read_four_digits(text, &year) || (text[4u] != '-') || !read_two_digits(text + 5u, &month) ||
        (text[7u] != '-') || !read_two_digits(text + 8u, &day) || (text[10u] != 'T') ||
        !read_two_digits(text + 11u, &hour) || (text[13u] != ':') ||
        !read_two_digits(text + 14u, &minute))
    {
        return false;
    }

    if ((year == 0u) || (month == 0u) || (day == 0u) || (day > days_in_month(year, month)) ||
        (hour > 23u) || (minute > 59u))
    {
        return false;
    }

    size_t index = 16u;
    if (text[index] == ':')
    {
        if ((index + 2u >= text_length) || !read_two_digits(text + index + 1u, &second) ||
            (second > 59u))
        {
            return false;
        }
        index += 3u;
    }

    if (text[index] == '.')
    {
        ++index;
        const size_t fraction_start = index;
        while ((index < text_length) && is_decimal_digit(text[index]))
        {
            ++index;
        }
        if (index == fraction_start)
        {
            return false;
        }
    }

    int32_t timezone_offset_s = 0;
    if (text[index] == 'Z')
    {
        ++index;
    }
    else if ((text[index] == '+') || (text[index] == '-'))
    {
        const bool is_negative_offset = text[index] == '-';
        uint8_t timezone_hour = 0u;
        uint8_t timezone_minute = 0u;
        if ((index + 5u >= text_length) || !read_two_digits(text + index + 1u, &timezone_hour) ||
            (text[index + 3u] != ':') || !read_two_digits(text + index + 4u, &timezone_minute) ||
            (timezone_hour > 14u) || ((timezone_hour == 14u) && (timezone_minute != 0u)))
        {
            return false;
        }
        timezone_offset_s = static_cast<int32_t>(timezone_hour) * 3600 +
            static_cast<int32_t>(timezone_minute) * 60;
        if (is_negative_offset)
        {
            timezone_offset_s = -timezone_offset_s;
        }
        index += 6u;
    }
    else
    {
        return false;
    }

    if (index != text_length)
    {
        return false;
    }

    const int32_t local_time_s = static_cast<int32_t>(hour) * 3600 +
        static_cast<int32_t>(minute) * 60 + static_cast<int32_t>(second);
    int32_t utc_time_s = local_time_s - timezone_offset_s;
    if (utc_time_s < 0)
    {
        utc_time_s += 86400;
    }
    else if (utc_time_s >= 86400)
    {
        utc_time_s -= 86400;
    }
    *departure_time_s = static_cast<uint32_t>(utc_time_s);
    return true;
}

void parse_stop_object(local_api_parser_t *const source_parser)
{
    if ((source_parser == nullptr) || (source_parser->stops == nullptr) ||
        source_parser->object_overflow || (source_parser->stops->count >= fw_stop_capacity))
    {
        return;
    }

    source_parser->object_buffer[source_parser->object_length] = '\0';
    json_document.clear();
    if (deserializeJson(json_document, source_parser->object_buffer))
    {
        return;
    }

    const JsonObject source_stop = json_document.as<JsonObject>();
    const char *const id = source_stop["id"] | "";
    const char *const name = source_stop["name"] | "";
    fw_stop_t &stop = source_parser->stops->items[source_parser->stops->count];
    if ((id[0u] == '\0') || (name[0u] == '\0') ||
        !copy_text(stop.id, sizeof(stop.id), id) || !copy_text(stop.name, sizeof(stop.name), name))
    {
        return;
    }

    ++source_parser->stops->count;
}

void parse_departure_object(local_api_parser_t *const source_parser)
{
    if ((source_parser == nullptr) || (source_parser->departures == nullptr) ||
        source_parser->object_overflow ||
        (source_parser->departures->count >= fw_departure_capacity))
    {
        return;
    }

    source_parser->object_buffer[source_parser->object_length] = '\0';
    json_document.clear();
    if (deserializeJson(json_document, source_parser->object_buffer))
    {
        return;
    }

    const JsonObject source_departure = json_document.as<JsonObject>();
    const char *const route = source_departure["route"] | "?";
    const char *const destination = source_departure["destination"] | "Brak kierunku";
    const char *const departure_time = source_departure["estimated_at"] | "";
    fw_departure_t &departure = source_parser->departures->items[source_parser->departures->count];
    if (!copy_text(departure.route_name, sizeof(departure.route_name), route) ||
        !copy_text(departure.headsign, sizeof(departure.headsign), destination) ||
        !parse_iso_time(departure_time, &departure.departure_time_s))
    {
        return;
    }

    departure.delay_s = source_departure["delay_seconds"] | 0;
    departure.is_realtime = departure.delay_s != 0;
    ++source_parser->departures->count;
}

fw_result_t process_response_data(
    const uint8_t *const data,
    const size_t data_length,
    void *const user_context,
    bool *const transfer_complete)
{
    if ((data == nullptr) || (user_context == nullptr) || (transfer_complete == nullptr))
    {
        return fw_result_invalid_argument;
    }

    *transfer_complete = false;
    local_api_parser_t *const source_parser = static_cast<local_api_parser_t *>(user_context);
    for (size_t index = 0u; index < data_length; ++index)
    {
        const char character = static_cast<char>(data[index]);
        if (source_parser->object_depth == 0u)
        {
            if (character == '{')
            {
                source_parser->is_in_string = false;
                source_parser->is_escape_sequence = false;
                source_parser->object_overflow = false;
                source_parser->object_depth = 1u;
                source_parser->object_length = 0u;
                source_parser->object_buffer[source_parser->object_length++] = character;
            }
            continue;
        }

        if ((source_parser->object_length + 1u) < sizeof(source_parser->object_buffer))
        {
            source_parser->object_buffer[source_parser->object_length++] = character;
        }
        else
        {
            source_parser->object_overflow = true;
        }

        if (source_parser->is_in_string)
        {
            if (source_parser->is_escape_sequence)
            {
                source_parser->is_escape_sequence = false;
            }
            else if (character == '\\')
            {
                source_parser->is_escape_sequence = true;
            }
            else if (character == '"')
            {
                source_parser->is_in_string = false;
            }
            continue;
        }

        if (character == '"')
        {
            source_parser->is_in_string = true;
        }
        else if (character == '{')
        {
            ++source_parser->object_depth;
        }
        else if (character == '}')
        {
            --source_parser->object_depth;
            if (source_parser->object_depth == 0u)
            {
                if (source_parser->kind == local_api_parse_stops)
                {
                    parse_stop_object(source_parser);
                    if (source_parser->stops->count >= fw_stop_capacity)
                    {
                        *transfer_complete = true;
                        return fw_result_ok;
                    }
                }
                else
                {
                    parse_departure_object(source_parser);
                    if (source_parser->departures->count >= fw_departure_capacity)
                    {
                        *transfer_complete = true;
                        return fw_result_ok;
                    }
                }
            }
        }
    }

    return fw_result_ok;
}

bool append_url_encoded(char *const destination, const size_t destination_size, const char *const source)
{
    if ((destination == nullptr) || (destination_size == 0u) || (source == nullptr))
    {
        return false;
    }

    static const char hex[] = "0123456789ABCDEF";
    size_t written = strlen(destination);
    for (size_t index = 0u; source[index] != '\0'; ++index)
    {
        const unsigned char character = static_cast<unsigned char>(source[index]);
        const bool is_unreserved = ((character >= 'a') && (character <= 'z')) ||
            ((character >= 'A') && (character <= 'Z')) || ((character >= '0') && (character <= '9')) ||
            (character == '-') || (character == '_') || (character == '.') || (character == '~');
        const size_t required = is_unreserved ? 1u : 3u;
        if ((written + required) >= destination_size)
        {
            return false;
        }

        if (is_unreserved)
        {
            destination[written++] = static_cast<char>(character);
        }
        else
        {
            destination[written++] = '%';
            destination[written++] = hex[(character >> 4u) & 0x0fu];
            destination[written++] = hex[character & 0x0fu];
        }
    }

    destination[written] = '\0';
    return true;
}

bool append_text(char *const destination, const size_t destination_size, const char *const text)
{
    if ((destination == nullptr) || (destination_size == 0u) || (text == nullptr))
    {
        return false;
    }

    const size_t current_length = strlen(destination);
    const size_t text_length = strlen(text);
    if ((current_length + text_length) >= destination_size)
    {
        return false;
    }

    for (size_t index = 0u; index <= text_length; ++index)
    {
        destination[current_length + index] = text[index];
    }
    return true;
}

bool make_base_url(
    char *const destination,
    const size_t destination_size,
    const char *const provider_url,
    const char *const provider_slug)
{
    if ((destination == nullptr) || (destination_size == 0u) || (provider_url == nullptr) ||
        (provider_slug == nullptr) || (provider_url[0u] == '\0') || (provider_slug[0u] == '\0'))
    {
        return false;
    }

    const size_t url_length = strlen(provider_url);
    const bool has_trailing_slash = url_length > 0u && provider_url[url_length - 1u] == '/';
    const int written = snprintf(
        destination,
        destination_size,
        has_trailing_slash ? "%stransit/%s" : "%s/transit/%s",
        provider_url,
        provider_slug);
    return (written >= 0) && (static_cast<size_t>(written) < destination_size);
}
}

fw_local_api_source_t::fw_local_api_source_t(
    driver_http_client_t *const http_client,
    const char *const provider_url) :
    _http_client(http_client),
    _provider_url(provider_url),
    _provider_slug(nullptr),
    _provider_name(nullptr)
{
}

const char *fw_local_api_source_t::name() const
{
    return _provider_name == nullptr ? "Lokalny provider" : _provider_name;
}

void fw_local_api_source_t::set_provider(
    const char *const provider_slug,
    const char *const provider_name)
{
    _provider_slug = provider_slug;
    _provider_name = provider_name;
}

fw_result_t fw_local_api_source_t::find_stops(const char *const query, fw_stop_list_t *const stops)
{
    if ((_http_client == nullptr) || (query == nullptr) || (query[0u] == '\0') || (stops == nullptr))
    {
        return fw_result_invalid_argument;
    }

    char url[320u];
    if (!make_base_url(url, sizeof(url), _provider_url, _provider_slug))
    {
        return fw_result_invalid_argument;
    }

    if (!append_text(url, sizeof(url), "/stops?query=") ||
        !append_url_encoded(url, sizeof(url), query))
    {
        return fw_result_buffer_too_small;
    }

    stops->count = 0u;
    parser = {};
    parser.kind = local_api_parse_stops;
    parser.stops = stops;
    return _http_client->get_stream(url, process_response_data, &parser);
}

fw_result_t fw_local_api_source_t::get_departures(
    const char *const stop_name,
    const size_t requested_count,
    fw_departure_list_t *const departures)
{
    if ((_http_client == nullptr) || (stop_name == nullptr) || (stop_name[0u] == '\0') ||
        (departures == nullptr) || (requested_count == 0u) ||
        (requested_count > fw_departure_capacity))
    {
        return fw_result_invalid_argument;
    }

    char url[320u];
    if (!make_base_url(url, sizeof(url), _provider_url, _provider_slug))
    {
        return fw_result_invalid_argument;
    }

    char count_text[4u];
    const int count_length = snprintf(
        count_text, sizeof(count_text), "%u", static_cast<unsigned int>(requested_count));
    if ((count_length < 0) || (static_cast<size_t>(count_length) >= sizeof(count_text)) ||
        !append_text(url, sizeof(url), "/schedule/") ||
        !append_url_encoded(url, sizeof(url), stop_name) ||
        !append_text(url, sizeof(url), "/") ||
        !append_text(url, sizeof(url), count_text))
    {
        return fw_result_buffer_too_small;
    }

    departures->count = 0u;
    parser = {};
    parser.kind = local_api_parse_departures;
    parser.departures = departures;
    return _http_client->get_stream(url, process_response_data, &parser);
}
