#include "ui/ui_departures_screen.h"

#include <stdio.h>

namespace
{
static const uint16_t color_white = 0xffffu;
static const uint16_t color_dark_blue = 0x0019u;
static const uint16_t color_black = 0x0000u;
static const uint16_t color_gray = 0x7befu;
static const uint16_t color_red = 0xf800u;
static const uint16_t color_green = 0x07e0u;
static const int16_t header_height = 42;
}

ui_departures_screen_t::ui_departures_screen_t(ui_display_t *const display) :
    _display(display)
{
}

void ui_departures_screen_t::render_header(
    const char *const title,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_rectangle({0, 0, _display->width(), header_height}, color_dark_blue);
    _display->draw_text(10, 13, title, color_white, 2u);

    char indicator[16];
    const uint16_t indicator_color = network_status.is_connected ? color_green : color_red;
    if (network_status.is_connected)
    {
        (void)snprintf(
            indicator, sizeof(indicator), "WiFi %d", static_cast<int>(network_status.rssi_dbm));
    }
    else
    {
        (void)snprintf(indicator, sizeof(indicator), "%s", "WiFi OFF");
    }

    const int16_t indicator_x = static_cast<int16_t>(_display->width() - 76);
    _display->draw_text(indicator_x, 16, indicator, indicator_color, 1u);
}

void ui_departures_screen_t::render_loading(
    const char *const station_name,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header(station_name, network_status);
    _display->draw_text(10, 62, "Pobieranie odjazdów...", color_black, 2u);
}

void ui_departures_screen_t::render_departures(
    const char *const station_name,
    const fw_departure_list_t &departures,
    const uint32_t now_epoch_s,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header(station_name, network_status);

    if (departures.count == 0u)
    {
        _display->draw_text(10, 62, "Brak odjazdow", color_black, 2u);
        return;
    }

    for (size_t index = 0u; index < departures.count; ++index)
    {
        const int16_t row_y = static_cast<int16_t>(52 + index * 26u);
        const fw_departure_t &departure = departures.items[index];
        const uint32_t remaining_seconds = departure.departure_epoch_s > now_epoch_s ?
            departure.departure_epoch_s - now_epoch_s : 0u;
        char remaining[12];
        (void)snprintf(remaining, sizeof(remaining), "%lum", static_cast<unsigned long>(remaining_seconds / 60u));

        _display->draw_text(8, row_y, departure.route_name, color_dark_blue, 2u);
        _display->draw_text(62, row_y + 4, departure.headsign, color_black, 1u);
        _display->draw_text(
            static_cast<int16_t>(_display->width() - 46), row_y + 4, remaining,
            departure.is_realtime ? color_red : color_gray, 1u);
    }
}

void ui_departures_screen_t::render_city_picker(
    const char *const *const city_names,
    const size_t city_count,
    const size_t selected_index,
    const ui_network_status_t &network_status) const
{
    if ((_display == nullptr) || (city_names == nullptr))
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header("Wybierz miasto", network_status);
    for (size_t index = 0u; index < city_count; ++index)
    {
        const int16_t row_y = static_cast<int16_t>(54 + index * 34u);
        const uint16_t color = index == selected_index ? color_dark_blue : color_black;
        _display->draw_text(10, row_y, city_names[index], color, 2u);
    }
}

void ui_departures_screen_t::render_stop_picker(
    const fw_stop_list_t &stops,
    const size_t selected_index,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header("Przystanek", network_status);
    if (stops.count == 0u)
    {
        _display->draw_text(10, 62, "Brak przystankow", color_black, 2u);
        return;
    }

    for (size_t index = 0u; index < stops.count; ++index)
    {
        const int16_t row_y = static_cast<int16_t>(54 + index * 34u);
        const uint16_t color = index == selected_index ? color_dark_blue : color_black;
        _display->draw_text(10, row_y, stops.items[index].name, color, 2u);
    }
}

void ui_departures_screen_t::render_message(
    const char *const title,
    const char *const message,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header(title, network_status);
    _display->draw_text(10, 62, message, color_black, 2u);
}
