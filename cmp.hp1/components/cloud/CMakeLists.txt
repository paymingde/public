file(GLOB_RECURSE source_list *.c)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/nvs_flash/include)
                         
idf_component_register(SRCS "estore_cld.c" "cloud_task.c" "update.c" "save_inv_data.c" "asw_mqtt.c" "app_mqtt.c" "${source_list}"
                        INCLUDE_DIRS "include"
                                      "../../main"
                        REQUIRES wifi common  mqtt meter)
                
