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
typedef enum
{
    app_picker_stage_city = 0,
    app_picker_stage_stop
} app_picker_stage_t;

typedef enum
{
    app_screen_departures = 0,
    app_screen_city_picker,
    app_screen_stop_search,
    app_screen_stop_searching,
    app_screen_stop_picker,
    app_screen_message
} app_screen_t;

ui_departures_screen_t departures_screen(sys_platform_display());
ui_stop_picker_t stop_picker;
ui_stop_search_t stop_search;
fw_local_api_source_t local_api_source(
    sys_platform_http_client(), sys_platform_provider_url());
fw_departure_list_t departures{};
fw_departure_list_t downloaded_departures{};
fw_city_list_t downloaded_cities{};
fw_stop_list_t *available_stops = nullptr;
fw_stop_t selected_stop{};
sys_platform_station_config_t saved_station_config{};
char station_name[fw_stop_name_max_length]{};
char download_stop_name[fw_stop_name_max_length]{};
char download_stop_query[fw_stop_name_max_length]{};
size_t download_departure_count = 1u;
uint32_t last_refresh_ms = 0u;
uint32_t last_server_check_ms = 0u;
uint32_t last_animation_ms = 0u;
bool is_touch_active = false;
bool has_selected_stop = false;
bool is_server_available = false;
volatile bool has_downloaded_departures = false;
volatile fw_result_t downloaded_departures_result = fw_result_network_error;
volatile bool has_downloaded_cities = false;
volatile fw_result_t downloaded_cities_result = fw_result_network_error;
volatile bool has_downloaded_stops = false;
volatile fw_result_t downloaded_stops_result = fw_result_network_error;
fw_city_list_t cities{};
const char *city_names[fw_city_capacity]{};
size_t selected_city_index = 0u;
static const uint32_t server_check_interval_ms = 30000u;
static const uint32_t animation_interval_ms = 500u;
app_picker_stage_t picker_stage = app_picker_stage_city;
app_screen_t current_screen = app_screen_departures;

void render_city_picker(bool refresh_content_only = false);
void render_departures();
void render_departure_animation();
void show_stop_results(fw_result_t result);
bool is_departures_screen_active();

bool copy_station_config_value(
    char *const destination,
    const size_t destination_size,
    const char *const source)
{
    if ((destination == nullptr) || (destination_size == 0u) || (source == nullptr))
    {
        return false;
    }

    const int written = snprintf(destination, destination_size, "%s", source);
    return (written >= 0) && (static_cast<size_t>(written) < destination_size);
}

void save_selected_station()
{
    if (!sys_platform_save_station_config(&saved_station_config))
    {
        sys_platform_debug_log("Konfiguracja przystanku nie zostala zapisana");
    }
}

ui_network_status_t network_status()
{
    return {
        sys_platform_network_is_ready(),
        sys_platform_network_rssi_dbm(),
        is_server_available
    };
}

bool is_departures_screen_active()
{
    return has_selected_stop && (current_screen == app_screen_departures) && !is_touch_active &&
           !stop_picker.is_interacting();
}

void download_cities(void *const user_context)
{
    (void)user_context;
    const fw_result_t result = fw_city_catalogue_load(
        sys_platform_http_client(), sys_platform_provider_url(), &downloaded_cities);
    downloaded_cities_result = result;
    has_downloaded_cities = true;
}

void request_cities()
{
    if (!sys_platform_network_is_ready() || sys_platform_background_task_is_busy())
    {
        return;
    }

    has_downloaded_cities = false;
    (void)sys_platform_queue_background_task(download_cities, nullptr);
}

void apply_downloaded_cities()
{
    if (!has_downloaded_cities)
    {
        return;
    }

    has_downloaded_cities = false;
    const fw_result_t result = downloaded_cities_result;
    is_server_available = result == fw_result_ok;
    last_server_check_ms = sys_platform_millis();
    if (is_server_available)
    {
        cities = downloaded_cities;
        for (size_t index = 0u; index < cities.count; ++index)
        {
            city_names[index] = cities.items[index].name;
        }
    }
    if (current_screen == app_screen_city_picker)
    {
        render_city_picker();
    }
    else if (!has_selected_stop && (current_screen == app_screen_departures))
    {
        render_departures();
    }
}

void render_city_picker(const bool refresh_content_only)
{
    departures_screen.render_city_picker(
        city_names, cities.count, stop_picker.selected_index(), stop_picker.page_index(),
        sys_platform_millis(), refresh_content_only, network_status());
}

void render_departures()
{
    if (current_screen != app_screen_departures)
    {
        return;
    }

    departures_screen.render_departures(
        station_name, departures, sys_platform_epoch_s(), sys_platform_millis(), network_status());
}

void render_departure_animation()
{
    if (current_screen != app_screen_departures)
    {
        return;
    }

    departures_screen.render_departure_animation(
        station_name, departures, sys_platform_epoch_s(), sys_platform_millis());
}

void download_departures(void *const user_context)
{
    (void)user_context;
    fw_departure_list_t next_departures{};
    const fw_result_t result = local_api_source.get_departures(
        download_stop_name, download_departure_count, &next_departures);
    downloaded_departures = next_departures;
    downloaded_departures_result = result;
    has_downloaded_departures = true;
}

void request_departures()
{
    if (!is_departures_screen_active() || !sys_platform_network_is_ready() ||
        sys_platform_background_task_is_busy())
    {
        return;
    }

    (void)snprintf(download_stop_name, sizeof(download_stop_name), "%s", selected_stop.name);
    download_departure_count = fw_departure_capacity;
    has_downloaded_departures = false;
    if (!sys_platform_queue_background_task(download_departures, nullptr))
    {
        return;
    }
    last_refresh_ms = sys_platform_millis();
}

void apply_downloaded_departures()
{
    if (!has_downloaded_departures)
    {
        return;
    }

    has_downloaded_departures = false;
    const fw_result_t result = downloaded_departures_result;
    if (result == fw_result_ok)
    {
        departures = downloaded_departures;
    }
    is_server_available = result == fw_result_ok;
    char message[80u];
    (void)snprintf(
        message, sizeof(message), "Odjazdy: wynik=%d, liczba=%u", static_cast<int>(result),
        static_cast<unsigned int>(departures.count));
    sys_platform_debug_log(message);
    if (is_departures_screen_active())
    {
        render_departures();
    }
}

void download_stops(void *const user_context)
{
    (void)user_context;
    const fw_result_t result = local_api_source.find_stops(download_stop_query, available_stops);
    downloaded_stops_result = result;
    has_downloaded_stops = true;
}

bool request_stop_search()
{
    if (!sys_platform_network_is_ready() || (available_stops == nullptr) ||
        sys_platform_background_task_is_busy())
    {
        return false;
    }

    (void)snprintf(download_stop_query, sizeof(download_stop_query), "%s", stop_search.query());
    has_downloaded_stops = false;
    return sys_platform_queue_background_task(download_stops, nullptr);
}

void apply_downloaded_stops()
{
    if (!has_downloaded_stops)
    {
        return;
    }

    has_downloaded_stops = false;
    is_server_available = downloaded_stops_result == fw_result_ok;
    show_stop_results(downloaded_stops_result);
}

void show_stop_searching()
{
    departures_screen.render_message(
        cities.items[selected_city_index].name, "Szukanie...", network_status());
}

void show_stop_results(const fw_result_t result)
{
    if ((result == fw_result_ok) && (available_stops != nullptr) && (available_stops->count > 0u))
    {
        picker_stage = app_picker_stage_stop;
        current_screen = app_screen_stop_picker;
        stop_picker.open();
        departures_screen.render_stop_picker(
            *available_stops, stop_picker.selected_index(), stop_picker.page_index(),
            sys_platform_millis(), false, network_status());
        return;
    }

    current_screen = app_screen_message;
    departures_screen.render_message(
        cities.items[selected_city_index].name,
        result == fw_result_ok ? "Brak wynikow" :
        (result == fw_result_busy ? "Poczekaj chwile" : "Blad polaczenia"),
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

    if (sys_platform_load_station_config(&saved_station_config))
    {
        local_api_source.set_provider(
            saved_station_config.city_provider_slug, saved_station_config.city_name);
        (void)copy_station_config_value(
            selected_stop.id, sizeof(selected_stop.id), saved_station_config.stop_id);
        (void)copy_station_config_value(
            selected_stop.name, sizeof(selected_stop.name), saved_station_config.stop_name);
        (void)copy_station_config_value(
            station_name, sizeof(station_name), selected_stop.name);
        has_selected_stop = true;
    }
    else
    {
        (void)snprintf(station_name, sizeof(station_name), "%s", "Wybierz miasto");
    }
    render_departures();
    last_server_check_ms = sys_platform_millis() - server_check_interval_ms;
    if (has_selected_stop)
    {
        request_departures();
    }
    else
    {
        request_cities();
    }
}

void loop()
{
    const uint32_t now_ms = sys_platform_millis();
    const bool is_touched = sys_platform_is_touched();
    const int16_t touch_x = sys_platform_touch_x();
    const int16_t touch_y = sys_platform_touch_y();
    is_touch_active = is_touched;

    apply_downloaded_cities();
    apply_downloaded_stops();
    apply_downloaded_departures();

    if (current_screen == app_screen_stop_searching)
    {
        return;
    }

    if (current_screen == app_screen_stop_search)
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
            picker_stage = app_picker_stage_city;
            current_screen = app_screen_city_picker;
            stop_picker.reopen();
            render_city_picker();
        }
        else if (event == ui_stop_search_event_submitted)
        {
            if (request_stop_search())
            {
                current_screen = app_screen_stop_searching;
                show_stop_searching();
            }
            else
            {
                show_stop_results(fw_result_busy);
            }
        }
        return;
    }

    const size_t item_count = picker_stage == app_picker_stage_city ?
        cities.count : (available_stops == nullptr ? 0u : available_stops->count);
    const size_t visible_rows = departures_screen.stop_picker_visible_rows();
    const ui_stop_picker_event_t event = stop_picker.update_touch(
        is_touched, touch_x, touch_y, now_ms, item_count,
        departures_screen.stop_picker_row_height(), departures_screen.stop_picker_first_row_y(),
        visible_rows, sys_platform_display()->width(), sys_platform_display()->height(),
        departures_screen.stop_picker_pagination_height());

    if (event == ui_stop_picker_event_opened)
    {
        picker_stage = app_picker_stage_city;
        current_screen = app_screen_city_picker;
        render_city_picker();
    }
    else if (event == ui_stop_picker_event_selected)
    {
        if (sys_platform_background_task_is_busy())
        {
            stop_picker.open();
            if (picker_stage == app_picker_stage_city)
            {
                current_screen = app_screen_city_picker;
                render_city_picker();
            }
            else if (available_stops != nullptr)
            {
                current_screen = app_screen_stop_picker;
                departures_screen.render_stop_picker(
                    *available_stops, stop_picker.selected_index(), stop_picker.page_index(),
                    sys_platform_millis(), false, network_status());
            }
        }
        else if (picker_stage == app_picker_stage_city)
        {
            selected_city_index = stop_picker.selected_index();
            saved_station_config = {};
            if (!copy_station_config_value(
                    saved_station_config.city_provider_slug,
                    sizeof(saved_station_config.city_provider_slug),
                    cities.items[selected_city_index].provider_slug) ||
                !copy_station_config_value(
                    saved_station_config.city_name, sizeof(saved_station_config.city_name),
                    cities.items[selected_city_index].name))
            {
                return;
            }
            local_api_source.set_provider(
                saved_station_config.city_provider_slug, saved_station_config.city_name);
            has_selected_stop = false;
            current_screen = app_screen_stop_search;
            stop_search.reset();
            departures_screen.render_stop_search(
                stop_search.query(), sys_platform_has_full_keyboard(), network_status());
        }
        else if ((available_stops != nullptr) &&
                 (stop_picker.selected_index() < available_stops->count))
        {
            selected_stop = available_stops->items[stop_picker.selected_index()];
            if (!copy_station_config_value(
                    saved_station_config.stop_id, sizeof(saved_station_config.stop_id), selected_stop.id) ||
                !copy_station_config_value(
                    saved_station_config.stop_name, sizeof(saved_station_config.stop_name),
                    selected_stop.name))
            {
                return;
            }
            (void)snprintf(station_name, sizeof(station_name), "%s", selected_stop.name);
            has_selected_stop = true;
            current_screen = app_screen_departures;
            stop_picker.reset();
            save_selected_station();
            request_departures();
            render_departures();
        }
    }
    else if ((event == ui_stop_picker_event_scrolled) &&
             ((current_screen == app_screen_city_picker) ||
              (current_screen == app_screen_stop_picker)))
    {
        if (picker_stage == app_picker_stage_city)
        {
            render_city_picker();
        }
        else if (available_stops != nullptr)
        {
            departures_screen.render_stop_picker(
                *available_stops, stop_picker.selected_index(), stop_picker.page_index(),
                sys_platform_millis(), false, network_status());
        }
    }

    if (!sys_platform_background_task_is_busy() &&
        ((now_ms - last_server_check_ms) >= server_check_interval_ms))
    {
        request_cities();
    }

    if (is_departures_screen_active() && ((now_ms - last_refresh_ms) >= 30000u))
    {
        request_departures();
    }

    if ((now_ms - last_animation_ms) >= animation_interval_ms)
    {
        last_animation_ms = now_ms;
        if (current_screen == app_screen_city_picker)
        {
            render_city_picker(true);
        }
        else if ((current_screen == app_screen_stop_picker) && (available_stops != nullptr))
        {
            departures_screen.render_stop_picker(
                *available_stops, stop_picker.selected_index(), stop_picker.page_index(), now_ms,
                true, network_status());
        }
        else if (is_departures_screen_active())
        {
            render_departure_animation();
        }
    }
}
