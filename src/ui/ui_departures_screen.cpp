#include "ui/ui_departures_screen.h"

#include "operation/fw_station_config.h"

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
#if defined(FW_PLATFORM_TAB5)
static const uint8_t ui_text_scale = 4u;
static const uint8_t ui_small_text_scale = 2u;
static const int16_t header_height = 54;
static const int16_t picker_first_row_y = 66;
static const int16_t picker_row_height = 48;
static const int16_t pagination_height = 48;
static const int16_t departure_first_row_y = 66;
static const int16_t departure_row_height = 46;
static const int16_t departure_text_height = 34;
static const int16_t departure_route_area_width = 104;
static const int16_t departure_time_area_width = 128;
static const size_t departure_visible_row_count = 14u;
#else
static const uint8_t ui_text_scale = 2u;
static const uint8_t ui_small_text_scale = 1u;
static const int16_t header_height = 42;
static const int16_t picker_first_row_y = 54;
static const int16_t picker_row_height = 34;
static const int16_t pagination_height = 38;
static const int16_t departure_first_row_y = 52;
static const int16_t departure_row_height = 34;
static const int16_t departure_text_height = 20;
static const int16_t departure_route_area_width = 64;
static const int16_t departure_time_area_width = 54;
static const size_t departure_visible_row_count = 5u;
#endif
/* Change this one value to adjust the marquee pace. */
static const uint32_t marquee_character_step_ms = 500u;
static const int16_t marquee_gap_px = 24;

size_t utf8_character_count(const char *const text)
{
    if (text == nullptr)
    {
        return 0u;
    }

    size_t character_count = 0u;
    for (size_t index = 0u; text[index] != '\0'; ++index)
    {
        const uint8_t byte = static_cast<uint8_t>(text[index]);
        if ((byte & 0xc0u) != 0x80u)
        {
            ++character_count;
        }
    }
    return character_count;
}

bool marquee_text_is_needed(
    const char *const text,
    const int16_t width,
    const uint8_t text_scale)
{
    if ((text == nullptr) || (width <= 0) || (text_scale == 0u))
    {
        return false;
    }

    const size_t character_width = 6u * static_cast<size_t>(text_scale);
    return utf8_character_count(text) * character_width > static_cast<size_t>(width);
}

bool departure_rows_match(const fw_departure_t &left, const fw_departure_t &right)
{
    return (strcmp(left.route_name, right.route_name) == 0) &&
           (strcmp(left.headsign, right.headsign) == 0) &&
           (left.departure_time_s == right.departure_time_s) &&
           (left.delay_s == right.delay_s) &&
           (left.is_realtime == right.is_realtime);
}

bool departure_has_not_passed(const fw_departure_t &departure, const uint32_t now_time_s)
{
    return departure.departure_time_s >= now_time_s;
}

size_t sorted_pending_departure_indices(
    const fw_departure_list_t &departures,
    const uint32_t now_time_s,
    size_t *const departure_indices,
    const size_t departure_indices_capacity)
{
    if ((departure_indices == nullptr) || (departure_indices_capacity == 0u))
    {
        return 0u;
    }

    const size_t available_departure_count = departures.count < fw_departure_capacity ?
        departures.count : fw_departure_capacity;
    size_t pending_departure_count = 0u;
    for (size_t index = 0u;
         (index < available_departure_count) && (pending_departure_count < departure_indices_capacity);
         ++index)
    {
        if (departure_has_not_passed(departures.items[index], now_time_s))
        {
            departure_indices[pending_departure_count] = index;
            ++pending_departure_count;
        }
    }

    for (size_t index = 0u; index < pending_departure_count; ++index)
    {
        size_t nearest_index = index;
        for (size_t candidate = index + 1u; candidate < pending_departure_count; ++candidate)
        {
            const fw_departure_t &nearest = departures.items[departure_indices[nearest_index]];
            const fw_departure_t &next = departures.items[departure_indices[candidate]];
            if (next.departure_time_s < nearest.departure_time_s)
            {
                nearest_index = candidate;
            }
        }
        const size_t swap_index = departure_indices[index];
        departure_indices[index] = departure_indices[nearest_index];
        departure_indices[nearest_index] = swap_index;
    }

    return pending_departure_count;
}

#if WIFI_DEBUG
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
#endif

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
        text, color_white, ui_text_scale);
}

void draw_keyboard_key(
    ui_display_t *const display,
    const ui_rectangle_t &touch_rectangle,
    const char *const text,
    const uint16_t color)
{
    static const int16_t border_size = 2;
    const ui_rectangle_t visible_rectangle =
    {
        static_cast<int16_t>(touch_rectangle.x + border_size),
        static_cast<int16_t>(touch_rectangle.y + border_size),
        static_cast<int16_t>(touch_rectangle.width - 2 * border_size),
        static_cast<int16_t>(touch_rectangle.height - 2 * border_size)
    };

    draw_button(display, visible_rectangle, text, color);
}

void draw_marquee_text(
    ui_display_t *const display,
    const int16_t x,
    const int16_t y,
    const int16_t width,
    const char *const text,
    const uint16_t color,
    const uint8_t text_scale,
    const uint32_t animation_ms)
{
    if ((display == nullptr) || (text == nullptr) || (width <= 0) || (text_scale == 0u))
    {
        return;
    }

    const size_t text_length = utf8_character_count(text);
    const int16_t character_width = static_cast<int16_t>(6 * text_scale);
    const int16_t text_width = static_cast<int16_t>(
        text_length * static_cast<size_t>(character_width));
    if (!marquee_text_is_needed(text, width, text_scale))
    {
        display->draw_text(x, y, text, color, text_scale);
        return;
    }

    const int16_t cycle_width = static_cast<int16_t>(text_width + marquee_gap_px);
    const size_t cycle_character_count = static_cast<size_t>(cycle_width / character_width);
    const int16_t offset = static_cast<int16_t>(
        ((animation_ms / marquee_character_step_ms) % cycle_character_count) *
        static_cast<uint32_t>(character_width));
    display->draw_text(static_cast<int16_t>(x - offset), y, text, color, text_scale);
    display->draw_text(static_cast<int16_t>(x + cycle_width - offset), y, text, color, text_scale);
}
}

ui_departures_screen_t::ui_departures_screen_t(ui_display_t *const display) :
    _display(display),
    _cached_departure_rows{},
    _cached_remaining_minutes{},
    _cached_departure_row_count(0u)
{
}

void ui_departures_screen_t::render_header(
    const char *const title,
    const uint32_t animation_ms,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_rectangle({0, 0, _display->width(), header_height}, color_dark_blue);
    render_header_title(title, animation_ms);
#if WIFI_DEBUG
    const int16_t wifi_x = static_cast<int16_t>(_display->width() - 90);
    const int16_t server_x = static_cast<int16_t>(_display->width() - 42);
    _display->draw_text(wifi_x, 15, "WiFi", wifi_indicator_color(network_status), ui_small_text_scale);
    _display->draw_text(
        server_x, 15, "Srv", network_status.is_server_available ? color_green : color_red,
        ui_small_text_scale);
#else
    (void)network_status;
#endif
}

void ui_departures_screen_t::render_header_title(
    const char *const title,
    const uint32_t animation_ms) const
{
    if (_display == nullptr)
    {
        return;
    }

#if WIFI_DEBUG
    const int16_t clear_width = static_cast<int16_t>(_display->width() - 96);
    const int16_t title_width = static_cast<int16_t>(_display->width() - 106);
#else
    const int16_t clear_width = _display->width();
#if WIFI_DEBUG
    const int16_t title_width = static_cast<int16_t>(_display->width() - 106);
#else
    const int16_t title_width = static_cast<int16_t>(_display->width() - 26);
#endif
#endif
    _display->fill_rectangle({0, 0, clear_width, header_height}, color_dark_blue);
    draw_marquee_text(
        _display, 10, 13, title_width, title, color_white, ui_text_scale,
        animation_ms);
}

void ui_departures_screen_t::render_departures(
    const char *const station_name,
    const fw_departure_list_t &departures,
    const uint32_t now_epoch_s,
    const uint32_t animation_ms,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    _display->fill_screen(color_white);
    _cached_departure_row_count = 0u;
    render_header(station_name, animation_ms, network_status);

    if (departures.count == 0u)
    {
        _display->draw_text(10, departure_first_row_y, "Brak odjazdow", color_black, ui_text_scale);
        return;
    }

    const uint32_t now_time_s = now_epoch_s % 86400u;
    size_t departure_indices[fw_departure_capacity];
    const size_t pending_departure_count = sorted_pending_departure_indices(
        departures, now_time_s, departure_indices, fw_departure_capacity);

    const size_t displayed_departure_count = pending_departure_count < departure_visible_row_count ?
        pending_departure_count : departure_visible_row_count;
    if (displayed_departure_count == 0u)
    {
        _display->draw_text(10, departure_first_row_y, "Brak odjazdow", color_black, ui_text_scale);
        return;
    }

    for (size_t index = 0u; index < displayed_departure_count; ++index)
    {
        const int16_t row_y = static_cast<int16_t>(
            departure_first_row_y + index * static_cast<size_t>(departure_row_height));
        if ((row_y + departure_text_height) >= _display->height())
        {
            break;
        }
        const fw_departure_t &departure = departures.items[departure_indices[index]];
        const uint32_t remaining_seconds = departure.departure_time_s - now_time_s;
        char remaining[12];
        (void)snprintf(remaining, sizeof(remaining), "%lum", static_cast<unsigned long>(remaining_seconds / 60u));

        _display->draw_text(8, row_y, departure.route_name, color_dark_blue, ui_text_scale);
        const int16_t headsign_x = static_cast<int16_t>(departure_route_area_width + 4);
        const int16_t headsign_width = static_cast<int16_t>(
            _display->width() - headsign_x - departure_time_area_width);
        draw_marquee_text(
            _display, headsign_x, row_y, headsign_width, departure.headsign, color_black, ui_text_scale,
            animation_ms);
        _display->fill_rectangle(
            {0, row_y, departure_route_area_width, departure_text_height}, color_white);
        _display->fill_rectangle(
            {static_cast<int16_t>(_display->width() - departure_time_area_width), row_y,
             departure_time_area_width, departure_text_height}, color_white);
        _display->draw_text(8, row_y, departure.route_name, color_dark_blue, ui_text_scale);
        const int16_t remaining_width = static_cast<int16_t>(
            utf8_character_count(remaining) * 6u * static_cast<size_t>(ui_text_scale));
        _display->draw_text(
            static_cast<int16_t>(_display->width() - remaining_width - 4), row_y,
            remaining, departure.is_realtime ? color_red : color_gray, ui_text_scale);
        _cached_departure_rows[index] = departure;
        _cached_remaining_minutes[index] = remaining_seconds / 60u;
        _cached_departure_row_count = index + 1u;
    }
}

void ui_departures_screen_t::render_departure_animation(
    const char *const station_name,
    const fw_departure_list_t &departures,
    const uint32_t now_epoch_s,
    const uint32_t animation_ms) const
{
    if (_display == nullptr)
    {
        return;
    }

    const int16_t title_width = static_cast<int16_t>(_display->width() - 26);
    if (marquee_text_is_needed(station_name, title_width, ui_text_scale))
    {
        render_header_title(station_name, animation_ms);
    }

    const uint32_t now_time_s = now_epoch_s % 86400u;
    size_t departure_indices[fw_departure_capacity];
    const size_t pending_departure_count = sorted_pending_departure_indices(
        departures, now_time_s, departure_indices, fw_departure_capacity);

    const size_t displayed_departure_count = pending_departure_count < departure_visible_row_count ?
        pending_departure_count : departure_visible_row_count;

    for (size_t index = displayed_departure_count; index < _cached_departure_row_count; ++index)
    {
        const int16_t row_y = static_cast<int16_t>(
            departure_first_row_y + index * static_cast<size_t>(departure_row_height));
        if ((row_y + departure_text_height) >= _display->height())
        {
            break;
        }
        _display->fill_rectangle(
            {0, row_y, _display->width(), departure_text_height}, color_white);
    }

    for (size_t index = 0u; index < displayed_departure_count; ++index)
    {
        const int16_t row_y = static_cast<int16_t>(
            departure_first_row_y + index * static_cast<size_t>(departure_row_height));
        if ((row_y + departure_text_height) >= _display->height())
        {
            break;
        }

        const fw_departure_t &departure = departures.items[departure_indices[index]];
        const uint32_t remaining_seconds = departure.departure_time_s - now_time_s;
        const uint32_t remaining_minutes = remaining_seconds / 60u;
        const int16_t headsign_x = static_cast<int16_t>(departure_route_area_width + 4);
        const int16_t headsign_width = static_cast<int16_t>(
            _display->width() - headsign_x - departure_time_area_width);
        const bool is_animated = marquee_text_is_needed(
            departure.headsign, headsign_width, ui_text_scale);
        const bool has_changed = (index >= _cached_departure_row_count) ||
            !departure_rows_match(_cached_departure_rows[index], departure) ||
            (_cached_remaining_minutes[index] != remaining_minutes);
        if (!is_animated && !has_changed)
        {
            continue;
        }

        _display->fill_rectangle(
            {0, row_y, _display->width(), departure_text_height}, color_white);
        draw_marquee_text(
            _display, headsign_x, row_y, headsign_width, departure.headsign, color_black,
            ui_text_scale, animation_ms);

        /* The display does not clip text, so restore the neighboring text zones. */
        _display->fill_rectangle(
            {0, row_y, departure_route_area_width, departure_text_height}, color_white);
        _display->fill_rectangle(
            {static_cast<int16_t>(_display->width() - departure_time_area_width), row_y,
             departure_time_area_width, departure_text_height}, color_white);
        _display->draw_text(8, row_y, departure.route_name, color_dark_blue, ui_text_scale);

        char remaining[12];
        (void)snprintf(
            remaining, sizeof(remaining), "%lum",
            static_cast<unsigned long>(remaining_seconds / 60u));
        const int16_t remaining_width = static_cast<int16_t>(
            utf8_character_count(remaining) * 6u * static_cast<size_t>(ui_text_scale));
        _display->draw_text(
            static_cast<int16_t>(_display->width() - remaining_width - 4), row_y,
            remaining, departure.is_realtime ? color_red : color_gray, ui_text_scale);
        _cached_departure_rows[index] = departure;
        _cached_remaining_minutes[index] = remaining_minutes;
    }
    _cached_departure_row_count = displayed_departure_count;
}

void ui_departures_screen_t::render_city_picker(
    const char *const *const city_names,
    const size_t city_count,
    const size_t selected_index,
    const size_t page_index,
    const uint32_t animation_ms,
    const bool refresh_content_only,
    const ui_network_status_t &network_status) const
{
    if ((_display == nullptr) || (city_names == nullptr))
    {
        return;
    }

    const size_t visible_rows = stop_picker_visible_rows();
    const int16_t pagination_y = static_cast<int16_t>(_display->height() - pagination_height);
    if (!refresh_content_only)
    {
        _display->fill_screen(color_white);
        render_header("Wybierz miasto", animation_ms, network_status);
    }
    _display->fill_rectangle(
        {0, picker_first_row_y, _display->width(),
         static_cast<int16_t>(pagination_y - picker_first_row_y)}, color_white);
    if (city_count == 0u)
    {
        _display->draw_text(10, picker_first_row_y, "Brak miast", color_black, ui_text_scale);
    }
    const size_t first_index = page_index * visible_rows;
    const size_t last_index = first_index + visible_rows < city_count ?
        first_index + visible_rows : city_count;
    for (size_t index = first_index; index < last_index; ++index)
    {
        const size_t visible_index = index - first_index;
        const int16_t row_y = static_cast<int16_t>(
            picker_first_row_y + visible_index * static_cast<size_t>(picker_row_height));
        const uint16_t color = index == selected_index ? color_dark_blue : color_black;
        draw_marquee_text(
            _display, 10, row_y, static_cast<int16_t>(_display->width() - 20), city_names[index],
            color, ui_text_scale, animation_ms);
    }

    const size_t page_count = city_count == 0u ? 1u :
        (city_count + visible_rows - 1u) / visible_rows;
    if (refresh_content_only)
    {
        return;
    }
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
        color_black, ui_text_scale);
}

void ui_departures_screen_t::render_stop_picker(
    const fw_stop_list_t &stops,
    const size_t selected_index,
    const size_t page_index,
    const uint32_t animation_ms,
    const bool refresh_content_only,
    const ui_network_status_t &network_status) const
{
    if (_display == nullptr)
    {
        return;
    }

    const int16_t pagination_y = static_cast<int16_t>(_display->height() - pagination_height);
    if (!refresh_content_only)
    {
        _display->fill_screen(color_white);
        render_header("Przystanek", animation_ms, network_status);
    }
    _display->fill_rectangle(
        {0, picker_first_row_y, _display->width(),
         static_cast<int16_t>(pagination_y - picker_first_row_y)}, color_white);
    if (stops.count == 0u)
    {
        _display->draw_text(10, picker_first_row_y, "Brak przystankow", color_black, ui_text_scale);
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
        draw_marquee_text(
            _display, 10, row_y, static_cast<int16_t>(_display->width() - 20), stops.items[index].name,
            color, ui_text_scale, animation_ms);
    }

    const size_t page_count = (stops.count + visible_rows - 1u) / visible_rows;
    if (refresh_content_only)
    {
        return;
    }
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
        color_black, ui_text_scale);
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
    render_header("Szukaj przystanku", 0u, network_status);
    _display->draw_text(
        8, picker_first_row_y, query[0u] == '\0' ? "Wpisz nazwe" : query, color_black,
        ui_text_scale);

    if (has_full_keyboard)
    {
        static const char *const key_labels[] =
        {
            "Q", "W", "E/Ę", "R", "T", "Y", "U", "I", "O/Ó", "P",
            "A/Ą", "S/Ś", "D", "F", "G", "H", "J", "K", "L/Ł",
            "Z/ŹŻ", "X", "C/Ć", "V", "B", "N/Ń", "M", "_"
        };
        static const uint8_t row_key_counts[] = {10u, 9u, 8u};
        const int16_t keyboard_top = static_cast<int16_t>(_display->height() / 3);
        const int16_t row_height = static_cast<int16_t>((_display->height() - keyboard_top - 54) / 3);
        size_t key_offset = 0u;
        for (uint8_t row = 0u; row < 3u; ++row)
        {
            const int16_t key_width = static_cast<int16_t>(_display->width() / row_key_counts[row]);
            for (uint8_t key = 0u; key < row_key_counts[row]; ++key)
            {
                draw_keyboard_key(
                    _display,
                    {static_cast<int16_t>(key * static_cast<size_t>(key_width)),
                     static_cast<int16_t>(keyboard_top + row * row_height), key_width,
                     row_height},
                    key_labels[key_offset + key],
                    color_dark_blue);
            }
            key_offset += row_key_counts[row];
        }
        const int16_t action_y = static_cast<int16_t>(_display->height() - 50);
        const int16_t action_width = static_cast<int16_t>(_display->width() / 3);
        draw_keyboard_key(_display, {0, action_y, action_width, 50}, "WROC", color_gray);
        draw_keyboard_key(_display, {action_width, action_y, action_width, 50}, "USUN", color_gray);
        draw_keyboard_key(
            _display,
            {static_cast<int16_t>(2 * action_width), action_y,
             static_cast<int16_t>(_display->width() - 2 * action_width), 50},
            "SZUKAJ", color_green);
        return;
    }

    static const char *const phone_labels[] =
    {
        "0 _", "2 ABCĄĆ", "3 DEFĘ", "4 GHI", "5 JKLŁ", "6 MNOŃÓ", "7 PQRSŚ", "8 TUV", "9 WXYZŹŻ"
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

size_t ui_departures_screen_t::departure_visible_rows() const
{
    if ((_display == nullptr) ||
        (_display->height() <= (departure_first_row_y + departure_text_height)))
    {
        return 1u;
    }

    const int16_t available_height = static_cast<int16_t>(
        _display->height() - departure_first_row_y - departure_text_height - 1);
    const size_t visible_rows = static_cast<size_t>(available_height / departure_row_height) + 1u;
    const size_t visible_capacity = departure_visible_row_count < fw_departure_capacity ?
        departure_visible_row_count : fw_departure_capacity;
    return visible_rows < visible_capacity ? visible_rows : visible_capacity;
}

int16_t ui_departures_screen_t::stop_picker_first_row_y() const
{
    return picker_first_row_y;
}

int16_t ui_departures_screen_t::stop_picker_row_height() const
{
    return picker_row_height;
}

int16_t ui_departures_screen_t::stop_picker_pagination_height() const
{
    return pagination_height;
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
    render_header(title, 0u, network_status);
    _display->draw_text(10, picker_first_row_y, message, color_black, ui_text_scale);
}
