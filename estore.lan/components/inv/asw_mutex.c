#include <inttypes.h>
#include "data_process.h"
#include "asw_nvs.h"

const char *TAG = "asw_mutex.c";

#define SEMA_NUM 10
static SemaphoreHandle_t sema[SEMA_NUM] = {NULL};
MonitorPara g_meter_monitor_para = {0};

Bat_arr_t g_bat_arr = {0}; /// Lanstick-MultiInv +

static meter_data_t mutex_inv_meter = {0}; // g_inv_meter--> mutex_inv_meter; [mark]

void asw_sema_ini(void)
{
    for (uint8_t j = 0; j < SEMA_NUM; j++)
    {
        sema[j] = xSemaphoreCreateMutex();
    }
}

/** protect inv_arr cpy***/
void private_memcpy(void *des, void *src, int type_semaIndex, int is_write_nvs)
{
    if (sema[type_semaIndex] == NULL)
    {
        ESP_LOGW("-- private_memcpy warning --", "the sema handle is NULL.");
        return;
    }

    /////////////////////////////////////
    if (xSemaphoreTake(sema[type_semaIndex], (TickType_t)6000 / portTICK_RATE_MS) == pdTRUE)
    {
        switch (type_semaIndex)
        {
        case GLOBAL_INV_ARR:
            memcpy(des, src, sizeof(Inv_arr_t));
            break;

        case PARA_CONFIG:
            memcpy(des, src, sizeof(Bat_Monitor_arr_t));
            if (is_write_nvs == 1)
            {
                int res = general_add(NVS_CONFIG, src);

                ASW_LOGI(" PARA_CONFIG  general_add  NVS_CONFIG  res:%d", res);
            }
            break;

        case METER_CONTROL_CONFIG:
            memcpy(des, src, sizeof(MonitorPara));
            if (is_write_nvs == 1)
            {
                general_add(NVS_METER_CONTROL, src);
            }
            break;

        case PARA_SCHED_BAT:
            memcpy(des, src, sizeof(ScheduleBat));
            // memcpy(des, src, sizeof(Bat_Schdle_arr_t));
            if (is_write_nvs == 1)
            {
                general_add(NVS_SCHED_BAT, src);
            }
            break;

        case GLOBAL_BAT_DATA:
            memcpy(des, src, sizeof(Batt_data));
            break;
        case GLOBAL_METER_DATA:
            memcpy(des, src, sizeof(meter_data_t));
            break;

            // add g_battery_data
        case GLOBAL_BATTERY_DATA:
            memcpy(des, src, sizeof(Bat_arr_t));
            break;

        default:
            break;
        }
        xSemaphoreGive(sema[type_semaIndex]);
    }
    else
    {
        ESP_LOGW("-- private_memcpy warning --", "the sema  take is timeout.");
        return;
    }
}

///////////////////////////////////////////

/** protect global varible in multi-task usage **/
void read_global_var(int type, void *p)
{
    switch (type)
    {
    case PARA_CONFIG:
        private_memcpy(p, &g_monitor_para, type, 0);
        // ((MonitorPara *)p)->adv.meter_mod = 100; /** Acrel only */
        break;

    case PARA_SCHED_BAT:
        private_memcpy(p, &g_schedule_bat, type, 0);
        break;
    case GLOBAL_BAT_DATA:
        private_memcpy(p, &g_bat_data, type, 0);
        break;
    case GLOBAL_METER_DATA:
        private_memcpy(p, &mutex_inv_meter, type, 0);
        break;
        /*             MultilInv Lanstick  */
    case METER_CONTROL_CONFIG:
        private_memcpy(p, &g_meter_monitor_para, type, 0);
        break;

        // add g_battery_data
    case GLOBAL_BATTERY_DATA:
        private_memcpy(p, &g_bat_arr, type, 0);
        break;

    default:
        break;
    }
}
///////////////////////////////////////////
void write_global_var(int type, void *p)
{
    switch (type)
    {
    case PARA_CONFIG:
        private_memcpy(&g_monitor_para, p, type, 0);
        break;

    case PARA_SCHED_BAT:
        private_memcpy(&g_schedule_bat, p, type, 0);
        break;
    case GLOBAL_BAT_DATA:
        private_memcpy(&g_bat_data, p, type, 0);
        break;

    case GLOBAL_METER_DATA:
        private_memcpy(&mutex_inv_meter, p, type, 0);
        break;
        ////////////////////// Lanstick-MultilInv /////////////
    case METER_CONTROL_CONFIG:
        private_memcpy(&g_meter_monitor_para, p, type, 0);
        break;

    case GLOBAL_BATTERY_DATA:
        private_memcpy(&g_bat_arr, p, type, 0);
        break;

    default:
        break;
    }
}
/////////////////////////////////////
void write_global_var_to_nvs(int type, void *p)
{
    switch (type)
    {
    case PARA_CONFIG:
        private_memcpy(&g_monitor_para, p, type, 1);
        break;
    case PARA_SCHED_BAT:
        private_memcpy(&g_schedule_bat, p, type, 1);
        break;
        ////////////////////// Lanstick-MultilInv /////////////
    case METER_CONTROL_CONFIG:
        private_memcpy(&g_meter_monitor_para, p, type, 1);
        break;

    default:
        break;
    }
}
///////////////////////////////////
void load_global_var(void)
{
    int res = 0;
    res = general_query(NVS_METER_CONTROL, &g_meter_monitor_para);

    ESP_LOGI("-load_global_var-", "---------  read  NVS_METER_CONTROL res:%d-------\n", res);

    if (res != 0)
    {
        ESP_LOGI("-load_global_var-", "===========   load defualt to NVS_METER_CONTROL ==========");

        /** 默认设置：如果从flash读取失败*/
        /** Ethernet*/
        // g_monitor_para.eth_para.dhcp = true;
        // g_monitor_para.eth_para.auto_dns = true;

        /** Samples*/
        g_meter_monitor_para.dc_sample = 5;
        g_meter_monitor_para.connect_cnt = 1;

        /** zero export*/
        g_meter_monitor_para.adv.meter_enb = 0;      // enable meter
        g_meter_monitor_para.adv.meter_regulate = 0; // enable zero export
        g_meter_monitor_para.adv.meter_target = 0;   // zero export target

        // g_monitor_para.adv.meter_mod = 100;    // meter module
        g_meter_monitor_para.adv.meter_mod = 2; // meter module.2 is SDM230

        g_meter_monitor_para.is_parallel = 0; //并机模式 1-打开 0-关闭
        g_meter_monitor_para.host_adr = 3;    //并机模式下主机modbus id
    }

    ////////////////  SSC ENABLE ///////////
    if (g_meter_monitor_para.ssc_enable)
        g_ssc_enable = 1;

    g_parallel_enable = g_meter_monitor_para.is_parallel; //并机模式 1-打开 0-关闭
#if !PARALLEL_HOST_SET_WITH_SN
    g_host_modbus_id = g_meter_monitor_para.host_adr; //并机模式下主机modbus id
#endif
    // res = general_query(NVS_CONFIG, &g_monitor_para);
    res = general_query(NVS_CONFIG, &g_monitor_para);
    if (res != 0)
    {
        ESP_LOGI("-load_global_var-", "===========   load defualt to NVS CONFIG ==========");
        for (uint8_t i = 0; i < INV_NUM; i++)
        {
            /** battery schedule*/
            g_monitor_para[i].batmonitor.dc_per = 0; //!<  BEAST type

            g_monitor_para[i].batmonitor.uu1 = 0; //!< running mode
            g_monitor_para[i].batmonitor.up1 = 0; //!< manufactory
            g_monitor_para[i].batmonitor.uu2 = 0; //!< battery model
            g_monitor_para[i].batmonitor.up2 = 0; //!< battery number

            /** inverter operation*/
            g_monitor_para[i].batmonitor.uu3 = 0;    //!< offline for DRM
            g_monitor_para[i].batmonitor.up3 = 0;    //!< active power percent for DRM
            g_monitor_para[i].batmonitor.uu4 = 0;    //!< power on/off
            g_monitor_para[i].batmonitor.up4 = 0xAF; //!< first power on

            /** inverter charging power*/
            g_monitor_para[i].batmonitor.freq_mode = 1; //!< 1.2.3 stop/charging/discharging
            // g_monitor_para[i].batmonitor.fq_t1 = 3000;  //!< charging power
            // g_monitor_para[i].batmonitor.ov_t1 = 3000;  //!< discharging power

            /** DRM setting*/
            g_monitor_para[i].batmonitor.active_mode = 0; // enable drm
            g_monitor_para[i].batmonitor.fq_ld_sp = 0;    //!<  q value;

            /** Discharge/Charge configt **/
            g_monitor_para[i].batmonitor.dchg_max = 10; // enable drm
            g_monitor_para[i].batmonitor.chg_max = 100; // enable drm
        }
    }

    /** 强制设置：不论是否从flash读取成功*/
    g_meter_monitor_para.dc_sample = 5;
    //    g_monitor_para.inv_sample = 5;
    g_meter_monitor_para.connect_cnt = 1;

    res = general_query(NVS_SCHED_BAT, &g_schedule_bat);
    if (res != 0)
    {
        g_schedule_bat.fq_t1 = 3000; //!< charging power
        g_schedule_bat.ov_t1 = 3000; //!< discharging power

        /** 默认设置：如果从flash读取失败*/
        for (uint8_t i = 0; i < 7; i++)
        {
            /*default charging 11:00 - 14:00 0B 00 03 02*/
            g_schedule_bat.daySchedule[i].time_sch[0] = 0x0B000302;
            /*default discharging 17:00 - 24:00  11 00 06 03*/
            g_schedule_bat.daySchedule[i].time_sch[1] = 0x11000603;
            /*default others zero*/
            g_schedule_bat.daySchedule[i].time_sch[2] = 0;
            g_schedule_bat.daySchedule[i].time_sch[3] = 0;
            g_schedule_bat.daySchedule[i].time_sch[4] = 0;
            g_schedule_bat.daySchedule[i].time_sch[5] = 0;
        }
    }

#if PARALLEL_HOST_SET_WITH_SN
    for (uint8_t i = 0; i < INV_NUM; i++)
    {
        if (g_monitor_para[i].modbus_id < 3)
            break;
        if (strcmp(g_monitor_para[i].sn, g_meter_monitor_para.host_psn) == 0)
        {
            g_host_modbus_id = g_monitor_para[i].modbus_id; //并机模式下主机modbus id
        }
    }
#endif

    if (g_parallel_enable == 0)
    {
        uint8_t i = 0, j = 0;
        for (i = 0; i < INV_NUM; i++)
        {
            if (g_monitor_para[i].modbus_id < 3)
            {
                break;
                j = i;
            }

            if (g_monitor_para[i].batmonitor.uu1 != 4 || g_monitor_para[i].batmonitor.dc_per != 1)
            {
                break;
            }
        }

        if (i == j)
            g_battery_selfmode_is_same = 1;
        else
            g_battery_selfmode_is_same = 0;
    }
    else if (g_parallel_enable == 1)
    {

        for (uint8_t i = 0; i < INV_NUM; i++)
        {
            if (g_monitor_para[i].modbus_id < 3)
                break;
            if (g_monitor_para[i].modbus_id == g_host_modbus_id)
            {
                if (g_monitor_para[i].batmonitor.uu1 == 4 && g_monitor_para[i].batmonitor.dc_per == 1)
                {
                    g_battery_selfmode_is_same = 1;
                }
            }
        }
    }

    ESP_LOGI("-- Load Sys Info --", "the g_battery_selfmode_is_same:%d,g_parallel_enable:%d, g_host_modbus_id:%d\n",
             g_battery_selfmode_is_same, g_parallel_enable, g_host_modbus_id);
}

/** special api******************************************************/

int is_com_has_estore(void)
{
    for (uint8_t j = 0; j < INV_NUM; j++)
    {
        int mach_type = inv_arr[j].regInfo.mach_type;
        int status = inv_arr[j].status;
        if (mach_type > 10 && status == 1) // estore && online
        {
            return 1;
        }
    }

    return 0;
}
//--------------------------------//

int is_cgi_has_estore(void)
{
    for (uint8_t j = 0; j < INV_NUM; j++)
    {
        int mach_type = cgi_inv_arr[j].regInfo.mach_type;
        int status = cgi_inv_arr[j].status;
        if (mach_type > 10 && status == 1) // estore && online
        {
            return 1;
        }
    }

    return 0;
}
//--------------------------------//
int is_inv_has_estore(void)
{
    for (uint8_t j = 0; j < INV_NUM; j++)
    {
        // int mach_type = ;
        // int status = ;
        if (inv_arr[j].status && inv_arr[j].regInfo.mach_type > 10) // estore && online
        {
            return 1;
        }
    }

    return 0;
}
//--------------------------------//
//--------------------------//
int8_t is_has_inv_online(void)
{
    for (uint8_t j = 0; j < INV_NUM; j++)
    {

        if (inv_arr[j].status) // estore && online
        {
            return 1;
        }
    }

    return 0;
}
//--------------------------------//

void inv_arr_memcpy(Inv_arr_t *des, Inv_arr_t *src)
{
    private_memcpy(des, src, GLOBAL_INV_ARR, 0);
}

#if 0
/** 逆变器重启后，某些信息需要再次发送*/
void check_estore_become_online(void)
{
    static char last_state = 0;
    char curr_state = is_inv_has_estore();
    if (last_state == 0 && curr_state == 1)
    {
        event_group_0 |= ESTORE_BECOME_ONLINE_MASK; /** 电表状态发送*/
        task_inv_msg |= MSG_DSP_ZV_CLD_INDEX;       /** 仅限储能机,告知逆变器MQTT状态*/
    }
    last_state = curr_state;
}
#endif

/////////////////////////////////////////////
//////////// Lanstick-MultilInv
/** 逆变器重启后，某些信息需要再次发送*/
void check_estore_become_online(void)
{
    static uint8_t last_state[INV_NUM] = {0};
    // char curr_state = is_inv_has_estore();
    uint8_t curr_state = 0;
    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);
    uint8_t mui_status = 0;

    for (uint8_t j = 0; j < INV_NUM; j++)
    {

        if (inv_arr[j].status && inv_arr[j].regInfo.mach_type > 10) // estore && online
        {
            curr_state = 1;
            if (last_state[j] == 0)
            {
                if (monitor_para.is_parallel) //并机模式下，发送到主机
                {

                    //有一个状态发生变化，只需要发送一次状态到主机
                    mui_status = 1;
                }
                else //普通模式下，发送给状态发生变化的机器
                {
                    task_inv_msg_arr[j] |= MSG_DSP_ZV_CLD_INDEX; /** 仅限储能机,告知逆变器MQTT状态*/
                }

                event_group_0 |= ESTORE_BECOME_ONLINE_MASK; /** 电表状态发送*/
            }
        }
        else
        {
            curr_state = 0;
        }
        last_state[j] = curr_state;
    }

    if (monitor_para.is_parallel && mui_status) //并机模式下，发送到主机
    {
        //有一个状态发生变化，只需要发送一次状态到主机
        g_task_inv_broadcast_msg |= MSG_BRDCST_DSP_ZV_CLD_INDEX;
    }
}