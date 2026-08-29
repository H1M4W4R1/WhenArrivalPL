#include "ui/ui_stop_search.h"

#include <string.h>

namespace
{
static const uint32_t phone_key_cycle_ms = 900u;
static const char full_keyboard_keys[] = "QWERTYUIOPASDFGHJKLZXCVBNM ";
static const char *const phone_keys[] =
{
    " ", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ"
};

bool is_inside(
    const int16_t x,
    const int16_t y,
    const int16_t left,
    const int16_t top,
    const int16_t width,
    const int16_t height)
{
    return (x >= left) && (y >= top) && (x < static_cast<int16_t>(left + width)) &&
           (y < static_cast<int16_t>(top + height));
}
}

ui_stop_search_t::ui_stop_search_t() :
    _was_touched(false),
    _touch_start_x(0),
    _touch_start_y(0),
    _last_phone_key(0xffu),
    _last_phone_character(0u),
    _last_phone_key_ms(0u),
    _query{},
    _query_length(0u)
{
}

ui_stop_search_event_t ui_stop_search_t::append_full_key(const uint8_t key_index)
{
    if ((key_index >= (sizeof(full_keyboard_keys) - 1u)) ||
        (_query_length >= (sizeof(_query) - 1u)))
    {
        return ui_stop_search_event_none;
    }

    _query[_query_length++] = full_keyboard_keys[key_index];
    _query[_query_length] = '\0';
    _last_phone_key = 0xffu;
    return ui_stop_search_event_changed;
}

ui_stop_search_event_t ui_stop_search_t::append_phone_key(
    const uint8_t key_index,
    const uint32_t now_ms)
{
    if (key_index >= (sizeof(phone_keys) / sizeof(phone_keys[0])))
    {
        return ui_stop_search_event_none;
    }

    const char *const letters = phone_keys[key_index];
    const uint8_t letter_count = static_cast<uint8_t>(strlen(letters));
    if (letter_count == 0u)
    {
        return ui_stop_search_event_none;
    }

    if ((key_index == _last_phone_key) && (_query_length > 0u) &&
        ((now_ms - _last_phone_key_ms) <= phone_key_cycle_ms))
    {
        _last_phone_character = static_cast<uint8_t>((_last_phone_character + 1u) % letter_count);
        _query[_query_length - 1u] = letters[_last_phone_character];
    }
    else if (_query_length < (sizeof(_query) - 1u))
    {
        _last_phone_key = key_index;
        _last_phone_character = 0u;
        _query[_query_length++] = letters[0u];
        _query[_query_length] = '\0';
    }
    else
    {
        return ui_stop_search_event_none;
    }

    _last_phone_key_ms = now_ms;
    return ui_stop_search_event_changed;
}

ui_stop_search_event_t ui_stop_search_t::remove_last_character()
{
    if (_query_length == 0u)
    {
        return ui_stop_search_event_none;
    }

    --_query_length;
    _query[_query_length] = '\0';
    _last_phone_key = 0xffu;
    return ui_stop_search_event_changed;
}

ui_stop_search_event_t ui_stop_search_t::update_touch(
    const bool is_touched,
    const int16_t touch_x,
    const int16_t touch_y,
    const uint32_t now_ms,
    const int16_t screen_width,
    const int16_t screen_height,
    const bool has_full_keyboard)
{
    if ((screen_width <= 0) || (screen_height <= 0))
    {
        return ui_stop_search_event_none;
    }

    if (is_touched && !_was_touched)
    {
        _touch_start_x = touch_x;
        _touch_start_y = touch_y;
    }

    if (is_touched || !_was_touched)
    {
        _was_touched = is_touched;
        return ui_stop_search_event_none;
    }

    _was_touched = false;
    if (has_full_keyboard)
    {
        static const uint8_t row_key_counts[] = {10u, 9u, 8u};
        const int16_t keyboard_top = static_cast<int16_t>(screen_height / 3);
        const int16_t row_height = static_cast<int16_t>((screen_height - keyboard_top - 54) / 3);
        size_t key_offset = 0u;
        for (uint8_t row = 0u; row < 3u; ++row)
        {
            const int16_t row_y = static_cast<int16_t>(keyboard_top + row * row_height);
            const int16_t key_width = static_cast<int16_t>(screen_width / row_key_counts[row]);
            if (is_inside(
                    _touch_start_x, _touch_start_y, 0, row_y, screen_width, row_height))
            {
                const uint8_t key = static_cast<uint8_t>(_touch_start_x / key_width);
                if (key < row_key_counts[row])
                {
                    return append_full_key(static_cast<uint8_t>(key_offset + key));
                }
            }
            key_offset += row_key_counts[row];
        }

        const int16_t action_y = static_cast<int16_t>(screen_height - 50);
        if (is_inside(_touch_start_x, _touch_start_y, 0, action_y, screen_width / 3, 50))
        {
            return ui_stop_search_event_cancelled;
        }
        if (is_inside(
                _touch_start_x, _touch_start_y, screen_width / 3, action_y, screen_width / 3, 50))
        {
            return remove_last_character();
        }
        if (is_inside(
                _touch_start_x, _touch_start_y, static_cast<int16_t>(2 * (screen_width / 3)), action_y,
                static_cast<int16_t>(screen_width - 2 * (screen_width / 3)), 50) &&
            (_query_length > 0u))
        {
            return ui_stop_search_event_submitted;
        }
        return ui_stop_search_event_none;
    }

    const int16_t keyboard_top = 76;
    const int16_t action_height = 34;
    const int16_t keypad_height = static_cast<int16_t>(screen_height - keyboard_top - action_height);
    const int16_t key_width = static_cast<int16_t>(screen_width / 3);
    const int16_t key_height = static_cast<int16_t>(keypad_height / 3);
    if (is_inside(_touch_start_x, _touch_start_y, 0, keyboard_top, screen_width, keypad_height))
    {
        const uint8_t column = static_cast<uint8_t>(_touch_start_x / key_width);
        const uint8_t row = static_cast<uint8_t>((_touch_start_y - keyboard_top) / key_height);
        if ((column < 3u) && (row < 3u))
        {
            return append_phone_key(static_cast<uint8_t>(row * 3u + column), now_ms);
        }
    }

    const int16_t action_y = static_cast<int16_t>(screen_height - action_height);
    if (is_inside(_touch_start_x, _touch_start_y, 0, action_y, screen_width / 3, action_height))
    {
        return ui_stop_search_event_cancelled;
    }
    if (is_inside(
            _touch_start_x, _touch_start_y, screen_width / 3, action_y, screen_width / 3, action_height))
    {
        return remove_last_character();
    }
    if (is_inside(
            _touch_start_x, _touch_start_y, static_cast<int16_t>(2 * (screen_width / 3)), action_y,
            static_cast<int16_t>(screen_width - 2 * (screen_width / 3)), action_height) &&
        (_query_length > 0u))
    {
        return ui_stop_search_event_submitted;
    }
    return ui_stop_search_event_none;
}

const char *ui_stop_search_t::query() const
{
    return _query;
}

void ui_stop_search_t::reset()
{
    _was_touched = false;
    _touch_start_x = 0;
    _touch_start_y = 0;
    _last_phone_key = 0xffu;
    _last_phone_character = 0u;
    _last_phone_key_ms = 0u;
    _query_length = 0u;
    _query[0u] = '\0';
}
