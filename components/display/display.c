#include "display.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "Display";

/* I2C Configuration */
#define I2C_MASTER_NUM      (CONFIG_I2C_MASTER_NUM == 0 ? I2C_NUM_0 : I2C_NUM_1)
#define I2C_MASTER_SDA_IO   CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_SCL_IO   CONFIG_I2C_MASTER_SCL
#define I2C_FREQ_HZ         CONFIG_I2C_FREQ_HZ
#define I2C_TIMEOUT_MS      1000
#define I2C_DISPLAY_ADDRESS CONFIG_I2C_DISPLAY_ADDRESS
#define X_OFFSET            2

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t display_dev_handle = NULL;
static u8g2_t u8g2;
static bool initialized = false;
static display_page_t current_page = DISPLAY_PAGE_MAIN;

/* U8G2 callbacks */
static uint8_t u8x8_byte_i2c_cb(u8x8_t *u8x8, uint8_t msg,
                                uint8_t arg_int, void *arg_ptr)
{
    static uint8_t buffer[132];
    static uint8_t buf_idx;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT: {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = I2C_DISPLAY_ADDRESS,
            .scl_speed_hz = I2C_FREQ_HZ,
            .scl_wait_us = 0,
            .flags.disable_ack_check = false,
        };
        i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &display_dev_handle);
        break;
    }
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;
    case U8X8_MSG_BYTE_SEND:
        for (size_t i = 0; i < arg_int; ++i) {
            if (buf_idx < sizeof(buffer)) {
                buffer[buf_idx++] = *((uint8_t*)arg_ptr + i);
            }
        }
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        if (buf_idx > 0 && display_dev_handle != NULL) {
            ESP_LOGD(TAG, "I2C transmit %d bytes", buf_idx);
            i2c_master_transmit(display_dev_handle, buffer, buf_idx, I2C_TIMEOUT_MS);
            ESP_LOGD(TAG, "I2C transmit done");
        }
        break;
    default:
        return 0;
    }
    return 1;
}

static uint8_t u8x8_gpio_delay_cb(u8x8_t *u8x8, uint8_t msg,
                                  uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(arg_int * 10);
        break;
    case U8X8_MSG_DELAY_100NANO:
        __asm__ __volatile__("nop");
        break;
    default:
        return 0;
    }
    return 1;
}

/* Page drawing functions */
static void draw_main_page(const display_data_t *data);
static void draw_details_page(const display_data_t *data);
static void draw_system_page(const display_data_t *data);

/* Format signal bars */
static void format_signal_bars(int8_t rssi, char *out)
{
    if (rssi >= -50) {
        strcpy(out, "||||");
    } else if (rssi >= -60) {
        strcpy(out, "|||");
    } else if (rssi >= -70) {
        strcpy(out, "||");
    } else if (rssi >= -80) {
        strcpy(out, "|");
    } else {
        strcpy(out, "");
    }
}

/* Initialize OLED display */
esp_err_t display_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Display already initialized");
        return ESP_OK;
    }
    
    // Initialize I2C bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize U8G2
    u8g2_Setup_ssd1306_i2c_128x64_vcomh0_f(
        &u8g2, U8G2_R0,
        u8x8_byte_i2c_cb,
        u8x8_gpio_delay_cb
    );
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    
    initialized = true;
    ESP_LOGI(TAG, "Display initialized");
    return ESP_OK;
}

/* Get U8G2 handle */
u8g2_t *display_get_u8g2(void)
{
    return initialized ? &u8g2 : NULL;
}

/* Draw startup screen */
void display_draw_startup(void)
{
    if (!initialized) {
        return;
    }
    
    u8g2_ClearBuffer(&u8g2);
    
    u8g2_SetFont(&u8g2, u8g2_font_10x20_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 10, 30, "OpenClaw");
    
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 20, 45, "WiFi Monitor");
    u8g2_DrawStr(&u8g2, X_OFFSET + 25, 58, "Connecting...");
    
    u8g2_SendBuffer(&u8g2);
    ESP_LOGI(TAG, "Startup screen drawn");
}

/* Draw main page (original main screen) */
static void draw_main_page(const display_data_t *data)
{
    if (!initialized || !data) {
        return;
    }
    
    char buf[32];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_7x13B_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 15, 12, "OpenClaw Mon");
    u8g2_DrawHLine(&u8g2, X_OFFSET + 0, 14, 128 - X_OFFSET);
    
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    
    // WiFi Status
    if (data->wifi_connected) {
        format_signal_bars(data->rssi, buf);
        u8g2_DrawStr(&u8g2, X_OFFSET + 0, 26, "WiFi:");
        u8g2_DrawStr(&u8g2, X_OFFSET + 35, 26, buf);
        snprintf(buf, sizeof(buf), "%d dBm", data->rssi);
        u8g2_DrawStr(&u8g2, X_OFFSET + 65, 26, buf);
    } else {
        u8g2_DrawStr(&u8g2, X_OFFSET + 0, 26, "WiFi: Disconnected");
    }
    
    // Gateway Status
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 38, "Gateway:");
    if (data->gateway_online) {
        u8g2_DrawStr(&u8g2, X_OFFSET + 55, 38, "[ONLINE]");
    } else {
        u8g2_DrawStr(&u8g2, X_OFFSET + 55, 38, "[OFFLINE]");
    }
    
    // Local IP
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 50, "IP:");
    if (data->local_ip) {
        u8g2_DrawStr(&u8g2, X_OFFSET + 22, 50, data->local_ip);
    } else {
        u8g2_DrawStr(&u8g2, X_OFFSET + 22, 50, "N/A");
    }
    
    // Stats
    snprintf(buf, sizeof(buf), "OK:%" PRIu32 " ERR:%" PRIu32, 
             data->success_count, data->fail_count);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 62, buf);
    
    u8g2_SendBuffer(&u8g2);
}

/* Clear display buffer */
void display_clear(void)
{
    if (initialized) {
        u8g2_ClearBuffer(&u8g2);
    }
}

/* Draw details page with extended OpenClaw information */
static void draw_details_page(const display_data_t *data)
{
    if (!initialized || !data) {
        return;
    }
    
    char buf[32];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_7x13B_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 15, 12, "OpenClaw Details");
    u8g2_DrawHLine(&u8g2, X_OFFSET + 0, 14, 128 - X_OFFSET);
    
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    
    // Success Rate
    if (data->total_checks > 0) {
        snprintf(buf, sizeof(buf), "Success: %.1f%%", data->success_rate);
    } else {
        snprintf(buf, sizeof(buf), "Success: N/A");
    }
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 26, buf);
    
    // Total Checks
    snprintf(buf, sizeof(buf), "Checks: %" PRIu32, data->total_checks);
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 38, buf);
    
    // Last Check
    if (data->last_check_time > 0) {
        uint32_t seconds_ago = data->uptime_seconds - data->last_check_time;
        if (seconds_ago < 60) {
            snprintf(buf, sizeof(buf), "Last: %" PRIu32 "s ago", seconds_ago);
        } else {
            snprintf(buf, sizeof(buf), "Last: %" PRIu32 "m ago", seconds_ago / 60);
        }
    } else {
        snprintf(buf, sizeof(buf), "Last: Never");
    }
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 50, buf);
    
    // Uptime
    uint32_t uptime_minutes = data->uptime_seconds / 60;
    uint32_t uptime_hours = uptime_minutes / 60;
    if (uptime_hours > 0) {
        snprintf(buf, sizeof(buf), "Uptime: %" PRIu32 "h%" PRIu32 "m", 
                 uptime_hours, uptime_minutes % 60);
    } else {
        snprintf(buf, sizeof(buf), "Uptime: %" PRIu32 "m", uptime_minutes);
    }
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 62, buf);
    
    u8g2_SendBuffer(&u8g2);
}

/* Draw system information page */
static void draw_system_page(const display_data_t *data)
{
    if (!initialized || !data) {
        return;
    }
    
    char buf[32];
    
    u8g2_ClearBuffer(&u8g2);
    
    // Title
    u8g2_SetFont(&u8g2, u8g2_font_7x13B_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 15, 12, "System Info");
    u8g2_DrawHLine(&u8g2, X_OFFSET + 0, 14, 128 - X_OFFSET);
    
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    
    // WiFi Signal
    if (data->wifi_connected) {
        snprintf(buf, sizeof(buf), "WiFi: %d dBm", data->rssi);
        u8g2_DrawStr(&u8g2, X_OFFSET + 0, 26, buf);
        
        // Signal quality
        if (data->rssi >= -50) {
            strcpy(buf, "Quality: Excellent");
        } else if (data->rssi >= -60) {
            strcpy(buf, "Quality: Good");
        } else if (data->rssi >= -70) {
            strcpy(buf, "Quality: Fair");
        } else if (data->rssi >= -80) {
            strcpy(buf, "Quality: Poor");
        } else {
            strcpy(buf, "Quality: Very Poor");
        }
        u8g2_DrawStr(&u8g2, X_OFFSET + 0, 38, buf);
    } else {
        u8g2_DrawStr(&u8g2, X_OFFSET + 0, 26, "WiFi: Disconnected");
    }
    
    // Gateway Status
    u8g2_DrawStr(&u8g2, X_OFFSET + 0, 50, "Gateway:");
    if (data->gateway_online) {
        u8g2_DrawStr(&u8g2, X_OFFSET + 55, 50, "[ONLINE]");
    } else {
        u8g2_DrawStr(&u8g2, X_OFFSET + 55, 50, "[OFFLINE]");
    }
    
    // Page indicator
    snprintf(buf, sizeof(buf), "Page %d/%d", current_page + 1, DISPLAY_PAGE_COUNT);
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(&u8g2, X_OFFSET + 85, 62, buf);
    
    u8g2_SendBuffer(&u8g2);
}

/* Page management functions */

/* Set current display page */
void display_set_page(display_page_t page)
{
    if (page < DISPLAY_PAGE_COUNT) {
        current_page = page;
        ESP_LOGI(TAG, "Page set to %d", page);
    }
}

/* Get current display page */
display_page_t display_get_current_page(void)
{
    return current_page;
}

/* Switch to next page */
void display_next_page(void)
{
    current_page = (current_page + 1) % DISPLAY_PAGE_COUNT;
    ESP_LOGI(TAG, "Switched to page %d", current_page);
}

/* Draw current page with data */
void display_draw_current_page(const display_data_t *data)
{
    display_draw_page(current_page, data);
}

/* Draw specific page with data */
void display_draw_page(display_page_t page, const display_data_t *data)
{
    if (!initialized || !data) {
        return;
    }
    
    switch (page) {
        case DISPLAY_PAGE_MAIN:
            draw_main_page(data);
            break;
        case DISPLAY_PAGE_DETAILS:
            draw_details_page(data);
            break;
        case DISPLAY_PAGE_SYSTEM:
            draw_system_page(data);
            break;
        default:
            draw_main_page(data);
            break;
    }
}

/* Backward compatibility: draw main screen */
void display_draw_main(const display_data_t *data)
{
    draw_main_page(data);
}

/* Send buffer to display */
void display_send_buffer(void)
{
    if (initialized) {
        u8g2_SendBuffer(&u8g2);
    }
}

/* Deinitialize display */
void display_deinit(void)
{
    if (!initialized) {
        return;
    }
    
    u8g2_SetPowerSave(&u8g2, 1);
    
    if (display_dev_handle) {
        i2c_master_bus_rm_device(display_dev_handle);
        display_dev_handle = NULL;
    }
    
    if (i2c_bus_handle) {
        i2c_del_master_bus(i2c_bus_handle);
        i2c_bus_handle = NULL;
    }
    
    initialized = false;
    ESP_LOGI(TAG, "Display deinitialized");
}