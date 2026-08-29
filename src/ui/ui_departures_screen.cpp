#include "ui/ui_departures_screen.h"

#include <stdio.h>
#include <string.h>

namespace
{
static const uint16_t color_white = 0xffffu;
static const uint16_t color_dark_blue = 0x0019u;
static const uint16_t color_black = 0x0000u;
static const uint16_t color_gray = 0x7befu;
static const uint16_t color_red = 0xf800u;
static const uint16_t color_green = 0x07e0u;
static const int16_t header_height = 42;
static const int16_t picker_first_row_y = 54;
static const int16_t picker_row_height = 34;

void draw_button(
    ui_display_t *const display,
    const ui_rectangle_t &rectangle,
    const char *const text,
    const uint16_t color)
{
    if ((display == nullptr) || (text == nullptr))
    {
        return;
    }

    display->fill_rectangle(rectangle, color);
    display->draw_text(
        static_cast<int16_t>(rectangle.x + 4), static_cast<int16_t>(rectangle.y + 4),
        text, color_white, 2u);
}
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

    const int16_t indicator_x = static_cast<int16_t>(_display->width() - 128);
    _display->draw_text(indicator_x, 13, indicator, indicator_color, 2u);
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
        const int16_t row_y = static_cast<int16_t>(52 + index * 34u);
        if ((row_y + 20) >= _display->height())
        {
            break;
        }
        const fw_departure_t &departure = departures.items[index];
        const uint32_t now_time_s = now_epoch_s % 86400u;
        const uint32_t remaining_seconds = departure.departure_time_s >= now_time_s ?
            departure.departure_time_s - now_time_s :
            (86400u - now_time_s) + departure.departure_time_s;
        char remaining[12];
        (void)snprintf(remaining, sizeof(remaining), "%lum", static_cast<unsigned long>(remaining_seconds / 60u));

        _display->draw_text(8, row_y, departure.route_name, color_dark_blue, 2u);
        _display->draw_text(68, row_y, departure.headsign, color_black, 2u);
        _display->draw_text(
            static_cast<int16_t>(_display->width() - 52), row_y,
            remaining, departure.is_realtime ? color_red : color_gray, 2u);
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
    const size_t scroll_offset,
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

    const size_t visible_rows = stop_picker_visible_rows();
    const size_t last_index = scroll_offset + visible_rows < stops.count ?
        scroll_offset + visible_rows : stops.count;
    for (size_t index = scroll_offset; index < last_index; ++index)
    {
        const size_t visible_index = index - scroll_offset;
        const int16_t row_y = static_cast<int16_t>(
            picker_first_row_y + visible_index * static_cast<size_t>(picker_row_height));
        const uint16_t color = index == selected_index ? color_dark_blue : color_black;
        _display->draw_text(10, row_y, stops.items[index].name, color, 2u);
    }

    if (scroll_offset > 0u)
    {
        _display->draw_text(
            static_cast<int16_t>(_display->width() - 20), picker_first_row_y, "^", color_dark_blue, 2u);
    }
    if (last_index < stops.count)
    {
        const int16_t marker_y = static_cast<int16_t>(
            picker_first_row_y + (visible_rows - 1u) * static_cast<size_t>(picker_row_height));
        _display->draw_text(
            static_cast<int16_t>(_display->width() - 20), marker_y, "v", color_dark_blue, 2u);
    }
}

void ui_departures_screen_t::render_stop_search(
    const char *const query,
    const bool has_full_keyboard,
    const ui_network_status_t &network_status) const
{
    if ((_display == nullptr) || (query == nullptr))
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header("Szukaj przystanku", network_status);
    _display->draw_text(8, 52, query[0u] == '\0' ? "Wpisz nazwe" : query, color_black, 2u);

    if (has_full_keyboard)
    {
        static const char *const rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM_"};
        const int16_t keyboard_top = static_cast<int16_t>(_display->height() / 3);
        const int16_t row_height = static_cast<int16_t>((_display->height() - keyboard_top - 54) / 3);
        for (uint8_t row = 0u; row < 3u; ++row)
        {
            const size_t key_count = strlen(rows[row]);
            const int16_t key_width = static_cast<int16_t>(_display->width() / key_count);
            for (size_t key = 0u; key < key_count; ++key)
            {
                char label[2u] = {rows[row][key], '\0'};
                draw_button(
                    _display,
                    {static_cast<int16_t>(key * static_cast<size_t>(key_width)),
                     static_cast<int16_t>(keyboard_top + row * row_height), key_width,
                     row_height},
                    label,
                    color_dark_blue);
            }
        }
        const int16_t action_y = static_cast<int16_t>(_display->height() - 50);
        const int16_t action_width = static_cast<int16_t>(_display->width() / 3);
        draw_button(_display, {0, action_y, action_width, 50}, "WROC", color_gray);
        draw_button(_display, {action_width, action_y, action_width, 50}, "USUN", color_gray);
        draw_button(
            _display,
            {static_cast<int16_t>(2 * action_width), action_y,
             static_cast<int16_t>(_display->width() - 2 * action_width), 50},
            "SZUKAJ", color_green);
        return;
    }

    static const char *const phone_labels[] =
    {
        "0 _", "2 ABC", "3 DEF", "4 GHI", "5 JKL", "6 MNO", "7 PQRS", "8 TUV", "9 WXYZ"
    };
    const int16_t keyboard_top = 76;
    const int16_t action_height = 34;
    const int16_t keypad_height = static_cast<int16_t>(_display->height() - keyboard_top - action_height);
    const int16_t key_width = static_cast<int16_t>(_display->width() / 3);
    const int16_t key_height = static_cast<int16_t>(keypad_height / 3);
    for (uint8_t row = 0u; row < 3u; ++row)
    {
        for (uint8_t column = 0u; column < 3u; ++column)
        {
            const uint8_t key = static_cast<uint8_t>(row * 3u + column);
            draw_button(
                _display,
                {static_cast<int16_t>(column * key_width),
                 static_cast<int16_t>(keyboard_top + row * key_height), key_width, key_height},
                phone_labels[key],
                color_dark_blue);
        }
    }
    const int16_t action_y = static_cast<int16_t>(_display->height() - action_height);
    const int16_t action_width = static_cast<int16_t>(_display->width() / 3);
    draw_button(_display, {0, action_y, action_width, action_height}, "WROC", color_gray);
    draw_button(_display, {action_width, action_y, action_width, action_height}, "USUN", color_gray);
    draw_button(
        _display,
        {static_cast<int16_t>(2 * action_width), action_y,
         static_cast<int16_t>(_display->width() - 2 * action_width), action_height},
        "SZUK", color_green);
}

size_t ui_departures_screen_t::stop_picker_visible_rows() const
{
    if ((_display == nullptr) || (_display->height() <= picker_first_row_y))
    {
        return 1u;
    }

    const int16_t available_height = static_cast<int16_t>(_display->height() - picker_first_row_y);
    const size_t visible_rows = static_cast<size_t>(available_height / picker_row_height);
    return visible_rows > 0u ? visible_rows : 1u;
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
