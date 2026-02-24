idf_component_register(SRCS "wifi_manager.c"
                       PRIV_REQUIRES esp_wifi esp_netif esp_netif_stack lwip
                       INCLUDE_DIRS ".")