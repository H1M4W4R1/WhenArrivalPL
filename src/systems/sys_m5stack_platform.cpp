#include "systems/sys_platform.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wundef"
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>
#pragma GCC diagnostic pop

#include <stdio.h>
#include <string.h>

#include "operation/fw_station_config.h"

namespace
{
const char *http_error_name(const int status_code)
{
    switch (status_code)
    {
        case HTTPC_ERROR_CONNECTION_REFUSED:
            return "connection_refused";
        case HTTPC_ERROR_SEND_HEADER_FAILED:
            return "send_header_failed";
        case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
            return "send_payload_failed";
        case HTTPC_ERROR_NOT_CONNECTED:
            return "not_connected";
        case HTTPC_ERROR_CONNECTION_LOST:
            return "connection_lost";
        case HTTPC_ERROR_NO_STREAM:
            return "no_stream";
        case HTTPC_ERROR_NO_HTTP_SERVER:
            return "no_http_server";
        case HTTPC_ERROR_TOO_LESS_RAM:
            return "too_less_ram";
        case HTTPC_ERROR_ENCODING:
            return "encoding_error";
        case HTTPC_ERROR_STREAM_WRITE:
            return "stream_write_failed";
        case HTTPC_ERROR_READ_TIMEOUT:
            return "read_timeout";
        default:
            return status_code >= 0 ? "http_response" : "unknown";
    }
}

void log_http_failure(
    const char *const operation,
    const char *const stage,
    const int status_code)
{
    char message[112u];
    (void)snprintf(
        message,
        sizeof(message),
        "HTTP %s: stage=%s, code=%d, reason=%s",
        operation,
        stage,
        status_code,
        http_error_name(status_code));
    sys_platform_debug_log(message);
}

class sys_m5stack_display_t final : public ui_display_t
{
public:
    int16_t width() const override
    {
        return static_cast<int16_t>(M5.Display.width());
    }

    int16_t height() const override
    {
        return static_cast<int16_t>(M5.Display.height());
    }

    void fill_screen(const uint16_t color) override
    {
        M5.Display.fillScreen(color);
    }

    void fill_rectangle(const ui_rectangle_t &rectangle, const uint16_t color) override
    {
        M5.Display.fillRect(rectangle.x, rectangle.y, rectangle.width, rectangle.height, color);
    }

    void draw_text(
        const int16_t x,
        const int16_t y,
        const char *const text,
        const uint16_t color,
        const uint8_t text_scale) override
    {
        M5.Display.setTextColor(color);
        M5.Display.setTextSize(text_scale);
        M5.Display.setCursor(x, y);
        M5.Display.print(text);
    }
};

class sys_m5stack_http_client_t final : public driver_http_client_t
{
public:
    fw_result_t get(
        const char *const url,
        char *const response_buffer,
        const size_t response_buffer_size,
        size_t *const response_length) override
    {
        if ((url == nullptr) || (response_buffer == nullptr) ||
            (response_buffer_size < 2u) || (response_length == nullptr))
        {
            return fw_result_invalid_argument;
        }

        HTTPClient client;
        if (!client.begin(url))
        {
            log_http_failure("get", "begin", 0);
            return fw_result_network_error;
        }

        const int status_code = client.GET();
        if (status_code != HTTP_CODE_OK)
        {
            log_http_failure("get", "response", status_code);
            client.end();
            return fw_result_network_error;
        }

        WiFiClient *const stream = client.getStreamPtr();
        size_t copied = 0u;
        uint32_t last_data_ms = millis();
        while (client.connected() && ((millis() - last_data_ms) < 5000u))
        {
            const size_t available = stream->available();
            if (available == 0u)
            {
                delay(1u);
                continue;
            }

            const size_t space = response_buffer_size - copied - 1u;
            if (space == 0u)
            {
                log_http_failure("get", "response_buffer_full", 0);
                client.end();
                return fw_result_buffer_too_small;
            }

            const size_t requested = available < space ? available : space;
            const size_t read_count = stream->readBytes(
                reinterpret_cast<uint8_t *>(&response_buffer[copied]), requested);
            if (read_count == 0u)
            {
                log_http_failure("get", "stream_read", 0);
                break;
            }

            copied += read_count;
            last_data_ms = millis();
        }

        response_buffer[copied] = '\0';
        *response_length = copied;
        client.end();
        return fw_result_ok;
    }

    fw_result_t get_stream(
        const char *const url,
        const driver_http_data_callback_t callback,
        void *const user_context) override
    {
        if ((url == nullptr) || (callback == nullptr))
        {
            return fw_result_invalid_argument;
        }

        HTTPClient client;
        if (!client.begin(url))
        {
            log_http_failure("get_stream", "begin", 0);
            return fw_result_network_error;
        }

        const int status_code = client.GET();
        if (status_code != HTTP_CODE_OK)
        {
            log_http_failure("get_stream", "response", status_code);
            client.end();
            return fw_result_network_error;
        }

        WiFiClient *const stream = client.getStreamPtr();
        uint8_t buffer[512u];
        uint32_t last_data_ms = millis();
        while (client.connected() || (stream->available() > 0u))
        {
            const size_t available = stream->available();
            if (available == 0u)
            {
                if ((millis() - last_data_ms) >= 5000u)
                {
                    log_http_failure("get_stream", "idle_timeout", 0);
                    client.end();
                    return fw_result_network_error;
                }

                delay(1u);
                continue;
            }

            const size_t requested = available < sizeof(buffer) ? available : sizeof(buffer);
            const size_t read_count = stream->readBytes(buffer, requested);
            if (read_count == 0u)
            {
                log_http_failure("get_stream", "stream_read", 0);
                client.end();
                return fw_result_network_error;
            }

            const fw_result_t callback_result = callback(buffer, read_count, user_context);
            if (callback_result != fw_result_ok)
            {
                log_http_failure(
                    "get_stream",
                    "callback",
                    static_cast<int>(callback_result));
                client.end();
                return callback_result;
            }

            last_data_ms = millis();
        }

        client.end();
        return fw_result_ok;
    }
};

sys_m5stack_display_t display;
sys_m5stack_http_client_t http_client;
bool is_network_ready = false;
}

void sys_platform_initialize(void)
{
    Serial.begin(115200u);
    auto configuration = M5.config();
    M5.begin(configuration);
    M5.Display.setRotation(1);

    if (strlen(FW_WIFI_SSID) == 0u)
    {
        sys_platform_debug_log("WiFi: brak konfiguracji");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(FW_WIFI_SSID, FW_WIFI_PASSWORD);
    const uint32_t connection_started_ms = millis();
    while ((WiFi.status() != WL_CONNECTED) && ((millis() - connection_started_ms) < 15000u))
    {
        delay(100u);
    }

    is_network_ready = WiFi.status() == WL_CONNECTED;
    if (is_network_ready)
    {
        configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
        char message[40];
        (void)snprintf(message, sizeof(message), "WiFi: polaczono, RSSI %d dBm", static_cast<int>(WiFi.RSSI()));
        sys_platform_debug_log(message);
    }
    else
    {
        sys_platform_debug_log("WiFi: polaczenie nieudane");
    }
}

ui_display_t *sys_platform_display(void)
{
    return &display;
}

driver_http_client_t *sys_platform_http_client(void)
{
    return &http_client;
}

bool sys_platform_is_touched(void)
{
    M5.update();
    return M5.Touch.getDetail().isPressed();
}

int16_t sys_platform_touch_x(void)
{
    return static_cast<int16_t>(M5.Touch.getDetail().x);
}

int16_t sys_platform_touch_y(void)
{
    return static_cast<int16_t>(M5.Touch.getDetail().y);
}

bool sys_platform_network_is_ready(void)
{
    return is_network_ready && (WiFi.status() == WL_CONNECTED);
}

int16_t sys_platform_network_rssi_dbm(void)
{
    if (!sys_platform_network_is_ready())
    {
        return 0;
    }

    return static_cast<int16_t>(WiFi.RSSI());
}

void sys_platform_debug_log(const char *const message)
{
#if FW_ENABLE_DEBUG
    if (message != nullptr)
    {
        Serial.println(message);
    }
#else
    (void)message;
#endif
}

uint32_t sys_platform_millis(void)
{
    return millis();
}

uint32_t sys_platform_epoch_s(void)
{
    const time_t now = time(nullptr);
    return now > 0 ? static_cast<uint32_t>(now) : 0u;
}
