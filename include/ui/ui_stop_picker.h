#ifndef UI_UI_STOP_PICKER_H
#define UI_UI_STOP_PICKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* H1M4W4R1 */
typedef enum
{
    ui_stop_picker_event_none = 0,
    ui_stop_picker_event_opened,
    ui_stop_picker_event_scrolled,
    ui_stop_picker_event_selected
} ui_stop_picker_event_t;

class ui_stop_picker_t
{
public:
    ui_stop_picker_t();

    ui_stop_picker_event_t update_touch(
        bool is_touched,
        int16_t touch_y,
        uint32_t now_ms,
        size_t item_count,
        int16_t row_height,
        int16_t first_row_y,
        size_t visible_row_count);
    void reset();
    void open();
    bool is_open() const;
    size_t selected_index() const;
    size_t scroll_offset() const;
    void select_next(size_t item_count);
    void select_previous(size_t item_count);

private:
    bool _is_open;
    bool _was_touched;
    bool _ignore_release;
    uint32_t _touch_started_ms;
    int16_t _touch_start_y;
    int16_t _last_touch_y;
    size_t _selected_index;
    size_t _scroll_offset;
};

#endif /* UI_UI_STOP_PICKER_H */
