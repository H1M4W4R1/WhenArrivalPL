#include <stdio.h>

#include "operation/fw_city_catalogue.h"
#include "operation/fw_local_api_source.h"
#include "operation/fw_station_name.h"
#include "systems/sys_platform.h"
#include "ui/ui_departures_screen.h"
#include "ui/ui_stop_picker.h"
#include "ui/ui_stop_search.h"

namespace
{
ui_departures_screen_t departures_screen(sys_platform_display());
ui_stop_picker_t stop_picker;
ui_stop_search_t stop_search;
fw_local_api_source_t local_api_source(
    sys_platform_http_client(), sys_platform_provider_url());
fw_departure_list_t departures{};
fw_stop_list_t *available_stops = nullptr;
fw_stop_t selected_stop{};
char station_name[fw_stop_name_max_length]{};
uint32_t last_refresh_ms = 0u;
bool shows_picker = false;
bool shows_search = false;
bool has_selected_stop = false;
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

void render_city_picker()
{
    departures_screen.render_city_picker(
        city_names, city_count, stop_picker.selected_index(), network_status());
}

void refresh_departures()
{
    if (!has_selected_stop || !sys_platform_network_is_ready())
    {
        return;
    }

    departures_screen.render_loading(station_name, network_status());
    const fw_result_t result = local_api_source.get_departures(selected_stop.name, &departures);
    char message[80u];
    (void)snprintf(
        message, sizeof(message), "Odjazdy: wynik=%d, liczba=%u", static_cast<int>(result),
        static_cast<unsigned int>(departures.count));
    sys_platform_debug_log(message);
    last_refresh_ms = sys_platform_millis();
}

fw_result_t search_stops()
{
    if (!sys_platform_network_is_ready())
    {
        return fw_result_network_error;
    }
    if (available_stops == nullptr)
    {
        return fw_result_out_of_memory;
    }

    departures_screen.render_message(
        city_configs[selected_city_index].name, "Szukanie...", network_status());
    return local_api_source.find_stops(stop_search.query(), available_stops);
}

void show_stop_results(const fw_result_t result)
{
    if ((result == fw_result_ok) && (available_stops != nullptr) && (available_stops->count > 0u))
    {
        picker_stage = app_picker_stage_stop;
        shows_picker = true;
        stop_picker.open();
        departures_screen.render_stop_picker(
            *available_stops, stop_picker.selected_index(), stop_picker.scroll_offset(), network_status());
        return;
    }

    shows_search = false;
    shows_picker = false;
    departures_screen.render_message(
        city_configs[selected_city_index].name,
        result == fw_result_ok ? "Brak wynikow" : "Blad polaczenia",
        network_status());
    picker_stage = app_picker_stage_city;
    stop_picker.reset();
}
}

void setup()
{
    sys_platform_initialize();
    available_stops = static_cast<fw_stop_list_t *>(sys_platform_allocate_psram(sizeof(*available_stops)));
    if (available_stops != nullptr)
    {
        *available_stops = {};
    }
    else
    {
        sys_platform_debug_log("PSRAM: brak miejsca na liste przystankow");
    }

    city_configs = fw_city_catalogue_get(&city_count);
    if (city_count > 5u)
    {
        city_count = 5u;
    }
    for (size_t index = 0u; index < city_count; ++index)
    {
        city_names[index] = city_configs[index].name;
    }

    (void)snprintf(station_name, sizeof(station_name), "%s", "Wybierz miasto");
    departures_screen.render_departures(
        station_name, departures, sys_platform_epoch_s(), network_status());
}

void loop()
{
    const uint32_t now_ms = sys_platform_millis();
    const bool is_touched = sys_platform_is_touched();
    const int16_t touch_x = sys_platform_touch_x();
    const int16_t touch_y = sys_platform_touch_y();

    if (shows_search)
    {
        const ui_stop_search_event_t event = stop_search.update_touch(
            is_touched, touch_x, touch_y, now_ms, sys_platform_display()->width(),
            sys_platform_display()->height(), sys_platform_has_full_keyboard());
        if (event == ui_stop_search_event_changed)
        {
            departures_screen.render_stop_search(
                stop_search.query(), sys_platform_has_full_keyboard(), network_status());
        }
        else if (event == ui_stop_search_event_cancelled)
        {
            shows_search = false;
            picker_stage = app_picker_stage_city;
            stop_picker.open();
            render_city_picker();
        }
        else if (event == ui_stop_search_event_submitted)
        {
            shows_search = false;
            show_stop_results(search_stops());
        }
        return;
    }

    const size_t item_count = picker_stage == app_picker_stage_city ?
        city_count : (available_stops == nullptr ? 0u : available_stops->count);
    const size_t visible_rows = picker_stage == app_picker_stage_city ?
        city_count : departures_screen.stop_picker_visible_rows();
    const ui_stop_picker_event_t event = stop_picker.update_touch(
        is_touched, touch_y, now_ms, item_count, 34, 54, visible_rows);

    if (event == ui_stop_picker_event_opened)
    {
        picker_stage = app_picker_stage_city;
        shows_picker = true;
        render_city_picker();
    }
    else if (event == ui_stop_picker_event_selected)
    {
        if (picker_stage == app_picker_stage_city)
        {
            selected_city_index = stop_picker.selected_index();
            local_api_source.set_provider(
                city_configs[selected_city_index].provider_slug, city_configs[selected_city_index].name);
            has_selected_stop = false;
            shows_picker = false;
            shows_search = true;
            stop_search.reset();
            departures_screen.render_stop_search(
                stop_search.query(), sys_platform_has_full_keyboard(), network_status());
        }
        else if ((available_stops != nullptr) &&
                 (stop_picker.selected_index() < available_stops->count))
        {
            selected_stop = available_stops->items[stop_picker.selected_index()];
            (void)snprintf(station_name, sizeof(station_name), "%s", selected_stop.name);
            has_selected_stop = true;
            refresh_departures();
            departures_screen.render_departures(
                station_name, departures, sys_platform_epoch_s(), network_status());
            shows_picker = false;
            stop_picker.reset();
        }
    }
    else if ((event == ui_stop_picker_event_scrolled) &&
             (picker_stage == app_picker_stage_stop) && shows_picker && (available_stops != nullptr))
    {
        departures_screen.render_stop_picker(
            *available_stops, stop_picker.selected_index(), stop_picker.scroll_offset(), network_status());
    }

    if (has_selected_stop && !shows_picker && ((now_ms - last_refresh_ms) >= 30000u))
    {
        refresh_departures();
        departures_screen.render_departures(
            station_name, departures, sys_platform_epoch_s(), network_status());
    }
}
