
file(GLOB_RECURSE source_list *.c)

idf_component_register(SRCS "native_ota.c" "sha256.c" "asw_job_http.c" "wifi_sta_server.c" "${source_list}" 
                    INCLUDE_DIRS "include"
                               "../../main"
                   REQUIRES json cloud inv esp_netif protocol_examples_common esp_eth esp_http_server esp_netif
                              common freertos  esp_wifi protocol app_update)
                              # esp-tls 
                            #   
          
