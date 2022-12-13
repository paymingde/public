#include "Asw_global.h"
#include "asw_fat.h"
#include "asw_mqueue.h"
#include "inv_task.h"
#include "led_task.h"
#include "cloud_task.h"

#include "native_ota.h"
#include "asw_mutex.h"

#include "esp32/spiram.h"
#include "sdkconfig.h"

/**********************************/
static const char *TAG = "main";

TaskHandle_t inv_task_handle = NULL;
TaskHandle_t cloud_task_handle = NULL;
TaskHandle_t led_task_handle = NULL;
TaskHandle_t cgi_task_handle = NULL;

BaseType_t xReturned;
//===============================================//

void stick_network_init(void);
void create_wlan_configled(void);

void task_ate_create();
void task_inv_create();
void task_cloud_create();
void task_led_create();
void print_meminfo_task();

uint8_t g_asw_debug_enable = 0;

bool g_net_connnect_flag = false; //网络连接状态   0-连接断开;1-连接正常
bool g_ap_connnect_flag = false;  // AP连接状态   0-连接断开;1-连接正常
bool g_asw_static_ip_enable = 0;
bool g_meter_inv_synupload_flag = 0;
int64_t g_uint_meter_data_time; //逆变器和电表数据上传同步 < 10s

uint8_t g_stick_run_mode; // 0---初始状态;1---sta模式;2--- AP配网模式;3---Lan 模式

uint8_t g_ssc_enable;

uint8_t g_parallel_enable = 99; //并机模式 1-打开 0-关闭
uint8_t g_host_modbus_id = 3;   //并机模式下主机modbus id
bool g_safety_is_96_97 = 0;
bool g_battery_selfmode_is_same=0;  

// char g_p2p_mode = 0 ;
//===================================//
// xSemaphoreHandle server_ready;
xSemaphoreHandle g_semar_psn_ready;
xSemaphoreHandle g_semar_wrt_sync_reboot;
int set_ate(void);
char firmware_sha_256_print[13] = {0}; //打印firmware SHA-256 后六位
//=================================//
void app_main()
{
    boot_rollback_check();

    g_net_connnect_flag = false; //网络连接状态   0-连接断开;1-连接正常
    g_ap_connnect_flag = false;  // AP连接状态   0-连接断开;1-连接正常

    g_meter_inv_synupload_flag = 0;
    g_uint_meter_data_time = 0;
    g_state_ethernet_connect = 0;
    g_ssc_enable = 0;

    g_stick_run_mode = Work_Mode_STA; // 默认为 wifi-sta

    xSemaphoreHandle server_ready = xSemaphoreCreateBinary();
    g_semar_psn_ready = xSemaphoreCreateBinary();
    g_semar_wrt_sync_reboot = xSemaphoreCreateBinary();
    // wifi_sta_para_t sta_para = {0};
    ESP_LOGE(TAG, "total memory: %d bytes,2022-06-14 monitor VER %s,firmware SHA-256:...%s.\n",
             esp_get_free_heap_size(), FIRMWARE_REVISION, firmware_sha_256_print);

    asw_sema_ini(); // Eng.Strg.Mch

    asw_nvs_init();  //
    asw_db_open();   //
    led_gpio_init(); // gpio led init

    /* 均衡磨损部分，分区表分配的大小及性能。16K整数倍  4096*4=16k*/
    asw_fat_mount(); //  storage:/home
    inv_fat_mount(); //  invdata:/inv

    remove("/home/inv.bin");
    remove("/home/update.bin");

    load_global_var(); // Eng.Strg.Mch

    /* v2.0.0 add */
    if (general_query(NVS_WORK_MODE, &g_stick_run_mode) < 0)
    {
        ESP_LOGW(TAG, "work_mode****: %d\n", g_stick_run_mode);
        g_stick_run_mode = Work_Mode_AP_PROV;
        general_add(NVS_WORK_MODE, &g_stick_run_mode);
    }

    /********************/

    net_http_server_start(&server_ready);
    xSemaphoreTake(server_ready, portMAX_DELAY);

    if (get_device_sn() == ASW_FAIL)
    {
        ESP_LOGE(TAG, "psn is empty\n");
        // vTaskSuspend(NULL);
        xSemaphoreTake(g_semar_psn_ready, portMAX_DELAY);
    }

    vSemaphoreDelete(server_ready);

    create_queue_msg();
    sleep(1); // tgl mark for test
    task_inv_create();

    task_cloud_create();

    task_led_create(); //[tgl lan]注意引脚 不要与eth引脚冲突

    // xTaskCreate(print_meminfo_task, "print_meminfo_task", 4096, NULL, 5, NULL); //[ mem]debug
}

//-------------------//
void task_inv_create()
{
    ASW_LOGW("DEBUG,Task_inv_create..");
    xReturned = xTaskCreate(
        inv_task,   /* Function that implements the task. */
        "inv task", /* Text name for the task. */
        15 * 1024,  /* Stack size in bytes. */
        (void *)1,  /* Parameter passed into the task. */
        5,          /* Priority at which the task is created. */
        &inv_task_handle);
    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "create inv task failed.\n");
    }
}

//--------------------//
void task_cloud_create()
{
    xReturned = xTaskCreate(
        cloud_task,   /* Function that implements the task. */
        "cloud task", /* Text name for the task. */
        20 * 1024,    /* Stack size in bytes, 15+5 KB */
        (void *)1,    /* Parameter passed into the task. */
        5,            /* Priority at which the task is created. */
        &cloud_task_handle);
    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "create cloud task failed.\n");
    }
}

//------------------------//
void task_led_create()
{
    xReturned = xTaskCreate(
        led_task,   /* Function that implements the task. */
        "led task", /* Text name for the task. */
        6 * 1024,   /* Stack size in bytes. */
        (void *)1,  /* Parameter passed into the task. */
        5,          /* Priority at which the task is created. */
        &led_task_handle);

    if (xReturned != pdPASS)
    {
        ESP_LOGE(TAG, "led task failed.\n");
    }
}
//-------------------------//
void print_meminfo_task()
{
    static char InfoBuffer[512] = {0};
    while (1)
    {
        ASW_LOGE("mem free has %dKiB.", esp_get_free_heap_size() / 1024);

        ESP_LOGW("Stick Run Mode", " [%d] 0:IDLE,1:AP-Prov,2:WIFI-STA,3:LAN ,lan-status:%d ,cloud-status:%d",
                 g_stick_run_mode, get_eth_connect_status(), g_state_mqtt_connect);

        vTaskList((char *)&InfoBuffer);
        ASW_LOGW("|任务状态|优先级|剩余栈|任务序号\r\n");
        printf("\r\n%s\r\n", InfoBuffer);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
