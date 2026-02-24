idf_component_register(SRCS "openclaw_monitor_main.c"
                       PRIV_REQUIRES nvs_flash esp_timer wifi_manager display gateway_checker
                       INCLUDE_DIRS ".")
