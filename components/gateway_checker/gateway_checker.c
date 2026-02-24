#include "gateway_checker.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <errno.h>

static const char *TAG = "GatewayChecker";

/* Default configuration */
#define DEFAULT_GATEWAY_HOST  CONFIG_GATEWAY_HOST
#define DEFAULT_GATEWAY_PORT  CONFIG_GATEWAY_PORT
#define DEFAULT_TIMEOUT_MS    3000

/* Internal state */
static struct {
    bool initialized;
    char host[32];
    uint16_t port;
    uint32_t timeout_ms;
    uint32_t last_check_time;
    uint32_t success_count;
    uint32_t fail_count;
} checker_state = {0};

/* Initialize gateway checker */
esp_err_t gateway_checker_init(const gateway_checker_config_t *config)
{
    if (checker_state.initialized) {
        ESP_LOGW(TAG, "Gateway checker already initialized");
        return ESP_OK;
    }
    
    // Set configuration
    if (config) {
        strncpy(checker_state.host, config->host ? config->host : DEFAULT_GATEWAY_HOST,
                sizeof(checker_state.host) - 1);
        checker_state.host[sizeof(checker_state.host) - 1] = '\0';
        checker_state.port = config->port ? config->port : DEFAULT_GATEWAY_PORT;
        checker_state.timeout_ms = config->timeout_ms ? config->timeout_ms : DEFAULT_TIMEOUT_MS;
    } else {
        // Use defaults
        strcpy(checker_state.host, DEFAULT_GATEWAY_HOST);
        checker_state.port = DEFAULT_GATEWAY_PORT;
        checker_state.timeout_ms = DEFAULT_TIMEOUT_MS;
    }
    
    checker_state.success_count = 0;
    checker_state.fail_count = 0;
    checker_state.last_check_time = 0;
    checker_state.initialized = true;
    
    ESP_LOGI(TAG, "Gateway checker initialized: %s:%d, timeout: %dms",
             checker_state.host, checker_state.port, checker_state.timeout_ms);
    return ESP_OK;
}

/* Check gateway status */
bool gateway_checker_check(uint32_t *success_count, uint32_t *fail_count)
{
    if (!checker_state.initialized) {
        ESP_LOGE(TAG, "Gateway checker not initialized");
        return false;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed: errno=%d", errno);
        return false;
    }
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(checker_state.host);
    if (dest_addr.sin_addr.s_addr == INADDR_NONE) {
        ESP_LOGE(TAG, "Invalid gateway host address: %s", checker_state.host);
        close(sock);
        return false;
    }
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(checker_state.port);
    
    /* Set socket timeout */
    struct timeval tv;
    tv.tv_sec = checker_state.timeout_ms / 1000;
    tv.tv_usec = (checker_state.timeout_ms % 1000) * 1000;
    
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        ESP_LOGW(TAG, "Failed to set socket receive timeout: errno=%d", errno);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        ESP_LOGW(TAG, "Failed to set socket send timeout: errno=%d", errno);
    }
    
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    int connect_errno = errno; // Capture errno immediately after connect
    
    // Only shutdown if connection succeeded
    if (err == 0) {
        shutdown(sock, SHUT_RDWR);
    }
    if (close(sock) < 0) {
        ESP_LOGW(TAG, "Failed to close socket: errno=%d", errno);
    }
    
    // Update timestamp
    checker_state.last_check_time = esp_timer_get_time() / 1000000;
    
    bool success = (err == 0);
    
    if (success) {
        checker_state.success_count++;
        ESP_LOGI(TAG, "Gateway connection successful to %s:%d",
                 checker_state.host, checker_state.port);
    } else {
        checker_state.fail_count++;
        ESP_LOGW(TAG, "Gateway connection failed to %s:%d: errno=%d (%s)",
                 checker_state.host, checker_state.port,
                 connect_errno, strerror(connect_errno));
    }
    
    // Return counts if requested
    if (success_count) {
        *success_count = checker_state.success_count;
    }
    if (fail_count) {
        *fail_count = checker_state.fail_count;
    }
    
    return success;
}

/* Get last check timestamp */
uint32_t gateway_checker_get_last_check_time(void)
{
    return checker_state.last_check_time;
}

/* Deinitialize gateway checker */
void gateway_checker_deinit(void)
{
    if (!checker_state.initialized) {
        return;
    }
    
    memset(&checker_state, 0, sizeof(checker_state));
    ESP_LOGI(TAG, "Gateway checker deinitialized");
}