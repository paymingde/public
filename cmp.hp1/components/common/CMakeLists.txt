file(GLOB_RECURSE source_list *.c)
# idf_component_register(SRCS "esp32_time.c" "led_task.c" "asw_mutex.c" "utility.c" "asw_nvs.c" "asw_fat.c" "asw_ota.c" "${source_list}"
# set(EXTRA_COMPONENT_DIRS  $ENV{COMPONENT_DIRS}/main)

idf_component_register(SRCS "${source_list}"
                    INCLUDE_DIRS "include" 
                                 "../../main"
                    REQUIRES  nvs_flash inv json fatfs cloud esp_http_client esp_https_ota)
                    # REQUIRES mqtt inv wifi common update cloud fatfs esp_http_client esp_https_ota)
                    #  wifi   json vfs  esp-tls  meter
 
