#include "operation/cities/fw_gdansk_source.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wundef"
#include <ArduinoJson.h>
#pragma GCC diagnostic pop

#include <stdio.h>
#include <ctype.h>
#include <string.h>

namespace
{
static StaticJsonDocument<8192u> response_document;
static StaticJsonDocument<1024u> stop_document;
static const char stops_url[] =
    "https://ckan.multimediagdansk.pl/dataset/c24aa637-3619-4dc2-a171-a23eec8f2172/"
    "resource/d3e96eb6-25ad-4d6c-8651-b1eb39155945/download/stopsingdansk.json";
static const char stops_key[] = "\"stops\"";

typedef struct
{
    fw_stop_list_t *stops;
    const char *query;
    bool has_stops_key;
    bool is_in_stops;
    bool is_capturing_object;
    bool is_in_string;
    bool is_escape_sequence;
    bool object_overflow;
    uint8_t key_match_length;
    uint16_t object_depth;
    size_t object_length;
    char object_buffer[768u];
} gdansk_stop_parser_t;

/* One blocking HTTP transfer runs in the foreground, so module storage avoids
 * putting the JSON object buffer on the firmware task stack. */
static gdansk_stop_parser_t stop_parser;

bool copy_text(char *const destination, const size_t destination_size, const char *const source)
{
    if ((destination == nullptr) || (destination_size == 0u) || (source == nullptr))
    {
        return false;
    }

    const int written = snprintf(destination, destination_size, "%s", source);
    return (written >= 0) && (static_cast<size_t>(written) < destination_size);
}

bool text_contains_case_insensitive(const char *const text, const char *const query)
{
    if ((text == nullptr) || (query == nullptr))
    {
        return false;
    }

    if (query[0u] == '\0')
    {
        return true;
    }

    for (size_t text_index = 0u; text[text_index] != '\0'; ++text_index)
    {
        size_t query_index = 0u;
        while ((query[query_index] != '\0') && (text[text_index + query_index] != '\0') &&
               (tolower(static_cast<unsigned char>(text[text_index + query_index])) ==
                tolower(static_cast<unsigned char>(query[query_index]))))
        {
            ++query_index;
        }

        if (query[query_index] == '\0')
        {
            return true;
        }
    }

    return false;
}

void parse_stop_object(gdansk_stop_parser_t *const parser)
{
    if ((parser == nullptr) || (parser->stops == nullptr) || parser->object_overflow ||
        (parser->stops->count >= fw_stop_capacity))
    {
        return;
    }

    parser->object_buffer[parser->object_length] = '\0';
    stop_document.clear();
    if (deserializeJson(stop_document, parser->object_buffer))
    {
        return;
    }

    const JsonObject source_stop = stop_document.as<JsonObject>();
    const char *stop_name = source_stop["stopName"] | "";
    if (stop_name[0u] == '\0')
    {
        stop_name = source_stop["stopDesc"] | "";
    }

    if ((stop_name[0u] == '\0') || !text_contains_case_insensitive(stop_name, parser->query))
    {
        return;
    }

    const uint32_t stop_id = source_stop["stopId"] | 0u;
    if (stop_id == 0u)
    {
        return;
    }

    const char *const sub_name = source_stop["subName"] | "";
    fw_stop_t &stop = parser->stops->items[parser->stops->count];
    const int id_length = snprintf(stop.id, sizeof(stop.id), "%lu", static_cast<unsigned long>(stop_id));
    const int name_length = sub_name[0u] == '\0' ?
        snprintf(stop.name, sizeof(stop.name), "%s", stop_name) :
        snprintf(stop.name, sizeof(stop.name), "%s (%s)", stop_name, sub_name);
    if ((id_length < 0) || (static_cast<size_t>(id_length) >= sizeof(stop.id)) ||
        (name_length < 0) || (static_cast<size_t>(name_length) >= sizeof(stop.name)))
    {
        return;
    }

    ++parser->stops->count;
}

fw_result_t process_stop_data(
    const uint8_t *const data,
    const size_t data_length,
    void *const user_context)
{
    if ((data == nullptr) || (user_context == nullptr))
    {
        return fw_result_invalid_argument;
    }

    gdansk_stop_parser_t *const parser = static_cast<gdansk_stop_parser_t *>(user_context);
    for (size_t index = 0u; index < data_length; ++index)
    {
        const char character = static_cast<char>(data[index]);
        if (!parser->has_stops_key)
        {
            if (character == stops_key[parser->key_match_length])
            {
                ++parser->key_match_length;
                if (parser->key_match_length == (sizeof(stops_key) - 1u))
                {
                    parser->has_stops_key = true;
                }
            }
            else
            {
                parser->key_match_length = character == stops_key[0u] ? 1u : 0u;
            }
            continue;
        }

        if (!parser->is_in_stops)
        {
            if (character == '[')
            {
                parser->is_in_stops = true;
            }
            continue;
        }

        if (!parser->is_capturing_object)
        {
            if (character == '{')
            {
                parser->is_capturing_object = true;
                parser->is_in_string = false;
                parser->is_escape_sequence = false;
                parser->object_overflow = false;
                parser->object_depth = 1u;
                parser->object_length = 0u;
                parser->object_buffer[parser->object_length++] = character;
            }
            continue;
        }

        if ((parser->object_length + 1u) < sizeof(parser->object_buffer))
        {
            parser->object_buffer[parser->object_length++] = character;
        }
        else
        {
            parser->object_overflow = true;
        }

        if (parser->is_in_string)
        {
            if (parser->is_escape_sequence)
            {
                parser->is_escape_sequence = false;
            }
            else if (character == '\\')
            {
                parser->is_escape_sequence = true;
            }
            else if (character == '"')
            {
                parser->is_in_string = false;
            }
        }
        else if (character == '"')
        {
            parser->is_in_string = true;
        }
        else if (character == '{')
        {
            ++parser->object_depth;
        }
        else if (character == '}')
        {
            --parser->object_depth;
            if (parser->object_depth == 0u)
            {
                parse_stop_object(parser);
                parser->is_capturing_object = false;
            }
        }
    }

    return fw_result_ok;
}

bool parse_iso8601_utc(const char *const text, uint32_t *const epoch_s)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    const int item_count = sscanf(text, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    if ((item_count < 5) || (epoch_s == nullptr) || (month < 1) || (month > 12) ||
        (day < 1) || (day > 31) || (hour < 0) || (hour > 23) ||
        (minute < 0) || (minute > 59) || (second < 0) || (second > 59))
    {
        return false;
    }

    const int adjusted_year = year - (month <= 2 ? 1 : 0);
    const int era = (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
    const unsigned int year_of_era = static_cast<unsigned int>(adjusted_year - era * 400);
    const unsigned int adjusted_month = static_cast<unsigned int>(month + (month > 2 ? -3 : 9));
    const unsigned int day_of_year = (153u * adjusted_month + 2u) / 5u +
                                     static_cast<unsigned int>(day - 1);
    const unsigned int day_of_era = year_of_era * 365u + year_of_era / 4u -
                                    year_of_era / 100u + day_of_year;
    const int32_t days_since_epoch = era * 146097 + static_cast<int32_t>(day_of_era) - 719468;

    if (days_since_epoch < 0)
    {
        return false;
    }

    *epoch_s = static_cast<uint32_t>(days_since_epoch) * 86400u +
               static_cast<uint32_t>(hour) * 3600u + static_cast<uint32_t>(minute) * 60u +
               static_cast<uint32_t>(second);
    return true;
}
}

fw_gdansk_source_t::fw_gdansk_source_t(driver_http_client_t *const http_client) :
    _http_client(http_client),
    _response_buffer{}
{
}

const char *fw_gdansk_source_t::name() const
{
    return "Gdańsk TRISTAR";
}

fw_result_t fw_gdansk_source_t::find_stops(const char *const query, fw_stop_list_t *const stops)
{
    if ((_http_client == nullptr) || (query == nullptr) || (stops == nullptr))
    {
        return fw_result_invalid_argument;
    }

    stops->count = 0u;
    stop_parser = {};
    stop_parser.stops = stops;
    stop_parser.query = query;
    return _http_client->get_stream(stops_url, process_stop_data, &stop_parser);
}

fw_result_t fw_gdansk_source_t::get_departures(
    const char *const stop_id,
    fw_departure_list_t *const departures)
{
    if ((_http_client == nullptr) || (stop_id == nullptr) || (departures == nullptr))
    {
        return fw_result_invalid_argument;
    }

    char url[112];
    const int url_length = snprintf(
        url,
        sizeof(url),
        "https://ckan2.multimediagdansk.pl/departures?stopId=%s",
        stop_id);
    if ((url_length < 0) || (static_cast<size_t>(url_length) >= sizeof(url)))
    {
        return fw_result_buffer_too_small;
    }

    size_t response_length = 0u;
    const fw_result_t http_result = _http_client->get(
        url, _response_buffer, sizeof(_response_buffer), &response_length);
    if (http_result != fw_result_ok)
    {
        return http_result;
    }

    response_document.clear();
    const DeserializationError error = deserializeJson(
        response_document, _response_buffer, response_length);
    if (error)
    {
        return fw_result_parse_error;
    }

    departures->count = 0u;
    const JsonArray source_departures = response_document["departures"].as<JsonArray>();
    for (JsonObject source_departure : source_departures)
    {
        if (departures->count >= fw_departure_capacity)
        {
            break;
        }

        const char *const route_name = source_departure["routeShortName"] | "?";
        const char *const headsign = source_departure["headsign"] | "Brak kierunku";
        const char *const departure_time = source_departure["estimatedTime"] | "";
        fw_departure_t &departure = departures->items[departures->count];
        if (!copy_text(departure.route_name, sizeof(departure.route_name), route_name) ||
            !copy_text(departure.headsign, sizeof(departure.headsign), headsign) ||
            !parse_iso8601_utc(departure_time, &departure.departure_epoch_s))
        {
            continue;
        }

        departure.delay_s = source_departure["delayInSeconds"] | 0;
        departure.is_realtime = strcmp(source_departure["status"] | "", "REALTIME") == 0;
        ++departures->count;
    }

    return fw_result_ok;
}
