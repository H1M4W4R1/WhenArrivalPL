#ifndef UI_UI_STOP_SEARCH_H
#define UI_UI_STOP_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

/* H1M4W4R1 */
typedef enum
{
    ui_stop_search_event_none = 0,
    ui_stop_search_event_changed,
    ui_stop_search_event_submitted,
    ui_stop_search_event_cancelled
} ui_stop_search_event_t;

class ui_stop_search_t
{
public:
    ui_stop_search_t();

    ui_stop_search_event_t update_touch(
        bool is_touched,
        int16_t touch_x,
        int16_t touch_y,
        uint32_t now_ms,
        int16_t screen_width,
        int16_t screen_height,
        bool has_full_keyboard);
    const char *query() const;
    void reset();

private:
    ui_stop_search_event_t append_full_key(uint8_t key_index, uint32_t touch_duration_ms);
    ui_stop_search_event_t append_phone_key(uint8_t key_index, uint32_t now_ms);
    ui_stop_search_event_t append_utf8_character(const char *character);
    ui_stop_search_event_t remove_last_character();

    bool _was_touched;
    int16_t _touch_start_x;
    int16_t _touch_start_y;
    uint32_t _touch_start_ms;
    uint8_t _last_phone_key;
    uint8_t _last_phone_character;
    uint32_t _last_phone_key_ms;
    char _query[101u];
    uint8_t _query_length;
};

#endif /* UI_UI_STOP_SEARCH_H */
