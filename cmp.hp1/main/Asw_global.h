/**
 * @file Asw_global.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _ASW_GLOBAL_H
#define _ASW_GLOBAL_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <esp_event.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"

#define ASW_FAIL -1
#define ASW_OK 0

#define DEBUG_PRINT_ENABLE 1
#define STATIC_IP_SET_ENABLE 0
#define PARALLEL_HOST_SET_WITH_SN 0 // host采用sn确认测试
#define TRIPHASE_ARM_SUPPORT 0
// #define DEFINE_BATTERY_SAME_ENBALE 1 //电池设置参数一致

// // #define CONFIG_NET_CONNECT_ETHERNET

/* V2.0.0 change  */
typedef enum
{
    Work_Mode_IDLE,    // 0
    Work_Mode_AP_PROV, // 1
    Work_Mode_STA,     // 2
    Work_Mode_LAN,     // 3
    Work_Mode_END      // 4
} Enum_Work_Mode;

// #define CONFIG_ASW_CONNECT_ETHERNET 1
// #define CONFIG_ASW_CONNECT_AP 1

// /************************************************
//  * 启用蓝牙近场协议，关闭wifi近场协议，打开ate http方式，
//  * 否则，启用wiifi近场协议，关闭ate http方式。
//  *********************************************/
// #define CONFIG_ASW_NEARBY_BLE_ENABLE 0

// #define CONFIG_ASW_ATE_HTTP_ENABLE 1
// #define CONFIG_ASW_NEARBY_HTTP_ENABLE 1

typedef enum
{
    TASK_IDLE,
    TASK_QUERY_DATA,
    TASK_TRANS_UP_INV,
    TASK_TRANS_SCAN_INV,
    TASK_QUERY_METER,
    CLEAR_METER_SET,
    SEND_METER_DATA, // SSC
#if TRIPHASE_ARM_SUPPORT
    TASK_ENERGE_METER_CONTROL,
#endif
    RESTART
} SERIAL_STATE;

typedef enum
{
    CMD_REBOOT = 1,
    CMD_RESET,
    CMD_FORMAT,
    CMD_SCAN,
    CMD_PARALLEL,
    CMD_SSC_SET,

    ///// FOR STATIC SET ////
    CMD_STATIC_DISABLE, // 5
    CMD_STATIC_ENABLE,
    CMD_SYNC_REBOOT //設置ap信息到逆變器

} SettingCmdOpCode;

typedef enum
{
    WIFI_JOB_SCAN = 0,
    WIFI_JOB_AP_SCAN,
    WIFI_JOB_AP,
    WIFI_JOB_STA,
    WIFI_JOB_END
} enmWiFiJob;

//-----------------------------

#define ASW_LOGI(...)                   \
    do                                  \
    {                                   \
        if (g_asw_debug_enable > 1)     \
            ESP_LOGI(TAG, __VA_ARGS__); \
    } while (0)
#define ASW_LOGW(...)                   \
    do                                  \
    {                                   \
        if (g_asw_debug_enable > 1)     \
            ESP_LOGW(TAG, __VA_ARGS__); \
    } while (0)
#define ASW_LOGE(...)                   \
    do                                  \
    {                                   \
        if (g_asw_debug_enable > 1)     \
            ESP_LOGE(TAG, __VA_ARGS__); \
    } while (0)

//=======================

//-----------------------------

#if TRIPHASE_ARM_SUPPORT
#define FIRMWARE_REVISION "Energy.Tri.22A10-001R-T" //"22602-001R"
#else
#define FIRMWARE_REVISION "Eng.Sgl.22A10-001R-T" //"22602-001R"
// #define FIRMWARE_REVISION "ASW-LanStick-22602-001R-01U" //"22602-001R"
#endif

#define MONITOR_MODULE_VERSION "ESP32-WROVER-IE"
// #define CGI_VERSION "V1.0"
#define CGI_VERSION "V2.2"
#define PROTOCOL_VERTION_SUPPORT_PARALLEL "V2.1.0"

extern int g_ap_enable;         // define in the led_task.c ap_enable ->g_ap_enable
extern uint8_t g_monitor_state; //[mark] define inv_com.c  monitor_state->g_monitor_state;
extern int g_scan_stop;         //[mark] defined in wifi_sta_server.c

extern uint8_t g_asw_debug_enable; // 0-不打印 1-只打印串口数据 2-全部打印
extern bool g_asw_static_ip_enable;
extern bool g_net_connnect_flag; //网络连接状态   0-连接断开;1-连接正常
extern bool g_ap_connnect_flag;  // AP连接状态   0-连接断开;1-连接正常

extern int g_state_mqtt_connect;        //  //g_state_mqtt_connect-->g_state_mqtt_connect 0--链接正常；-1：链接错误
extern uint8_t g_num_real_inv;          // g_num_real_inv->g_num_real_inv; 逆变器的在线数量
extern int8_t g_state_ethernet_connect; //-1 初始状态; 0: 连接断开 ; 1: 连接成功 ; 2: 获取ip地址

extern bool g_meter_inv_synupload_flag; //逆变器和电表数据上传同步 5min/次 1：逆变器数据上传，等待电表数据上传；
extern int64_t g_uint_meter_data_time;  //逆变器和电表数据上传同步

extern char g_p2p_mode; // p2p

extern xSemaphoreHandle g_semar_psn_ready;
extern xSemaphoreHandle g_semar_wrt_sync_reboot;
extern char firmware_sha_256_print[13]; //打印firmware SHA-256 后六位

// V2.0.0 add
extern EventGroupHandle_t s_net_mode_group; //当前stick网络连接状态
extern uint8_t g_stick_run_mode;            // 0---初始状态;1---sta模式;2--- AP配网模式;3---Lan 模式

// Eng.Stg.Mch-Lanstick
extern int g_meter_sync; // g_meter_sync  -- > g_meter_sync

extern uint8_t g_ssc_enable;            //使能光储充 1-打开 0-关闭
extern uint8_t g_parallel_enable;       //并机模式 1-打开 0-关闭
extern uint8_t g_host_modbus_id;        //并机模式下主机modbus id
extern bool g_safety_is_96_97;          //安规判断，1-支持96、97、80;0--不支持
extern bool g_battery_selfmode_is_same; // 1-一致则发送电池调度信息通过主机或广播发送  0-则发送到对应的逆变器

#endif
