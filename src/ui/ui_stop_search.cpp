#include "ui/ui_stop_search.h"

#include <string.h>

namespace
{
static const uint32_t phone_key_cycle_ms = 900u;
static const uint32_t full_key_polish_hold_ms = 600u;
static const uint32_t full_key_second_z_hold_ms = 1200u;
static const char full_keyboard_keys[] = "QWERTYUIOPASDFGHJKLZXCVBNM ";
static const char *const phone_keys[] =
{
    " ", "ABCĄĆ", "DEFĘ", "GHI", "JKLŁ", "MNOŃÓ", "PQRSŚ", "TUV", "WXYZŹŻ"
};

size_t utf8_character_byte_length(const char *const character)
{
    if ((character == nullptr) || (character[0u] == '\0'))
    {
        return 0u;
    }

    const uint8_t first_byte = static_cast<uint8_t>(character[0u]);
    if (first_byte < 0x80u)
    {
        return 1u;
    }
    if ((first_byte >= 0xc2u) && (first_byte <= 0xdfu) &&
        ((static_cast<uint8_t>(character[1u]) & 0xc0u) == 0x80u))
    {
        return 2u;
    }
    return 0u;
}

size_t utf8_character_count(const char *const text)
{
    if (text == nullptr)
    {
        return 0u;
    }

    size_t count = 0u;
    size_t index = 0u;
    while (text[index] != '\0')
    {
        const size_t character_length = utf8_character_byte_length(&text[index]);
        if (character_length == 0u)
        {
            return 0u;
        }
        index += character_length;
        ++count;
    }
    return count;
}

const char *utf8_character_at(const char *const text, const uint8_t character_index)
{
    if (text == nullptr)
    {
        return nullptr;
    }

    size_t index = 0u;
    for (uint8_t current_index = 0u; text[index] != '\0'; ++current_index)
    {
        if (current_index == character_index)
        {
            return &text[index];
        }
        const size_t character_length = utf8_character_byte_length(&text[index]);
        if (character_length == 0u)
        {
            return nullptr;
        }
        index += character_length;
    }
    return nullptr;
}

const char *full_keyboard_character(const uint8_t key_index, const uint32_t touch_duration_ms)
{
    if (key_index >= (sizeof(full_keyboard_keys) - 1u))
    {
        return nullptr;
    }

    const char key = full_keyboard_keys[key_index];
    if (touch_duration_ms < full_key_polish_hold_ms)
    {
        return &full_keyboard_keys[key_index];
    }

    switch (key)
    {
        case 'A': return "Ą";
        case 'C': return "Ć";
        case 'E': return "Ę";
        case 'L': return "Ł";
        case 'N': return "Ń";
        case 'O': return "Ó";
        case 'S': return "Ś";
        case 'Z': return touch_duration_ms >= full_key_second_z_hold_ms ? "Ż" : "Ź";
        default: return &full_keyboard_keys[key_index];
    }
}

size_t utf8_last_character_start(const char *const text, const size_t text_length)
{
    if ((text == nullptr) || (text_length == 0u))
    {
        return 0u;
    }

    size_t index = text_length - 1u;
    while ((index > 0u) && ((static_cast<uint8_t>(text[index]) & 0xc0u) == 0x80u))
    {
        --index;
    }
    return index;
}

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
    _touch_start_ms(0u),
    _last_phone_key(0xffu),
    _last_phone_character(0u),
    _last_phone_key_ms(0u),
    _query{},
    _query_length(0u)
{
}

ui_stop_search_event_t ui_stop_search_t::append_full_key(
    const uint8_t key_index,
    const uint32_t touch_duration_ms)
{
    const char *const character = full_keyboard_character(key_index, touch_duration_ms);
    const ui_stop_search_event_t event = append_utf8_character(character);
    _last_phone_key = 0xffu;
    return event;
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
    const uint8_t letter_count = static_cast<uint8_t>(utf8_character_count(letters));
    if (letter_count == 0u)
    {
        return ui_stop_search_event_none;
    }

    if ((key_index == _last_phone_key) && (_query_length > 0u) &&
        ((now_ms - _last_phone_key_ms) <= phone_key_cycle_ms))
    {
        const uint8_t next_character = static_cast<uint8_t>(
            (_last_phone_character + 1u) % letter_count);
        const char *const character = utf8_character_at(letters, next_character);
        const size_t character_length = utf8_character_byte_length(character);
        const size_t previous_start = utf8_last_character_start(_query, _query_length);
        if ((character_length == 0u) ||
            ((previous_start + character_length) >= sizeof(_query)))
        {
            return ui_stop_search_event_none;
        }
        _query_length = static_cast<uint8_t>(previous_start);
        _query[_query_length] = '\0';
        const ui_stop_search_event_t event = append_utf8_character(character);
        if (event != ui_stop_search_event_none)
        {
            _last_phone_character = next_character;
            _last_phone_key_ms = now_ms;
        }
        return event;
    }
    else
    {
        _last_phone_key = key_index;
        _last_phone_character = 0u;
        const ui_stop_search_event_t event = append_utf8_character(letters);
        if (event == ui_stop_search_event_none)
        {
            _last_phone_key = 0xffu;
        }
        else
        {
            _last_phone_key_ms = now_ms;
        }
        return event;
    }
}

ui_stop_search_event_t ui_stop_search_t::append_utf8_character(const char *const character)
{
    const size_t character_length = utf8_character_byte_length(character);
    if ((character_length == 0u) ||
        ((static_cast<size_t>(_query_length) + character_length) >= sizeof(_query)))
    {
        return ui_stop_search_event_none;
    }

    (void)memcpy(&_query[_query_length], character, character_length);
    _query_length = static_cast<uint8_t>(_query_length + character_length);
    _query[_query_length] = '\0';
    return ui_stop_search_event_changed;
}

ui_stop_search_event_t ui_stop_search_t::remove_last_character()
{
    if (_query_length == 0u)
    {
        return ui_stop_search_event_none;
    }

    _query_length = static_cast<uint8_t>(utf8_last_character_start(_query, _query_length));
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
        _touch_start_ms = now_ms;
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
                    return append_full_key(
                        static_cast<uint8_t>(key_offset + key), now_ms - _touch_start_ms);
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
    _touch_start_ms = 0u;
    _last_phone_key = 0xffu;
    _last_phone_character = 0u;
    _last_phone_key_ms = 0u;
    _query_length = 0u;
    _query[0u] = '\0';
}
