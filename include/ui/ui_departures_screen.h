#ifndef UI_UI_DEPARTURES_SCREEN_H
#define UI_UI_DEPARTURES_SCREEN_H

#include "operation/fw_transit_types.h"
#include "ui/ui_display.h"

/* H1M4W4R1 */
typedef struct
{
    bool is_connected;
    int16_t rssi_dbm;
} ui_network_status_t;

class ui_departures_screen_t
{
public:
    explicit ui_departures_screen_t(ui_display_t *display);

    void render_loading(
        const char *station_name,
        const ui_network_status_t &network_status) const;
    void render_departures(
        const char *station_name,
        const fw_departure_list_t &departures,
        uint32_t now_epoch_s,
        const ui_network_status_t &network_status) const;
    void render_city_picker(
        const char *const *city_names,
        size_t city_count,
        size_t selected_index,
        const ui_network_status_t &network_status) const;
    void render_stop_picker(
        const fw_stop_list_t &stops,
        size_t selected_index,
        size_t scroll_offset,
        const ui_network_status_t &network_status) const;
    size_t stop_picker_visible_rows() const;
    void render_message(
        const char *title,
        const char *message,
        const ui_network_status_t &network_status) const;

private:
    void render_header(
        const char *title,
        const ui_network_status_t &network_status) const;

    ui_display_t *_display;
};

#endif /* UI_UI_DEPARTURES_SCREEN_H */
