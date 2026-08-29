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
static const uint16_t color_yellow = 0xffe0u;
static const uint16_t color_orange = 0xfd20u;
static const int16_t header_height = 42;
static const int16_t picker_first_row_y = 54;
static const int16_t picker_row_height = 34;
static const int16_t pagination_height = 38;

uint16_t wifi_indicator_color(const ui_network_status_t &network_status)
{
    if (!network_status.is_connected)
    {
        return color_red;
    }
    if (network_status.rssi_dbm >= -60)
    {
        return color_green;
    }
    if (network_status.rssi_dbm >= -70)
    {
        return color_yellow;
    }
    if (network_status.rssi_dbm >= -80)
    {
        return color_orange;
    }
    return color_red;
}

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

    const int16_t wifi_x = static_cast<int16_t>(_display->width() - 90);
    const int16_t server_x = static_cast<int16_t>(_display->width() - 42);
    _display->draw_text(wifi_x, 15, "WiFi", wifi_indicator_color(network_status), 1u);
    _display->draw_text(
        server_x, 15, "Srv", network_status.is_server_available ? color_green : color_red, 1u);
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
    const size_t page_index,
    const ui_network_status_t &network_status) const
{
    if ((_display == nullptr) || (city_names == nullptr))
    {
        return;
    }

    _display->fill_screen(color_white);
    render_header("Wybierz miasto", network_status);
    const size_t visible_rows = stop_picker_visible_rows();
    if (city_count == 0u)
    {
        _display->draw_text(10, picker_first_row_y, "Brak miast", color_black, 2u);
    }
    const size_t first_index = page_index * visible_rows;
    const size_t last_index = first_index + visible_rows < city_count ?
        first_index + visible_rows : city_count;
    for (size_t index = first_index; index < last_index; ++index)
    {
        const size_t visible_index = index - first_index;
        const int16_t row_y = static_cast<int16_t>(54 + visible_index * 34u);
        const uint16_t color = index == selected_index ? color_dark_blue : color_black;
        _display->draw_text(10, row_y, city_names[index], color, 2u);
    }

    const size_t page_count = city_count == 0u ? 1u :
        (city_count + visible_rows - 1u) / visible_rows;
    const int16_t pagination_y = static_cast<int16_t>(_display->height() - pagination_height);
    const int16_t button_width = static_cast<int16_t>(_display->width() / 3);
    draw_button(_display, {0, pagination_y, button_width, pagination_height}, "<", color_dark_blue);
    draw_button(
        _display,
        {static_cast<int16_t>(2 * button_width), pagination_y,
         static_cast<int16_t>(_display->width() - 2 * button_width), pagination_height},
        ">", color_dark_blue);
    char page_label[16u];
    (void)snprintf(page_label, sizeof(page_label), "%u/%u", static_cast<unsigned int>(page_index + 1u),
        static_cast<unsigned int>(page_count));
    _display->draw_text(
        static_cast<int16_t>(button_width + 8), static_cast<int16_t>(pagination_y + 10), page_label,
        color_black, 2u);
}

void ui_departures_screen_t::render_stop_picker(
    const fw_stop_list_t &stops,
    const size_t selected_index,
    const size_t page_index,
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
    const size_t first_index = page_index * visible_rows;
    const size_t last_index = first_index + visible_rows < stops.count ?
        first_index + visible_rows : stops.count;
    for (size_t index = first_index; index < last_index; ++index)
    {
        const size_t visible_index = index - first_index;
        const int16_t row_y = static_cast<int16_t>(
            picker_first_row_y + visible_index * static_cast<size_t>(picker_row_height));
        const uint16_t color = index == selected_index ? color_dark_blue : color_black;
        _display->draw_text(10, row_y, stops.items[index].name, color, 2u);
    }

    const size_t page_count = (stops.count + visible_rows - 1u) / visible_rows;
    const int16_t pagination_y = static_cast<int16_t>(_display->height() - pagination_height);
    const int16_t button_width = static_cast<int16_t>(_display->width() / 3);
    draw_button(_display, {0, pagination_y, button_width, pagination_height}, "<", color_dark_blue);
    draw_button(
        _display,
        {static_cast<int16_t>(2 * button_width), pagination_y,
         static_cast<int16_t>(_display->width() - 2 * button_width), pagination_height},
        ">", color_dark_blue);
    char page_label[16u];
    (void)snprintf(page_label, sizeof(page_label), "%u/%u", static_cast<unsigned int>(page_index + 1u),
        static_cast<unsigned int>(page_count));
    _display->draw_text(
        static_cast<int16_t>(button_width + 8), static_cast<int16_t>(pagination_y + 10), page_label,
        color_black, 2u);
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
    if ((_display == nullptr) || (_display->height() <= (picker_first_row_y + pagination_height)))
    {
        return 1u;
    }

    const int16_t available_height = static_cast<int16_t>(
        _display->height() - picker_first_row_y - pagination_height);
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
