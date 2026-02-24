#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "sdkconfig.h"

static const char *TAG = "WiFiManager";

/* FreeRTOS event group for WiFi events */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

/* Internal state */
static struct {
    bool initialized;
    bool connected;
    char ip_address[16];
    int8_t rssi;
    int retry_num;
    esp_netif_t *sta_netif;
    wifi_status_callback_t status_callback;
} wifi_state = {0};

/* WiFi event handler */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "WiFi disconnected, reason: %d", event->reason);
        
        wifi_state.connected = false;
        wifi_state.ip_address[0] = '\0';
        
        // Notify callback
        if (wifi_state.status_callback) {
            wifi_state.status_callback(false, "");
        }
        
        if (wifi_state.retry_num < CONFIG_WIFI_MAX_RETRY) {
            esp_wifi_connect();
            wifi_state.retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection (%d/%d)", wifi_state.retry_num, CONFIG_WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Store IP address
        snprintf(wifi_state.ip_address, sizeof(wifi_state.ip_address), 
                 IPSTR, IP2STR(&event->ip_info.ip));
        
        wifi_state.retry_num = 0;
        wifi_state.connected = true;
        
        // Notify callback
        if (wifi_state.status_callback) {
            wifi_state.status_callback(true, wifi_state.ip_address);
        }
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Initialize WiFi manager */
esp_err_t wifi_manager_init(wifi_status_callback_t callback)
{
    if (wifi_state.initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }
    
    wifi_state.status_callback = callback;
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_state.sta_netif = esp_netif_create_default_wifi_sta();
    if (wifi_state.sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return ESP_FAIL;
    }
    
    // Convert string IP addresses to binary
    uint32_t static_ip = ipaddr_addr(CONFIG_STATIC_IP_ADDR);
    uint32_t static_gw = ipaddr_addr(CONFIG_STATIC_GATEWAY_ADDR);
    uint32_t static_netmask = ipaddr_addr(CONFIG_STATIC_NETMASK_ADDR);
    
    // Configure static IP with gateway 192.168.1.139
    // This will route all traffic (including 192.168.2.x) through this gateway
    esp_netif_ip_info_t ip_info = {
        .ip = { .addr = static_ip },
        .gw = { .addr = static_gw },
        .netmask = { .addr = static_netmask }
    };
    esp_netif_dhcpc_stop(wifi_state.sta_netif);
    esp_netif_set_ip_info(wifi_state.sta_netif, &ip_info);
    ESP_LOGI(TAG, "Configured static IP: " IPSTR ", Gateway: " IPSTR,
             IP2STR(&ip_info.ip), IP2STR(&ip_info.gw));
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    wifi_state.initialized = true;
    wifi_state.retry_num = 0;
    
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

/* Start WiFi connection */
esp_err_t wifi_manager_start(void)
{
    if (!wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi started, connecting to %s...", CONFIG_WIFI_SSID);
    return ESP_OK;
}

/* Get current WiFi connection status */
bool wifi_manager_is_connected(void)
{
    return wifi_state.connected;
}

/* Get local IP address as string */
const char *wifi_manager_get_ip_address(void)
{
    return wifi_state.ip_address;
}

/* Get WiFi RSSI */
int8_t wifi_manager_get_rssi(void)
{
    if (!wifi_state.connected) {
        return 0;
    }
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        wifi_state.rssi = ap_info.rssi;
        return wifi_state.rssi;
    }
    
    return 0;
}

/* Deinitialize WiFi manager */
void wifi_manager_deinit(void)
{
    if (!wifi_state.initialized) {
        return;
    }
    
    esp_wifi_stop();
    esp_wifi_deinit();
    
    if (wifi_state.sta_netif) {
        esp_netif_destroy(wifi_state.sta_netif);
        wifi_state.sta_netif = NULL;
    }
    
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    
    memset(&wifi_state, 0, sizeof(wifi_state));
    ESP_LOGI(TAG, "WiFi manager deinitialized");
}

/* Wait for WiFi connection (blocking) */
esp_err_t wifi_manager_wait_for_connection(TickType_t timeout)
{
    if (!wifi_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           timeout);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP: %s", CONFIG_WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to AP: %s", CONFIG_WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}