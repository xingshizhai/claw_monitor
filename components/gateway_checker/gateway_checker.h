#ifndef GATEWAY_CHECKER_H
#define GATEWAY_CHECKER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gateway checker configuration
 */
typedef struct {
    const char *host;           // Gateway host (IP address)
    uint16_t port;              // Gateway port
    uint32_t timeout_ms;        // Connection timeout in milliseconds
} gateway_checker_config_t;

/**
 * @brief Initialize gateway checker
 * 
 * @param config Configuration (NULL for default)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t gateway_checker_init(const gateway_checker_config_t *config);

/**
 * @brief Check gateway status
 * 
 * Attempts to establish a TCP connection to the gateway.
 * 
 * @param success_count Optional pointer to store success count increment
 * @param fail_count Optional pointer to store fail count increment
 * @return true if connection successful, false otherwise
 */
bool gateway_checker_check(uint32_t *success_count, uint32_t *fail_count);

/**
 * @brief Get last check timestamp
 * 
 * @return uint32_t Last check timestamp in seconds
 */
uint32_t gateway_checker_get_last_check_time(void);

/**
 * @brief Deinitialize gateway checker
 */
void gateway_checker_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // GATEWAY_CHECKER_H