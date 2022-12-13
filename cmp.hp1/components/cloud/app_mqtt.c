#include "app_mqtt.h"
#include "save_inv_data.h"
#include "estore_cld.h"

static const char *TAG = "app_mqtt.c";

int connect_stage = 0;
// static unsigned short pcout = 3; //[tgl mark del]

static int state_meter_last = 0; //[tgl mark add]
int g_meter_sync = 0;

static uint8_t sg_uMqttReconnectCount = 0;

uint8_t inv_info_sync[30] = {0};
EXT_RAM_ATTR Setting_Para ini_para = {0};

int clear_cloud_invinfo(void)
{
    memset(inv_info_sync, 0, sizeof(inv_info_sync));
    return 0;
}
//----------------------------------------//
///////////////// Eng.Stg.Mch-Lanstick ///////////////
void clear_all_sync_info(void)
{
    g_monitor_state &= ~INV_SYNC_PMU_STATE;
    memset(inv_info_sync, 0, sizeof(inv_info_sync));
    g_meter_sync = 0;
}

void clear_inv_pmu_sync_info(void)
{
    g_monitor_state &= ~INV_SYNC_PMU_STATE;
    memset(inv_info_sync, 0, sizeof(inv_info_sync));
}

int is_all_info_sync_ok(void)
{
    if (g_monitor_state & INV_SYNC_PMU_STATE)
        return 0;
    for (int j = 0; j < 30; j++)
    {
        if (inv_info_sync[j] == 1)
            break;
        if (j == 29)
            return 0;
    }
    if (g_meter_sync == 0)
        return 0;
    return 1;
}
// int check_sn_format(char *str)
// {
//     return strlen(str);
// }

//---------------------------------------//

int GetMinute1(int ihour, int iminute, int imsec)
{
    uint32_t dSeconds = (ihour * 60 + iminute + imsec);
    return (dSeconds);
}

//---------------------------------------//

int get_connect_stage(void)
{
    return connect_stage;
}

//--------------------------------------------//
/*this function generate the login key*/

void sn_to_reg(const char *sn, char *reg_key)
{
    uint32_t iLoop;

    if (NULL == sn)
    {
        return;
    }

    for (iLoop = 0; iLoop < 12; iLoop++)
    {
        reg_key[iLoop] = ((~(sn[iLoop] | 0xf0)) + 0x40);
    }
    for (iLoop = 12; iLoop < 16; iLoop++)
    {
        reg_key[iLoop] = (reg_key[iLoop - 10] & reg_key[iLoop - 4]);
    }
}
//----------------------------------------//

/*get time by zeversolar*/
TDCEvent gettime(void)
{
    struct timeval tv = {0};
    struct tm ptime = {0};
    static int last_time_ret = -5; //-5 means ini
    static int64_t cnnot_get_time = 0;

    // Monitor monitor;

    char time_url[200] = {0};
    char reg_key[17] = {0};
    char ts[128] = {0};
    // G123456789AB
    // sn_to_reg(monitor.sn, reg_key);
    sn_to_reg(DEMO_DEVICE_NAME, reg_key);

    sprintf(time_url, "http://mqtt.aisweicloud.com/api/v1/updatetime?psno=%s&sign=%s", DEMO_DEVICE_NAME, reg_key);
    ASW_LOGI("timeurl %s\n", time_url);
    int ret = http_get_time(time_url, &ptime);

    if (ret != 0)
    {
        if ((last_time_ret == 0 || last_time_ret == -5) && ret != 0)
        {
            network_log(",[err] get time");
            last_time_ret = ret;
        }

        if (!cnnot_get_time)
            cnnot_get_time = get_second_sys_time();

        // network_log("tm get er");
        sleep(16);
        // printf("cnnot get over 30mins reboot ooppp %lld \n", cnnot_get_time);
        if (get_second_sys_time() - cnnot_get_time > 1800)
        {
            ESP_LOGE(TAG, "cnnot get over 30mins reboot \n");
            sleep(1);
            sent_newmsg();
        }

        ESP_LOGW("--GetTime--", "gettime error\r\n");
        return dcIdle;
    }
    else if (ptime.tm_year > 2019 && ptime.tm_year < 2050)
    {
        ptime.tm_year -= 1900;
        ptime.tm_mon -= 1;
        ASW_LOGI("[year] %d\n", ptime.tm_year);
        ASW_LOGI("[mon] %d\n", ptime.tm_mon);
        ASW_LOGI("[day] %d\n", ptime.tm_mday);
        ASW_LOGI("[hour] %d\n", ptime.tm_hour);
        ASW_LOGI("[min] %d\n", ptime.tm_min);
        ASW_LOGI("[sec] %d\n", ptime.tm_sec);

        tv.tv_sec = mktime(&ptime);
        settimeofday(&tv, NULL);
        // system("hwclock -w");
        // system("sync");
        sprintf(ts, "date %02d%02d%02d%02d%d", (ptime.tm_mon + 1), ptime.tm_mday, ptime.tm_hour, ptime.tm_min, (ptime.tm_year + 1900));
        ASW_LOGI("set time %s \n", ts);
        system(ts);
        system("sync");

        // display time we have set
        char display[20] = {0};
        get_time(display, 20);
        ASW_LOGI("system time: %s\n", display);
        // pthread_mutex_lock(&monitor_mutex);
        g_monitor_state |= SYNC_TIME_FROM_CLOUD_INDEX;
        // pthread_mutex_unlock(&monitor_mutex);
        connect_stage = 1;
    }
    // printf("write time log %d \n", ((last_time_ret != 0 || last_time_ret == -5) && ret == 0 && ptime.tm_year > 2019 && ptime.tm_year < 2050));
    // printf("write ######### %d %d %d \n", last_time_ret, ret, ptime.tm_year);

    if ((last_time_ret != 0 || last_time_ret == -5) && ret == 0 && ptime.tm_year > 119 && ptime.tm_year < 150)
    {
        network_log(",[ok] get time");
        last_time_ret = ret;
    }
    return dcalilyun_authenticate;
}

int get_time_manual(void)
{
    struct timeval tv;
    struct tm ptime = {0};

    char time_url[200] = {0};
    char reg_key[17] = {0};
    char ts[128] = {0};
    // G123456789AB
    //  sn_to_reg(monitor.sn, reg_key);
    sn_to_reg(DEMO_DEVICE_NAME, reg_key);

    sprintf(time_url, "http://mqtt.aisweicloud.com/api/v1/updatetime?psno=%s&sign=%s", DEMO_DEVICE_NAME, reg_key);
    ASW_LOGI("[ url ] :\n%s\n", time_url);
    int ret = http_get_time(time_url, &ptime);
    if (ret != 0)
    {
        ASW_LOGW("gettime error\n");
        return -1;
    }
    else if (ptime.tm_year > 2019 && ptime.tm_year < 2070)
    {
        ptime.tm_year -= 1900;
        ptime.tm_mon -= 1;

        ASW_LOGI("[year] %d\n", ptime.tm_year);
        ASW_LOGI("[mon] %d\n", ptime.tm_mon);
        ASW_LOGI("[day] %d\n", ptime.tm_mday);
        ASW_LOGI("[hour] %d\n", ptime.tm_hour);
        ASW_LOGI("[min] %d\n", ptime.tm_min);
        ASW_LOGI("[sec] %d\n", ptime.tm_sec);

        tv.tv_sec = mktime(&ptime);
        settimeofday(&tv, NULL);
        // system("hwclock -w");
        // system("sync");
        sprintf(ts, "date %02d%02d%02d%02d%d", (ptime.tm_mon + 1), ptime.tm_mday, ptime.tm_hour, ptime.tm_min, (ptime.tm_year + 1900));
        ASW_LOGI("set time %s \n", ts);
        system(ts);
        system("sync");
        return 0;
    }
    else
    {
        ESP_LOGW(TAG, "gettime ok but year error\n");
        return -1;
    }
}
//---------------------------------------//

/*get net state*/
int get_net_state(void)
{
    int ret0 = get_eth_connect_status();
    int ret1 = get_wifi_sta_connect_status(); // v2.0.0 add
    if (ret0 == 0 || ret1 == 0)
    {
        if (ret0 == 0)
            ESP_LOGW(TAG, "stick ETH CONNECT IS OK ");
        else
            ESP_LOGW(TAG, "stick WIFI CONNECT IS OK ");
        return (gettime());
    }
    else
    {
        ESP_LOGE(TAG, "wifi not conn...");
        return dcIdle;
    }
}

//--------------------------------------//
int cloud_setup(void)
{
    Setting_Para para = {0};
    general_query(NVS_ATE, &para);

    ASW_LOGI("\n**DEBUG_PRINT:host,%s\n", para.host);

    set_mqtt_server(para.product_key, para.host, para.port);
    // set_mqtt_server(para.host, para.port);

    set_product_key(para.product_key);
    set_device_name(para.psn);
    set_device_secret(para.device_secret);

    // snprintf(outbuf, sizeof(outbuf), "%d", *adder);
    snprintf(pub_topic, 150, "/%s/%s/user/Action", para.product_key, para.psn); //[tgl mark]

    snprintf(sub_topic_get, 150, "/%s/%s/user/get", para.product_key, para.psn); //"/sys/a1tqX123itk/B80052030075/rrpc/request/+";
    snprintf(sub_topic, 150, "/sys/%s/%s/rrpc/request/+", para.product_key, para.psn);

    int res = asw_mqtt_setup();
    if (res == 0)
    {
        ESP_LOGI(TAG, "[ ok ] mqtt connect >>>>>>>\n");
    }
    else
    {
        ESP_LOGW(TAG, "[ err ] mqtt connect >>>>>>>\n");
    }

    return 0;
}
//-------------------------------------------//
/*mqtt connect*/
int mqtt_connect(void)
{
    static int mqtt_create_fail = 0;
    static int mqtt_create_fail_time = 0;

    // v2.0.0 change && add
    /*
      Lan 模式下，无网络连接 ，AP模式下，不启用mqtt 连接
    */
    if ((g_stick_run_mode == Work_Mode_LAN && get_eth_connect_status() != 0) || (g_stick_run_mode == Work_Mode_STA && get_wifi_sta_connect_status() != 0))
        return dcalilyun_authenticate;

    if (g_stick_run_mode == Work_Mode_AP_PROV)
        return dcalilyun_authenticate;

    if (asw_mqtt_reconncet() == ESP_OK)
    {
        ESP_LOGI("--DEBUG MQTT CONNECT--", "mqtt reconnect is ok. %d", sg_uMqttReconnectCount);

        if (sg_uMqttReconnectCount++ > 10)
        {
            sg_uMqttReconnectCount = 0;
            ESP_LOGW("--DEBUG MQTT CONNECT--", "mqtt reconnect is  count is > 10, mqtt destory ...");
            mqtt_client_destroy_free();
        }
        else
        {
            return dcalilyun_publish;
        }
    }
    // else
    // {

    //     ESP_LOGW("--DEBUG MQTT CONNECT--", "mqtt reconnect is failed , mqtt destory ...");
    //     sg_uMqttReconnectCount = 0;
    //     mqtt_client_destroy_free();
    // }

    if ((get_eth_connect_status() == 0 || get_wifi_sta_connect_status() == 0) && 0 == mqtt_app_start()) //网络连接成功状态下 去启动mqtt client
    {
        connect_stage = 2;
        ASW_LOGI("\n=====DEBUG PRIntf ====\n=====eth connect status:%d,wifi connect status:%d=======   conncet stage :%d ============\n",
                 get_eth_connect_status(), get_wifi_sta_connect_status(), connect_stage);
        return dcalilyun_publish;
    }
    else
    {
        if (!mqtt_create_fail)
            mqtt_create_fail_time = get_second_sys_time(); // esp_timer_get_time();
        mqtt_create_fail++;
        if (get_second_sys_time() - mqtt_create_fail_time > 600) // esp_timer_get_time() - mqtt_create_fail_time > 300 * 1000000)
        {
            net_com_reboot_flg = 1; // reboot
        }
        network_log("mq recon er");

        // if ((g_stick_run_mode == Work_Mode_LAN && get_eth_connect_status() != 0) ||
        //     (g_stick_run_mode == Work_Mode_STA && get_wifi_sta_connect_status() != 0))
        // {
        ESP_LOGE(TAG, "mqtt app start err and local net is disconnect ,destroy mqtt client...");
        mqtt_client_destroy_free(); // tgl add +++
        // }
        sleep(16);

        return dcalilyun_authenticate;
    }
}

//------------------------------
void Check_upload_unit32(const char *field, uint32_t value, char *body, int *datalen) //[tgl mark] uint8_t->uint32_t
{
    uint16_t len = (*datalen);
    if (value != 0xFFFFFFFF)
    {
        (*datalen) = len + sprintf(body + len, field, value);
    }
}

void Check_upload_sint16(const char *field, uint16_t value, char *body, int *datalen)
{
    uint16_t len = (*datalen);
    if (value != 0xFFFF)
    {
        (*datalen) = len + sprintf(body + len, field, value);
    }
}

//---------------------------------//
static int mqtt_msg1_syncpmu_state_fun(char *payload)
{
    wifi_ap_para_t ap_para_tmp = {0};
    general_query(NVS_AP_PARA, &ap_para_tmp);

    char rshstr[16] = {0};
    itoa(get_rssi(), rshstr, 10);
    char stass[32] = {0};
    get_stassid(stass);

    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);

    general_query(NVS_ATE, &ini_para);
    sprintf(payload, "{\"Action\":\"SyncPmuInfo\","
                     "\"sn\":\"%s\","
                     "\"typ\":%d,"
                     "\"nam\":\"%s\","
                     "\"sw\":\"%s\","
                     "\"hw\":\"%s\","
                     "\"sys\":\"%s\","
                     "\"md1\":\"%s\","
                     "\"md2\":\"%s\","
                     "\"mod\":\"%s\","
                     "\"muf\":\" %s\","
                     "\"brd\":\"%s\","
                     "\"cmd\":%d,"
                     "\"csq\":\"%s\","
                     "\"state\":\"%s\","
                     "\"name\":\"%s\","
                     "\"ap\":\"%s\","
                     "\"psw\":\"%s\","
                     "\"parallel\":\"%s\","
                     "\"ssc\":\"%s\"}",
            DEMO_DEVICE_NAME,
            ini_para.typ,
            ini_para.nam,
            FIRMWARE_REVISION,
            ini_para.hw,
            "freertos",
            ini_para.md1,
            CGI_VERSION, // ini_para.md2,//////////////////////////////V1.0
            ini_para.mod,
            ini_para.mfr,
            ini_para.brw,
            g_stick_run_mode == Work_Mode_LAN ? 1 : 2, /* 1-- 有線，2--wifi */
            // 2, // ini_para.cmd,
            rshstr,
            "88",
            stass,
            ap_para_tmp.ssid,
            ap_para_tmp.password,
            mt.is_parallel == 1 ? "1" : "0",
            mt.ssc_enable == 1 ? "1" : "0");
    ASW_LOGI("PUB MONITOR INFO %s **********************\n", payload);
    if (asw_publish(payload /*, pcout++*/) >= 0)
    {
        ASW_LOGI("[ asw_publish ] pmu info\n");
        g_monitor_state |= INV_SYNC_PMU_STATE;

        return ASW_OK;
    }
    else
        return ASW_FAIL;
}

//////////////////////////////////////////////

//---------------------------------------------//

int mqtt_publish(char *json_msg)
{

    if (g_num_real_inv <= 0)
    {
        ASW_LOGI("\n-------mqtt_publish------\n  ");
        ASW_LOGI(" the inv num is 0 .\n");
        goto END;
    }

    char payload[1000] = {0};
    InvRegister devicedata = {0};

    static int ncout = 0;
    static int inv_sync_num = 0;
    static int publish_fail = 0;
    int64_t publish_fail_time = 0;

    // Eng.Stg.Mch-lanstick +
    MonitorPara monitor_para = {0};
    read_global_var(METER_CONTROL_CONFIG, &monitor_para);

    char brand[16] = {0};
    char name[16] = {0};

    if ((g_stick_run_mode == Work_Mode_LAN && get_eth_connect_status() != 0) ||
        (g_stick_run_mode == Work_Mode_STA && get_wifi_sta_connect_status() != 0))
    {
        if (!publish_fail)
            publish_fail_time = get_second_sys_time();
        publish_fail++;
        if (get_second_sys_time() - publish_fail_time > 200) // 200
        {
            publish_fail_time = get_second_sys_time();
            ESP_LOGW(TAG, " net is disconnect , will destroy mqtt client.");
            mqtt_client_destroy_free();
            return dcalilyun_authenticate;
        }
        goto END;
    }

    /**
     * @brief 同步stick信息
     **/
    if (0 == (g_monitor_state & INV_SYNC_PMU_STATE))
    {
        if (mqtt_msg1_syncpmu_state_fun(payload) != ASW_OK)
            return dcalilyun_authenticate;
    }
    memset(payload, 0, sizeof(payload));

    /**
     * @brief 同步逆变器信息
     */
    if ((g_monitor_state & INV_SYNC_PMU_STATE) && !inv_info_sync[29])
    {
        if (!inv_info_sync[inv_sync_num] && cld_inv_arr[inv_sync_num].regInfo.modbus_id > 0 && strlen(cld_inv_arr[inv_sync_num].regInfo.sn) > 0 && strlen(inv_arr[inv_sync_num].regInfo.msw_ver) > 0 && strlen(cld_inv_arr[inv_sync_num].regInfo.msw_ver) > 0)
        {
            // memcpy(&devicedata, &(cld_inv_arr[inv_sync_num].regInfo), sizeof(InvRegister));
            memcpy(&devicedata, &(cld_inv_arr[inv_sync_num].regInfo), sizeof(InvRegister));
            if (strlen(devicedata.sn) > 0)
            {
                ASW_LOGI("\n-------mqtt_publish------\n  ");
                get_estore_invinfo_payload(devicedata, payload);
                ASW_LOGI("PUB  %s inv info**************\n", payload);
                if (asw_publish(payload) > -1)
                {
                    inv_info_sync[inv_sync_num] = 1;
                    ASW_LOGI("[ asw_publish ] %d inv info\n", inv_sync_num);

                    inv_sync_num++; // tgl mark
                }
                else
                {
                    ESP_LOGE("---  MARK ERROR---", "publish is erro , need to reconnect to mqtt");
                    return dcalilyun_authenticate;
                }
                memset(payload, 0, sizeof(payload));
            }
            else
            {
                ESP_LOGW(TAG, "############%s--SN ERROR isn't 16 numerics or letters\n", devicedata.sn);
            }
            // inv_sync_num++;
        } // inv_sync_num++;

        printf("\n----------test debug A-----------\n");
        printf("inv_sync_num :%d ,g_num_real_inv:%d", inv_sync_num, g_num_real_inv);
        printf("\n----------test debug A-----------\n");

        if ((inv_sync_num >= g_num_real_inv))
        {
            inv_sync_num = 0;
            uint8_t i = 0, j = 0;
            for (i = 0; i < g_num_real_inv; i++)
            {
                if (1 == inv_info_sync[i])
                {
                    j++;
                    ASW_LOGI("rht %d %dinvter info upload\n", i, j);
                }
            }

            printf("\n----------test debug B-----------\n");
            printf("j:%d ,g_num_real_inv:%d", j, g_num_real_inv);
            printf("\n----------test debug B-----------\n");

            if (j && j >= g_num_real_inv)
            {
                ASW_LOGI("all invter info upload\n");
                inv_info_sync[29] = 1;
            }
        }
    }
    memset(payload, 0, sizeof(payload));
    /**
     * @brief 同步电表信息
     **/
    if (monitor_para.adv.meter_enb != state_meter_last || g_meter_sync == 0)
    {

        memset(brand, 0, sizeof(brand));
        memset(name, 0, sizeof(name));
        get_meter_info(monitor_para.adv.meter_mod, brand, name);
        /** 含有储能机*/
        get_estore_meterinfo_payload(payload);
        if (asw_publish(payload) >= 0)
        { // pcout++;
            g_meter_sync = 1;
            state_meter_last = monitor_para.adv.meter_enb;
        }
        else
        { ///// TGL Mark TODO  publish失败超过3次，进入重新连接mqtt
            ESP_LOGE("--- TGL MARK ERROR---", "publish is erro , need to reconnect to mqtt");
            return dcalilyun_authenticate;
        }
    }
    memset(payload, 0, sizeof(payload));
    /////////////////////////////// Eng.Stg.Mch-Lanstick /////////////////////////////
    char this_msg[JSON_MSG_SIZE] = {0};
    /** 实时数据*/
    if (strlen(json_msg) > 0)
    {
        ASW_LOGI("payload++=   %s\n", json_msg);
        memcpy(this_msg, json_msg, strlen(json_msg));
    }
    else
    {
        read_hist_payload(this_msg);
        set_payload_to_hist(this_msg);
    }
    /* 发布实时数据（历史数据）*/
    if (strlen(this_msg) > 0)
    {
        int pub_res = asw_publish(this_msg);
        int chk_res = -100;
        int read_pub_ack_time = get_second_sys_time();
        while ((get_second_sys_time() - read_pub_ack_time) < 10)
        {
            if (pub_res == get_mqtt_pub_ack())
            {
                ASW_LOGI("read ack %d \n", pub_res);
                chk_res = 1;
                break;
            }
            usleep(20 * 1000);
        }
        if (g_asw_debug_enable == 1)
            ESP_LOGI(TAG, "time leasp %lld pub ack %d %d \n", (get_second_sys_time() - read_pub_ack_time), pub_res, get_mqtt_pub_ack());

        if (chk_res == 1)
        {
            ASW_LOGI("publish succeful ncout:%d\r\n", ncout++);
            if (ncout > 32766)
                ncout = 0;

            if (strlen(json_msg) > 0)
            {
                memset(json_msg, 0, JSON_MSG_SIZE); /** 发成功的是即时数据，清空，表示不需要存储*/
            }
            else
            {
                hist_offset_next(); /** 发成功的是历史数据，指向下一个*/
            }

            ASW_LOGI("publish inv data ok\n");
            publish_fail = 0; // pcout++;
            netework_state = 0;
            sg_uMqttReconnectCount = 0;
            //   g_state_mqtt_connect = 0;  //will test just for test
        }
        else
        {
            ESP_LOGW(TAG, "publish data fail\n");
            if (!publish_fail)
                publish_fail_time = esp_timer_get_time();
            publish_fail++;
            if (esp_timer_get_time() - publish_fail_time > 200 * 1000000) //&& publish_fail > 100)
            {
                ESP_LOGW(TAG, "==============mqtt state:%d, mqtt_client_reconnect will be called ,==============", asw_get_mqtt_state());
                // mqtt_client_destroy_free();
                return dcalilyun_authenticate;
            }
            sleep(2);
        }
    }
END:
    sleep(3);

    return dcalilyun_publish;
}

//---------------------------------------//
int trans_resrrpc_pub(cloud_inv_msg *resp, unsigned char *ws, int len) //[tgl mark] ws未用到
{
    char payload[1024] = {0};
    char _data[512] = {0};

    if (len > 0 && len < 255)
    {
        HexToStr((unsigned char *)_data, (unsigned char *)resp->data, resp->len);
        snprintf(payload, 1024, "{\"dat\":\"ok\",\"data\":\"%s\"}", _data);
    }
    else
        snprintf(payload, 1024, "{\"dat\":\"err\",\"msg\":\"%s\"}", "no response");

    if (0 != asw_mqtt_publish(rrpc_res_topic, (char *)payload, strlen(payload), 0)) // res scan(payload))
    {
        ESP_LOGW(TAG, "[err] publish trans resp\n");

        return -1;
    }
    memset(rrpc_res_topic, 0, 256);
    return 0;
}
