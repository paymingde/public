#include "asw_invtask_fun.h"
#include "asw_mutex.h"
#include "inv_com.h"

#define DAY_CHARG_SCH_NUM 6 /// mark +

#define INV_BROADCAST_DELAYMS 500 * 1000 // 200ms
#define INV_BROADCAST_NUM 3              //广播次数

// static DayDetailSch dayDetailSch[DAY_CHARG_SCH_NUM]; // current day schedule of charge/discharge power into/from battery

static DayDetailSch Bat_DaySchdle_arr_t[DAY_CHARG_SCH_NUM];

static int16_t cur_week_day = 0; // current day

static uint16_t last_mdata[7] = {0}; // for meter status frame change detect

static DATE_STRUCT mS_curr_date;
uint16_t u16_lChargeValue = 0; // 0-stop;1-;2-charg;3-discharge

//*************************************//
static const char *TAG = "asw_invtask_fun.c";

// int8_t inv_set_para(uint32_t msg_index);
int8_t inv_set_para(int inv_index, uint32_t msg_index);

void handleMsg_setChangefun(void *p);
//-----------------------------------------------//
// Eng.Stg.Mch-Lanstick 20220907 +
#define REG_41153_IDLE 0
#define REG_41153_METER_PAC 1
#define REG_41153_PWR_LIMIT 2

#define BATTERY_CHARGING_STOP 1 // stop charging/discharging
#define BATTERY_CHARGING 2      // charging
#define BATTERY_DISCHARGING 3   // discharging

// static uint8_t g_is_in_schedule[INV_NUM] = {0};
// static uint8_t g_41153_state[INV_NUM] = {0};
//----------------------------------------/
void handleMsg_getSafty_fun()
{
    DATE_STRUCT curr_date;
    uint8_t i;
    get_current_date(&curr_date);
    ASW_LOGI("------current data:%04d-%02d-%02d %02d:%02d:%02d\n",
             curr_date.YEAR, curr_date.MONTH, curr_date.DAY, curr_date.HOUR, curr_date.MINUTE, curr_date.SECOND);
    /*update the charging/discharging/stop schedule*/
    uint16_t uHour = 0;
    uint16_t uMinutes = 0;
    cur_week_day = curr_date.WDAY;  /** 产生数据，给全局变量*/
    ScheduleBat schedule_bat = {0}; /** 产生数据，给全局变量*/
    // Bat_Schdle_arr_t schedule_bat = {0}; /** 产生数据，给全局变量*/

    read_global_var(PARA_SCHED_BAT, &schedule_bat);
    ASW_LOGI("curr: weekday var1 var2 %d %u %u", cur_week_day, schedule_bat.daySchedule[cur_week_day].time_sch[0],
             schedule_bat.daySchedule[cur_week_day].time_sch[1]);
    for (i = 0; i < 6; i++)
    {
        uHour = (schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0xFF000000) >> 24;
        uMinutes = (schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0x00FF0000) >> 16;
        Bat_DaySchdle_arr_t[i].minutes_inday = uHour * 60 + uMinutes;
        Bat_DaySchdle_arr_t[i].duration = ((schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0x0000FF00) >> 8);
        Bat_DaySchdle_arr_t[i].charge_flag = schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0x000000FF;
        ASW_LOGI("------dayDetailSch:%d, minutes_inday=%d, duration=%d, charge_flag=%d\n", i, Bat_DaySchdle_arr_t[i].minutes_inday,
                 Bat_DaySchdle_arr_t[i].duration, Bat_DaySchdle_arr_t[i].charge_flag);
    }
}
//-----------------------------------------------------//
//---Eng.Stg.Mch-lanstick 20220907 +-
void handleMsg_setChange_fun(int mIndex, MonitorBat_info *p_monitor_para)
{
    uint16_t data[2] = {0};
    uint16_t data_mode = 0;
    //-------------------------//
    ScheduleBat schedule_bat = {0}; /** 产生数据，给全局变量*/
    read_global_var(PARA_SCHED_BAT, &schedule_bat);

    Bat_arr_t mBattery_arr_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &mBattery_arr_data);
    //-------------------------//

    static uint16_t sche_last_data[INV_NUM][2] = {0};
    static uint16_t sche_last_data_mode[INV_NUM] = {0};

    data[0] = p_monitor_para->batmonitor.freq_mode;

    ASW_LOGI("charge mode: %d\n", p_monitor_para->batmonitor.freq_mode);
    switch (p_monitor_para->batmonitor.freq_mode)
    {
    case 1: // stop

        data_mode = 2; /** 自发自用模式*/

        break;
    case 2: // charging -
        // data[1] = (p_monitor_para->batmonitor.fq_t1 * -1);
        data[1] = (schedule_bat.fq_t1 * -1);
        data_mode = 4; /** 自定义模式*/

        break;
    case 3: // discharging +
        data[1] = schedule_bat.ov_t1;
        data_mode = 4; /** 自定义模式*/

        break;
    default:
        break;
    }
    ///////////////////////////////////////////////////////////////////////////
    if (g_asw_debug_enable > 1)
    {
        printf("\n===================== handleMsg_setChange_fun [%d]=================\n", p_monitor_para->batmonitor.freq_mode);
        for (uint8_t i = 0; i < 2; i++)
        {
            printf("* %d:%04X  *", data[i], data[i]);
        }
    }

    // printf("\n\n");
    // printf("data_mode:%d,sche_last_data_mode[%d]:%d \n",data_mode,mIndex,sche_last_data_mode[mIndex]);
    // printf("data0[%d]data1[%d],sche_last_data[%d]data0[%d]data1[%d]\n",data[0],data[1],mIndex,sche_last_data[mIndex][0],sche_last_data[mIndex][1]);

    // printf("\n\n");

    //////////////////////////////////////////////////////////////////////

    if (data_mode > 0 && data_mode != sche_last_data_mode[mIndex])
    {
        if (g_asw_debug_enable == 1)
        {
            char time[30] = {0};
            get_time(time, sizeof(time));
            printf("\n [%s] run mode set ----   send data[%d] to inv %d reg addr:%d\n", time, data_mode, mBattery_arr_data[mIndex].modbus_id, 0x044F);
        }

        if (modbus_write_inv(mBattery_arr_data[mIndex].modbus_id, 0x044F, 1, &data_mode) == ASW_OK) /** 储能机运行模式：41104*/
        {
            sche_last_data_mode[mIndex] = data_mode;
        }
        else
        {
            ESP_LOGE("-- Write inv cmd Error--", "modebus failed write data to 41104.");
        }
    }

    if (p_monitor_para->batmonitor.freq_mode == 1) //停止
    {
        if (data[0] > 0 && data[0] != sche_last_data[mIndex][0]) //
        {
            if (g_asw_debug_enable == 1)
            {
                char time[30] = {0};
                get_time(time, sizeof(time));
                printf("\n [%s]  stop charge set ----   send data[%d] to inv %d reg addr:%d\n", time, data[0], mBattery_arr_data[mIndex].modbus_id, INV_REG_ADDR_CHARGE);
            }
            if (modbus_write_inv(mBattery_arr_data[mIndex].modbus_id, INV_REG_ADDR_CHARGE, 1, data) == ASW_OK) /** 储能机充放电标志：41152*/
            {
                sche_last_data[mIndex][0] = data[0];
            }
            else
            {
                ESP_LOGE("-- Write inv cmd Error--", "modebus failed write data to 41153.");
            }
        }
    }
    else // 冲放电
    {
#if 0 ////新版的去掉发送电表功率到逆变器 20221027
        if ((data[0] > 0 && memcmp(data, sche_last_data[mIndex], sizeof(data)) != 0) || g_41153_state[mIndex] != REG_41153_PWR_LIMIT)
#endif
        if ((data[0] > 0 && memcmp(data, sche_last_data[mIndex], sizeof(data)) != 0))
        {
            if (g_asw_debug_enable == 1)
            {
                char time[30] = {0};
                get_time(time, sizeof(time));
                printf("\n [%s] charge set ----   send data[%d,%d] to inv %d reg addr:%d\n", time, data[0], (int16_t)data[1], mBattery_arr_data[mIndex].modbus_id, INV_REG_ADDR_CHARGE);
            }
            /** 储能机充放电标志：41152*/
            if (modbus_write_inv(mBattery_arr_data[mIndex].modbus_id, INV_REG_ADDR_CHARGE, 2, data) == ASW_OK)
            {
                memcpy(sche_last_data[mIndex], data, sizeof(data));
                // g_41153_state[mIndex] = REG_41153_PWR_LIMIT;  //新版的去掉发送电表功率到逆变器 20221027
            }
            else
            {
                ESP_LOGE("-- Write inv cmd Error-- ", "  inv modbusID:%d modebus failed write data to 41152+2.", mBattery_arr_data[mIndex].modbus_id);
            }
        }
    }
}

//------------------------------------------------------//
//并机模式下，只发送主机，，普通模式下广播发送 电表及放逆流参数 有变化发送
void handleMsg_pwrActive_fun(MonitorPara *p_monitor_para, meter_data_t *p_inv_meter)
{
    Bat_Monitor_arr_t bat_monitor_para = {0};

    read_global_var(PARA_CONFIG, &bat_monitor_para);

    static uint8_t need_send = 1; /** 开机后, 发一次*/
    uint16_t m16data[7] = {0};    // data form active power

    int ret;
    uint8_t m_invModbus_Id = 0;

    m16data[0] = 0x000A; // status of the meter
    m16data[1] = 0x000A; // flag of the function

    m16data[2] = ((p_monitor_para->adv.meter_target & 0xFFFF0000) >> 16);
    m16data[3] = ((p_monitor_para->adv.meter_target & 0x0000FFFF));
    m16data[4] = ((p_inv_meter->invdata.pac & 0xFFFF0000) >> 16);
    m16data[5] = (p_inv_meter->invdata.pac & 0x0000FFFF);
    m16data[6] = p_monitor_para->adv.meter_regulate;
    /** 某些设置如果变化, 发一次*/
    if (memcmp(m16data, last_mdata, 2 * sizeof(uint16_t)) != 0 || memcmp(&m16data[6], &last_mdata[6], sizeof(uint16_t)) != 0)
    {
        memcpy(last_mdata, m16data, sizeof(m16data));
        need_send = 1;
    }

    if (event_group_0 & ESTORE_BECOME_ONLINE_MASK) /** 发现逆变器重启，也发一次*/
    {
        event_group_0 &= ~ESTORE_BECOME_ONLINE_MASK;
        need_send = 1;
    }

    if (need_send == 1)
    {
        Bat_arr_t m_battery_data = {0};
        read_global_var(GLOBAL_BATTERY_DATA, &m_battery_data);

#if !PARALLEL_HOST_SET_WITH_SN

        MonitorPara monitor_para = {0};
        read_global_var(METER_CONTROL_CONFIG, &monitor_para);

        if (monitor_para.is_parallel == 0)

#else
        if (g_parallel_enable == 0)
#endif
        {
            for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
            {
                ret = modbus_write_inv(0, INV_REG_ADDR_METER, 7, m16data); /** 电表防逆流: 41108-41114*/

                usleep(INV_BROADCAST_DELAYMS); // 260ms
            }

            ASW_LOGI("Send meter status to broadcast  + regulate: 000A000A %d\n", p_monitor_para->adv.meter_regulate);
        }
        else
        {
#if PARALLEL_HOST_SET_WITH_SN
            ret = modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_METER, 7, m16data); /** 电表防逆流: 41108-41114*/

            ASW_LOGI("*** cast inv host modebus_id:%d + regulate: 000A000A %d\n",
                     g_host_modbus_id, p_monitor_para->adv.meter_regulate);

#else
            ret = modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_METER, 7, m16data); /** 电表防逆流: 41108-41114*/

            ASW_LOGI("*** cast inv host modebus_id:%d + regulate: 000A000A %d\n",
                     monitor_para.host_adr, p_monitor_para->adv.meter_regulate);
#endif
        }
        if (ret == 0)
            need_send = 0;
    }

/** **********************************/
/** 非自定义模式下，发电表功率*/
#if 0 //新版的去掉发送电表功率到逆变器 20221027
    memset(m16data, 0, sizeof(m16data));

    Bat_arr_t m_batterys_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_batterys_data);

    for (uint8_t m = 0; m < g_num_real_inv; m++)
    {
        ////////////////////////////////////////////////////////
        ASW_LOGI("inv%d,sn:%s,run-mode store-type: %d %d,g_is_in_schedule[%d]\n", m, bat_monitor_para[m].sn, bat_monitor_para[m].batmonitor.uu1,
                 bat_monitor_para[m].batmonitor.dc_per, g_is_in_schedule[m]);

        // Eng.Stg.Mch-Lanstick 20220907 +-
        /** 非自定义模式下，发电表功率*/


        if (bat_monitor_para[m].batmonitor.uu1 != 4 || (bat_monitor_para[m].batmonitor.uu1 == 4 && g_is_in_schedule[m] == 0)) /** 非自定义模式*/
        {
            m16data[0] = (int16_t)((int)p_inv_meter->invdata.pac);
            event_group_0 |= LOW_TIMEOUT_FOR_MODBUS;
            ////////////////////////////////////////////////////////
            m_invModbus_Id = m_batterys_data[m].modbus_id;
            if (m_invModbus_Id >= 3 && m_invModbus_Id < 255)
            {
                ret = modbus_write_inv(m_invModbus_Id, 0x0480, 1, m16data); /** 41153: 电表实际功率*/
                ASW_LOGI("send meter to inv:%d pac: %d\n", m_invModbus_Id, (int16_t)m16data[0]);
            }
            else
            {
                ESP_LOGE("--LanStick Multi Eng.Stg Erro--", "handleMsg_pwrActive_funB: faied to get the inv modebus ID");
            }
            g_41153_state[m] = REG_41153_METER_PAC;
        }
    }
#endif
}

//------------------------------------------------------------//
void handleMsg_setAdv_fun()
{
    // int ret;
    uint16_t data[2] = {0};
    data[0] = 0x0005;
    data[1] = 0x0005;
// modbus_write_inv(3, INV_REG_ADDR_METER, 2, data); /** 电表状态: 41108*/

/////////////////////////////////////////////////////////
#if !PARALLEL_HOST_SET_WITH_SN

    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    if (monitor_para.is_parallel == 0)

#else
    if (g_parallel_enable == 0)
#endif
    {
        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_ADDR_METER, 2, data); /** 电表状态: 41108*/

            usleep(INV_BROADCAST_DELAYMS); // 260ms
        }

        ASW_LOGI("Send meter broadcast to inv, status command:%04X %04X \r\n", data[0], data[1]);
    }
    else
    {
#if PARALLEL_HOST_SET_WITH_SN
        modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_METER, 2, data); /** 电表状态: 41108*/

        ASW_LOGI("*** cast inv host modebus_id:%d meter status :data[0]=%d,data[1]=%d\r\n",
                 g_host_modbus_id, data[0], data[1]);

#else
        modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_METER, 2, data); /** 电表状态: 41108*/

        ASW_LOGI("*** cast inv host modebus_id:%d meter status :data[0]=%d,data[1]=%d\r\n",
                 monitor_para.host_adr, data[0], data[1]);
#endif
    }

    ///////////////////////////////////////////////////////

    memcpy(last_mdata, data, 2 * sizeof(uint16_t));
    // ASW_LOGI("Send meter status command:%04X %04X \r\n", data[0], data[1]);
}
//------------------------------------------------------------//

//-----------------------------------------------------//
// void handleMsg_setBattery_fun(int mIndex, MonitorPara *p_monitor_para)
void handleMsg_setBattery_fun(int mIndex, MonitorBat_info *p_monitor_para)
{
    int ret;
    uint16_t data[6] = {0};

    Bat_arr_t m_bats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_bats_data);

    data[0] = p_monitor_para->batmonitor.dc_per;                                          // estore mach type
    data[1] = p_monitor_para->batmonitor.uu1;                                             // running model
    data[2] = p_monitor_para->batmonitor.up1;                                             // battery factory
    data[3] = p_monitor_para->batmonitor.uu2;                                             // battery model
    data[4] = p_monitor_para->batmonitor.up2;                                             // battery quantity
    ret = modbus_write_inv(m_bats_data[mIndex].modbus_id, INV_REG_ADDR_BATTERY, 5, data); /** 储能机种类别选择*/
    if (ret == ASW_OK)
    {
        ASW_LOGI("**********com task->set battery index:data[0]=%d,data[1]=%d,"
                 "data[2]=%d,data[3]=%d,data[4]=%d\r\n",
                 data[0], data[1], data[2], data[3], data[4]);
    }

    memset(data, 0, sizeof(data));
    data[0] = p_monitor_para->batmonitor.chg_max * 100; // estore mach type
    data[1] = p_monitor_para->batmonitor.dchg_max * 100;
    ret = modbus_write_inv(m_bats_data[mIndex].modbus_id, INV_REG_ADDR_BATTERY_CHAGE_CONFIG, 2, data); /** 储能机种类别选择*/
    if (ret == ASW_OK)
    {
        ASW_LOGI("**********com task->set battery dischage/chage info index:data[0]=%d,data[1]=%d", data[0], data[1]);
    }
}
///////////////////////////////////////////////////////
void handleMsg_broadCast_setBattery_fun(MonitorBat_info *p_monitor_para)
{
    int ret;
    uint16_t data[6] = {0};
    // MonitorPara *p_monitor_para = (MonitorPara)p1;
    data[0] = p_monitor_para->batmonitor.dc_per; // estore mach type
    data[1] = p_monitor_para->batmonitor.uu1;    // running model
    data[2] = p_monitor_para->batmonitor.up1;    // battery factory
    data[3] = p_monitor_para->batmonitor.uu2;    // battery model
    data[4] = p_monitor_para->batmonitor.up2;    // battery quantity
#if !PARALLEL_HOST_SET_WITH_SN

    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    ASW_LOGI("\n===========handleMsg_broadCast_setBattery_fun[%d]=== \n", monitor_para.is_parallel);

    if (monitor_para.is_parallel == 0)

#else

    if (g_parallel_enable == 0)

#endif
    {

        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_ADDR_BATTERY, 5, data); /** 储能机种类别选择*/
            usleep(INV_BROADCAST_DELAYMS);                      // 260ms
        }

        ASW_LOGI("*****broadcast *****com task->set battery index:data[0]=%d,data[1]=%d,"
                 "data[2]=%d,data[3]=%d,data[4]=%d\r\n",
                 data[0], data[1], data[2], data[3], data[4]);

        memset(data, 0, sizeof(data));
        data[0] = p_monitor_para->batmonitor.chg_max * 100; // estore mach type
        data[1] = p_monitor_para->batmonitor.dchg_max * 100;

        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_ADDR_BATTERY_CHAGE_CONFIG, 2, data); /** 储能机种类别选择*/

            usleep(INV_BROADCAST_DELAYMS); // 260ms
        }

        ASW_LOGI("*********broad cast *com task->set battery dischage/chage info index:data[0]=%d,data[1]=%d", data[0], data[1]);
    }
    else
    {

#if PARALLEL_HOST_SET_WITH_SN
        modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_BATTERY, 5, data); /** 储能机种类别选择*/
        usleep(200);
        modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_BATTERY_CHAGE_CONFIG, 2, data); /** 储能机种类别选择*/

        ASW_LOGI("********* cast inv host modbus_id:%d *com task->set battery dischage/chage info index:data[0]=%d,data[1]=%d",
                 g_host_modbus_id, data[0], data[1]);

#else
        modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_BATTERY, 5, data); /** 储能机种类别选择*/
        usleep(200);
        modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_BATTERY_CHAGE_CONFIG, 2, data); /** 储能机种类别选择*/

        ASW_LOGI("********* cast inv host modbus_id:%d *com task->set battery dischage/chage info index:data[0]=%d,data[1]=%d",
                 monitor_para.host_adr, data[0], data[1]);
#endif
    }
}

//------------------------------------------------------//
void handleMsg_broadCast_setRunMode_fun(MonitorBat_info *p_monitor_para)
{
    int ret;
    uint16_t data[6] = {0};
    // MonitorPara *p_monitor_para = (MonitorPara)p1;

    data[0] = p_monitor_para->batmonitor.dc_per; // beast type
    data[1] = p_monitor_para->batmonitor.uu1;    // running model
#if !PARALLEL_HOST_SET_WITH_SN

    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    if (monitor_para.is_parallel == 0)

#else
    if (g_parallel_enable == 0)
#endif
    {

        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_ADDR_BATTERY, 2, data); /** 储能机种类别选择*/

            usleep(INV_BROADCAST_DELAYMS); // 260ms
        }

        ASW_LOGI("***broad cast com task->set run mode index:data[0]=%d,data[1]=%d\r\n",
                 data[0], data[1]);
    }
    else
    {
#if PARALLEL_HOST_SET_WITH_SN
        modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_BATTERY, 2, data); /** 储能机种类别选择*/
        ASW_LOGI("*** cast inv host modebus_id:%d com task->set run mode index:data[0]=%d,data[1]=%d\r\n",
                 g_host_modbus_id, data[0], data[1]);

#else

        modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_BATTERY, 2, data); /** 储能机种类别选择*/
        ASW_LOGI("*** cast inv host modebus_id:%d com task->set run mode index:data[0]=%d,data[1]=%d\r\n",
                 monitor_para.host_adr, data[0], data[1]);
#endif
    }
}
//------------------------------------------------------//
void handleMsg_broadCast_setPower_fun(MonitorBat_info *p_monitor_para)
{
    int ret;
    // MonitorPara *p_monitor_para = (MonitorPara)p1;
    uint16_t midata = p_monitor_para->batmonitor.uu4;
#if !PARALLEL_HOST_SET_WITH_SN
    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);
    if (monitor_para.is_parallel == 0)
#else
    if (g_parallel_enable == 0)

#endif
    {
        // if (is_inv_has_estore() == 1) /** 储能机*/
        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_ADDR_POWER_ON, 1, &midata); /** 储能机开关: 41102*/
            usleep(INV_BROADCAST_DELAYMS);                          // 260ms
        }

        if (p_monitor_para->batmonitor.uu4 == 2)
            midata = 1;
        else
            midata = 0;

        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_DRM_0N_OFF, 1, &midata); /** 启停： 40201*/
            usleep(INV_BROADCAST_DELAYMS);
        }

        ASW_LOGI("****** Broadcast ****com task->set power on/off index:address=%d,data=%d\r\n",
                 INV_REG_ADDR_POWER_ON, p_monitor_para->batmonitor.uu4);
    }
    else
    {
#if PARALLEL_HOST_SET_WITH_SN
        // modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_BATTERY, 2, data); /** 储能机种类别选择*/
        modbus_write_inv(g_host_modbus_id, INV_REG_ADDR_POWER_ON, 1, &midata); /** 储能机开关: 41102*/
        usleep(200);

        if (p_monitor_para->batmonitor.uu4 == 2)
            midata = 1;
        else
            midata = 0;
        modbus_write_inv(g_host_modbus_id, INV_REG_DRM_0N_OFF, 1, &midata); /** 启停： 40201*/
        ASW_LOGI("****** inv host modebus_id:%d ****com task->set power on/off index:address=%d,data=%d\r\n",
                 g_host_modbus_id, INV_REG_ADDR_POWER_ON, p_monitor_para->batmonitor.uu4);

#else
        modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_POWER_ON, 1, &midata); /** 储能机开关: 41102*/
        usleep(200);

        if (p_monitor_para->batmonitor.uu4 == 2)
            midata = 1;
        else
            midata = 0;
        modbus_write_inv(monitor_para.host_adr, INV_REG_DRM_0N_OFF, 1, &midata); /** 启停： 40201*/
        ASW_LOGI("****** inv host modebus_id:%d ****com task->set power on/off index:address=%d,data=%d\r\n",
                 monitor_para.host_adr, INV_REG_ADDR_POWER_ON, p_monitor_para->batmonitor.uu4);
#endif
    }
}
//---------------------------------------------------//

// 判断逻辑： 储能电池是玉泽（电池厂商代码是6）&& 逆变器通讯协议版本是2.1.2
void handleMsg_wrt_staInfo2Inv_fun()
{
    int ret;
    uint8_t buf[64] = {0};
    wifi_sta_para_t sta_para = {0};

    general_query(NVS_STA_PARA, &sta_para);
    if (strlen((char *)sta_para.ssid) == 0 || strlen((char *)sta_para.password) == 0)
    {
        ESP_LOGW("-- Wrt SSID & PSW  WARN --", "the SSID or  PASSWORD is NULL.");
        xSemaphoreGive(g_semar_wrt_sync_reboot);
        return;
    }
    Bat_Monitor_arr_t monitor_para = {0};
    read_global_var(PARA_CONFIG, &monitor_para);

    char m_com_protocol_inv[13];

    memcpy(buf, sta_para.ssid, sizeof(sta_para.ssid));
    memcpy(buf + 32, sta_para.password, sizeof(sta_para.password));

    for (uint8_t i = 0; i < g_num_real_inv; i++)
    {

        //////////////////////////////////////////////////////////
        if (g_asw_debug_enable > 1)
        {
            printf("---- test sn ----,the monitor[%d] sn:%s, the inv[%d] sn:%s",
                   i, monitor_para[i].sn, i, inv_arr[i].regInfo.sn);
        }
        /////////////////////////////////////////////////////////
        memcpy(m_com_protocol_inv, &(inv_arr[i].regInfo.protocol_ver), 13);

        ASW_LOGI("inv  %d protocol ver %s , bat_type:%d \n",
                 i, m_com_protocol_inv, monitor_para[i].batmonitor.dc_per);
        int8_t res = 0;
        if (monitor_para[i].batmonitor.dc_per == 6 && strncmp(m_com_protocol_inv, "V2.1.2", sizeof("V2.1.2")) == 0)
        {
            res = modbus_write_inv_bit8(inv_arr[i].regInfo.modbus_id, INV_REG_ADDR_SSID_PSSWD, 32, buf);
        }

        ASW_LOGI("-- write sta info to inv res:%d\n", res);

        usleep(INV_BROADCAST_DELAYMS);
    }

    ASW_LOGI("****** ***com write inv ssid:%s  & password:%s \r\n",
             sta_para.ssid, sta_para.password);
    xSemaphoreGive(g_semar_wrt_sync_reboot);
}

//--------------------------------------------------------//
//写并联模式到逆变器
void handleMsg_wrt_parallelInfo2Inv_fun()
{
    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    /////////////  Lanstick-MutltivInv //////////////////

    uint16_t data = monitor_para.is_parallel;

    for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
    {
        modbus_write_inv(0, INV_REG_ADDR_SET_PARALLEL, 1, &data); /** 并联模式: 41118*/
        usleep(INV_BROADCAST_DELAYMS);                            // 260ms
        usleep(INV_BROADCAST_DELAYMS);                            // 260ms
    }

    // if (monitor_para.is_parallel)
    int res = modbus_write_inv(monitor_para.host_adr, INV_REG_ADDR_SET_HOST, 1, &data); /** 主从模式: 41117*/

    ASW_LOGI("\n====== handleMsg_wrt_parallelInfo2Inv_fun res[%d]=====\n ", res);
}

//----------------------------------------//
//----------------------------------------//
void handleMsg_broadCast_handleMsg_dspZvCld_fun()
{
    uint16_t data = 0;
    // int ret=0;

    wifi_sta_para_t sta_para = {0};
    //////////////////////
    Bat_arr_t m_arrbats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_arrbats_data);

#if !PARALLEL_HOST_SET_WITH_SN

    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);
#endif

    general_query(NVS_STA_PARA, &sta_para);
    if (strlen((char *)sta_para.ssid) == 0 && strlen((char *)sta_para.password) == 0)
    {
        data = 0x00AF;
    }
    else
    {
        if (g_state_mqtt_connect == 0) // Eng.Stg.Mch-Lanstick 20220907 +-
        {
            data = 0x000A;
        }
        else
        {
            data = 0x0005;
        }
    }
#if !PARALLEL_HOST_SET_WITH_SN
    if (monitor_para.is_parallel == 0) //普通模式下广播发送
#else
    if (g_parallel_enable == 0)                                                //普通模式下广播发送

#endif
    {
        for (uint8_t i = 0; i < INV_BROADCAST_NUM; i++)
        {
            modbus_write_inv(0, INV_REG_CLD_STATUS, 1, &data); /** 告知逆变器MQTT状态*/
            usleep(INV_BROADCAST_DELAYMS);                     // 260ms
        }

        ASW_LOGI("******broadcast ****com task->set cloud status index:data=0x%04X\r\n", data);
    }
    else //并机模式下 主机发送
    {
#if PARALLEL_HOST_SET_WITH_SN

        modbus_write_inv(g_host_modbus_id, INV_REG_CLD_STATUS, 1, &data); /** 告知逆变器MQTT状态*/
        ASW_LOGI("*******host inv***com %d,task->set cloud status index:data=0x%04X\r\n", g_host_modbus_id, data);

#else
        modbus_write_inv(monitor_para.host_adr, INV_REG_CLD_STATUS, 1, &data); /** 告知逆变器MQTT状态*/
        ASW_LOGI("*******host inv***com %d,task->set cloud status index:data=0x%04X\r\n", monitor_para.host_adr, data);
#endif
    }
}
//-------------------------------------------------------//
//发送充放电调度信息到逆变器 广播or 主机
void handleMsg_broadCast_charge2inv_fun()
{
    ////// TODO ////////
    uint16_t data[2] = {0};
    uint16_t data_mode = 0;
    //-------------------------//
    ScheduleBat mstrScheduleBat = {0}; /** 产生数据，给全局变量*/
    read_global_var(PARA_SCHED_BAT, &mstrScheduleBat);

    Bat_arr_t mBattery_arr_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &mBattery_arr_data);
    //-------------------------//

    static uint16_t sarr_last_data[2] = {0};
    static uint16_t s_last_data_mode = {0};
    uint8_t mModbusId = 0;

    data[0] = u16_lChargeValue;

    ASW_LOGI("charge mode: %d\n", u16_lChargeValue);
    switch (u16_lChargeValue)
    {
    case 1: // stop

        data_mode = 2; /** 自发自用模式*/

        break;
    case 2: // charging -
        data[1] = (mstrScheduleBat.fq_t1 * -1);
        data_mode = 4; /** 自定义模式*/

        break;
    case 3: // discharging +
        data[1] = mstrScheduleBat.ov_t1;
        data_mode = 4; /** 自定义模式*/

        break;
    default:
        break;
    }

    if (g_asw_debug_enable > 1)
    {
        printf("\n\n");
        printf("data_mode:%d,sche_last_data_mode:%d \n", data_mode, s_last_data_mode);
        printf("data0[%d]data1[%d],sche_last_datadata0[%d]data1[%d]\n", data[0], data[1], sarr_last_data[0], sarr_last_data[1]);

        printf("\n\n");
    }

    //并机模式发送主机
    if (g_parallel_enable == 1 && g_host_modbus_id >= 3)
    {
        mModbusId = g_host_modbus_id;
    }
    //普通模式发送广播
    else
    {
        mModbusId = 0;
    }

    if (data_mode > 0 && data_mode != s_last_data_mode)
    {
        if (g_asw_debug_enable == 1)
        {
            char time[30] = {0};
            get_time(time, sizeof(time));
            printf("\n [%s] -- same -- run mode set ----   send data[%d] to inv %d reg addr:%d\n", time, data_mode, mModbusId, 0x044F);
        }

        if (modbus_write_inv(mModbusId, 0x044F, 1, &data_mode) == ASW_OK) /** 储能机运行模式：41104*/
        {
            s_last_data_mode = data_mode;
        }
        else
        {
            ESP_LOGE("-- same --  Write inv cmd Error--", "modebus failed write data to 41104.");
        }
    }

    if (u16_lChargeValue == 1) //停止
    {
        if (data[0] > 0 && data[0] != sarr_last_data[0]) //
        {
            if (g_asw_debug_enable == 1)
            {
                char time[30] = {0};
                get_time(time, sizeof(time));
                printf("\n [%s]  -- same -- stop charge set ----   send data[%d] to inv %d reg addr:%d\n", time, data[0], mModbusId, INV_REG_ADDR_CHARGE);
            }
            if (modbus_write_inv(mModbusId, INV_REG_ADDR_CHARGE, 1, data) == ASW_OK) /** 储能机充放电标志：41152*/
            {
                sarr_last_data[0] = data[0];
            }
            else
            {
                ESP_LOGE("-- same -- Write inv cmd Error--", "modebus failed write data to 41153.");
            }
        }
    }
    else // 冲放电
    {

        if ((data[0] > 0 && memcmp(data, sarr_last_data, sizeof(data)) != 0))
        {
            if (g_asw_debug_enable == 1)
            {
                char time[30] = {0};
                get_time(time, sizeof(time));
                printf("\n [%s]  -- same -- charge set ----   send data[%d,%d] to inv %d reg addr:%d\n", time, data[0], (int16_t)data[1], mModbusId, INV_REG_ADDR_CHARGE);
            }
            /** 储能机充放电标志：41152*/
            if (modbus_write_inv(mModbusId, INV_REG_ADDR_CHARGE, 2, data) == ASW_OK)
            {
                memcpy(sarr_last_data, data, sizeof(data));
                // g_41153_state[mIndex] = REG_41153_PWR_LIMIT;  //新版的去掉发送电表功率到逆变器 20221027
            }
            else
            {
                ESP_LOGE(" -- same -- Write inv cmd Error-- ", "  inv modbusID:%d modebus failed write data to 41152+2.", g_host_modbus_id);
            }
        }
    }
}
//------------------------------------------------------//
void handleMsg_setRunMode_fun(int mIndex, MonitorBat_info *p_monitor_para)
{
    int ret;
    uint16_t data[6] = {0};
    Bat_arr_t m_bat_datas = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_bat_datas);

    data[0] = p_monitor_para->batmonitor.dc_per; // beast type
    data[1] = p_monitor_para->batmonitor.uu1;    // running model

    ret = modbus_write_inv(m_bat_datas[mIndex].modbus_id, INV_REG_ADDR_BATTERY, 2, data); /** 储能机种类别选择*/
    if (ret == ASW_OK)
    {
        ASW_LOGI("***com task->set run mode index:data[0]=%d,data[1]=%d\r\n",
                 data[0], data[1]);
    }
}
//------------------------------------------------------//
void handleMsg_setFirstRun_fun(int mIndex, MonitorBat_info *p_monitor_para)
{
    int ret;
    // uint16_t data[6] = {0};
    // MonitorPara *p_monitor_para = (MonitorPara)p1;

    Bat_arr_t m_bat_datas = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_bat_datas);

    uint16_t midata = p_monitor_para->batmonitor.up4;
    ret = modbus_write_inv(m_bat_datas[mIndex].modbus_id, INV_REG_ADDR_FIRST_ON, 1, &midata); /** 首次开机状态：41101*/
    if (ret == ASW_OK)
    {
        int inv_idx = get_estore_inv_arr_first();
        if (inv_idx >= 0)
        {
            inv_arr[inv_idx].status_first_running = p_monitor_para->batmonitor.up4;
            ASW_LOGI("**********com task->set first power on/off index:address=%d,data=%d\r\n",
                     INV_REG_ADDR_FIRST_ON, p_monitor_para->batmonitor.up4);
        }
    }
}
//--------------------------------------------------------//
void handleMsg_setPower_fun(int mIndex, MonitorBat_info *p_monitor_para)
{
    Bat_arr_t m_arrbats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_arrbats_data);
    int ret;
    // MonitorPara *p_monitor_para = (MonitorPara)p1;
    uint16_t midata = p_monitor_para->batmonitor.uu4;
    // if (is_inv_has_estore() == 1) /** 储能机*/
    if (inv_arr[mIndex].status && inv_arr[mIndex].regInfo.mach_type > 10) // estore && online
    {
        ret = modbus_write_inv(m_arrbats_data[mIndex].modbus_id, INV_REG_ADDR_POWER_ON, 1, &midata); /** 储能机开关: 41102*/
    }

    if (p_monitor_para->batmonitor.uu4 == 2)
        midata = 1;
    else
    {
        midata = 0;
    }
    ret = modbus_write_inv(m_arrbats_data[mIndex].modbus_id, INV_REG_DRM_0N_OFF, 1, &midata); /** 启停： 40201*/
    if (ret == ASW_OK)
    {
        ASW_LOGI("**********com task->set power on/off index:address=%d,data=%d\r\n",
                 INV_REG_ADDR_POWER_ON, p_monitor_para->batmonitor.uu4);
    }
}
//------------------------------------------------------//
void handleMsg_loadSpeed_fun(int mIndex, MonitorBat_info *p_monitor_para)
{
    int ret;
    Bat_arr_t m_arrbats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_arrbats_data);
    uint16_t data[6] = {0};
    // MonitorPara *p_monitor_para = (MonitorPara)p1;

    ret = modbus_holding_read_inv(m_arrbats_data[mIndex].modbus_id, INV_REG_ADDR_Q_PHASE, 1, data);
    if (ret == ASW_OK)
    {
        p_monitor_para->batmonitor.fq_ld_sp = data[0]; //[tgl mark] 测试下是否修改成功 传入的monitorpara
    }
}
//------------------------------------------------------//
void handleMsg_invTime_fun(int mIndex)
{
    int ret;
    DATE_STRUCT curr_date = {0};
    get_current_date(&curr_date);
    uint16_t u_time[6] = {0};
    u_time[0] = curr_date.YEAR;
    u_time[1] = curr_date.MONTH;
    u_time[2] = curr_date.DAY;
    u_time[3] = curr_date.HOUR;
    u_time[4] = curr_date.MINUTE;
    u_time[5] = curr_date.SECOND;
    //////////////////////
    Bat_arr_t m_arrbats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_arrbats_data);

    ret = modbus_write_inv(m_arrbats_data[mIndex].modbus_id, INV_REG_ADDR_TIME, 6, u_time); /** 设置逆变器RTC时间*/
    if (ret == ASW_OK)
    {
        ASW_LOGI("-------set time to inverter ok!! time: %d-%d-%d %d:%d:%d\n",
                 u_time[0], u_time[1], u_time[2], u_time[3], u_time[4], u_time[5]);
    }
    else
    {
        ESP_LOGW(TAG, "-------set time to inverter fail!! time: %d-%d-%d %d:%d:%d\n",
                 u_time[0], u_time[1], u_time[2], u_time[3], u_time[4], u_time[5]);
    }
}
//----------------------------------------------//
void handleMsg_comBoxTime_fun(int mIndex)
{
    uint16_t data[8] = {0};
    int ret;
    //////////////////////
    Bat_arr_t m_arrbats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_arrbats_data);

    ret = modbus_holding_read_inv(m_arrbats_data[mIndex].modbus_id, INV_REG_ADDR_TIME, 6, data); /** 从逆变器对时*/
    if (ret == ASW_OK)
    {
        DATE_STRUCT date_time = {0};
        date_time.YEAR = data[0];
        date_time.MONTH = data[1];
        date_time.DAY = data[2];
        date_time.HOUR = data[3];
        date_time.MINUTE = data[4];
        date_time.SECOND = data[5];
        if (date_time.YEAR > 2020)
            set_current_date(&date_time);
    }
}

//-------------------------------------------------//
void handleMsg_dspZvCld_fun(int mIndex)
{
    uint16_t data = 0;
    // int ret=0;

    wifi_sta_para_t sta_para = {0};
    //////////////////////
    Bat_arr_t m_arrbats_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_arrbats_data);

    general_query(NVS_STA_PARA, &sta_para);
    if (strlen((char *)sta_para.ssid) == 0 && strlen((char *)sta_para.password) == 0)
    {
        data = 0x00AF;
    }
    else
    {

        // if (get_mqtt_status() == 0)
        if (g_state_mqtt_connect == 0) // Eng.Stg.Mch-Lanstick 20220907 +-
        {
            data = 0x000A;
        }
        else
        {
            data = 0x0005;
        }
    }

    modbus_write_inv(m_arrbats_data[mIndex].modbus_id, INV_REG_CLD_STATUS, 1, &data); /** 告知逆变器MQTT状态*/
    ASW_LOGI("**********com task->set cloud status index:data=%d\r\n", data);
}

//////////////////// Lanstick-MultilInv +/////////////////////

//------------------------------------//
int8_t asw_meter2inv_para_set(uint32_t msg_index)
{

    MonitorPara monitor_para = {0};
    meter_data_t inv_meter = {0};

    read_global_var(METER_CONTROL_CONFIG, &monitor_para);
    read_global_var(GLOBAL_METER_DATA, &inv_meter);
    switch (msg_index)
    {
    case MSG_PWR_ACTIVE_INDEX:
        handleMsg_pwrActive_fun(&monitor_para, &inv_meter);
        break;

        // zero export: send the meter status, while meter is offline.
    case MSG_INV_SET_ADV_INDEX:
        handleMsg_setAdv_fun();
        break;

    default:
        break;
    }
    task_inv_meter_msg &= (~msg_index);
    return 0;
}

///////////////////  broadcast ////////////////////////
int8_t inv_broadcast_set_para(uint32_t msg_index)
{
    Bat_Monitor_arr_t monitor_para_arr = {0};
    read_global_var(PARA_CONFIG, &monitor_para_arr);
    MonitorBat_info monitor_para = monitor_para_arr[0];

    switch (msg_index)
    {
        // battery: set battery information
    case MSG_BRDCST_SET_BATTERY_INDEX:
        handleMsg_broadCast_setBattery_fun(&monitor_para);
        break;

    case MSG_BRDCST_SET_RUN_MODE_INDEX:
        handleMsg_broadCast_setRunMode_fun(&monitor_para);
        break;

    case MSG_BRDCST_SET_POWER_INDEX:
        handleMsg_broadCast_setPower_fun(&monitor_para);
        break;

        //同步电池调度信息
    case MSG_BRDCST_GET_SAFETY_INDEX:
        handleMsg_getSafty_fun();
        break;

        //写sta联网信息到逆变器
    case MSG_WRT_STA_INFO_INDEX:
        handleMsg_wrt_staInfo2Inv_fun();
        break;

        //写并联模式到逆变器
    case MSG_WRT_SET_HOST_INDEX:
        handleMsg_wrt_parallelInfo2Inv_fun();
        break;
        //同步云端通讯状态到逆变器
    case MSG_BRDCST_DSP_ZV_CLD_INDEX:
        handleMsg_broadCast_handleMsg_dspZvCld_fun();
        break;
        //发送电池调度信息到逆变器
    case MSG_BRDCST_CHARGE_INDEX:
        handleMsg_broadCast_charge2inv_fun();
        break;

    default:
        break;
    }
    g_task_inv_broadcast_msg &= (~msg_index);

    return 0;
}

/////////////////////////////////////////////////////////////
int8_t inv_set_para(int inv_index, uint32_t msg_index)
{

    Bat_Monitor_arr_t monitor_para_arr = {0};
    read_global_var(PARA_CONFIG, &monitor_para_arr);

    MonitorBat_info monitor_para = monitor_para_arr[inv_index];

    switch (msg_index)
    {
        // battery: set battery information
    case MSG_SET_BATTERY_INDEX:
        handleMsg_setBattery_fun(inv_index, &monitor_para);
        break;

    // battery: set running mode
    case MSG_SET_RUN_MODE_INDEX:
        handleMsg_setRunMode_fun(inv_index, &monitor_para);
        break;

    // battery: set inverter charging / discharging
    case MSG_SET_CHARGE_INDEX:
        handleMsg_setChange_fun(inv_index, &monitor_para);
        break;

        // battery: set inverter power on/off
    case MSG_SET_POWER_INDEX:
        handleMsg_setPower_fun(inv_index, &monitor_para);
        break;

    case MSG_DSP_ZV_CLD_INDEX:
        handleMsg_dspZvCld_fun(inv_index);
        break;

    /** fresh setdefine of current day*/
    case MSG_GET_SAFETY_INDEX:
        handleMsg_getSafty_fun();
        break;

    default:
        break;
    }
    task_inv_msg_arr[inv_index] &= (~msg_index);
    return 0;
}

//-------------------------------//
//电池充放电调度 不同工作模式下
static int asw_battery_diff_schedule_manager(int mInvIndex)
{

    static int mdrm_ld_sp[INV_NUM] = {0};
    bool mboolIsSchedule;
    int miPeriod = 0;
    int mSchedlePeriod = 0;
    int now_sec = get_second_sys_time(); // now_sec
    uint16_t cur_minutes;

    static uint64_t m_second_time_last = 0;

    if (now_sec - m_second_time_last <= 1) // 1s调度一次
        return 1;

    Bat_Monitor_arr_t monitor_para = {0};
    read_global_var(PARA_CONFIG, &monitor_para);

    m_second_time_last = get_second_sys_time();
    get_current_date(&mS_curr_date);
    cur_minutes = mS_curr_date.HOUR * 60 + mS_curr_date.MINUTE; /** 当前每天分钟数*/

    // printf("inv index:%d, battery mode:%d ", mInvIndex - 1, monitor_para[mInvIndex - 1].batmonitor.uu1);

    if (monitor_para[mInvIndex - 1].batmonitor.dc_per == 1                               // Energy stored type
        && monitor_para[mInvIndex - 1].batmonitor.uu1 == 4 && check_time_correct() == 0) //  user define mode && date
    {

        mboolIsSchedule = false; // not in schedule
        miPeriod = now_sec - mdrm_ld_sp[mInvIndex - 1];

        if (miPeriod >= 2) // Eng.Stg.Mch change
        {
            ASW_LOGI("\n******timestamp:%d start to check the schedule\n", now_sec);

            // Eng.Stg.Mch change
            mdrm_ld_sp[mInvIndex - 1] = now_sec;

            /*1. judge the current day time whether in schedule: > start time and < start time+duration*/
            for (uint8_t i = 0; i < DAY_CHARG_SCH_NUM; i++)
            {
                mSchedlePeriod = cur_minutes - Bat_DaySchdle_arr_t[i].minutes_inday; /** 当前时刻-计划开始时刻*/
                if (mSchedlePeriod >= 0 && mSchedlePeriod < Bat_DaySchdle_arr_t[i].duration)
                {
                    mboolIsSchedule = true; // in schedule

                    monitor_para[mInvIndex - 1].batmonitor.freq_mode = Bat_DaySchdle_arr_t[i].charge_flag; /** 充放电标志*/

                    ASW_LOGI("----schedule cur_minutes:%d,minutes_inday:%d,period:%d, duration:%d  freq_mode:%d\n",
                             cur_minutes, Bat_DaySchdle_arr_t[i].minutes_inday, miPeriod,
                             Bat_DaySchdle_arr_t[i].duration, monitor_para[mInvIndex - 1].batmonitor.freq_mode);

                    write_global_var(PARA_CONFIG, &monitor_para);
                    task_inv_msg_arr[mInvIndex - 1] |= MSG_SET_CHARGE_INDEX;
                    return 0; /** 每30秒进来一次，发送最终充放电命令，跳出*/
                }
            }
            ASW_LOGI("  inv:%d ,monitor_para.adv.freq_mode:%d\n", mInvIndex - 1, monitor_para[mInvIndex - 1].batmonitor.freq_mode);
            /*if out of the schedule, stop the current operation(charging/discharging)*/
            if (false == mboolIsSchedule) //
            {
                ASW_LOGI("set monitor_para.adv.freq_mode = BATTERY_CHARGING_STOP\n");
                monitor_para[mInvIndex - 1].batmonitor.freq_mode = BATTERY_CHARGING_STOP;
                write_global_var(PARA_CONFIG, &monitor_para);
                task_inv_msg_arr[mInvIndex - 1] |= MSG_SET_CHARGE_INDEX;
                return 0;
            }
        }
    }

    return 1;
}

//---------------------------------------//
//电池充放电调度 同一个工作模式下 自定义
static int asw_battery_same_schedule_manager(void)
{
    int now_sec = get_second_sys_time(); // now_sec
    static uint64_t m_second_time_last = 0;

    if (now_sec - m_second_time_last <= 1) // 1s调度一次
        return 1;

    bool mboolIsSchedule = false;
    uint16_t mSchedlePeriod = 0;
    uint16_t mCurMinutes = 0;

    m_second_time_last = get_second_sys_time();

    Bat_Monitor_arr_t monitor_para = {0};
    read_global_var(PARA_CONFIG, &monitor_para);

    get_current_date(&mS_curr_date);
    mCurMinutes = mS_curr_date.HOUR * 60 + mS_curr_date.MINUTE; /** 当前每天分钟数*/

    if (check_time_correct() == 0) //  user define mode && date
    {
        mboolIsSchedule = false; // not in schedule
                                 /*1. judge the current day time whether in schedule: > start time and < start time+duration*/
        for (uint8_t i = 0; i < DAY_CHARG_SCH_NUM; i++)
        {
            mSchedlePeriod = mCurMinutes - Bat_DaySchdle_arr_t[i].minutes_inday; /** 当前时刻-计划开始时刻*/
            if (mSchedlePeriod >= 0 && mSchedlePeriod < Bat_DaySchdle_arr_t[i].duration)
            {
                mboolIsSchedule = true; // in schedule

                u16_lChargeValue = Bat_DaySchdle_arr_t[i].charge_flag; /** 充放电标志*/

                ASW_LOGI("---- same schedule cur_minutes,minutes_inday:%d, duration:%d  freq_mode:%d\n",
                         Bat_DaySchdle_arr_t[i].minutes_inday,
                         Bat_DaySchdle_arr_t[i].duration, u16_lChargeValue);

                g_task_inv_broadcast_msg |= MSG_BRDCST_CHARGE_INDEX;

                return 0;
            }
        }
        if (false == mboolIsSchedule) //
        {
            ASW_LOGI(" same set monitor_para.adv.freq_mode = BATTERY_CHARGING_STOP\n");
            u16_lChargeValue = BATTERY_CHARGING_STOP;
            g_task_inv_broadcast_msg |= MSG_BRDCST_CHARGE_INDEX;

            return 0;
        }
    }

    return 1;
}

//----------------------------------------------//
void asw_update_schedule_battery()
{

    if ((cur_week_day != mS_curr_date.WDAY) && check_time_correct() == ASW_OK)
    {
        ESP_LOGI("-- SCHEDULE BATTERY IN NEW DAY --", "week day change, old new %d %d\n", cur_week_day, mS_curr_date.WDAY);
        cur_week_day = mS_curr_date.WDAY;

        ScheduleBat schedule_bat = {0};
        read_global_var(PARA_SCHED_BAT, &schedule_bat);
        uint16_t uHour = 0;
        uint16_t uMinutes = 0;

        /*calculate the dayly detail schedule after synchronize the time from cloud or inverter*/
        /** 每天，最多6段充放电计划*/
        for (uint8_t i = 0; i < DAY_CHARG_SCH_NUM; i++)
        {
            uHour = (schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0xFF000000) >> 24;
            uMinutes = (schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0x00FF0000) >> 16;
            Bat_DaySchdle_arr_t[i].minutes_inday = uHour * 60 + uMinutes;
            Bat_DaySchdle_arr_t[i].duration = ((schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0x0000FF00) >> 8);
            Bat_DaySchdle_arr_t[i].charge_flag = schedule_bat.daySchedule[cur_week_day].time_sch[i] & 0x000000FF;
        }
    }
}

////////////////////////////////
int8_t setting_event_handler(void)
{
    static int8_t last_check_time = -1;

    // DATE_STRUCT curr_date;
    int8_t curr_check_time;

    uint8_t i;

    static int mIndex = 0; // Lanstick-MultilInv +
    /** 无逆变器在线*/

    if (mIndex++ > g_num_real_inv)
    {
        mIndex = 1;
    }

    if (inv_arr[mIndex - 1].status == 0) // estore && online
    {
        return 0;
    }

    /** 储能机处理*/
    if (task_inv_msg_arr[mIndex - 1] & MSG_INV_INDEX_GROUP)
    {
        ASW_LOGI("----------------------setting_event_handler ----------------- ");
        ASW_LOGI(" task inv msg value:%08x", task_inv_msg_arr[mIndex] & MSG_INV_INDEX_GROUP);

        // battery information setting--1
        if (task_inv_msg_arr[mIndex - 1] & MSG_SET_BATTERY_INDEX)
            return inv_set_para(mIndex - 1, MSG_SET_BATTERY_INDEX);

        // battery running mode---2
        else if (task_inv_msg_arr[mIndex - 1] & MSG_SET_RUN_MODE_INDEX)
            return inv_set_para(mIndex - 1, MSG_SET_RUN_MODE_INDEX);

        // battery charging/discharging/stop --3
        else if (task_inv_msg_arr[mIndex - 1] & MSG_SET_CHARGE_INDEX)
            return inv_set_para(mIndex - 1, MSG_SET_CHARGE_INDEX);

        // set first running status after finished configuration --4
        else if (task_inv_msg_arr[mIndex - 1] & MSG_SET_FIRST_RUN_INDEX)
            return inv_set_para(mIndex - 1, MSG_SET_FIRST_RUN_INDEX);

        // set power on/off  --5
        else if (task_inv_msg_arr[mIndex - 1] & MSG_SET_POWER_INDEX)
            return inv_set_para(mIndex - 1, MSG_SET_POWER_INDEX);

        // set cloud status to inverter --9
        else if (task_inv_msg_arr[mIndex - 1] & MSG_DSP_ZV_CLD_INDEX)
            return inv_set_para(mIndex - 1, MSG_DSP_ZV_CLD_INDEX);

        // update charging/discharging immediately --10
        else if (task_inv_msg_arr[mIndex - 1] & MSG_GET_SAFETY_INDEX)
            return inv_set_para(mIndex - 1, MSG_GET_SAFETY_INDEX);
    }
    ////////////////////////////  add for broadcast ///////////////////////
    else if (g_task_inv_broadcast_msg != 0)
    {
        // setting? device=4,action=setbattery
        if (g_task_inv_broadcast_msg & MSG_BRDCST_SET_BATTERY_INDEX)
        {
            return inv_broadcast_set_para(MSG_BRDCST_SET_BATTERY_INDEX);
        }

        else if (g_task_inv_broadcast_msg & MSG_BRDCST_SET_RUN_MODE_INDEX)
        {
            return inv_broadcast_set_para(MSG_BRDCST_SET_RUN_MODE_INDEX); // only set running mode)
        }

        else if (g_task_inv_broadcast_msg & MSG_BRDCST_SET_POWER_INDEX)
        {
            return inv_broadcast_set_para(MSG_BRDCST_SET_POWER_INDEX);
        }

        else if (g_task_inv_broadcast_msg & MSG_BRDCST_GET_SAFETY_INDEX)
        {
            return inv_broadcast_set_para(MSG_BRDCST_GET_SAFETY_INDEX);
        }

        else if (g_task_inv_broadcast_msg & MSG_WRT_STA_INFO_INDEX)
        {
            return inv_broadcast_set_para(MSG_WRT_STA_INFO_INDEX);
        }

        else if (g_task_inv_broadcast_msg & MSG_WRT_SET_HOST_INDEX)
        {
            return inv_broadcast_set_para(MSG_WRT_SET_HOST_INDEX);
        }

        else if (g_task_inv_broadcast_msg & MSG_BRDCST_DSP_ZV_CLD_INDEX)
        {
            return inv_broadcast_set_para(MSG_BRDCST_DSP_ZV_CLD_INDEX);
        }

        else if (g_task_inv_broadcast_msg & MSG_BRDCST_CHARGE_INDEX)
        {
            return inv_broadcast_set_para(MSG_BRDCST_CHARGE_INDEX);
        }
    }

    /* 广播消息 */
    curr_check_time = check_time_correct();

    if (last_check_time == -1 && curr_check_time == 0)
    {
        g_task_inv_broadcast_msg |= MSG_BRDCST_GET_SAFETY_INDEX;
    }
    last_check_time = curr_check_time;

    //////////////////////////////////
    /** 自定义充放电计划：调度*/

    if (g_battery_selfmode_is_same == 1)
    {
        if (asw_battery_same_schedule_manager() == 0)
            return 0;
    }
    else
    {
        if (asw_battery_diff_schedule_manager(mIndex) == 0)
            return 0;
    }

    /** 日发生变化，刷新 **/
    asw_update_schedule_battery();

    /** Check active & inactive power adjust */

    if (task_inv_meter_msg & MSG_PWR_ACTIVE_INDEX)
    {
        ASW_LOGI("****MSG_PWR_ACTIVE_INDEX\n");
        return asw_meter2inv_para_set(MSG_PWR_ACTIVE_INDEX); /** 电表在线,有功功率调节*/
    }
    else if (task_inv_meter_msg & MSG_INV_SET_ADV_INDEX)
    {
        ASW_LOGI("****MSG_INV_SET_ADV_INDEX\n");
        return asw_meter2inv_para_set(MSG_INV_SET_ADV_INDEX); /** 电表离线*/
    }
    /*DRM priority is the last one*/
    // dred_fun_ctrl(0);

    return 0;
}

//--------------------------------------//
