#include "systems/sys_platform.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wundef"
#include <SPI.h>
#include <SD.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#pragma GCC diagnostic pop

#include <stdio.h>
#include <string.h>

#include "operation/fw_station_config.h"

namespace
{
static const size_t wifi_ssid_max_length = 33u;
static const size_t wifi_password_max_length = 65u;
static const size_t provider_url_max_length = 128u;
static const int16_t display_font_character_width = 6;
/* ESP-IDF interprets xTaskCreatePinnedToCore stack depth as bytes. HTTP client
 * calls need room for SDK frames in addition to firmware parsing. */
static const uint32_t background_task_stack_bytes = 10240u;
static const uint32_t http_idle_timeout_ms = 60000u;

typedef struct
{
    char wifi_ssid[wifi_ssid_max_length];
    char wifi_password[wifi_password_max_length];
    char provider_url[provider_url_max_length];
} sys_provider_config_t;

static sys_provider_config_t provider_config{};
static StaticJsonDocument<512u> config_document;
static TaskHandle_t background_task_handle = nullptr;
static sys_platform_background_callback_t background_callback = nullptr;
static void *background_context = nullptr;
static volatile bool is_background_task_busy = false;

typedef enum
{
    polish_diacritic_none = 0,
    polish_diacritic_acute,
    polish_diacritic_dot,
    polish_diacritic_ogonek,
    polish_diacritic_stroke
} polish_diacritic_t;

bool decode_utf8_character(const char *&text, uint16_t *const character)
{
    if ((text == nullptr) || (character == nullptr) || (text[0u] == '\0'))
    {
        return false;
    }

    const uint8_t first_byte = static_cast<uint8_t>(text[0u]);
    if (first_byte < 0x80u)
    {
        *character = first_byte;
        ++text;
        return true;
    }

    const uint8_t second_byte = static_cast<uint8_t>(text[1u]);
    if ((first_byte >= 0xc2u) && (first_byte <= 0xdfu) &&
        ((second_byte & 0xc0u) == 0x80u))
    {
        *character = static_cast<uint16_t>(
            ((first_byte & 0x1fu) << 6u) | (second_byte & 0x3fu));
        text += 2;
        return true;
    }

    if ((first_byte >= 0xe0u) && (first_byte <= 0xefu) &&
        ((second_byte & 0xc0u) == 0x80u) && (text[1u] != '\0') && (text[2u] != '\0'))
    {
        const uint8_t third_byte = static_cast<uint8_t>(text[2u]);
        if ((third_byte & 0xc0u) != 0x80u)
        {
            *character = static_cast<uint16_t>('?');
            ++text;
            return true;
        }
        *character = static_cast<uint16_t>(
            ((first_byte & 0x0fu) << 12u) | ((second_byte & 0x3fu) << 6u) |
            (third_byte & 0x3fu));
        text += 3;
        return true;
    }

    *character = static_cast<uint16_t>('?');
    ++text;
    return true;
}

char polish_base_character(const uint16_t character, polish_diacritic_t *const diacritic)
{
    if (diacritic == nullptr)
    {
        return '?';
    }

    *diacritic = polish_diacritic_none;
    switch (character)
    {
        case 0x0104u: *diacritic = polish_diacritic_ogonek; return 'A';
        case 0x0105u: *diacritic = polish_diacritic_ogonek; return 'a';
        case 0x0106u: *diacritic = polish_diacritic_acute; return 'C';
        case 0x0107u: *diacritic = polish_diacritic_acute; return 'c';
        case 0x0118u: *diacritic = polish_diacritic_ogonek; return 'E';
        case 0x0119u: *diacritic = polish_diacritic_ogonek; return 'e';
        case 0x0141u: *diacritic = polish_diacritic_stroke; return 'L';
        case 0x0142u: *diacritic = polish_diacritic_stroke; return 'l';
        case 0x0143u: *diacritic = polish_diacritic_acute; return 'N';
        case 0x0144u: *diacritic = polish_diacritic_acute; return 'n';
        case 0x00d3u: *diacritic = polish_diacritic_acute; return 'O';
        case 0x00f3u: *diacritic = polish_diacritic_acute; return 'o';
        case 0x015au: *diacritic = polish_diacritic_acute; return 'S';
        case 0x015bu: *diacritic = polish_diacritic_acute; return 's';
        case 0x0179u: *diacritic = polish_diacritic_acute; return 'Z';
        case 0x017au: *diacritic = polish_diacritic_acute; return 'z';
        case 0x017bu: *diacritic = polish_diacritic_dot; return 'Z';
        case 0x017cu: *diacritic = polish_diacritic_dot; return 'z';
        default: return character < 0x80u ? static_cast<char>(character) : '?';
    }
}

void draw_polish_diacritic(
    const int16_t x,
    const int16_t y,
    const uint8_t text_scale,
    const uint16_t color,
    const polish_diacritic_t diacritic)
{
    const int16_t pixel_size = static_cast<int16_t>(text_scale);
    switch (diacritic)
    {
        case polish_diacritic_acute:
            M5.Display.fillRect(x + 3 * pixel_size, y, pixel_size, pixel_size, color);
            M5.Display.fillRect(x + 2 * pixel_size, y + pixel_size, pixel_size, pixel_size, color);
            break;
        case polish_diacritic_dot:
            M5.Display.fillRect(x + 2 * pixel_size, y, pixel_size, pixel_size, color);
            break;
        case polish_diacritic_ogonek:
            M5.Display.fillRect(
                x + 3 * pixel_size, y + 7 * pixel_size, pixel_size, pixel_size, color);
            M5.Display.fillRect(
                x + 4 * pixel_size, y + 8 * pixel_size, pixel_size, pixel_size, color);
            break;
        case polish_diacritic_stroke:
            M5.Display.fillRect(
                x + pixel_size, y + 4 * pixel_size, 4 * pixel_size, pixel_size, color);
            break;
        case polish_diacritic_none:
        default:
            break;
    }
}

void draw_utf8_text(
    const int16_t x,
    const int16_t y,
    const char *const text,
    const uint16_t color,
    const uint8_t text_scale)
{
    if ((text == nullptr) || (text_scale == 0u))
    {
        return;
    }

    const char *cursor = text;
    int16_t cursor_x = x;
    uint16_t character = 0u;
    while (decode_utf8_character(cursor, &character))
    {
        polish_diacritic_t diacritic = polish_diacritic_none;
        const char base_character = polish_base_character(character, &diacritic);
        (void)M5.Display.drawChar(static_cast<uint16_t>(base_character), cursor_x, y);
        draw_polish_diacritic(cursor_x, y, text_scale, color, diacritic);
        cursor_x = static_cast<int16_t>(
            cursor_x + display_font_character_width * static_cast<int16_t>(text_scale));
    }
}

void background_task(void *const task_context)
{
    (void)task_context;
    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const sys_platform_background_callback_t callback = background_callback;
        void *const user_context = background_context;
        if (callback != nullptr)
        {
            callback(user_context);
        }
        is_background_task_busy = false;
    }
}

bool copy_config_value(char *const destination, const size_t destination_size, const char *const source)
{
    if ((destination == nullptr) || (destination_size == 0u) || (source == nullptr) ||
        (source[0u] == '\0'))
    {
        return false;
    }

    const int written = snprintf(destination, destination_size, "%s", source);
    return (written >= 0) && (static_cast<size_t>(written) < destination_size);
}

void load_compile_time_config()
{
    (void)copy_config_value(
        provider_config.wifi_ssid, sizeof(provider_config.wifi_ssid), SECRETS_WIFI_SSID);
    (void)copy_config_value(
        provider_config.wifi_password, sizeof(provider_config.wifi_password), SECRETS_WIFI_PASSWORD);
    (void)copy_config_value(
        provider_config.provider_url, sizeof(provider_config.provider_url), SECRETS_PROVIDER_URL);
}

bool initialize_sd_card()
{
    if (!M5.hasSD())
    {
        return false;
    }

    const int8_t clock_pin = M5.getPin(m5::sd_spi_sclk);
    const int8_t miso_pin = M5.getPin(m5::sd_spi_miso);
    const int8_t mosi_pin = M5.getPin(m5::sd_spi_mosi);
    const int8_t chip_select_pin = M5.getPin(m5::sd_spi_cs);
    if ((clock_pin < 0) || (miso_pin < 0) || (mosi_pin < 0) || (chip_select_pin < 0))
    {
        return false;
    }

    SPI.begin(clock_pin, miso_pin, mosi_pin, chip_select_pin);
    return SD.begin(static_cast<uint8_t>(chip_select_pin), SPI, 25000000u);
}

void load_sd_card_config()
{
    if (!initialize_sd_card())
    {
        sys_platform_debug_log("SD: brak karty lub inicjalizacja nieudana");
        return;
    }

    File config_file = SD.open("/config.json", FILE_READ);
    if (!config_file)
    {
        sys_platform_debug_log("SD: brak /config.json");
        return;
    }

    config_document.clear();
    const DeserializationError error = deserializeJson(config_document, config_file);
    config_file.close();
    if (error)
    {
        sys_platform_debug_log("SD: config.json ma bledny JSON");
        return;
    }

    const JsonObject wifi = config_document["wifi"].as<JsonObject>();
    (void)copy_config_value(
        provider_config.wifi_ssid, sizeof(provider_config.wifi_ssid), wifi["ssid"] | "");
    (void)copy_config_value(
        provider_config.wifi_password, sizeof(provider_config.wifi_password), wifi["password"] | "");
    (void)copy_config_value(
        provider_config.provider_url, sizeof(provider_config.provider_url),
        config_document["provider_url"] | "");
}

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
        M5.Display.setTextWrap(false, false);
        draw_utf8_text(x, y, text, color, text_scale);
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
        client.setReuse(false);
        client.setTimeout(static_cast<uint16_t>(http_idle_timeout_ms));

        const int status_code = client.GET();
        if (status_code != HTTP_CODE_OK)
        {
            log_http_failure("get", "response", status_code);
            client.end();
            return fw_result_network_error;
        }

        WiFiClient *const stream = client.getStreamPtr();
        const int response_size = client.getSize();
        const bool has_response_size = response_size >= 0;
        const size_t expected_length = has_response_size ?
            static_cast<size_t>(response_size) : 0u;
        size_t copied = 0u;
        uint32_t last_data_ms = millis();
        while (!has_response_size || (copied < expected_length))
        {
            const size_t available = stream->available();
            if (available == 0u)
            {
                if (!client.connected())
                {
                    if (has_response_size)
                    {
                        log_http_failure("get", "incomplete_response", 0);
                        client.end();
                        return fw_result_network_error;
                    }
                    break;
                }
                if ((millis() - last_data_ms) >= http_idle_timeout_ms)
                {
                    log_http_failure("get", "idle_timeout", 0);
                    client.end();
                    return fw_result_network_error;
                }
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

            size_t requested = available < space ? available : space;
            if (has_response_size)
            {
                const size_t remaining = expected_length - copied;
                requested = requested < remaining ? requested : remaining;
            }
            const size_t read_count = stream->readBytes(
                reinterpret_cast<uint8_t *>(&response_buffer[copied]), requested);
            if (read_count == 0u)
            {
                log_http_failure("get", "stream_read", 0);
                client.end();
                return fw_result_network_error;
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
        void *const user_context,
        const uint32_t idle_timeout_ms) override
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
        client.setReuse(false);
        client.setTimeout(static_cast<uint16_t>(idle_timeout_ms));

        const int status_code = client.GET();
        if (status_code != HTTP_CODE_OK)
        {
            log_http_failure("get_stream", "response", status_code);
            client.end();
            return fw_result_network_error;
        }

        WiFiClient *const stream = client.getStreamPtr();
        const int response_size = client.getSize();
        const bool has_response_size = response_size >= 0;
        const size_t expected_length = has_response_size ?
            static_cast<size_t>(response_size) : 0u;
        size_t received = 0u;
        uint8_t buffer[512u];
        uint32_t last_data_ms = millis();
        while (!has_response_size || (received < expected_length))
        {
            const size_t available = stream->available();
            if (available == 0u)
            {
                if (!client.connected())
                {
                    if (has_response_size)
                    {
                        log_http_failure("get_stream", "incomplete_response", 0);
                        client.end();
                        return fw_result_network_error;
                    }
                    break;
                }
                if ((idle_timeout_ms > 0u) && ((millis() - last_data_ms) >= idle_timeout_ms))
                {
                    log_http_failure("get_stream", "idle_timeout", 0);
                    client.end();
                    return fw_result_network_error;
                }

                delay(1u);
                continue;
            }

            size_t requested = available < sizeof(buffer) ? available : sizeof(buffer);
            if (has_response_size)
            {
                const size_t remaining = expected_length - received;
                requested = requested < remaining ? requested : remaining;
            }
            const size_t read_count = stream->readBytes(buffer, requested);
            if (read_count == 0u)
            {
                log_http_failure("get_stream", "stream_read", 0);
                client.end();
                return fw_result_network_error;
            }

            received += read_count;
            bool transfer_complete = false;
            const fw_result_t callback_result = callback(
                buffer, read_count, user_context, &transfer_complete);
            if (callback_result != fw_result_ok)
            {
                log_http_failure(
                    "get_stream",
                    "callback",
                    static_cast<int>(callback_result));
                client.end();
                return callback_result;
            }

            if (transfer_complete)
            {
                client.end();
                return fw_result_ok;
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

    const BaseType_t create_result = xTaskCreatePinnedToCore(
        background_task,
        "network_worker",
        background_task_stack_bytes,
        nullptr,
        1u,
        &background_task_handle,
        0);
    if (create_result != pdPASS)
    {
        background_task_handle = nullptr;
        sys_platform_debug_log("Zadanie tla: inicjalizacja nieudana");
    }

    provider_config = {};
    load_compile_time_config();
    load_sd_card_config();

    if (provider_config.wifi_ssid[0u] == '\0')
    {
        sys_platform_debug_log("WiFi: brak konfiguracji");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(provider_config.wifi_ssid, provider_config.wifi_password);
    const uint32_t connection_started_ms = millis();
    while ((WiFi.status() != WL_CONNECTED) && ((millis() - connection_started_ms) < 15000u))
    {
        delay(100u);
    }

    is_network_ready = WiFi.status() == WL_CONNECTED;
    if (is_network_ready)
    {
        (void)setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
        tzset();
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

const char *sys_platform_provider_url(void)
{
    return provider_config.provider_url;
}

bool sys_platform_has_full_keyboard(void)
{
#if defined(FW_PLATFORM_TAB5)
    return true;
#else
    return false;
#endif
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

uint32_t sys_platform_local_time_s(void)
{
    const time_t now = time(nullptr);
    if (now <= 0)
    {
        return 0u;
    }

    struct tm local_time{};
    if (localtime_r(&now, &local_time) == nullptr)
    {
        return 0u;
    }

    return static_cast<uint32_t>(local_time.tm_hour) * 3600u +
           static_cast<uint32_t>(local_time.tm_min) * 60u +
           static_cast<uint32_t>(local_time.tm_sec);
}

void *sys_platform_allocate_psram(const size_t size)
{
    if (size == 0u)
    {
        return nullptr;
    }

    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

bool sys_platform_queue_background_task(
    const sys_platform_background_callback_t callback,
    void *const user_context)
{
    if ((callback == nullptr) || (background_task_handle == nullptr) || is_background_task_busy)
    {
        return false;
    }

    background_callback = callback;
    background_context = user_context;
    is_background_task_busy = true;
    (void)xTaskNotifyGive(background_task_handle);
    return true;
}

bool sys_platform_background_task_is_busy(void)
{
    return is_background_task_busy;
}
