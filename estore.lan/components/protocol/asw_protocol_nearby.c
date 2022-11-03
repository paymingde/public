#include "asw_protocol_nearby.h"

#include "data_process.h"
#include "asw_nvs.h"
#include "utility.h"
#include "meter.h"
#include "wifi_sta_server.h"
#include "asw_job_http.h"

#include "estore_cld.h"

// #include "asw_ap_prov.h"
// extern cJSON *scan_ap_res;
static const char *TAG = "asw_protocol_nearby";

/* Eng.Stg.Mch-Lanstick +*/

static meter_data_t m_prt_inv_meter = {0}; // g_inv_meter->m_prt_inv_meter

///////////  Lanstick-MultilInv ///////////////
static Batt_data m_bat_data = {0}; // bat_data-->m_bat_data
// static InvBat_data m_bat_data_arry = {0};         // bat_data-->m_bat_data

void fresh_before_http_handler(void)
{
    read_global_var(GLOBAL_METER_DATA, &m_prt_inv_meter);

    inv_arr_memcpy(&cgi_inv_arr, &ipc_inv_arr);
}

//--------------------------------------------------------------------------------------
static char *getdev_handle_type_1_fun()
{
    char buf[20] = {0};
    char *msgBuf = NULL;

    Setting_Para setting = {0};
    general_query(NVS_ATE, &setting);

    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);

    cJSON *res = cJSON_CreateObject();

    cJSON_AddStringToObject(res, "psn", setting.psn);
    cJSON_AddStringToObject(res, "key", setting.reg_key);
    cJSON_AddNumberToObject(res, "typ", setting.typ);
    cJSON_AddStringToObject(res, "nam", setting.nam);
    cJSON_AddStringToObject(res, "mod", setting.mod);
    cJSON_AddStringToObject(res, "muf", setting.mfr);
    cJSON_AddStringToObject(res, "brd", setting.brw);
    cJSON_AddStringToObject(res, "hw", setting.hw);
    cJSON_AddStringToObject(res, "sw", FIRMWARE_REVISION);
    cJSON_AddStringToObject(res, "wsw", MONITOR_MODULE_VERSION);
    get_time(buf, sizeof(buf));
    cJSON_AddStringToObject(res, "tim", buf);
    cJSON_AddStringToObject(res, "pdk", setting.product_key);
    cJSON_AddStringToObject(res, "ser", setting.device_secret);
    cJSON_AddStringToObject(res, "protocol", CGI_VERSION);

    cJSON_AddStringToObject(res, "ali_ip", setting.host);
    cJSON_AddNumberToObject(res, "ali_port", setting.port);
    ////////////////////////////////////////////////////////
    //-\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\--//
    //判断所有在线逆变器的版本，如果有一个不支持的，则不支持并机模式（主从）
    int8_t miSetHost = 1;
    uint8_t i;

    for (i = 0; i < g_num_real_inv; i++)
    {
        // memcpy(mInv_com_protocol, &(inv_arr[i].regInfo.protocol_ver), 13);
        if (strncmp(cgi_inv_arr[i].regInfo.protocol_ver, PROTOCOL_VERTION_SUPPORT_PARALLEL, sizeof("V2.1.2")) < 0)
        {
            printf("\n---  the inv:%s protocal version%s is not support host--\n", cgi_inv_arr[i].regInfo.sn, cgi_inv_arr[i].regInfo.protocol_ver);
            miSetHost = 0;
            break;
        }
    }
    // if (i == g_num_real_inv)
    //     miSetHost = 1;

    //--\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\-//
    if (miSetHost)
        cJSON_AddNumberToObject(res, "parallel", mt.is_parallel);
    cJSON_AddNumberToObject(res, "ssc", mt.ssc_enable);

    if (is_cgi_has_estore()) //储能机
    {
        // MonitorPara monitor_para = {0};
        // read_global_var(METER_CONTROL_CONFIG, &monitor_para);
        // cJSON_AddNumberToObject(res, "drm", monitor_para.adv.active_mode);
        // cJSON_AddNumberToObject(res, "drm_q", monitor_para.adv.fq_t2);
        printf("--DEBUG PRINT: NOthing TO DE DONE, what will DO???\n");
    }
    // cJSON_AddNumberToObject(res, "meter_en", setting.meter_en);
    // cJSON_AddNumberToObject(res, "meter_mod", setting.meter_mod);
    // cJSON_AddNumberToObject(res, "meter_add", setting.meter_add);

    cJSON_AddNumberToObject(res, "status", g_state_mqtt_connect);

    msgBuf = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);
    return msgBuf;
}
//-------------------------------------------------------
/* 逆变器 */
static char *getdev_handle_type_2_fun()
{
    cJSON *myArr = NULL;
    char *msgBuf = NULL;
    int i = 0;
    int cnt = 0;
    uint16_t host_status = 99;
    cJSON *res = cJSON_CreateObject();
    myArr = cJSON_AddArrayToObject(res, "inv");

    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);

    // if (mt.is_parallel == 0)
    //     host_status = 99;

    for (i = 0; i < g_num_real_inv; i++) // 220726 -
    // for (i = 0; i < INV_NUM; i++) // 220726 +
    {
        if (strlen(cgi_inv_arr[i].regInfo.sn) < 1)
            continue;
        cJSON *item = cJSON_CreateObject();

        cJSON_AddStringToObject(item, "isn", cgi_inv_arr[i].regInfo.sn);
        cJSON_AddNumberToObject(item, "add", cgi_inv_arr[i].regInfo.modbus_id);
        cJSON_AddNumberToObject(item, "safety", cgi_inv_arr[i].regInfo.safety_type);
        cJSON_AddNumberToObject(item, "rate", cgi_inv_arr[i].regInfo.rated_pwr);
        cJSON_AddStringToObject(item, "msw", cgi_inv_arr[i].regInfo.msw_ver);
        cJSON_AddStringToObject(item, "ssw", cgi_inv_arr[i].regInfo.ssw_ver);
        cJSON_AddStringToObject(item, "tsw", cgi_inv_arr[i].regInfo.tmc_ver);
        cJSON_AddNumberToObject(item, "pac", cgi_inv_arr[i].invdata.pac);
        cJSON_AddNumberToObject(item, "etd", cgi_inv_arr[i].invdata.e_today);
        cJSON_AddNumberToObject(item, "eto", cgi_inv_arr[i].invdata.e_total);
        cJSON_AddNumberToObject(item, "err", cgi_inv_arr[i].invdata.error);
        cJSON_AddStringToObject(item, "cmv", cgi_inv_arr[i].regInfo.protocol_ver);
        cJSON_AddNumberToObject(item, "mty", cgi_inv_arr[i].regInfo.mach_type);

        /////////////////////////////////////////////////
        cJSON_AddStringToObject(item, "model", cgi_inv_arr[i].regInfo.mode_name);

#if TRIPHASE_ARM_SUPPORT
        cJSON_AddStringToObject(item, "armssv", cgi_inv_arr[i].regInfo.csw_ver);

#endif

        /////////////////////////////////

        if (mt.is_parallel)
        {
#if PARALLEL_HOST_SET_WITH_SN
            if (strcmp(cgi_inv_arr[i].regInfo.sn, mt.host_psn) == 0)
#else
            if (cgi_inv_arr[i].regInfo.modbus_id == mt.host_adr)
#endif
                host_status = 1;
            else
                host_status = 0;
            ////////////////////////////////////
        }
        else
        {
            host_status = 99;
        }

        cJSON_AddNumberToObject(item, "host", host_status);
        ////////////////////////////////////////////////

        cJSON_AddItemToArray(myArr, item);
        cnt++;
    }
    cJSON_AddNumberToObject(res, "num", cnt);

    msgBuf = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);

    return msgBuf;
}

//------------------------------------------------------------------------//
/* 防逆流设备 */
static char *getdev_handle_type_3_fun()
{
    cJSON *res = cJSON_CreateObject();
    char *msgBuf = NULL;
    uint8_t i = 0;
    int all_pac = 0;
    int all_prc = 0;
    // int reg_tmp = 5;
    // int abs = 0;
    // int offset = 0;

    //-- Eng.Stg.Mch-lanstick 20220908 +-
    // meter_setdata meter = {0};
    // general_query(NVS_METER_SET, (void *)&meter);

    MonitorPara meter = {0};
    // read_global_var(METER_CONTROL_CONFIG, &meter);
    read_global_var(METER_CONTROL_CONFIG, &meter);

    ASW_LOGE("-----mod %d,enb %d,target %d,reg %d---", meter.adv.meter_mod, meter.adv.meter_enb, meter.adv.meter_target, meter.adv.meter_regulate);

    cJSON_AddNumberToObject(res, "mod", meter.adv.meter_mod);
    cJSON_AddNumberToObject(res, "enb", meter.adv.meter_enb);

    cJSON_AddNumberToObject(res, "exp_m", meter.adv.meter_target);

    cJSON_AddNumberToObject(res, "regulate", meter.adv.meter_regulate);
    cJSON_AddNumberToObject(res, "enb_PF", meter.adv.meter_enb_PF);
    cJSON_AddNumberToObject(res, "target_PF", meter.adv.meter_target_PF);

    // add abs, offset  Eng.Stg.Mch-lanstick
    // if (meter.adv.meter_regulate > 0x100)
    // {
    //     abs = 1;
    //     offset = meter.adv.meter_regulate - 0x100;
    // }
    cJSON_AddNumberToObject(res, "abs", meter.adv.regulate_abs);
    cJSON_AddNumberToObject(res, "abs_offset", meter.adv.regulate_offset);

    for (i = 0; i < g_num_real_inv; i++)
    {
        if (strlen((char *)cgi_inv_arr[i].regInfo.sn) < 1)
            continue;
        all_pac += cgi_inv_arr[i].invdata.pac;
        all_prc += cgi_inv_arr[i].regInfo.rated_pwr;
    }
    cJSON_AddNumberToObject(res, "total_pac", all_pac);
    cJSON_AddNumberToObject(res, "total_fac", all_prc);
    cJSON_AddNumberToObject(res, "meter_pac", (int)m_prt_inv_meter.invdata.pac);

    msgBuf = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    return msgBuf;
}

//------------------------------------------------------------------//
static char *getdev_handle_type_4_fun(char *psn)
{
    char *msgBuf = NULL;
    cJSON *res = cJSON_CreateObject();

    Bat_Monitor_arr_t monitor_para = {0};

    read_global_var(PARA_CONFIG, &monitor_para);

    /////////////////////////////////////////////////////////////////////
    // assert(strlen(psn) > 0);

    int index = -1;

    if (strlen(psn) <= 0)
        index = 0;
    else
        index = asw_get_index_byPsn(psn);

    cJSON_AddStringToObject(res, "isn", monitor_para[index].sn);

    cJSON_AddNumberToObject(res, "stu_r", monitor_para[index].batmonitor.uu4);
    cJSON_AddNumberToObject(res, "type", monitor_para[index].batmonitor.dc_per);
    cJSON_AddNumberToObject(res, "mod_r", monitor_para[index].batmonitor.uu1);
    cJSON_AddNumberToObject(res, "muf", monitor_para[index].batmonitor.up1);
    cJSON_AddNumberToObject(res, "mod", monitor_para[index].batmonitor.uu2);
    cJSON_AddNumberToObject(res, "num", monitor_para[index].batmonitor.up2);

    cJSON_AddNumberToObject(res, "discharge_max", monitor_para[index].batmonitor.dchg_max);
    cJSON_AddNumberToObject(res, "charge_max", monitor_para[index].batmonitor.chg_max);

    msgBuf = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);

    return msgBuf;
}

//------------------------------------------------------------------//
static char *getdev_handle_type_default_fun()
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    char buf[20] = {0};

    Setting_Para setting = {0};
    general_query(NVS_ATE, &setting);

    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);

    cJSON_AddStringToObject(res, "psn", setting.psn);
    cJSON_AddStringToObject(res, "key", setting.reg_key);
    cJSON_AddNumberToObject(res, "typ", setting.typ);
    cJSON_AddStringToObject(res, "nam", setting.nam);
    cJSON_AddStringToObject(res, "mod", setting.mod);
    cJSON_AddStringToObject(res, "muf", setting.mfr);
    cJSON_AddStringToObject(res, "brd", setting.brw);
    cJSON_AddStringToObject(res, "hw", setting.hw);
    cJSON_AddStringToObject(res, "sw", FIRMWARE_REVISION);
    cJSON_AddStringToObject(res, "wsw", MONITOR_MODULE_VERSION);
    get_time(buf, sizeof(buf));
    cJSON_AddStringToObject(res, "tim", buf);
    cJSON_AddStringToObject(res, "pdk", setting.product_key);
    cJSON_AddStringToObject(res, "ser", setting.device_secret);
    cJSON_AddStringToObject(res, "protocol", CGI_VERSION);

    cJSON_AddStringToObject(res, "ali_ip", setting.host);
    cJSON_AddNumberToObject(res, "ali_port", setting.port);

    cJSON_AddNumberToObject(res, "status", g_state_mqtt_connect); //[tgl mark] change
                                                                  ////////////////////////////////////////////////////////////////////////
    //判断所有在线逆变器的版本，如果有一个不支持的，则不支持并机模式（主从）
    int8_t miSetHost = 1;
    uint8_t i;

    for (i = 0; i < g_num_real_inv; i++)
    {
        // memcpy(mInv_com_protocol, &(inv_arr[i].regInfo.protocol_ver), 13);
        if (strncmp(cgi_inv_arr[i].regInfo.protocol_ver, PROTOCOL_VERTION_SUPPORT_PARALLEL, sizeof("V2.1.2")) < 0)
        {
            printf("\n---  the inv:%s protocal version%s is not support host--\n", cgi_inv_arr[i].regInfo.sn, cgi_inv_arr[i].regInfo.protocol_ver);
            miSetHost = 0;
            break;
        }
    }
    // if (i == g_num_real_inv)
    //     miSetHost = 1;
    ///////////////////////////////////////////////////////////
    if (miSetHost)
        cJSON_AddNumberToObject(res, "parallel", mt.is_parallel); // add for new

    cJSON_AddNumberToObject(res, "ssc", mt.ssc_enable); // add for new

    msg = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);
    return msg;
}
//--------------------------------------------------------------------------------
static char *getdevdata_handle_type_2_fun(char *inv_sn)
{
    char *msg = NULL;
    uint8_t i = 0;

    for (i = 0; i < g_num_real_inv; i++)
    {
        // #if DEBUG_PRINT_ENABLE

        ASW_LOGI("\n==========get devdata debug========\n");
        ASW_LOGI("current sn=%s\n", inv_sn);
        ASW_LOGI(" sn[%d]=%s", i, cgi_inv_arr[i].regInfo.sn);
        // #endif
        if (strcmp(cgi_inv_arr[i].regInfo.sn, inv_sn) == 0)
            break;
    }

    if (i >= g_num_real_inv)
    {
        ASW_LOGW("device not found");
        return NULL;
    }
    // #if DEBUG_PRINT_ENABLE

    ASW_LOGI("cgi_inv_arr[%d].PV_cur_voltg[0].iVol=%d\n", i, cgi_inv_arr[i].invdata.PV_cur_voltg[0].iVol);

    // #endif
    cJSON *res = cJSON_CreateObject();

    cJSON_AddNumberToObject(res, "flg", cgi_inv_arr[i].invdata.status);
    char time_tmp[64] = {0};
    fileter_time(cgi_inv_arr[i].invdata.time, time_tmp);
    cJSON_AddStringToObject(res, "tim", time_tmp);
    cJSON_AddNumberToObject(res, "tmp", cgi_inv_arr[i].invdata.col_temp);
    cJSON_AddNumberToObject(res, "fac", cgi_inv_arr[i].invdata.fac);
    cJSON_AddNumberToObject(res, "pac", cgi_inv_arr[i].invdata.pac);
    cJSON_AddNumberToObject(res, "sac", cgi_inv_arr[i].invdata.sac);
    cJSON_AddNumberToObject(res, "qac", cgi_inv_arr[i].invdata.qac);
    cJSON_AddNumberToObject(res, "eto", cgi_inv_arr[i].invdata.e_total);
    cJSON_AddNumberToObject(res, "etd", cgi_inv_arr[i].invdata.e_today);
    cJSON_AddNumberToObject(res, "hto", cgi_inv_arr[i].invdata.h_total);
    cJSON_AddNumberToObject(res, "pf", cgi_inv_arr[i].invdata.cosphi);
    cJSON_AddNumberToObject(res, "wan", cgi_inv_arr[i].invdata.warn[0]);
    cJSON_AddNumberToObject(res, "err", cgi_inv_arr[i].invdata.error);

    cJSON *myarr = NULL;
    int j;
    int ac_num = cgi_inv_arr[i].regInfo.type - 0X30; // inv_ptr->type
    int pv_num = cgi_inv_arr[i].regInfo.pv_num;
    int str_num = cgi_inv_arr[i].regInfo.str_num;

    myarr = cJSON_AddArrayToObject(res, "vac");
    for (j = 0; j < ac_num; j++)
    {
        int var = cgi_inv_arr[i].invdata.AC_vol_cur[j].iVol;
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "iac");
    for (j = 0; j < ac_num; j++)
    {
        int var = cgi_inv_arr[i].invdata.AC_vol_cur[j].iCur;
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "vpv");
    for (j = 0; j < pv_num; j++)
    {
        int var = cgi_inv_arr[i].invdata.PV_cur_voltg[j].iVol;
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "ipv");
    for (j = 0; j < pv_num; j++)
    {
        int var = cgi_inv_arr[i].invdata.PV_cur_voltg[j].iCur;
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "str");
    for (j = 0; j < str_num; j++)
    {
        int var = cgi_inv_arr[i].invdata.istr[j];
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    msg = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);
    return msg;
}

//----------------------------------------------
static char *getdevdata_handle_type_3_fun()
{
    cJSON *res = cJSON_CreateObject(); // Inverter g_inv_meter = {0};
    char *msg = NULL;

    ////////////////////////////// Eng.Stg.Mch-lanstick ////////////////////////
    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    if (is_cgi_has_estore() == 1)
    {
        cJSON_AddNumberToObject(res, "flg", 1);
    }
    else
    {
        cJSON_AddNumberToObject(res, "flg", m_prt_inv_meter.invdata.status);
    }

    cJSON_AddStringToObject(res, "tim", m_prt_inv_meter.invdata.time);
    // printf("inv_meter.invdata.pac %d \n", inv_meter.invdata.pac);
    cJSON_AddNumberToObject(res, "pac", (int)m_prt_inv_meter.invdata.pac);
    cJSON_AddNumberToObject(res, "itd", m_prt_inv_meter.invdata.e_in_today); // 0.01kwh
    cJSON_AddNumberToObject(res, "otd", m_prt_inv_meter.invdata.e_out_today);
    cJSON_AddNumberToObject(res, "iet", m_prt_inv_meter.invdata.e_in_total / 10); // 0.1kwh
    cJSON_AddNumberToObject(res, "oet", m_prt_inv_meter.invdata.e_out_total / 10);
    cJSON_AddNumberToObject(res, "mod", monitor_para.adv.meter_mod);
    // cJSON_AddNumberToObject(res, "mod", 100);
    cJSON_AddNumberToObject(res, "enb", monitor_para.adv.meter_enb);

    msg = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);
    return msg;
}

//----------------------------------------------//

static char *getdevdata_handle_type_4_fun(char *inv_sn)
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    uint8_t i;
    ///////////////////////////////////////////////////
    Bat_arr_t mBattArray_data = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &mBattArray_data);
    for (i = 0; i < g_num_real_inv; i++)
    {
        // #if DEBUG_PRINT_ENABLE

        // printf("\n==========get devdata debug handle ========\n");
        ASW_LOGI("current sn=%s\n", inv_sn);
        ASW_LOGI(" sn[%d]=%s", i, mBattArray_data[i].sn);
        // printf("\n==========get devdata debug handle========\n");
        // #endif

        if (strcmp(mBattArray_data[i].sn, inv_sn) == 0)
            break;
    }

    if (i >= g_num_real_inv)
    {
        ASW_LOGW("device not found");
        i = 0;
    }
    ///////////////// Eng.Stg.Mch-Lanstick ////////////

    cJSON_AddNumberToObject(res, "flg", 1);
    char sbuf[24] = {0};
    get_time_compact(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(res, "tim", sbuf); //???

    cJSON_AddNumberToObject(res, "ppv", mBattArray_data[i].batdata.ppv_batt);
    cJSON_AddNumberToObject(res, "etdpv", mBattArray_data[i].batdata.etoday_pv_batt);
    cJSON_AddNumberToObject(res, "etopv", mBattArray_data[i].batdata.etotal_pv_batt);
    cJSON_AddNumberToObject(res, "cst", mBattArray_data[i].batdata.status_comm_batt);
    cJSON_AddNumberToObject(res, "bst", mBattArray_data[i].batdata.status_batt);
    cJSON_AddNumberToObject(res, "eb1", mBattArray_data[i].batdata.error1_batt);

    cJSON_AddNumberToObject(res, "wb1", mBattArray_data[i].batdata.warn1_batt);
    cJSON_AddNumberToObject(res, "vb", mBattArray_data[i].batdata.v_batt);
    cJSON_AddNumberToObject(res, "cb", mBattArray_data[i].batdata.i_batt);
    cJSON_AddNumberToObject(res, "pb", mBattArray_data[i].batdata.p_batt);
    cJSON_AddNumberToObject(res, "tb", mBattArray_data[i].batdata.t_batt);
    cJSON_AddNumberToObject(res, "soc", mBattArray_data[i].batdata.SOC_batt);
    cJSON_AddNumberToObject(res, "soh", mBattArray_data[i].batdata.SOH_batt);

    cJSON_AddNumberToObject(res, "cli", mBattArray_data[i].batdata.i_in_limit_batt);
    cJSON_AddNumberToObject(res, "clo", mBattArray_data[i].batdata.i_out_limit_batt);
    cJSON_AddNumberToObject(res, "ebi", mBattArray_data[i].batdata.etd_in_batt);
    cJSON_AddNumberToObject(res, "ebo", mBattArray_data[i].batdata.etd_out_batt);
    cJSON_AddNumberToObject(res, "eaci", mBattArray_data[i].batdata.etd_AC_used_batt);
    cJSON_AddNumberToObject(res, "eaco", mBattArray_data[i].batdata.etd_AC_feed_batt);
    cJSON_AddNumberToObject(res, "vesp", mBattArray_data[i].batdata.v_EPS_batt);

    cJSON_AddNumberToObject(res, "cesp", mBattArray_data[i].batdata.i_EPS_batt);
    cJSON_AddNumberToObject(res, "fesp", mBattArray_data[i].batdata.f_EPS_batt);
    cJSON_AddNumberToObject(res, "pesp", mBattArray_data[i].batdata.p_ac_EPS_batt);
    cJSON_AddNumberToObject(res, "rpesp", mBattArray_data[i].batdata.p_ra_EPS_batt);
    cJSON_AddNumberToObject(res, "etdesp", mBattArray_data[i].batdata.etd_EPS_batt);
    cJSON_AddNumberToObject(res, "etoesp", mBattArray_data[i].batdata.eto_EPS_batt);

#if TRIPHASE_ARM_SUPPORT //增加3相支持

    cJSON *myarr = NULL;
    uint8_t j;
    myarr = cJSON_AddArrayToObject(res, "v_esp");
    for (j = 0; j < 3; j++)
    {
        uint16_t var = mBattArray_data[i].batdata.v_i_phase_ESP[j].iVol;
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "i_esp");
    for (j = 0; j < 3; j++)
    {
        uint16_t var = mBattArray_data[i].batdata.v_i_phase_ESP[j].iCur;
        if (var != 0xFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "p_esp");
    for (j = 0; j < 3; j++)
    {
        uint32_t var = mBattArray_data[i].batdata.p_q_phase_ESP[j].pac;
        if (var != 0xFFFFFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "q_esp");
    for (j = 0; j < 3; j++)
    {
        int32_t var = mBattArray_data[i].batdata.p_q_phase_ESP[j].qac;
        if (var != 0xFFFFFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "p_ac");
    for (j = 0; j < 3; j++)
    {
        uint32_t var = mBattArray_data[i].batdata.p_q_phase_AC[j].pac;
        if (var != 0xFFFFFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

    myarr = cJSON_AddArrayToObject(res, "q_ac");
    for (j = 0; j < 3; j++)
    {
        int32_t var = mBattArray_data[i].batdata.p_q_phase_AC[j].qac;
        if (var != 0xFFFFFFFF)
            cJSON_AddNumberToObject(myarr, "", var);
    }

#endif

    msg = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);
    return msg;
}
//----------------------------------------------------//
static char *wlanget_handler_info_1_fun()
{
    wifi_ap_para_t ap_para = {0};
    general_query(NVS_AP_PARA, &ap_para);

    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;

    cJSON_AddStringToObject(res, "mode", "AP");
    cJSON_AddStringToObject(res, "sid", (char *)ap_para.ssid);
    cJSON_AddStringToObject(res, "psw", (char *)ap_para.password);
    cJSON_AddStringToObject(res, "ip", "160.190.0.1");

    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);
    return msg;
}
//-----------------------------------------------------//
static char *wlanget_handler_info_2_fun() //[tgl mark]TODO这段需要改为ethernet
{
    if (g_stick_run_mode == Work_Mode_AP_PROV)
        return NULL;

    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    char *tmp;

    if (g_stick_run_mode == Work_Mode_LAN)
    {
        cJSON_AddStringToObject(res, "mode", "LAN"); // tgl mark ethernet

        // cJSON_AddNumberToObject(res, "srh", 0); // TGL MARK TODO

        if (strlen(my_eth_ip) > 0)
            cJSON_AddStringToObject(res, "ip", my_eth_ip);
        else
            cJSON_AddStringToObject(res, "ip", NULL);

        if (strlen(my_eth_gw) > 0)
            cJSON_AddStringToObject(res, "gtw", my_eth_gw);
        else
            cJSON_AddStringToObject(res, "gtw", NULL);

        if (strlen(my_eth_mk) > 0)
            cJSON_AddStringToObject(res, "msk", my_eth_mk);
        else
            cJSON_AddStringToObject(res, "msk", NULL);
    }
    else if (g_stick_run_mode == Work_Mode_STA)
    {
        tmp = get_ssid();
        cJSON_AddStringToObject(res, "sid", tmp);
        free(tmp);
        cJSON_AddStringToObject(res, "DHCP", NULL);
        cJSON_AddStringToObject(res, "mode", "STA");

        cJSON_AddNumberToObject(res, "srh", get_rssi());

        if (strlen(my_sta_ip) > 0)
            cJSON_AddStringToObject(res, "ip", my_sta_ip);

        if (strlen(my_sta_gw) > 0)
            cJSON_AddStringToObject(res, "gtw", my_sta_gw);
        else
            cJSON_AddStringToObject(res, "gtw", NULL);

        if (strlen(my_sta_mk) > 0)
            cJSON_AddStringToObject(res, "msk", my_sta_mk);
        else
            cJSON_AddStringToObject(res, "msk", NULL);
    }

    cJSON_AddStringToObject(res, "dns_a", NULL);
    cJSON_AddStringToObject(res, "dns", NULL);

    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);
    return msg;
}
//----------------------------------------------------//
static char *wlanget_handler_info_3_fun()
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;

    // V2.0.0 change

    // if (g_stick_run_mode == Work_Mode_AP_PROV)
    //     cJSON_AddStringToObject(res, "mode", "AP");
    // else if (g_stick_run_mode == Work_Mode_STA)
    // {
    //     cJSON_AddStringToObject(res, "mode", "STA");
    //     if (strlen(my_sta_ip) > 0)
    //         cJSON_AddStringToObject(res, "ip", my_sta_ip);

    //     if (strlen(my_sta_gw) > 0)
    //         cJSON_AddStringToObject(res, "gtw", my_sta_gw);
    //     else
    //         cJSON_AddStringToObject(res, "gtw", NULL);

    //     if (strlen(my_sta_mk) > 0)
    //         cJSON_AddStringToObject(res, "msk", my_sta_mk);
    //     else
    //         cJSON_AddStringToObject(res, "msk", NULL);
    // }
    // else
    if (g_stick_run_mode == Work_Mode_LAN)
    {

        cJSON_AddStringToObject(res, "mode", "LAN"); // chagne according cgi new version
        if (strlen(my_eth_ip) > 0)
            cJSON_AddStringToObject(res, "ip", my_eth_ip);

        if (strlen(my_eth_gw) > 0)
            cJSON_AddStringToObject(res, "gtw", my_eth_gw);
        else
            cJSON_AddStringToObject(res, "gtw", NULL);

        if (strlen(my_eth_mk) > 0)
            cJSON_AddStringToObject(res, "msk", my_eth_mk);
        else
            cJSON_AddStringToObject(res, "msk", NULL);
    }
    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);
    return msg;
}
//-----------------------------------------//
static char *wlanget_handler_info_4_fun()
{
    cJSON *res = NULL;
    char *msg = NULL;

    scan_ap_once();
    if (scan_ap_res != NULL)
    {
        res = scan_ap_res;
    }
    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);
    return msg;
}

//-----------------------------------------------//
static char *wlanget_handler_info_default_fun()
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    wifi_ap_para_t ap_para = {0};
    general_query(NVS_AP_PARA, &ap_para);

    if (g_stick_run_mode == Work_Mode_AP_PROV)
    {
        cJSON_AddStringToObject(res, "mode", "AP");
        cJSON_AddStringToObject(res, "sid", (char *)ap_para.ssid);
        // cJSON_AddStringToObject(res, "psw", (char *)ap_para.password);
        cJSON_AddStringToObject(res, "ip", "160.190.0.1");
        cJSON_AddNumberToObject(res, "srh", get_rssi());
    }

    else if (g_stick_run_mode == Work_Mode_STA)
    {
        cJSON_AddNumberToObject(res, "srh", get_rssi());
        cJSON_AddStringToObject(res, "mode", "STA");
        if (strlen(my_sta_ip) > 0)
            cJSON_AddStringToObject(res, "ip", my_sta_ip);

        if (strlen(my_sta_gw) > 0)
            cJSON_AddStringToObject(res, "gtw", my_sta_gw);
        else
            cJSON_AddStringToObject(res, "gtw", NULL);

        if (strlen(my_sta_mk) > 0)
            cJSON_AddStringToObject(res, "msk", my_sta_mk);
        else
            cJSON_AddStringToObject(res, "msk", NULL);
    }
    else if (g_stick_run_mode == Work_Mode_LAN)
    {

        cJSON_AddStringToObject(res, "mode", "LAN"); // chagne according cgi new version
        if (strlen(my_eth_ip) > 0)
            cJSON_AddStringToObject(res, "ip", my_eth_ip);

        if (strlen(my_eth_gw) > 0)
            cJSON_AddStringToObject(res, "gtw", my_eth_gw);
        else
            cJSON_AddStringToObject(res, "gtw", NULL);

        if (strlen(my_eth_mk) > 0)
            cJSON_AddStringToObject(res, "msk", my_eth_mk);
        else
            cJSON_AddStringToObject(res, "msk", NULL);
    }
    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    return msg;
}
//----------------------------------------------------//

static int8_t wlanset_handler_mode_staticip_fun(cJSON *value)
{
    int enable = -1;
    char cEnable[8] = {0};
    getJsonStr(cEnable, "oia", sizeof(cEnable), value);
    enable = atoi(cEnable);
    ASW_LOGI("static enbale: %d\n", enable);
#if STATIC_IP_SET_ENABLE

    uint8_t mstaic_flag = 0;

    if (g_stick_run_mode == Work_Mode_AP_PROV || g_stick_run_mode == Work_Mode_IDLE)
    {
        ESP_LOGW("--- setting static ip info WARN ---", "the work mode is AP or NULL");
        return ASW_FAIL;
    }

    net_static_info_t st_static_info = {0};
    char cIp[16] = {0};
    char cGw[16] = {0};
    char cMk[16] = {0};
    char cDns[16] = {0};

    if (enable)
    {
        st_static_info.enble = 1;

        if (getJsonStr(cIp, "ip", sizeof(cIp), value) != -1)
        {
            sscanf(cIp, "%hd.%hd.%hd.%hd", &st_static_info.ip[0], &st_static_info.ip[1],
                   &st_static_info.ip[2], &st_static_info.ip[3]);
        }
        if (getJsonStr(cMk, "msk", sizeof(cMk), value) != -1)
        {
            sscanf(cMk, "%hd.%hd.%hd.%hd", &st_static_info.mask[0], &st_static_info.mask[1],
                   &st_static_info.mask[2], &st_static_info.mask[3]);
        }
        if (getJsonStr(cGw, "gtw", sizeof(cGw), value) != -1)
        {
            sscanf(cGw, "%hd.%hd.%hd.%hd", &st_static_info.gateway[0], &st_static_info.gateway[1],
                   &st_static_info.gateway[2], &st_static_info.gateway[3]);
        }

        if (getJsonStr(cDns, "dns", sizeof(cGw), value) != -1)
        {
            sscanf(cDns, "%hd.%hd.%hd.%hd", &st_static_info.maindns[0], &st_static_info.maindns[1],
                   &st_static_info.maindns[2], &st_static_info.maindns[3]);
        }

        mstaic_flag = 1;
    }
    else
    {
        st_static_info.enble = 0;

        mstaic_flag = 0;
    }

    st_static_info.work_mode = g_stick_run_mode;
#if DEBUG_PRINT_ENABLE

    printf("\n-------  setting static ip info --[%d]------\n", st_static_info.enble);
    printf("   ip  :%hd.%hd.%d.%hd\n", st_static_info.ip[0], st_static_info.ip[1], st_static_info.ip[2], st_static_info.ip[3]);
    printf("  mask :%hd.%hd.%d.%hd\n", st_static_info.mask[0], st_static_info.mask[1], st_static_info.mask[2], st_static_info.mask[3]);
    printf("gateway:%hd.%hd.%hd.%hd\n", st_static_info.gateway[0], st_static_info.gateway[1], st_static_info.gateway[2], st_static_info.gateway[3]);
    printf("\n-------  setting static ip info --[%d]------\n", st_static_info.work_mode);
#endif
    if (general_add(NVS_NET_STATIC_INFO, &st_static_info) != ASW_OK)
    {
        ESP_LOGE("--- setting static ip info Error ---", "FAILED write the static info to nvs!!");
        return ASW_FAIL;
    }

    printf("\n set the static info , sys will restart...\n");

    if (st_static_info.enble)
    {
        // config_net_static_set();

        return CMD_STATIC_ENABLE;
    }
    else
    {
        // config_net_static_diable_set();
        return CMD_STATIC_DISABLE;
    }

#else
    return -1;
#endif
}

//--------------------------------------------//

#define NUM_SETTING_OPRT 6

static char valueJson[NUM_SETTING_OPRT][20] = {
    "reboot",
    "factory_reset",
    "ufs_format",
    "setscan",
    "parallel",
    "ssc" //光储充功能
};

static int16_t OpCodeCmd[NUM_SETTING_OPRT] = {CMD_REBOOT, CMD_RESET, CMD_FORMAT, CMD_SCAN, CMD_PARALLEL, CMD_SSC_SET};

// static int16_t OpCodeLanSetCmd[2] = {CMD_STATIC_DISABLE , CMD_STATIC_ENABLE};
/////////////////////////////////////////////////////////////////
static int asw_handle_paraller_msg(uint8_t is_parallel, cJSON *value)
{

#if PARALLEL_HOST_SET_WITH_SN

    char pcHostPsn[33] = {0};

    getJsonStr(pcHostPsn, "host_sn", sizeof(pcHostPsn), value);

#else
    uint16_t hostId = 0;

    getJsonNum(&hostId, "host", value);

#endif

    char mInv_com_protocol[13];

    ///   轮寻逆变器版本，有一个不匹配的，就退出
    for (uint8_t i = 0; i < g_num_real_inv; i++)
    {

        /// MARK TGL cgi_inv_arr?inv_arr?
        // if (cgi_inv_arr[i].regInfo.modbus_id == hostId)
        // {
        // memcpy(mInv_com_protocol, &(cgi_inv_arr[i].regInfo.protocol_ver), 13);
        if (strncmp(cgi_inv_arr[i].regInfo.protocol_ver, PROTOCOL_VERTION_SUPPORT_PARALLEL, sizeof("V2.1.2")) < 0)
        {
            ESP_LOGW("--- setting parallel erro ---",
                     "  Failed to write parellel error, cur-protol:%s not support the protocol version <%s", cgi_inv_arr[i].regInfo.protocol_ver, PROTOCOL_VERTION_SUPPORT_PARALLEL);
            return -1;
        }

#if PARALLEL_HOST_SET_WITH_SN
        if (strcmp(pcHostPsn, cgi_inv_arr[i].regInfo.sn) == 0)
        {
            g_host_modbus_id = cgi_inv_arr[i].regInfo.modbus_id;
        }

#endif
        // }
    }
    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);
    mt.is_parallel = is_parallel;
    // uint16_t hostId = 0;
    // getJsonNum(&hostId, "host", value);

#if PARALLEL_HOST_SET_WITH_SN
    memcpy(mt.host_psn, pcHostPsn, sizeof(pcHostPsn));
#else
    mt.host_adr = hostId;
#endif

    g_parallel_enable = mt.is_parallel; //并机模式 1-打开 0-关闭

#if !PARALLEL_HOST_SET_WITH_SN
    g_host_modbus_id = mt.host_adr; //并机模式下主机modbus id
#endif

    printf("\n============  host modbus_id:%d ============\n", g_host_modbus_id);

    write_global_var_to_nvs(METER_CONTROL_CONFIG, &mt);

    printf("----------asw_handle_paraller_msg :%d ", is_parallel);
    g_task_inv_broadcast_msg |= MSG_WRT_SET_HOST_INDEX;

    return 0;
}
//-------------------------------------------------//
static int asw_operation_ssc_handler(uint8_t value)
{
    MonitorPara mt = {0};

    read_global_var(METER_CONTROL_CONFIG, &mt);
    mt.ssc_enable = value;
    if (mt.ssc_enable)
    {
        g_ssc_enable = 1;
    }
    else
    {
        g_ssc_enable = 0;
    }
    write_global_var_to_nvs(METER_CONTROL_CONFIG, &mt);
    printf("----------asw_operation_ssc_handler value:%d write to nvs ", value);
    return 0;
}
//---------------------------------------------//
static int16_t setting_handler_device_1_fun(char *action, cJSON *value)
{
    if (strcmp(action, "settime") == 0)
    {
        char buf[20] = {0};
        getJsonStr(buf, "time", sizeof(buf), value);
        ASW_LOGI("recv time: %s", buf); // recv time: 20201022140813
        if (strlen(buf) > 10)
        {
            set_time_cgi(buf);
        }
        return 0;
    }
    else if (strcmp(action, "setdev") == 0)
    {
        char buf[100] = {0};
        Setting_Para setting = {0};
        wifi_ap_para_t ap_para = {0};
        general_query(NVS_ATE, &setting);

        getJsonStr(buf, "psn", sizeof(buf), value); //[tgl mark]
        ASW_LOGI("psn: %s\n", buf);
        memset(setting.psn, 0, sizeof(setting.psn));
        memcpy(setting.psn, buf, strlen(buf));
        //[tgl mark]
        if (strlen(setting.psn) > 0)
        {
            sprintf((char *)ap_para.ssid, "%s%s", "AISWEI-", setting.psn + strlen(setting.psn) - 4);
        }
        else
        {
            memset(ap_para.ssid, 0, sizeof(ap_para.ssid));
        }

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "key", sizeof(buf), value);
        ASW_LOGI("key: %s\n", buf);
        memset(setting.reg_key, 0, sizeof(setting.reg_key));
        memcpy(setting.reg_key, buf, strlen(buf));
        //[tgl mark]
        if (strlen(setting.reg_key) > 0)
        {
            sprintf((char *)ap_para.password, "%s", setting.reg_key);
        }
        else
        {
            memset(ap_para.ssid, 0, sizeof(ap_para.ssid));
        }

        general_add(NVS_AP_PARA, &ap_para);

        getJsonNum(&setting.typ, "typ", value);

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "nam", sizeof(buf), value);
        ASW_LOGI("nam: %s\n", buf);
        memset(setting.nam, 0, sizeof(setting.nam));
        memcpy(setting.nam, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "mod", sizeof(buf), value);
        ASW_LOGI("mod: %s\n", buf);
        memset(setting.mod, 0, sizeof(setting.mod));
        memcpy(setting.mod, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "muf", sizeof(buf), value);
        ASW_LOGI("muf: %s\n", buf);
        memset(setting.mfr, 0, sizeof(setting.mfr));
        memcpy(setting.mfr, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "brd", sizeof(buf), value);
        ASW_LOGI("brd: %s\n", buf);
        memset(setting.brw, 0, sizeof(setting.brw));
        memcpy(setting.brw, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "hw", sizeof(buf), value);
        ASW_LOGI("hw: %s\n", buf);
        memset(setting.hw, 0, sizeof(setting.hw));
        memcpy(setting.hw, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "wsw", sizeof(buf), value);
        ASW_LOGI("wsw: %s\n", buf);
        memset(setting.wsw, 0, sizeof(setting.wsw));
        memcpy(setting.wsw, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "pdk", sizeof(buf), value);
        ASW_LOGI("pdk: %s\n", buf);
        memset(setting.product_key, 0, sizeof(setting.product_key));
        memcpy(setting.product_key, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "ser", sizeof(buf), value);
        ASW_LOGI("ser: %s\n", buf);
        memset(setting.device_secret, 0, sizeof(setting.device_secret));
        memcpy(setting.device_secret, buf, strlen(buf));

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "ali_ip", sizeof(buf), value);
        ASW_LOGI("ali_ip: %s\n", buf);
        memset(setting.host, 0, sizeof(setting.host));
        memcpy(setting.host, buf, strlen(buf));

        getJsonNum(&setting.port, "ali_port", value);
        // getJsonNum(&setting.meter_en, "meter_en", value);
        // getJsonNum(&setting.meter_add, "meter_add", value);
        // getJsonNum(&setting.meter_mod, "meter_mod", value);

        if (legal_check(setting.psn) == ASW_OK)
        {
            xSemaphoreGive(g_semar_psn_ready);
        }

        general_add(NVS_ATE, &setting);

        // v2.0.0 add
        int work_mode_ate = Work_Mode_AP_PROV;
        general_add(NVS_WORK_MODE, &work_mode_ate);

        return 0;
    }
    else if (strcmp(action, "test") == 0)
    {
        char buf[100] = {0};

        getJsonStr(buf, "test_meter", sizeof(buf), value);
        ASW_LOGI("test_meter: %s\n", buf);

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "test_inv", sizeof(buf), value);
        ASW_LOGI("test_inv: %s\n", buf);

        memset(buf, 0, sizeof(buf));
        getJsonStr(buf, "test_drm", sizeof(buf), value);
        ASW_LOGI("test_drm: %s\n", buf);
        return 0;
    }
    else if (strcmp(action, "operation") == 0)
    {

        ASW_LOGW("=====come in operation.....");
        uint8_t i = 0;
        int num;
        for (i = 0; i < NUM_SETTING_OPRT; i++)
        {
            num = -1;
            ASW_LOGE("=====index:%d,g_scan_stop:%d,come in %s,%d.....", i, g_scan_stop, valueJson[i], num);

            if (getJsonNum(&num, valueJson[i], value) == 0)
            {
                if (num == 1)
                {
                    if (i == 4)
                    {
                        return asw_handle_paraller_msg(1, value);
                    }
                    if (i == 5)
                    {
                        return asw_operation_ssc_handler(1);
                    }
                    return OpCodeCmd[i];
                }
                else if (i == 3 && g_scan_stop == 0)
                {
                    g_scan_stop = 1;
                    break;
                }
                else if (i == 4 && num == 0)
                {
                    return asw_handle_paraller_msg(0, value);
                }
                else if (i == 5 && num == 0)
                {
                    return asw_operation_ssc_handler(0);
                }
                ASW_LOGE("=====index:%d,g_scan_stop:%d", i, g_scan_stop);
            }
        }
        return 0;
    }
    //-----------  lanset ------------//
    else if (strcmp(action, "lanset") == 0)
    {
        return wlanset_handler_mode_staticip_fun(value);
    }
    else
    {
        return -1;
    }
}

//----------------------------------------------------//
static int16_t setting_handler_device_2_fun(char *action, cJSON *value)
{
    if (strcmp(action, "setpower") == 0)
    {

        return asw_write_inv_onoff(value);
    }
    else
    {
        return -1;
    }
}
//------------------------------------//
static int16_t setting_handler_device_3_fun(char *action, cJSON *value)
{
    if (strcmp(action, "setmeter") == 0)
    {
        //-- Eng.Stg.Mch-lanstick 20220908 +-
        write_meter_configuration(value);
        event_group_0 |= PWR_REG_SOON_MASK;
        g_meter_sync = 0;
        /*根据调试打印设置值，判断是否进行数据打印*/
        if (g_asw_debug_enable > 1)
            event_group_0 |= METER_CONFIG_MASK;

        return 0;
    }
    else
    {
        return -1;
    }
}
//----------------------------------------------//
static int16_t setting_handler_device_4_fun(char *action, cJSON *value)
{
    if (strcmp(action, "setbattery") == 0)
    {

        return write_battery_configuration(value);
    }

    else if (strcmp(action, "setdefine") == 0)
    {
        return write_battery_define(value);
    }
    else
    {
        return -1;
    }
}

//------------------------------------------------
static int8_t wlanset_handler_mode_ap_fun(cJSON *value)
{
    wifi_ap_para_t _ap_para = {0};
    getJsonStr((char *)_ap_para.ssid, "ssid", sizeof(_ap_para.ssid), value);
    ASW_LOGW("*******888 ap ssid: %s\n", _ap_para.ssid);

    if (strlen((char *)_ap_para.ssid) > 0)
    {
        wifi_ap_para_t ap_para_tmp = {0};
        general_query(NVS_AP_PARA, &ap_para_tmp);
        memcpy(_ap_para.password, ap_para_tmp.password, strlen((char *)ap_para_tmp.password));

        //   ASW_LOGW("*******888 ap ps: %s\n", (char *)ap_para_tmp.password);

        general_add(NVS_AP_PARA, &_ap_para);

        return 0; // httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    else
    {
        return -1; //  httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}
//------------------------------------------------
static int8_t wlanset_handler_mode_state_fun(cJSON *value)
{
    wifi_sta_para_t _sta_para = {0};
    // ESP_LOGW("Wlan Set", "Wifi sta is not usable!!");
    getJsonStr((char *)_sta_para.ssid, "ssid", sizeof(_sta_para.ssid), value);
    ASW_LOGI("ssid-------> %s\n", _sta_para.ssid);

    getJsonStr((char *)_sta_para.password, "psw", sizeof(_sta_para.password), value);
    ASW_LOGI("psw--------> %s\n", _sta_para.password);

    if (strlen((char *)_sta_para.ssid) > 0)
    {
        general_add(NVS_STA_PARA, &_sta_para);
        int work_mode = Work_Mode_STA;
        general_add(NVS_WORK_MODE, &work_mode);

        ///--------------------------/////

        g_task_inv_broadcast_msg |= MSG_WRT_STA_INFO_INDEX;

        return CMD_SYNC_REBOOT;
    }
    else
    {
        return -1; // httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}

//-------------------------------------------------------//
static int8_t wlanset_handler_mode_easylink_fun(cJSON *value)
{
    int modex = 0;
    getJsonNum(&modex, "enable", value);
    ASW_LOGI("enable: %d\n", modex);
    // if (modex == 1)
    // {
    //     int work_mode = Work_Mode_SMART_PROV;
    //     general_add(NVS_WORK_MODE, &work_mode);

    //     return CMD_REBOOT;
    // }
    // if (modex == 2)
    // {
    /* v2.0.0 change */
    int work_mode = Work_Mode_AP_PROV;
    general_add(NVS_WORK_MODE, &work_mode);
    return CMD_REBOOT;
    // }

    return 0;
}

//---------------------------------------------//
static int8_t wlanset_handler_mode_prov_fun(cJSON *value)
{
    int modex = 0;
    getJsonNum(&modex, "enable", value);
    ASW_LOGI("prov %d\n", modex);
    if (modex == 1) /*  || modex == 3)  //v2.0.0 change */
    {
        general_add(NVS_WORK_MODE, &modex);
        return CMD_REBOOT;
    }

    return 0;
}

/********************************
 * @brief getdev_handle
 *
 * @param devecitType
 * @param isSecret
 * @return char *
 *********************************/
char *protocol_nearby_getdev_handler(uint16_t devecitType, uint16_t isSecret, char *psn) // tgl mark 返回的指针需要 free
{
    char *msg = NULL;
    char *sendBuf = NULL;

    ASW_LOGW(" Secret:%d,deviceType:%d,psn:%s", isSecret, devecitType, psn);

    switch (devecitType)
    {
    case 1:
        if (isSecret == 1) //
        {
            msg = getdev_handle_type_1_fun();
        }
        break;

    case 2:
        msg = getdev_handle_type_2_fun();
        break;

    case 3:
        msg = getdev_handle_type_3_fun();
        break;

    case 4:
        msg = getdev_handle_type_4_fun(psn);
        break;

    default:
        msg = getdev_handle_type_default_fun();
        break;
    }

    if (msg != NULL)
    {
        // sendBuf = (char *)malloc(sizeof(char) * (strlen(msg) + 1));
        // memset(sendBuf,'\0',strlen(msg) + 1);
        sendBuf = (char *)calloc((strlen(msg) + 1), sizeof(char));
        memcpy(sendBuf, msg, strlen(msg) + 1);
        free(msg);
        return sendBuf;
    }
    else
    {
        return NULL;
    }
}

/*****************************
 * @brief getdevdata_handler
 *
 * @param devecitType
 * @param inv_sn
 * @return char *
 ****************************/
char *protocol_nearby_getdevdata_handler(uint16_t devecitType, char *inv_sn)
{
    char *msg = NULL;
    char *sendBuf = NULL;

    ASW_LOGW("--- inv sn: %s", inv_sn);

    switch (devecitType)
    {
    case 2:
        msg = getdevdata_handle_type_2_fun(inv_sn);
        break;
    case 3:
        msg = getdevdata_handle_type_3_fun();
        break;
    case 4:
        msg = getdevdata_handle_type_4_fun(inv_sn);
        break;
    default:
        break;
    }

    if (msg != NULL)
    {

        sendBuf = (char *)calloc((strlen(msg) + 1), sizeof(char));
        memcpy(sendBuf, msg, strlen(msg) + 1);
        free(msg);
        return sendBuf;
    }
    else
    {
        return NULL;
    }
}

/*****************************************
 * @brief protocol_nearby_wlanget_handler
 *
 * @param info
 * @return char*
 *******************************************/

char *protocol_nearby_wlanget_handler(uint16_t info)
{
    char *msg = NULL;
    char *sendBuf = NULL;

    ASW_LOGW("--- inv sn: %d", info);

    switch (info)
    {
    case 1:
        msg = wlanget_handler_info_1_fun();
        break;
    case 2:
        msg = wlanget_handler_info_2_fun();
        break;
    case 3:
        msg = wlanget_handler_info_3_fun();
        break;
    case 4:
        msg = wlanget_handler_info_4_fun();
        break;
    default:
        msg = wlanget_handler_info_default_fun();
        break;
    }

    if (msg != NULL)
    {
        sendBuf = (char *)calloc((strlen(msg) + 1), sizeof(char));
        memcpy(sendBuf, msg, strlen(msg) + 1);

        ASW_LOGW("  get wlan set %s", msg);
        free(msg);
        return sendBuf;
    }
    else
    {
        return NULL;
    }
}

/*******************************************
 * @brief protocol_nearby_setting_handler
 *
 * @param deviceType
 * @param action
 * @param value
 * @return char*
 ********************************************/
int8_t protocol_nearby_setting_handler(uint16_t deviceType, char *action, cJSON *value)
{
    ASW_LOGW("---setting_handler  value:%d, %s", deviceType, action);
    int8_t op_cmd_code = 0;

    switch (deviceType)
    {
    case 1:
        op_cmd_code = setting_handler_device_1_fun(action, value);
        break;

    case 2:
        op_cmd_code = setting_handler_device_2_fun(action, value);
        break;

    case 3:
        op_cmd_code = setting_handler_device_3_fun(action, value);
        break;

    case 4:
        op_cmd_code = setting_handler_device_4_fun(action, value);
        break;

    default:
        op_cmd_code = -1;
        break;
    }

    return op_cmd_code;
}

/********************************************
 * @brief protocol_nearby_wlanset_handler
 * TODO  后期与app对接 关于启停AP、WIFI-Eth的设置
 * @param action
 * @param value
 * @return int8_t
 ********************************************/
int8_t protocol_nearby_wlanset_handler(char *action, cJSON *value)
{
    int8_t m_res_cmd = -1;
    if (strcmp(action, "ap") == 0)
    {
        m_res_cmd = wlanset_handler_mode_ap_fun(value);
    }
    else if (strcmp(action, "station") == 0)
    {
        m_res_cmd = wlanset_handler_mode_state_fun(value);
    }
    else if (strcmp(action, "easylink") == 0)
    {
        m_res_cmd = wlanset_handler_mode_easylink_fun(value);
    }
    else if (strcmp(action, "prov") == 0)
    {
        m_res_cmd = wlanset_handler_mode_prov_fun(value);
    }
    else if (strcmp(action, "staticip") == 0)
    {
        m_res_cmd = wlanset_handler_mode_staticip_fun(value);
    }
    else
    {
        m_res_cmd = -1;
    }

    return m_res_cmd;
}

/***************************************
 * @brief protocol_nearby_fdbg_handler
 *
 * @param json
 * @return char*
 ****************************************/
char *protocol_nearby_fdbg_handler(cJSON *json)
{
    ASW_LOGW("******* fdbg_handler  handle..");
    char *ascii_data = NULL; /** 动态内存*/
    int ascii_len;

    cJSON *res_json; /** 动态内存*/
    int BUF_LEN = 600;
    fdbg_msg_t fdbg_msg = {0};
    char *msg = NULL; /** 动态内存*/
    char *sendBuf = NULL;

    if (to_inv_fdbg_mq == NULL)
    {
        ASW_LOGE("to_inv_fdbg_mq is NULL\n");
        if (ascii_data != NULL)
            free(ascii_data);
        return NULL;
    }

    ascii_data = calloc(BUF_LEN, sizeof(char));
    getJsonStr(ascii_data, "data", BUF_LEN, json);

    //[tgl mark] 被注释掉的
    // fdbg_msg.data = calloc(strlen(ascii_data) / 2, sizeof(char));
    // fdbg_msg.len = StrToHex((unsigned char *)fdbg_msg.data, ascii_data, strlen(ascii_data));

    ASW_LOGI("fdbg req: %s\n", ascii_data);

    char ws[64] = {0};
    // char msgs[300] = {0};  [tgl mark]

    // strncpy(msgs, ascii_data, 300);
    memset(ws, 'A', 32);
    ASW_LOGI("cgi tans ms %s ws %s \n", ascii_data, ws);
#if !TRIPHASE_ARM_SUPPORT

    if (send_cgi_msg(20, ascii_data, strlen(ascii_data), ws) == 0)
#else
    if (send_cgi_msg(20, 0, ascii_data, strlen(ascii_data), ws) == 0)
#endif

    {

        xQueueReset(to_cgi_fdbg_mq);
        ASW_LOGI("to_inv_fdbg_mq send ok\n");
        memset(&fdbg_msg, 0, sizeof(fdbg_msg));
        memset(ascii_data, 0, BUF_LEN);

        if (to_cgi_fdbg_mq != NULL && xQueueReceive(to_cgi_fdbg_mq, &fdbg_msg, 10000 / portTICK_RATE_MS) == pdPASS)
        {
            if (fdbg_msg.type == MSG_FDBG)
            {
                ASW_LOGI("to_inv_fdbg_mq recv ok %d \n", fdbg_msg.len);
                ascii_len = HexToStr((unsigned char *)ascii_data, (unsigned char *)fdbg_msg.data, fdbg_msg.len);
                free(fdbg_msg.data); //  在 inv_com.c/upinv_transdata_proc()中 xQueueSend前分配的动态内存
                res_json = cJSON_CreateObject();
                cJSON_AddStringToObject(res_json, "dat", "ok");
                cJSON_AddStringToObject(res_json, "data", ascii_data);
                free(ascii_data);

                msg = cJSON_PrintUnformatted(res_json);
                if (res_json != NULL)
                    cJSON_Delete(res_json);
                if (msg != NULL)
                {
                    sendBuf = (char *)calloc((strlen(msg) + 1), sizeof(char));
                    memcpy(sendBuf, msg, strlen(msg) + 1);

                    ASW_LOGW("  get wlan set %s", msg);
                    free(msg);
                    return sendBuf;
                }

                return NULL;
            }
            else
            {
                ASW_LOGE("fdbg recv type is err\n");
                if (fdbg_msg.data != NULL)
                    free(fdbg_msg.data); //
            }
        }
        else
        {
            ASW_LOGE("to_cgi_fdbg_mq is NULL\n");
            if (ascii_data != NULL)
                free(ascii_data);

            if (fdbg_msg.data != NULL)
                free(fdbg_msg.data);
            return NULL;
        }
    }
    else
    {
        ASW_LOGE("to_inv_fdbg_mq send timeout 10 ms\n");
    }

    if (fdbg_msg.data != NULL)
        free(fdbg_msg.data);

    if (ascii_data != NULL)
        free(ascii_data);

    return NULL;
}