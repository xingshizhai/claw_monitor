/*
 * OpenClaw WiFi Monitor - ESP32-S2
 * Connects to WiFi and monitors OpenClaw Gateway status
 * Displays on SSD1306 OLED
 * 
 * WiFi: Pi-AP / Wxxncbdmm1804
 * Gateway: 192.168.2.195:18789
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "wifi_manager.h"
#include "display.h"
#include "gateway_checker.h"
#include "sdkconfig.h"

static const char *TAG = "OpenClawMon";

/* Check interval */
#define CHECK_INTERVAL_MS   CONFIG_CHECK_INTERVAL_MS    // Check interval from menuconfig

/* Monitoring state */
typedef struct {
    bool wifi_connected;
    bool gateway_online;
    int8_t rssi;
    char local_ip[16];
    uint32_t last_check;
    uint32_t success_count;
    uint32_t fail_count;
    uint32_t start_time;        // System start time (seconds since boot)
    uint32_t total_checks;      // Total checks performed
    uint32_t page_switch_time;  // Time of last page switch
} monitor_state_t;

static monitor_state_t g_state = {0};

/* WiFi status callback */
static void wifi_status_callback(bool connected, const char *ip_address)
{
    g_state.wifi_connected = connected;
    if (connected && ip_address) {
        strncpy(g_state.local_ip, ip_address, sizeof(g_state.local_ip) - 1);
        g_state.local_ip[sizeof(g_state.local_ip) - 1] = '\0';
        ESP_LOGI(TAG, "WiFi connected, IP: %s", ip_address);
    } else {
        g_state.local_ip[0] = '\0';
        ESP_LOGI(TAG, "WiFi disconnected");
    }
}

/* Monitor task */
static void monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Monitor task started");
    
    #define PAGE_SWITCH_INTERVAL_MS 10000  // Switch page every 10 seconds
    uint32_t last_page_switch = esp_timer_get_time() / 1000;  // milliseconds
    
    while (1) {
        uint32_t current_time_ms = esp_timer_get_time() / 1000;
        uint32_t current_time_seconds = current_time_ms / 1000;
        
        if (wifi_manager_is_connected()) {
            // Update WiFi RSSI
            g_state.rssi = wifi_manager_get_rssi();
            
            // Check gateway status
            uint32_t success_count, fail_count;
            g_state.gateway_online = gateway_checker_check(&success_count, &fail_count);
            
            // Update counts if returned
            if (success_count > 0) {
                g_state.success_count = success_count;
            }
            if (fail_count > 0) {
                g_state.fail_count = fail_count;
            }
            
            g_state.last_check = current_time_seconds;
        } else {
            g_state.wifi_connected = false;
            g_state.local_ip[0] = '\0';
        }
        
        // Update total checks
        g_state.total_checks = g_state.success_count + g_state.fail_count;
        
        // Calculate success rate
        float success_rate = 0.0f;
        if (g_state.total_checks > 0) {
            success_rate = (float)g_state.success_count / g_state.total_checks * 100.0f;
        }
        
        // Check if it's time to switch page
        if (current_time_ms - last_page_switch >= PAGE_SWITCH_INTERVAL_MS) {
            display_next_page();
            last_page_switch = current_time_ms;
            ESP_LOGD(TAG, "Page switched to %d", display_get_current_page());
        }
        
        // Prepare display data
        display_data_t display_data = {
            .wifi_connected = g_state.wifi_connected,
            .gateway_online = g_state.gateway_online,
            .rssi = g_state.rssi,
            .local_ip = g_state.local_ip,
            .success_count = g_state.success_count,
            .fail_count = g_state.fail_count,
            .uptime_seconds = current_time_seconds - g_state.start_time,
            .last_check_time = g_state.last_check,
            .total_checks = g_state.total_checks,
            .success_rate = success_rate
        };
        
        // Update display with current page
        display_draw_current_page(&display_data);
        
        vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
    }
}

/* Main application */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "OpenClaw WiFi Monitor Starting...");
    ESP_LOGI(TAG, "========================================");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);  // Enable debug logs for our component
    
    // Initialize monitoring state
    memset(&g_state, 0, sizeof(g_state));
    g_state.start_time = esp_timer_get_time() / 1000000;
    g_state.page_switch_time = esp_timer_get_time() / 1000;  // milliseconds

    // Initialize display
    ESP_ERROR_CHECK(display_init());
    display_draw_startup();
    ESP_LOGI(TAG, "Display initialized and startup screen shown");

    // Initialize WiFi manager
    ESP_ERROR_CHECK(wifi_manager_init(wifi_status_callback));
    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_LOGI(TAG, "WiFi manager started");

    // Wait for WiFi connection
    esp_err_t wifi_result = wifi_manager_wait_for_connection(portMAX_DELAY);
    if (wifi_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        // Continue anyway, display will show disconnected status
    }

    // Initialize gateway checker with default configuration
    gateway_checker_config_t gateway_config = {
        .host = CONFIG_GATEWAY_HOST,
        .port = CONFIG_GATEWAY_PORT,
        .timeout_ms = 3000
    };
    ESP_ERROR_CHECK(gateway_checker_init(&gateway_config));
    ESP_LOGI(TAG, "Gateway checker initialized");

    // Show connected screen briefly
    display_data_t initial_display_data = {
        .wifi_connected = wifi_manager_is_connected(),
        .gateway_online = false,
        .rssi = wifi_manager_get_rssi(),
        .local_ip = wifi_manager_get_ip_address(),
        .success_count = 0,
        .fail_count = 0,
        .uptime_seconds = 0,
        .last_check_time = 0,
        .total_checks = 0,
        .success_rate = 0.0f
    };
    display_draw_page(DISPLAY_PAGE_MAIN, &initial_display_data);

    // Start monitor task
    xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Monitor task created");

    ESP_LOGI(TAG, "OpenClaw WiFi Monitor started successfully");
}