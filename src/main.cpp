#include <stdio.h>
#include <string.h>

#include "operation/cities/fw_gdansk_source.h"
#include "operation/fw_city_catalogue.h"
#include "operation/fw_station_config.h"
#include "operation/fw_station_name.h"
#include "systems/sys_platform.h"
#include "ui/ui_departures_screen.h"
#include "ui/ui_stop_picker.h"

namespace
{
ui_departures_screen_t departures_screen(sys_platform_display());
ui_stop_picker_t stop_picker;
fw_gdansk_source_t gdansk_source(sys_platform_http_client());
fw_departure_list_t departures{};
fw_stop_list_t available_stops{};
fw_stop_t selected_stop{};
char station_name[fw_stop_name_max_length]{};
uint32_t last_refresh_ms = 0u;
bool shows_picker = false;
bool has_selected_stop = false;
bool has_gdansk_stops_cache = false;
const fw_city_config_t *city_configs = nullptr;
const char *city_names[5u]{};
size_t city_count = 0u;
size_t selected_city_index = 0u;

typedef enum
{
    app_picker_stage_city = 0,
    app_picker_stage_stop
} app_picker_stage_t;

app_picker_stage_t picker_stage = app_picker_stage_city;

ui_network_status_t network_status()
{
    return {
        sys_platform_network_is_ready(),
        sys_platform_network_rssi_dbm()
    };
}

void debug_log(const char *const message)
{
    sys_platform_debug_log(message);
}

void refresh_departures()
{
    if (!has_selected_stop || !sys_platform_network_is_ready() ||
        (city_configs[selected_city_index].city != fw_city_gdansk))
    {
        return;
    }

    departures_screen.render_loading(station_name, network_status());
    const fw_result_t departure_result = gdansk_source.get_departures(selected_stop.id, &departures);
    char message[80];
    (void)snprintf(
        message, sizeof(message), "Odjazdy: wynik=%d, liczba=%u", static_cast<int>(departure_result),
        static_cast<unsigned int>(departures.count));
    debug_log(message);
    last_refresh_ms = sys_platform_millis();
}

fw_result_t load_gdansk_stops()
{
    if (has_gdansk_stops_cache)
    {
        return fw_result_ok;
    }

    if (!sys_platform_network_is_ready())
    {
        return fw_result_network_error;
    }

    departures_screen.render_message("Gdańsk", "Pobieranie...", network_status());
    const fw_result_t result = gdansk_source.find_stops(FW_STOP_QUERY, &available_stops);
    char message[96];
    (void)snprintf(
        message, sizeof(message), "Przystanki: wynik=%d, liczba=%u, filtr=%s",
        static_cast<int>(result), static_cast<unsigned int>(available_stops.count), FW_STOP_QUERY);
    debug_log(message);
    if (result == fw_result_ok)
    {
        has_gdansk_stops_cache = true;
    }
    return result;
}
}

void setup()
{
    sys_platform_initialize();
    city_configs = fw_city_catalogue_get(&city_count);
    if (city_count > 5u)
    {
        city_count = 5u;
    }
    for (size_t index = 0u; (index < city_count) && (index < 5u); ++index)
    {
        city_names[index] = city_configs[index].name;
        if (city_configs[index].city == fw_city_gdansk)
        {
            selected_city_index = index;
        }
    }

    (void)snprintf(station_name, sizeof(station_name), "%s", "Wybierz miasto");
    debug_log("Aplikacja: gotowa do wyboru miasta");
    departures_screen.render_departures(
        station_name, departures, sys_platform_epoch_s(), network_status());
}

void loop()
{
    const uint32_t now_ms = sys_platform_millis();
    const size_t picker_item_count = picker_stage == app_picker_stage_city ?
        city_count : available_stops.count;
    const int16_t picker_row_height = 34;
    const int16_t picker_first_row_y = 54;
    const size_t picker_visible_rows = picker_stage == app_picker_stage_city ?
        city_count : departures_screen.stop_picker_visible_rows();
    const ui_stop_picker_event_t picker_event = stop_picker.update_touch(
        sys_platform_is_touched(), sys_platform_touch_y(), now_ms, picker_item_count,
        picker_row_height, picker_first_row_y, picker_visible_rows);

    if (picker_event == ui_stop_picker_event_opened)
    {
        picker_stage = app_picker_stage_city;
        departures_screen.render_city_picker(
            city_names, city_count, stop_picker.selected_index(), network_status());
        shows_picker = true;
    }

    if (picker_event == ui_stop_picker_event_selected)
    {
        if (picker_stage == app_picker_stage_city)
        {
            selected_city_index = stop_picker.selected_index();
            has_selected_stop = false;
            debug_log(city_configs[selected_city_index].name);
            picker_stage = app_picker_stage_stop;
            if (city_configs[selected_city_index].city == fw_city_gdansk)
            {
                const fw_result_t stop_result = load_gdansk_stops();
                if (stop_result == fw_result_ok)
                {
                    departures_screen.render_stop_picker(
                        available_stops, stop_picker.selected_index(), stop_picker.scroll_offset(),
                        network_status());
                    shows_picker = available_stops.count > 0u;
                    if (shows_picker)
                    {
                        stop_picker.open();
                    }
                    else
                    {
                        departures_screen.render_message(
                            "Gdańsk", "Brak wynikow", network_status());
                        picker_stage = app_picker_stage_city;
                        stop_picker.reset();
                    }
                }
                else
                {
                    departures_screen.render_message(
                        "Gdańsk", "Blad pobierania", network_status());
                    shows_picker = false;
                    picker_stage = app_picker_stage_city;
                    stop_picker.reset();
                }
            }
            else
            {
                departures_screen.render_message(
                    city_configs[selected_city_index].name,
                    "Adapter wkrotce",
                    network_status());
                shows_picker = false;
                stop_picker.reset();
            }
        }
        else
        {
            (void)snprintf(
                station_name, sizeof(station_name), "%s",
                available_stops.items[stop_picker.selected_index()].name);
            selected_stop = available_stops.items[stop_picker.selected_index()];
            has_selected_stop = true;
            debug_log(selected_stop.name);
            refresh_departures();
            departures_screen.render_departures(
                station_name, departures, sys_platform_epoch_s(), network_status());
            shows_picker = false;
            stop_picker.reset();
        }
    }

    if ((picker_event == ui_stop_picker_event_scrolled) &&
        (picker_stage == app_picker_stage_stop) && shows_picker)
    {
        departures_screen.render_stop_picker(
            available_stops, stop_picker.selected_index(), stop_picker.scroll_offset(), network_status());
    }

    if (has_selected_stop && !shows_picker && ((now_ms - last_refresh_ms) >= 30000u))
    {
        refresh_departures();
        departures_screen.render_departures(
            station_name, departures, sys_platform_epoch_s(), network_status());
    }
}
