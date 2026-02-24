#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "esp_err.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display page type
 */
typedef enum {
    DISPLAY_PAGE_MAIN,      // Main status page
    DISPLAY_PAGE_DETAILS,   // Detailed OpenClaw info
    DISPLAY_PAGE_SYSTEM,    // System information
    DISPLAY_PAGE_COUNT      // Total number of pages
} display_page_t;

/**
 * @brief Display data structure
 */
typedef struct {
    bool wifi_connected;
    bool gateway_online;
    int8_t rssi;
    const char *local_ip;
    uint32_t success_count;
    uint32_t fail_count;
    uint32_t uptime_seconds;    // System uptime
    uint32_t last_check_time;   // Last gateway check timestamp
    uint32_t total_checks;      // Total checks performed
    float success_rate;         // Success rate percentage
} display_data_t;

/**
 * @brief Initialize OLED display
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t display_init(void);

/**
 * @brief Get U8G2 handle
 * 
 * @return u8g2_t* Pointer to U8G2 instance
 */
u8g2_t *display_get_u8g2(void);

/**
 * @brief Draw startup screen
 */
void display_draw_startup(void);

/**
 * @brief Set current display page
 * 
 * @param page Page to display
 */
void display_set_page(display_page_t page);

/**
 * @brief Get current display page
 * 
 * @return display_page_t Current page
 */
display_page_t display_get_current_page(void);

/**
 * @brief Switch to next page
 */
void display_next_page(void);

/**
 * @brief Draw current page with data
 * 
 * @param data Display data to show
 */
void display_draw_current_page(const display_data_t *data);

/**
 * @brief Draw specific page with data
 * 
 * @param page Page to draw
 * @param data Display data to show
 */
void display_draw_page(display_page_t page, const display_data_t *data);

/**
 * @brief Clear display buffer
 */
void display_clear(void);

/**
 * @brief Send buffer to display
 */
void display_send_buffer(void);

/**
 * @brief Deinitialize display
 */
void display_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H