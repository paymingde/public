file(GLOB_RECURSE source_list *.c)
# set(EXTRA_COMPONENT_DIRS  $ENV{COMPONENT_DIRS}/main)
# idf_component_register(SRCS "asw_mutex.c" "mqueue.c" "estore_com.c" "datadecode.c" "crc16.c" "data_process.c" "asw_modbus.c" "asw_invtask_fun.c" "device_data.c" "inv_task.c" "inv_com.c" "asw_store.c" "${source_list}"
idf_component_register(SRCS  "${source_list}"
                    INCLUDE_DIRS "include"
                               "../../main"
                    REQUIRES common  driver  meter)
                    # nvs_flash meter update driver esp_adc_cal)

