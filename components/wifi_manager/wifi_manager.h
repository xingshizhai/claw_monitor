#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi connection status callback type
 */
typedef void (*wifi_status_callback_t)(bool connected, const char *ip_address);

/**
 * @brief Initialize WiFi manager
 * 
 * @param callback Callback function for WiFi status updates
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(wifi_status_callback_t callback);

/**
 * @brief Start WiFi connection
 * 
 * Starts connecting to the configured WiFi network.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Get current WiFi connection status
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get local IP address as string
 * 
 * @return const char* IP address string or empty string if not connected
 */
const char *wifi_manager_get_ip_address(void);

/**
 * @brief Get WiFi RSSI
 * 
 * @return int8_t RSSI in dBm, 0 if not connected
 */
int8_t wifi_manager_get_rssi(void);

/**
 * @brief Wait for WiFi connection (blocking)
 * 
 * @param timeout Timeout in ticks (use portMAX_DELAY for infinite)
 * @return esp_err_t ESP_OK if connected, error otherwise
 */
esp_err_t wifi_manager_wait_for_connection(TickType_t timeout);

/**
 * @brief Deinitialize WiFi manager
 */
void wifi_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H