#ifndef DRIVERS_DRIVER_HTTP_CLIENT_H
#define DRIVERS_DRIVER_HTTP_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "operation/fw_result.h"

/* H1M4W4R1 */
typedef fw_result_t (*driver_http_data_callback_t)(
    const uint8_t *data,
    size_t data_length,
    void *user_context,
    bool *transfer_complete);

class driver_http_client_t
{
public:
    virtual ~driver_http_client_t() = default;

    virtual fw_result_t get(
        const char *url,
        char *response_buffer,
        size_t response_buffer_size,
        size_t *response_length) = 0;
    virtual fw_result_t get_stream(
        const char *url,
        driver_http_data_callback_t callback,
        void *user_context,
        uint32_t idle_timeout_ms = 5000u) = 0;
};

#endif /* DRIVERS_DRIVER_HTTP_CLIENT_H */
