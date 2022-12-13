#include "estore_cld.h"
#include <math.h>
#include "asw_job_http.h"

extern char DEMO_DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1];
static const char *TAG = "asw_modbus.c";

int is_cld_has_estore(void)
{
    for (int j = 0; j < INV_NUM; j++)
    {
        int mach_type = cld_inv_arr[j].regInfo.mach_type;
        int status = cld_inv_arr[j].status;

        if (mach_type > 10 && status == 1) // estore && online
        {
            return 1;
        }
    }

    return 0;
}

int is_cld_has_inv_online(void)
{
    for (int j = 0; j < INV_NUM; j++)
    {
        int status = cld_inv_arr[j].status;
        if (status == 1) // estore && online
        {
            return 1;
        }
    }

    return 0;
}

int get_cld_mach_type(void)
{
    for (int j = 0; j < INV_NUM; j++)
    {
        int mach_type = cld_inv_arr[j].regInfo.mach_type;
        int status = cld_inv_arr[j].status;
        if (status == 1)
        {
            return mach_type;
        }
    }

    return 0;
}

void cJSON_AddNumAsStrToObject(cJSON *json, char *tag, int var)
{
    char buf[20] = {0};
    sprintf(buf, "%d", var);
    cJSON_AddStringToObject(json, tag, buf);
}

void get_meter_sn(char *msn)
{
    char mfr[20] = {0};
    char mod[20] = {0};

    get_meter_mfr_and_model(mfr, mod);
    sprintf(msn, "%s%s", mfr, DEMO_DEVICE_NAME);
}
/**************************************************************************
 * JSON PAYLOAD GENERATE
 *
 */
int get_estore_invinfo_payload(InvRegister devicedata, char *json_str)
{
    char buf[50] = {0};
    // MonitorPara monitor_para = {0};
    Bat_Monitor_arr_t monitor_para = {0};
    int mindex = -1;

    read_global_var(PARA_CONFIG, &monitor_para);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "Action", "SyncInvInfo");
    cJSON_AddStringToObject(res, "psn", DEMO_DEVICE_NAME);
    cJSON_AddStringToObject(res, "sn", devicedata.sn);
    cJSON_AddNumAsStrToObject(res, "typ", devicedata.type - 0x30);
    cJSON_AddStringToObject(res, "mod", devicedata.mode_name);
    cJSON_AddStringToObject(res, "muf", devicedata.mfr);
    cJSON_AddStringToObject(res, "brd", devicedata.brand);
    cJSON_AddStringToObject(res, "mst", devicedata.msw_ver);
    cJSON_AddStringToObject(res, "slv", devicedata.ssw_ver);
    cJSON_AddNumAsStrToObject(res, "rtp", devicedata.rated_pwr);
    cJSON_AddNumAsStrToObject(res, "MPT", devicedata.pv_num);
    cJSON_AddNumAsStrToObject(res, "sft", devicedata.safety_type);
    cJSON_AddStringToObject(res, "sfv", devicedata.tmc_ver);
    cJSON_AddStringToObject(res, "cmv", devicedata.protocol_ver);
    cJSON_AddNumAsStrToObject(res, "str", devicedata.str_num);
    cJSON_AddNumAsStrToObject(res, "mty", devicedata.mach_type);
    cJSON_AddNumAsStrToObject(res, "Imc", devicedata.modbus_id);

    for (uint8_t i = 0; i < g_num_real_inv; i++)
    {
        if (strcmp(devicedata.sn, monitor_para[i].sn) == 0)
        {
            mindex = i;
            break;
        }
    }

    ASW_LOGI("---------  get estore inv info payload ---- :%d  -------", mindex);
    ASW_LOGI("sta_b:%d,typ_b:%d,mod_r:%d", monitor_para[mindex].batmonitor.uu4, monitor_para[mindex].batmonitor.dc_per, monitor_para[mindex].batmonitor.uu1);
    ASW_LOGI("\n---------  get estore inv info payload -------- \n");

    if (mindex == -1)
    {
        mindex = 0;
        ESP_LOGW("--estore_cld.c-- ", "get_estore_invinfo_payload   publish data : found no inv %s", devicedata.sn);
    }

    cJSON_AddNumberToObject(res, "sta_b", monitor_para[mindex].batmonitor.uu4);
    cJSON_AddNumberToObject(res, "typ_b", monitor_para[mindex].batmonitor.dc_per);
    cJSON_AddNumberToObject(res, "mod_r", monitor_para[mindex].batmonitor.uu1);
    cJSON_AddNumberToObject(res, "muf_b", monitor_para[mindex].batmonitor.up1);
    cJSON_AddNumberToObject(res, "mod_b", monitor_para[mindex].batmonitor.uu2);
    cJSON_AddNumberToObject(res, "num_b", monitor_para[mindex].batmonitor.up2);

    cJSON_AddNumberToObject(res, "discharge_max", monitor_para[mindex].batmonitor.dchg_max);
    cJSON_AddNumberToObject(res, "charge_max", monitor_para[mindex].batmonitor.chg_max);

    ////////////////////////////////////////////////////

    uint16_t host_status = 99;

    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);
    if (mt.is_parallel)
    {
#if PARALLEL_HOST_SET_WITH_SN
        if (strcmp(devicedata.sn, mt.host_psn) == 0)
#else
        if (devicedata.modbus_id == mt.host_adr)
#endif
            host_status = 1;
        else
            host_status = 2;
    }
    else
    {
        host_status = 0;
    }

    cJSON_AddNumberToObject(res, "host", host_status);

#if TRIPHASE_ARM_SUPPORT
    cJSON_AddStringToObject(res, "lsw", inv_arr[mindex].reginfo.csw_ver);
#endif

    ////////////////////////////////////////////////////

    // cJSON_AddNumberToObject(res, "host", devicedata.is_host);

    /** output: **************/
    char *msg = NULL;
    msg = cJSON_PrintUnformatted(res);
    memcpy(json_str, msg, strlen(msg));
    free(msg);
    cJSON_Delete(res);
    return 0;
}
////////////////////////////////////////////////////////
int get_estore_meterinfo_payload(char *json_str)
{
    MonitorPara monitor_para = {0};
    meter_data_t inv_meter = {0};

    /////////////// Lanstick-MultiveInv /////////////////
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    read_global_var(GLOBAL_METER_DATA, &inv_meter);
    cJSON *res = cJSON_CreateObject();
    char meter_sn[40] = {0};
    char meter_mfr[20] = {0};
    char meter_mod[20] = {0};
    char mod[10] = {0};
    get_meter_sn(meter_sn);
    get_meter_mfr_and_model(meter_mfr, meter_mod);
    sprintf(mod, "%d", monitor_para.adv.meter_mod);

    /** **/
    cJSON_AddStringToObject(res, "Action", "SyncMeterInfo");
    cJSON_AddStringToObject(res, "psn", DEMO_DEVICE_NAME);
    cJSON_AddStringToObject(res, "sn", meter_sn);
    cJSON_AddStringToObject(res, "manufactory", meter_mfr); // AiSWEI
    cJSON_AddStringToObject(res, "type", "1");
    cJSON_AddStringToObject(res, "name", meter_mod);
    cJSON_AddNumAsStrToObject(res, "enb", monitor_para.adv.meter_enb); // always 1, enable for ever
    cJSON_AddStringToObject(res, "mod", mod);
    cJSON_AddNumAsStrToObject(res, "model", monitor_para.adv.meter_mod); // 100
    cJSON_AddNumAsStrToObject(res, "exp_m", monitor_para.adv.meter_target);
    cJSON_AddNumAsStrToObject(res, "regulate", monitor_para.adv.meter_regulate);
    cJSON_AddNumAsStrToObject(res, "enb_PF", monitor_para.adv.meter_enb_PF);
    cJSON_AddNumAsStrToObject(res, "target_PF", monitor_para.adv.meter_target_PF);
    cJSON_AddNumAsStrToObject(res, "abs", monitor_para.adv.regulate_abs);
    cJSON_AddNumAsStrToObject(res, "offset", monitor_para.adv.regulate_offset);
    /** output: **************/
    char *msg = NULL;
    msg = cJSON_PrintUnformatted(res);
    memcpy(json_str, msg, strlen(msg));
    free(msg);
    cJSON_Delete(res);

    return 0;
}

/** 储能机发布数据生成*/
int get_estore_invdata_payload(Inv_data *inv, char *json_str)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "Action", "UpdateInvData");
    cJSON_AddStringToObject(res, "psn", DEMO_DEVICE_NAME);
    cJSON_AddStringToObject(res, "sn", inv->psn);
    cJSON_AddNumberToObject(res, "mty", get_cld_mach_type());
    cJSON_AddNumberToObject(res, "tmstp", (double)inv->rtc_time * 1000); // ms
    cJSON_AddNumberToObject(res, "stu", inv->status);
    cJSON_AddStringToObject(res, "tim", inv->time);
    if (inv->data_type != INVDATA_TYPE_RT && inv->data_type != INVDATA_TYPE_TEST)
        inv->data_type = INVDATA_TYPE_TEST;
    cJSON_AddNumberToObject(res, "data_type", inv->data_type);

    cJSON_AddNumberToObject(res, "csq", get_rssi());
    cJSON_AddNumberToObject(res, "itv", GetMinute1((inv->time[11] - 0x30) * 10 + inv->time[12] - 0x30, (inv->time[14] - 0x30) * 10 + inv->time[15] - 0x30, 0));
    /** add arrays: */
    cJSON *general = cJSON_AddArrayToObject(res, "general");
    cJSON *vpv = cJSON_AddArrayToObject(res, "vpv");
    cJSON *ipv = cJSON_AddArrayToObject(res, "ipv");
    cJSON *vac = cJSON_AddArrayToObject(res, "vac");
    cJSON *iac = cJSON_AddArrayToObject(res, "iac");

#if TRIPHASE_ARM_SUPPORT
    cJSON *pac_array = cJSON_AddArrayToObject(res, "pac_array");
    cJSON *qac_array = cJSON_AddArrayToObject(res, "qac_array");

#endif

    cJSON *str = cJSON_AddArrayToObject(res, "str");
    cJSON *bat = cJSON_AddArrayToObject(res, "bat");
    cJSON *eps = cJSON_AddArrayToObject(res, "EPS");
    cJSON *history_array = cJSON_AddArrayToObject(res, "history_array");
    cJSON *meter = cJSON_AddArrayToObject(res, "meter");

    /** general: */
    cJSON_AddNumberToObject(general, "", round((double)inv->e_today));
    cJSON_AddNumberToObject(general, "", round((double)inv->e_total));
    cJSON_AddNumberToObject(general, "", round((double)inv->h_total));
    cJSON_AddNumberToObject(general, "", round((double)inv->status));
    cJSON_AddNumberToObject(general, "", round((double)inv->col_temp));
    cJSON_AddNumberToObject(general, "", round((double)inv->U_temp));      // boost temp
    cJSON_AddNumberToObject(general, "", round((double)0));                // default 0 temp
    cJSON_AddNumberToObject(general, "", round((double)0));                // default 0 temp
    cJSON_AddNumberToObject(general, "", round((double)inv->contrl_temp)); //  boost temp
    cJSON_AddNumberToObject(general, "", round((double)inv->bus_voltg));
    cJSON_AddNumberToObject(general, "", round((double)inv->fac));
    cJSON_AddNumberToObject(general, "", round((double)inv->sac));
    cJSON_AddNumberToObject(general, "", round((double)inv->pac));
    cJSON_AddNumberToObject(general, "", round((double)inv->qac));
    cJSON_AddNumberToObject(general, "", round((double)inv->cosphi));
    cJSON_AddNumberToObject(general, "", round((double)inv->error));
    cJSON_AddNumberToObject(general, "", round((double)inv->warn[0]));

    /** vpv: */
    int idx = -1;
    for (int j = 0; j < INV_NUM; j++)
    {
        if (strlen(cld_inv_arr[j].regInfo.sn) >= 2) // mark
        {

            if (strcmp(cld_inv_arr[j].regInfo.sn, inv->psn) == 0)
            {
                idx = j;
                break;
            }
        }
    }

    if (idx >= 0)
    {
        for (int j = 0; j < cld_inv_arr[idx].regInfo.pv_num; j++)
        {
            if ((inv->PV_cur_voltg[j].iVol != 0xFFFF) && (inv->PV_cur_voltg[j].iCur != 0xFFFF))
            {
                cJSON_AddNumberToObject(vpv, "", round((double)inv->PV_cur_voltg[j].iVol));
                cJSON_AddNumberToObject(ipv, "", round((double)inv->PV_cur_voltg[j].iCur));
            }
        }

        for (int j = 0; j < 3; j++)
        {
            if (inv->AC_vol_cur[j].iVol != 0xFFFF && inv->AC_vol_cur[j].iCur != 0xFFFF)
            {
                cJSON_AddNumberToObject(vac, "", round((double)inv->AC_vol_cur[j].iVol));
                cJSON_AddNumberToObject(iac, "", round((double)inv->AC_vol_cur[j].iCur));
            }
        }
        ASW_LOGI("the str num:%d", cld_inv_arr[idx].regInfo.str_num);
        for (uint8_t j = 0; j < cld_inv_arr[idx].regInfo.str_num; j++)
        {
            if ((inv->istr[j] != 0xFFFF))
            {
                cJSON_AddNumberToObject(str, "", round((double)inv->istr[j]));
            }
        }
    }

    /// history_array
    for (uint8_t k = 0; k < 10; k++)
    {
        cJSON_AddNumberToObject(history_array, "", inv->hist_err[k]);
    }

    //////////////////////   LanStick-MultilInv /////////////////////////////////psn
    Bat_arr_t m_battery_arr_data = {0};
    Bat_Monitor_arr_t monitor_para = {0};
    read_global_var(PARA_CONFIG, &monitor_para);
    read_global_var(GLOBAL_BATTERY_DATA, &m_battery_arr_data);

    int mInvIndex = -1;
    for (uint8_t i = 0; i < g_num_real_inv; i++)
    {

        if (strcmp(inv->psn, m_battery_arr_data[i].sn) == 0)
        {
            mInvIndex = i;
            ASW_LOGI("\n===== DEBUG PRINT:UpdateInvData=====\n ---get index:%d \n", i);
            break;
        }
    }
    if (mInvIndex == -1)
        mInvIndex = 0;

    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.ppv_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.etoday_pv_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.etotal_pv_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.status_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.error1_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.warn1_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.v_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.i_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.p_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.t_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.SOC_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.SOH_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.etd_in_batt));
    cJSON_AddNumberToObject(bat, "", round((double)m_battery_arr_data[mInvIndex].batdata.etd_out_batt));
    //电池通讯状态
    cJSON_AddNumberToObject(bat, "", m_battery_arr_data[mInvIndex].batdata.status_comm_batt);
    //电池充电限流值
    cJSON_AddNumberToObject(bat, "", monitor_para[mInvIndex].batmonitor.chg_max);
    //电池放电限流值
    cJSON_AddNumberToObject(bat, "", monitor_para[mInvIndex].batmonitor.dchg_max);

    /** eps: */
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.v_EPS_batt));
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.i_EPS_batt));
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.f_EPS_batt));
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.p_ac_EPS_batt));
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.p_ra_EPS_batt));
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.etd_EPS_batt));
    cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.eto_EPS_batt));
    //////////////////////////////////////////////////////////////////////////////

#if TRIPHASE_ARM_SUPPORT
    for (uint8_t i = 0; i < 3; i++)
    {

        /* TRI esp*/
        cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.v_i_phase_ESP[i].iVol));
        cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.v_i_phase_ESP[i].iCur));
        cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.v_i_phase_ESP[i].pac));
        cJSON_AddNumberToObject(eps, "", round((double)m_battery_arr_data[mInvIndex].batdata.v_i_phase_ESP[i].qac));
        /* TRI pac_array */
        cJSON_AddNumberToObject(pac_array, "", round((double)m_battery_arr_data[mInvIndex].batdata.p_q_phase_AC[i].pac));

        /* TRI qac_array */
        cJSON_AddNumberToObject(qac_array, "", round((double)m_battery_arr_data[mInvIndex].batdata.p_q_phase_AC[i].qac));
    }

#endif

    /** meter: */
    char msn[40] = {0};
    get_meter_sn(msn);
    cJSON_AddStringToObject(res, "msn", msn);
    meter_data_t inv_meter = {0};
    read_global_var(GLOBAL_METER_DATA, &inv_meter);
    cJSON_AddNumberToObject(meter, "", round((double)(int)inv_meter.invdata.pac));
    cJSON_AddNumberToObject(meter, "", round((double)(int)inv_meter.invdata.e_in_today));
    cJSON_AddNumberToObject(meter, "", round((double)(int)inv_meter.invdata.e_out_today));
    cJSON_AddNumberToObject(meter, "", round((double)(int)inv_meter.invdata.e_in_total / 10));
    cJSON_AddNumberToObject(meter, "", round((double)(int)inv_meter.invdata.e_out_total / 10));

    /** output: **************/
    char *msg = NULL;
    msg = cJSON_PrintUnformatted(res);
    memcpy(json_str, msg, strlen(msg));
    free(msg);
    cJSON_Delete(res);

    return 0;
}

/** ********************************************************
 *                   local api
 * ********************************************************/

void get_meter_mfr_and_model(char *mfr, char *model)
{
    MonitorPara monitor_para = {0};

    /////////////  Lanstick-MutltivInv //////////////////
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);
    // monitor_para.adv.meter_mod = 100; /** Acrel only */
    get_meter_info(monitor_para.adv.meter_mod, mfr, model);
}

/** **********************************************************
 *
 * CGI和RPC共用的函数，读取和设置参数
 *
 ************************************************************/

int asw_write_inv_onoff(cJSON *json)
{
    int setpower = 0;
    int setonoff = 0;

    int broadcastEnable = 0;
    Bat_Monitor_arr_t monitor_para = {0};
    read_global_var(PARA_CONFIG, &monitor_para);

    /////////////////////////////////////////////////
    char crt_psn[33];
    getJsonStr(crt_psn, "sn", sizeof(crt_psn), json);
    int index = -1;

    if (strcmp(crt_psn, "broadcast") == 0)
        broadcastEnable = 1;

    else
    {
        broadcastEnable = 0;
        for (uint8_t m = 0; m < g_num_real_inv; m++)
        {
            if (strcmp(monitor_para[m].sn, crt_psn) == 0)
            {
                index = m;
                break;
            }
        }
    }

    if (index == -1 && broadcastEnable == 0)
    {
        index = 0;
        ESP_LOGW("-- rrpc setpower--", " set power Erro, found no inv  with psn:%s.", crt_psn);
    }

    MonitorPara monitor_config_para = {0};

    read_global_var(METER_CONTROL_CONFIG, &monitor_config_para);
    if (monitor_config_para.is_parallel)
    {
        broadcastEnable = 1;
    }
    //////////////////////////////////////////////

    if (getJsonNum(&setpower, "power", json) == 0)
    {
        ASW_LOGI("\n--------- set power get power:%d------------", setpower);
        if (broadcastEnable)
        {
            for (uint8_t m = 0; m < g_num_real_inv; m++)
            {
                monitor_para[m].batmonitor.uu4 = setpower;
            }

            // write_global_var(PARA_CONFIG, &monitor_para);
            write_global_var_to_nvs(PARA_CONFIG, &monitor_para);

            g_task_inv_broadcast_msg |= MSG_BRDCST_SET_POWER_INDEX;
        }
        else
        {
            monitor_para[index].batmonitor.uu4 = setpower;

            // write_global_var(PARA_CONFIG, &monitor_para);
            write_global_var_to_nvs(PARA_CONFIG, &monitor_para);

            task_inv_msg_arr[index] |= MSG_SET_POWER_INDEX;
        }

        // if (setpower == 2)
        //     event_group_0 |= SYNC_ALL_ONCE; // turn on

        // return ASW_OK;

        // asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
    }
    // else
    // {
    //     return ASW_FAIL;
    //     // asw_mqtt_publish(resp_topic, recv_er, strlen(recv_er), 0);
    // }

    if (setpower == 2 || setonoff == 1)
    {
        event_group_0 |= SYNC_ALL_ONCE; // turn on
    }
    return ASW_OK;
}
//////////////// Lanstick-MultilInv //////////////////////
int write_battery_configuration(cJSON *setbattery)
{
    int num = -1;
    int tmpint = 0;
    uint16_t b_info = 0;
    int index = -1;
    Bat_Monitor_arr_t monitor_para = {0};
    read_global_var(PARA_CONFIG, &monitor_para);
    ////////////////////////////////////////////////////////////

    char crt_psn[33] = {0};
    getJsonStr(crt_psn, "sn", sizeof(crt_psn), setbattery);

    int mdchg_max = 0, mchg_max = 0;

    // printf("\n============= write_battery_configuration=====[%s] \n", crt_psn);
#if 1
    if (strcmp(crt_psn, "broadcast") == 0)
    {
        // broadcastEnable = 1;

        int mdc_per = 0, muu1 = 0, mup1 = 0, muu2 = 0, mup2 = 0;

        getJsonNum(&mdc_per, "type", setbattery);

        getJsonNum(&muu1, "mod_r", setbattery);

        //--------------------------------------//

        if (getJsonNum(&mup1, "muf", setbattery) == 0)
        {
            b_info = 1;
        }
        if (getJsonNum(&muu2, "mod", setbattery) == 0)
        {
            b_info = 1;
        }
        if (getJsonNum(&mup2, "num", setbattery) == 0)
        {
            b_info = 1;
        }
        if (getJsonNum(&mdchg_max, "discharge_max", setbattery) == 0)
        {
            b_info = 1;
        }
        if (getJsonNum(&mchg_max, "charge_max", setbattery) == 0)
        {
            b_info = 1;
        }
        ////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////
        ASW_LOGI("\n-----------broadcast set battery para --------------\n");
        ASW_LOGI("g_real_num:%d, type:%d,uu1:%d,up1:%d,uu2:%d,up2:%d,dchg_max:%d,chg_max:%d",
                 g_num_real_inv, mdc_per, muu1, mup1, muu2, mup2, mdchg_max, mchg_max);
        ASW_LOGI("\n-----------broadcast set battery para --------------\n");

        for (uint8_t m = 0; m < g_num_real_inv; m++)
        {
            monitor_para[m].batmonitor.dc_per = mdc_per;
            monitor_para[m].batmonitor.uu1 = muu1;
            monitor_para[m].batmonitor.up1 = mup1;
            monitor_para[m].batmonitor.uu2 = muu2;
            monitor_para[m].batmonitor.up2 = mup2;

            monitor_para[m].batmonitor.dchg_max = mdchg_max * 0.01;
            monitor_para[m].batmonitor.chg_max = mchg_max * 0.01;
        }

        write_global_var_to_nvs(PARA_CONFIG, &monitor_para);

        if (b_info == 1)
        {
            g_task_inv_broadcast_msg |= MSG_BRDCST_SET_BATTERY_INDEX;
        }
        else
        {
            g_task_inv_broadcast_msg |= MSG_BRDCST_SET_RUN_MODE_INDEX; // only set running mode
        }
    }
    else
#endif
    {
        if (strlen(crt_psn) <= 0)
            index = 0;
        else
            index = asw_get_index_byPsn(crt_psn);

        getJsonNum(&tmpint, "type", setbattery);
        monitor_para[index].batmonitor.dc_per = tmpint;

        getJsonNum(&tmpint, "mod_r", setbattery);
        monitor_para[index].batmonitor.uu1 = tmpint;

        //// 判断是否为自定义模式 并机模式下，主机设置为自定义模式，则发送到主机;普通模式下
        //    g_battery_selfmode_is_same, g_parallel_enable, g_host_modbus_id);

        if (monitor_para[index].batmonitor.uu1 == 4 && monitor_para[index].batmonitor.dc_per == 1 && g_parallel_enable && g_host_modbus_id == monitor_para[index].modbus_id)
        {
            g_battery_selfmode_is_same = 1;
        }
        else if (monitor_para[index].batmonitor.uu1 == 4 && g_parallel_enable == 0 && monitor_para[index].batmonitor.dc_per == 1)
        {
            uint8_t i = 0;
            for (i = 0; i < g_num_real_inv; i++)
            {
                if (monitor_para[i].batmonitor.uu1 != 4)
                {
                    g_battery_selfmode_is_same = 0;
                    break;
                }
            }
            if (i == g_num_real_inv)
                g_battery_selfmode_is_same = 1;
        }
        else if (monitor_para[index].batmonitor.uu1 != 4 && g_parallel_enable == 0)
        {
            g_battery_selfmode_is_same = 0;
        }

        else if ((monitor_para[index].batmonitor.uu1 != 4 || monitor_para[index].batmonitor.dc_per != 1) && g_parallel_enable && g_host_modbus_id == monitor_para[index].modbus_id)
        {
            g_battery_selfmode_is_same = 0;
        }

        printf("\n Set battery, g_battery_selfmode_is_same :%d\n", g_battery_selfmode_is_same);

        //--------------------------------------//

        if (getJsonNum(&tmpint, "muf", setbattery) == 0)
        {
            monitor_para[index].batmonitor.up1 = tmpint;
            b_info = 1;
        }
        if (getJsonNum(&tmpint, "mod", setbattery) == 0)
        {
            monitor_para[index].batmonitor.uu2 = tmpint;
            b_info = 1;
        }
        if (getJsonNum(&tmpint, "num", setbattery) == 0)
        {
            monitor_para[index].batmonitor.up2 = tmpint;
            b_info = 1;
        }

        if (getJsonNum(&mdchg_max, "discharge_max", setbattery) == 0)
        {
            monitor_para[index].batmonitor.dchg_max = mdchg_max;
            b_info = 1;
        }
        if (getJsonNum(&mchg_max, "charge_max", setbattery) == 0)
        {
            monitor_para[index].batmonitor.chg_max = mchg_max;
            b_info = 1;
        }

        // printf("\n------index:%d--invSn:%s-----------  BBBBBBBBBBBBBBBBBB ----type:%d,mod_r:%d,muf:%d,mod:%d,num:%d--------\n",
        //        index, monitor_para[index].sn,
        //        monitor_para[index].batmonitor.dc_per, monitor_para[index].batmonitor.uu1,
        //        monitor_para[index].batmonitor.up1, monitor_para[index].batmonitor.uu2, monitor_para[index].batmonitor.up2);

        write_global_var_to_nvs(PARA_CONFIG, &monitor_para);

        if (b_info == 1)
        {
            task_inv_msg_arr[index] |= MSG_SET_BATTERY_INDEX;
        }
        else
        {
            task_inv_msg_arr[index] |= MSG_SET_RUN_MODE_INDEX; // only set running mode
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////
int write_meter_configuration(cJSON *value)
{

    ASW_LOGI("\n------DEBUG Print------ write_meter_configuration -----\n ");
    int tmp_int = 0;
    MonitorPara monitor_para = {0};

    //////////////// lanstick-MultilInv ///////////////////
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    /** 如果字段不存在，此API不会改写变量*/
    getJsonNum(&monitor_para.adv.meter_enb, "enb", value);
    getJsonNum(&monitor_para.adv.meter_mod, "mod", value);
    getJsonNum(&monitor_para.adv.meter_target, "target", value);
    getJsonNum(&monitor_para.adv.meter_regulate, "regulate", value);
    getJsonNum(&monitor_para.adv.meter_enb_PF, "enb_PF", value);
    getJsonNum(&monitor_para.adv.meter_target_PF, "target_PF", value);
    if (getJsonNum(&tmp_int, "abs", value) == 0)
    {
        monitor_para.adv.regulate_abs = tmp_int;
    }

    // if(abs==1&& target ==0)
#if TRIPHASE_ARM_SUPPORT

    if (monitor_para.adv.meter_regulate == 10 && monitor_para.adv.regulate_abs == 1 && monitor_para.adv.meter_target == 0)
    {

        if (getJsonNum(&tmp_int, "offset", value) == 0) // offset改动，要保证在0~100的范围内
        {
            monitor_para.adv.regulate_offset = tmp_int;
            if (tmp_int <= 0 || tmp_int > 100)
            {
                monitor_para.adv.regulate_offset = 5;
            }
        }
        else
        {
            monitor_para.adv.regulate_offset = 0;
        }
    }
    else
    {
        monitor_para.adv.regulate_offset = 0;
    }

#else
    if (getJsonNum(&tmp_int, "offset", value) == 0)
    {
        monitor_para.adv.regulate_offset = tmp_int;
    }

#endif
    ASW_LOGI("\n------DEBUG Print------ set meter config -----\n ");
    // monitor_para.adv.meter_mod = 100;
    ASW_LOGI("enb: %d\n", monitor_para.adv.meter_enb);
    ASW_LOGI("mod: %d\n", monitor_para.adv.meter_mod);
    ASW_LOGI("target: %d\n", monitor_para.adv.meter_target);
    ASW_LOGI("regulate: %d\n", monitor_para.adv.meter_regulate);
    ASW_LOGI("enb_PF: %d\n", monitor_para.adv.meter_enb_PF);
    ASW_LOGI("target_PF: %d\n", monitor_para.adv.meter_target_PF);

    monitor_para.adv.meter_day = get_current_days();

    write_global_var_to_nvs(METER_CONTROL_CONFIG, &monitor_para);
    // send_cgi_msg(60, "clearmeter", 9, NULL);
    return 0;
}
/////////////////////////////  LanStick-MultilInv + //////////////////////

int write_battery_define(cJSON *value)
{
    // Bat_Monitor_arr_t monitor_para = {0};
    // Bat_Schdle_arr_t schedule_bat = {0};

    ScheduleBat schedule_bat = {0};

    read_global_var(PARA_SCHED_BAT, &schedule_bat);
    int num = 0;
    int i = 0;
    int j = 0;

    /////////////////////////////////////////////////////////////////////
    // char crt_psn[33];
    // getJsonStr(crt_psn, "sn", sizeof(crt_psn), value);
    // int index = -1;
    // index = asw_get_index_byPsn(crt_psn);

    if (getJsonNum(&num, "Pin", value) == 0)
    {
        // monitor_para[index].batmonitor.fq_t1 = num;

        schedule_bat.fq_t1 = num;
    }

    if (getJsonNum(&num, "Pout", value) == 0)
    {
        // monitor_para[index].batmonitor.ov_t1 = num;
        schedule_bat.ov_t1 = num;
    }
    ASW_LOGI("Pin Pout parsed ok\n");

    /////////////////////////////////////////////////////////////////////

    // TGL MARK TODO 多机并联自定义模式充放电时间及功率使用广播全部设置一致
    char *dayList[] = {"Sun", "Mon", "Tus", "Wen", "Thu", "Fri", "Sat"};
    for (j = 0; j < 7; j++)
    {
        /** 检查存在性*/
        if (cJSON_HasObjectItem(value, dayList[j]))
        {
            cJSON *arr = cJSON_GetObjectItem(value, dayList[j]);
            int arr_size = cJSON_GetArraySize(arr);
            arr_size = (arr_size > MAX_BATTERY_PLAN) ? MAX_BATTERY_PLAN : arr_size;
            for (i = 0; i < arr_size; i++)
            {
                cJSON *pItem = cJSON_GetArrayItem(arr, i);
                if (pItem != NULL)
                {
                    schedule_bat.daySchedule[j].time_sch[i] = pItem->valueint;

                    // ASW_LOGI("**********write_battery_define->schedule_bat.daySchedule[%d].time_sch[%d]:%d\r\n", j, i, pItem->valueint);
                }
            }
        }
        else
        {
            ASW_LOGI("Not found Sun Mon Tus ...\n");
        }
    }
    write_global_var_to_nvs(PARA_SCHED_BAT, &schedule_bat);
    // task_inv_msg |= MSG_GET_SAFETY_INDEX;

    g_task_inv_broadcast_msg |= MSG_BRDCST_GET_SAFETY_INDEX;
    // task_inv_msg_arr[index] |= MSG_GET_SAFETY_INDEX;

    return 0;
}

//////////////////////  Lanstick-MultilInv ///////////////
int read_battery_define(int day_idx, char *buffer)
{
    // MonitorPara monitor_para = {0};
    // ScheduleBat schedule_bat = {0};
    /////////////////////////  LanStick-MultivInv //////////////////////
    Bat_Monitor_arr_t monitor_para = {0};
    // Bat_Schdle_arr_t schedule_bat = {0};
    ScheduleBat schedule_bat = {0};

    read_global_var(PARA_CONFIG, &monitor_para);
    read_global_var(PARA_SCHED_BAT, &schedule_bat);
    char *dayNames[] = {"Sun", "Mon", "Tus", "Wen", "Thu", "Fri", "Sat"};
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;

    cJSON_AddNumberToObject(res, "Pin", schedule_bat.fq_t1);
    cJSON_AddNumberToObject(res, "Pout", schedule_bat.ov_t1);

    ASW_LOGI("\n--------------  getdefine --------------[%d]\n", day_idx);

    if (day_idx >= 0 && day_idx <= 6)
    {
        cJSON *dayPlanArr = cJSON_AddArrayToObject(res, dayNames[day_idx]);
        cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[0]);
        cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[1]);
        cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[2]);
        cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[3]);
        cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[4]);
        cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[5]);
    }
    else
    {
        for (day_idx = 1; day_idx < 2; day_idx++)
        {

            cJSON *dayPlanArr = cJSON_AddArrayToObject(res, dayNames[day_idx]);
            cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[0]);
            cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[1]);
            cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[2]);
            cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[3]);
            cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[4]);
            cJSON_AddNumberToObject(dayPlanArr, "", schedule_bat.daySchedule[day_idx].time_sch[5]);
        }
    }

    msg = cJSON_PrintUnformatted(res);
    memcpy(buffer, msg, strlen(msg));
    free(msg);
    cJSON_Delete(res);
    return 0;
}
//////////////////////////////////////////////////////////////////////
/*******************************************************************
 *                           储能rrpc处理
 * *****************************************************************
 */
int parse_estore_mqtt_msg_rrpc(char *rpc_topic, int rpc_len, void *payload, int data_len)
{
    char *recv_ok = "{\"dat\":\"ok\"}";
    char *recv_er = "{\"dat\":\"err\"}";

    ASW_LOGI("\n----------  DEBUG PRINT --parse_estore_mqtt_msg_rrpc-------\n");
    printf(" topic:%.*s,payload:%.*s", rpc_len, rpc_topic, data_len, (char *)payload);
    ASW_LOGI("\n----------  DEBUG PRINT --parse_estore_mqtt_msg_rrpc-------\n");

    if (strlen(payload) < 3)
        return -1;
    cJSON *json;
    json = cJSON_Parse(payload);
    if (json == NULL)
    {
        return -1;
    }

    char resp_topic[256] = {0};
    if (get_rrpc_restopic(rpc_topic, rpc_len, resp_topic) < 0)
    {
        ASW_LOGI("mqtt rrpc topic too long \n");
        // asw_mqtt_publish(resp_topic, (uint8_t *)recv_er, strlen(recv_er), 0);
        cJSON_Delete(json);
        return -1;
    }

    ////////////////////////////////////////////////////////////

    if (cJSON_HasObjectItem(json, "getdevdata") == 1)
    {
        int getdevdata = -1;
        getJsonNum(&getdevdata, "getdevdata", json);
        if (getdevdata == 1)
        {
            cJSON *res = cJSON_CreateObject();
            char time[30] = {0};
            get_time(time, sizeof(time));
            cJSON_AddStringToObject(res, "time", time);
            cJSON *inverter = cJSON_AddArrayToObject(res, "inverter");
            cJSON *meter = cJSON_AddArrayToObject(res, "meter");
            cJSON *battery = cJSON_AddArrayToObject(res, "battery");
            cJSON *eps = cJSON_AddArrayToObject(res, "EPS");

            ////////////////// LanStick MultilInv //////////////////////////////////////////

            Bat_arr_t m_bat_arr_data = {0};
            read_global_var(GLOBAL_BATTERY_DATA, &m_bat_arr_data);

            for (int j = 0; j < INV_NUM; j++)
            {
                if (cld_inv_arr[j].status == 1) // if online
                {
                    cJSON_AddNumberToObject(inverter, "", cld_inv_arr[j].invdata.e_today);
                    cJSON_AddNumberToObject(inverter, "", cld_inv_arr[j].invdata.status);
                    cJSON_AddNumberToObject(inverter, "", cld_inv_arr[j].invdata.pac);
                    cJSON_AddNumberToObject(inverter, "", cld_inv_arr[j].invdata.error);

                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.ppv_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.etoday_pv_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.status_comm_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.status_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.error1_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.p_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.t_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.SOC_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.SOH_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.etd_in_batt);
                    cJSON_AddNumberToObject(battery, "", m_bat_arr_data[j].batdata.etd_out_batt);

                    cJSON_AddNumberToObject(eps, "", m_bat_arr_data[j].batdata.p_ac_EPS_batt);
                    cJSON_AddNumberToObject(eps, "", m_bat_arr_data[j].batdata.etd_EPS_batt);
                }
            }

            meter_data_t inv_meter = {0};
            read_global_var(GLOBAL_METER_DATA, &inv_meter);
            cJSON_AddNumberToObject(meter, "", (int)inv_meter.invdata.pac);
            cJSON_AddNumberToObject(meter, "", inv_meter.invdata.e_in_today);
            cJSON_AddNumberToObject(meter, "", inv_meter.invdata.e_out_today);

            char *msg = NULL;
            msg = cJSON_PrintUnformatted(res);
            asw_mqtt_publish(resp_topic, msg, strlen(msg), 0);
            free(msg);
            cJSON_Delete(res);
        }
    }
    else if (cJSON_HasObjectItem(json, "getdefine") == 1)
    {
        int day_idx = -1;
        getJsonNum(&day_idx, "getdefine", json);

        //////////////////  Lanstick-MultilInv ////////////////////
        // char cPsn[33];
        // getJsonStr(cPsn, "psn", sizeof(cPsn), json);
        // int index = -1;
        // // assert(strlen(cPsn) > 0);

        // if (strlen(cPsn) <= 0)
        //     index = 0;
        // else

        //     index = asw_get_index_byPsn(cPsn);

        /////////////////////////////////////////////////////////////
        // char buffer[1536] = {0};
        // read_battery_define(index, day_idx, buffer);

        char buffer[1536] = {0};
        read_battery_define(day_idx, buffer);
        printf("\n---- debug printf: getdefine:%s\n",buffer);
        asw_mqtt_publish(resp_topic, buffer, strlen(buffer), 0);
    }
    else if (cJSON_HasObjectItem(json, "drm") == 1)
    {
        int drm = -1;
        getJsonNum(&drm, "drm", json);

        ////////////////////////////////////////////////////////////////

        if (drm == 1)
        {
            char cPsn[33];
            int index = -1;
            Bat_Monitor_arr_t monitor_para = {0};
            read_global_var(PARA_CONFIG, &monitor_para);

            getJsonStr(cPsn, "psn", sizeof(cPsn), json);

            // assert(strlen(cPsn) > 0);

            if (strlen(cPsn) <= 0)
                index = 0;
            else
                index = asw_get_index_byPsn(cPsn);

            monitor_para[index].batmonitor.active_mode = 1;
            write_global_var_to_nvs(PARA_CONFIG, &monitor_para);
            asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
        }
        else if (drm == 0)
        {

            char cPsn[33];
            int index = -1;
            Bat_Monitor_arr_t monitor_para = {0};
            read_global_var(PARA_CONFIG, &monitor_para);

            getJsonStr(cPsn, "psn", sizeof(cPsn), json);

            // assert(strlen(cPsn) > 0);
            if (strlen(cPsn) <= 0)
                index = 0;
            else
                index = asw_get_index_byPsn(cPsn);

            monitor_para[index].batmonitor.active_mode = 0;
            write_global_var_to_nvs(PARA_CONFIG, &monitor_para);
            asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
            task_inv_msg_arr[index] |= MSG_DRED_DISABLE_INDEX;
        }
        else
        {
            asw_mqtt_publish(resp_topic, recv_er, strlen(recv_er), 0);
        }
    }
    else if (cJSON_HasObjectItem(json, "testmode") == 1) ///// MARK-20221122
    {
        int x_value = -1;
        int x_keeptime = -1;
        test_ctrl_t ctrl = {0};

        cJSON *item = cJSON_GetObjectItem(json, "testmode");

        getJsonNum(&x_value, "value", item);
        getJsonNum(&x_keeptime, "keeptime", item);

        if (x_value == 0 || x_value == 1)
        {
            ctrl.on = x_value;
        }

        if (x_keeptime >= 0 && is_time_valid() == 1 && x_value == 1)
        {
            get_time_by_sec_later(ctrl.deadline, sizeof(ctrl.deadline), x_keeptime);
        }

        write_global_var_to_nvs(GLOBAL_TEST_CTRL, &ctrl);

        asw_mqtt_publish(resp_topic, (char *)recv_ok, strlen(recv_ok), 0);
    }

    else if (cJSON_HasObjectItem(json, "fdbg") == 1 && strlen(cJSON_GetObjectItem(json, "fdbg")->valuestring) > 5)
    {
        char ws[65] = {0};
        char msg[300] = {0};
        // cJSON *item = cJSON_GetObjectItem(json, "ws");
        // strncpy(ws, item->valuestring, 64);
        // item = cJSON_GetObjectItem(json, "message");
        // strncpy(msg, item->valuestring, 300);
        memset(ws, 'R', 64);
        strncpy(msg, cJSON_GetObjectItem(json, "fdbg")->valuestring, 300);

        ASW_LOGI("ms %s ws %s \n", msg, ws);
        memset(rrpc_res_topic, 0, sizeof(rrpc_res_topic));
        memcpy(rrpc_res_topic, resp_topic, strlen(resp_topic));
        send_msg(2, msg, strlen(msg), ws);
    }
    else if (cJSON_HasObjectItem(json, "upgrade") == 1 && cJSON_GetObjectItem(cJSON_GetObjectItem(json, "upgrade"), "type")->valueint > 0) //(!(strcmp(function, "upgrade")))
    {
        cJSON *item = cJSON_GetObjectItem(json, "upgrade");
        char up_type = cJSON_GetObjectItem(item, "type")->valueint;
        ASW_LOGI("update type %d \n", up_type);

        cJSON *item1 = cJSON_GetObjectItem(item, "path");
        char *url0 = cJSON_GetObjectItem(item1, "directory")->valuestring;
        ASW_LOGI("url %s \n", url0);

        cJSON *item2 = cJSON_GetObjectItem(item1, "files");
        ASW_LOGI("file type %d %d \n", item2->type, cJSON_GetArraySize(item2));
        ASW_LOGI("meter str %s \n", cJSON_GetArrayItem(item2, 0)->valuestring);
        char *p[cJSON_GetArraySize(item2)];

        update_url download_url = {0};

        if (cJSON_GetArraySize(item2) == 1)
        {
            p[0] = cJSON_GetArrayItem(item2, 0)->valuestring;

            if (up_type == 1) // item2->type == 1)
            {
                download_url.update_type = 1;
                sprintf(download_url.down_url, "%s/%s", url0, p[0]);
            }
            else
            {
                download_url.update_type = 2;
                sprintf(download_url.down_url, "%s/%s", url0, p[0]);
            }
        }
        asw_mqtt_publish(resp_topic, (char *)recv_ok, strlen(recv_ok), 0); // res update
        ASW_LOGI("dowan_loadurl %s  %d %d\n", download_url.down_url, download_url.update_type, strlen(download_url.down_url));
        ASW_LOGI("RRPCRESP %s  %s \n", resp_topic, recv_ok);

        TaskHandle_t download_task_handle = NULL;
        BaseType_t xReturned;
        xReturned = xTaskCreate(
            download_task,          /* Function that implements the task. */
            "download task",        /* Text name for the task. */
            10240,                  /* Stack size in words, not bytes. 50K = 25600 */
            (void *)&download_url,  /* Parameter passed into the task. */
            6,                      /* Priority at which the task is created. */
            &download_task_handle); /* Used to pass out the created task's handle. */
        if (xReturned != pdPASS)
        {
            ASW_LOGI("create cgi task failed.\n");
        }
    }
    else if (cJSON_HasObjectItem(json, "setpower") == 1)
    {
        if (asw_write_inv_onoff(json) == ASW_OK)
        {
            asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
        }
        else
        {
            asw_mqtt_publish(resp_topic, recv_er, strlen(recv_er), 0);
        }
    }
    else if (cJSON_HasObjectItem(json, "setbattery") == 1)
    {
        asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
        cJSON *setbattery = cJSON_GetObjectItem(json, "setbattery");
        write_battery_configuration(setbattery);
    }
    else if (cJSON_HasObjectItem(json, "setdefine") == 1)
    {
        asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
        cJSON *value = cJSON_GetObjectItem(json, "setdefine");
        write_battery_define(value);
    }
    else if (cJSON_HasObjectItem(json, "setmeter") == 1)
    {
        asw_mqtt_publish(resp_topic, recv_ok, strlen(recv_ok), 0);
        cJSON *setmeter = cJSON_GetObjectItem(json, "setmeter");
        write_meter_configuration(setmeter);

        event_group_0 |= PWR_REG_SOON_MASK;
        g_meter_sync = 0;

        /*根据调试打印设置值，判断是否进行数据打印*/
        if (g_asw_debug_enable > 1)
            event_group_0 |= METER_CONFIG_MASK;
        task_inv_meter_msg |= MSG_PWR_ACTIVE_INDEX;  /// debug tgl mark
    }
    /** K60中没有的********************************************************************************************/
    else if (cJSON_HasObjectItem(json, "setscan") == 1 && cJSON_GetObjectItem(json, "setscan")->valueint > 0)
    {
        // char ws=0;
        uint8_t u8msg[128] = {0};
        unsigned int cnt = 10;
        // cJSON *item = cJSON_GetObjectItem(json, "value");

        // ASW_LOGI("cnt int %d \n", cJSON_GetObjectItem(item, "setscan")->valueint);

        // cnt = cJSON_GetObjectItem(item, "setscan")->valueint;
        int byteLen = 1;
        u8msg[0] = cnt;
        ASW_LOGI("cnt int %d  %s\n", cnt, u8msg);
        send_msg(5, (char *)u8msg, byteLen, NULL);
        asw_mqtt_publish(resp_topic, (char *)recv_ok, strlen(recv_ok), 0); // res scan
    }
    else if (cJSON_HasObjectItem(json, "reboot") == 1)
    {
        // int reboot_num=cJSON_GetObjectItem(json, "reboot")->valueint;
        char ws[65] = {0};
        char msg[300] = {0};
        // cJSON *item = cJSON_GetObjectItem(json, "ws");
        // strncpy(ws, item->valuestring, 64);
        // item = cJSON_GetObjectItem(json, "message");
        // strncpy(msg, item->valuestring, 300);
        memset(ws, 'U', 64);
        // strncpy(msg, cJSON_GetObjectItem(json, "fdbg")->valuestring, 300);
        msg[0] = cJSON_GetObjectItem(json, "reboot")->valueint;
        ASW_LOGI("ms %d ws %s \n", msg[0], ws);
        // memcpy(rrpc_res_topic, resp_topic, strlen(resp_topic));
        send_msg(99, msg, 1, ws);
        asw_mqtt_publish(resp_topic, (char *)recv_ok, strlen(recv_ok), 0);
    }
    else if (cJSON_HasObjectItem(json, "factory_reset") == 1)
    {
        asw_mqtt_publish(resp_topic, (char *)recv_ok, strlen(recv_ok), 0);
        factory_initial();
    }
    else if (cJSON_HasObjectItem(json, "get_systime") == 1)
    {
        char now_timex[30] = {0};
        char ipx[30] = {0};
        char bufx[100] = {0};

        get_time(now_timex, sizeof(now_timex));
        get_sta_ip(ipx);
        sprintf(bufx, "%s ip:%s ", now_timex, ipx);

        asw_mqtt_publish(resp_topic, (char *)bufx, strlen(bufx), 0);
    }
    else if (cJSON_HasObjectItem(json, "get_invsts") == 1)
    {
        char bufx[60] = {0};
        sprintf(bufx, "inverter comm error %d ", inv_comm_error);

        asw_mqtt_publish(resp_topic, (char *)bufx, strlen(bufx), 0);
    }
    else if (cJSON_HasObjectItem(json, "get_syslog") == 1)
    {
        char buff[1024] = {0};
        // get_sys_log(bufx);
        get_all_log(buff);
        ASW_LOGI("rrpcsyslog %d--%s \n", strlen(buff), buff);

        if (strlen(buff))
            asw_mqtt_publish(resp_topic, (char *)buff, strlen(buff), 0);
        else
            asw_mqtt_publish(resp_topic, "no log", strlen("no log"), 0);
    }
    else if (cJSON_HasObjectItem(json, "get_hostcom") == 1)
    {
        char buf[128] = {0};
        char puf[128] = {0};
        get_hostcomm(buf, puf);

        char bufx[312] = {0};
        sprintf(bufx, "hcomm:%spcomm:%srsh:%d", buf, puf, get_rssi());

        if (strlen(buf))
            asw_mqtt_publish(resp_topic, (char *)bufx, strlen(bufx), 0);
        else
            asw_mqtt_publish(resp_topic, "no hcomm", strlen("no hcomm"), 0);
    }
    else if (cJSON_HasObjectItem(json, "setserver") == 1)
    {
        cJSON *new_server = cJSON_GetObjectItem(json, "setserver");

        setting_new_server(new_server);
        asw_mqtt_publish(resp_topic, (char *)recv_ok, strlen(recv_ok), 0);
        sleep(5);
        esp_restart();
    }

    cJSON_Delete(json);
    return 0;
}

////////////////// Lanstick-MultilInv ////////////////////
int asw_get_index_byPsn(char *psn)
{

    /////////////////////
    int index = -1;

    // if (strlen(psn) <= 0)
    // {
    //     ESP_LOGW("-- get index by psn Warn --", " the psn is null!!!");
    //     return 0;
    // }

    Bat_arr_t m_battery_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &m_battery_data);

    for (uint8_t i = 0; i < g_num_real_inv; i++)
    {
        if (strcmp(m_battery_data[i].sn, psn) == 0)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        index = 0;
        ESP_LOGW("---- asw_get_index_byPsn ---", "cannot find the inv:%s in g_bat_arr", psn);
    }

    return index;
}
