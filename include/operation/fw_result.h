#ifndef OPERATION_FW_RESULT_H
#define OPERATION_FW_RESULT_H

/* H1M4W4R1 */
typedef enum
{
    fw_result_ok = 0,
    fw_result_invalid_argument,
    fw_result_not_found,
    fw_result_network_error,
    fw_result_parse_error,
    fw_result_not_supported,
    fw_result_buffer_too_small,
    fw_result_out_of_memory,
    fw_result_busy
} fw_result_t;

#endif /* OPERATION_FW_RESULT_H */
