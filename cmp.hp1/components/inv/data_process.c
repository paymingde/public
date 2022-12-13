#include "data_process.h"

//--- Eng.Stg.Mch  ---//
ScheduleBat g_schedule_bat = {0}; // LanStick-MulitilInv -
Bat_Monitor_arr_t g_monitor_para = {0};
// Bat_Schdle_arr_t g_schedule_bat = {0};

Batt_data g_bat_data = {0};
// meter_data_t g_meter_data = {0};  //g_inv_meter-->g_meter_data

int g_drm_value = 0;

uint32_t task_cld_msg = 0;
uint32_t task_other_msg = 0;
uint32_t event_group_0 = 0;

// LanStick-MultilInv +-
//  uint32_t task_inv_msg = 0;
uint32_t task_inv_msg_arr[INV_NUM] = {0};
uint16_t g_task_inv_broadcast_msg = 0;
uint32_t task_inv_meter_msg = 0;

char g_p2p_mode = 0; // p2p
///////////////////////////////
void g_value_init(void)
{
    g_task_inv_broadcast_msg = 0;
    for (uint8_t i = 0; i < INV_NUM; i++)
    {
        task_inv_msg_arr[i] = 0;
    }
}
/////////////////////////////
void get_current_date(DATE_STRUCT *date)
{
    struct tm currtime = {0};
    time_t t = time(NULL);
    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;
    currtime.tm_mon += 1;

    date->YEAR = currtime.tm_year;
    date->MONTH = currtime.tm_mon;
    date->DAY = currtime.tm_mday;
    date->HOUR = currtime.tm_hour;
    date->MINUTE = currtime.tm_min;
    date->SECOND = currtime.tm_sec;
    date->WDAY = currtime.tm_wday;
    date->YDAY = currtime.tm_yday;
}

void set_current_date(DATE_STRUCT *date)
{
    struct timeval tv;
    struct tm ptime = {0};
    ptime.tm_year = date->YEAR - 1900;
    ptime.tm_mon = date->MONTH - 1;
    ptime.tm_mday = date->DAY;
    ptime.tm_hour = date->HOUR;
    ptime.tm_min = date->MINUTE;
    ptime.tm_sec = date->SECOND;

    tv.tv_sec = mktime(&ptime);
    settimeofday(&tv, NULL);
}
