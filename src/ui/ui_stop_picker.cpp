#include "ui/ui_stop_picker.h"

namespace
{
static const uint32_t open_delay_ms = 3000u;
}

ui_stop_picker_t::ui_stop_picker_t() :
    _is_open(false),
    _was_touched(false),
    _ignore_release(false),
    _touch_started_ms(0u),
    _touch_start_x(0),
    _touch_start_y(0),
    _last_touch_y(0),
    _selected_index(0u),
    _page_index(0u)
{
}

ui_stop_picker_event_t ui_stop_picker_t::update_touch(
    const bool is_touched,
    const int16_t touch_x,
    const int16_t touch_y,
    const uint32_t now_ms,
    const size_t item_count,
    const int16_t row_height,
    const int16_t first_row_y,
    const size_t visible_row_count,
    const int16_t screen_width,
    const int16_t screen_height)
{
    if (is_touched)
    {
        _last_touch_y = touch_y;
        if (!_was_touched)
        {
            _touch_start_x = touch_x;
            _touch_start_y = touch_y;
        }
    }

    if (!_is_open && is_touched && !_was_touched)
    {
        _touch_started_ms = now_ms;
    }

    if (!_is_open && is_touched && (now_ms - _touch_started_ms >= open_delay_ms))
    {
        _is_open = true;
        _ignore_release = true;
        _was_touched = is_touched;
        return ui_stop_picker_event_opened;
    }

    if (_is_open && !is_touched && _was_touched)
    {
        if (_ignore_release)
        {
            _ignore_release = false;
        }
        else if ((item_count > 0u) && (row_height > 0) && (visible_row_count > 0u) &&
                 (screen_width > 0) && (screen_height > 0))
        {
            const int16_t pagination_y = static_cast<int16_t>(screen_height - 38);
            const size_t page_count = (item_count + visible_row_count - 1u) / visible_row_count;
            if (_touch_start_y >= pagination_y)
            {
                if ((_touch_start_x < static_cast<int16_t>(screen_width / 3)) && (_page_index > 0u))
                {
                    --_page_index;
                    _was_touched = is_touched;
                    return ui_stop_picker_event_scrolled;
                }
                if ((_touch_start_x >= static_cast<int16_t>((2 * screen_width) / 3)) &&
                    ((_page_index + 1u) < page_count))
                {
                    ++_page_index;
                    _was_touched = is_touched;
                    return ui_stop_picker_event_scrolled;
                }
            }

            if ((_last_touch_y >= first_row_y) &&
                (_last_touch_y < static_cast<int16_t>(
                    first_row_y + static_cast<int16_t>(visible_row_count *
                                                       static_cast<size_t>(row_height)))))
            {
                const size_t selected = _page_index * visible_row_count + static_cast<size_t>(
                    (_last_touch_y - first_row_y) / row_height);
                if (selected < item_count)
                {
                    _selected_index = selected;
                    _is_open = false;
                    _was_touched = is_touched;
                    return ui_stop_picker_event_selected;
                }
            }
        }
    }

    _was_touched = is_touched;
    return ui_stop_picker_event_none;
}

void ui_stop_picker_t::reset()
{
    _is_open = false;
    _ignore_release = false;
    _was_touched = false;
    _touch_start_x = 0;
    _touch_start_y = 0;
    _last_touch_y = 0;
    _selected_index = 0u;
    _page_index = 0u;
}

void ui_stop_picker_t::open()
{
    _is_open = true;
    _was_touched = false;
    _ignore_release = false;
    _selected_index = 0u;
    _page_index = 0u;
}

bool ui_stop_picker_t::is_open() const
{
    return _is_open;
}

size_t ui_stop_picker_t::selected_index() const
{
    return _selected_index;
}

size_t ui_stop_picker_t::page_index() const
{
    return _page_index;
}

void ui_stop_picker_t::select_next(const size_t item_count)
{
    if ((_selected_index + 1u) < item_count)
    {
        ++_selected_index;
    }
}

void ui_stop_picker_t::select_previous(const size_t item_count)
{
    if (_selected_index > 0u)
    {
        --_selected_index;
    }
}
