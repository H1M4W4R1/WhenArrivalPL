#ifndef UI_UI_DISPLAY_H
#define UI_UI_DISPLAY_H

#include <stdint.h>

/* H1M4W4R1 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} ui_rectangle_t;

class ui_display_t
{
public:
    virtual ~ui_display_t() = default;

    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;
    virtual void fill_screen(uint16_t color) = 0;
    virtual void fill_rectangle(const ui_rectangle_t &rectangle, uint16_t color) = 0;
    virtual void draw_text(
        int16_t x,
        int16_t y,
        const char *text,
        uint16_t color,
        uint8_t text_scale) = 0;
};

#endif /* UI_UI_DISPLAY_H */
