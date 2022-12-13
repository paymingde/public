#include "asw_protocol_ate.h"
#include "data_process.h"
#include "asw_nvs.h"
// #include "utility.h"

static const char *TAG = "asw_protocol_ate.c";
//----------------1-------------------//
char *protocol_ate_paraget_handler(void)
{

    char *msg = NULL;
    char buf[20] = {0};

    Setting_Para setting = {0};
    cJSON *res = cJSON_CreateObject();

    general_query(NVS_ATE, &setting);
    ASW_LOGI("para get %d %d \n", setting.meter_add, setting.typ);
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
    cJSON_AddNumberToObject(res, "status", 1);

    cJSON_AddStringToObject(res, "ali_ip", setting.host);
    cJSON_AddNumberToObject(res, "ali_port", setting.port);
    cJSON_AddNumberToObject(res, "meter_en", setting.meter_en);
    cJSON_AddNumberToObject(res, "meter_add", setting.meter_add);
    cJSON_AddNumberToObject(res, "meter_mod", setting.meter_mod);
    int elk = -1;
    general_query(NVS_WORK_MODE, &elk);

    // if (elk == Work_Mode_SMART_PROV)
    //     elk = 1;
    // else
    elk = 0;

    cJSON_AddNumberToObject(res, "elink", elk); // work_mode);

    msg = cJSON_PrintUnformatted(res);
    ASW_LOGI("reply msg: %s\n", msg);
    ASW_LOGW("mem left --2: %d bytes", esp_get_free_heap_size());

    cJSON_Delete(res);

    return msg; //[tgl mark] 注意：need free()
}
//------------------2-----------------//
char *protocol_ate_ifconfig_handler(void)
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    char buf[20] = {0};

    /* V2.0.0 change */
    // get_eth_ip(buf);
    if (g_stick_run_mode == Work_Mode_LAN)
        get_eth_ip(buf);
    else if (g_stick_run_mode == Work_Mode_STA)
        get_sta_ip(buf);

    cJSON_AddStringToObject(res, "ip", buf);
    // cJSON_AddNumberToObject(res, "typ", 1);

    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);
    return msg;
}
//-----------------3------------------//
void protocol_ate_reboot_handler(void)
{
    ate_reboot_t msg = {0};
    memcpy(msg, "rebooting", strlen("rebooting"));
    general_add(NVS_ATE_REBOOT, msg);
    ASW_LOGW("Restart the stick by ATE!!");
    // esp_restart();
}
//-----------------4------------------//
char *protocol_ate_staget_handler(void)
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    // char buf[70] = {0};

    wifi_sta_para_t sta_para = {0};
    general_query(NVS_STA_PARA, &sta_para);

    cJSON_AddStringToObject(res, "ssid", (char *)sta_para.ssid);
    cJSON_AddStringToObject(res, "password", (char *)sta_para.password);
    // cJSON_AddNumberToObject(res, "typ", 1);

    msg = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);

    return msg;
}
//-----------------5------------------//

int8_t protocol_ate_staset_handler(char *body)
{
    wifi_sta_para_t sta_para = {0};
    cJSON *json;
    printf("%s\n", body);
    json = cJSON_Parse(body);
    if (json == NULL)
        return -1;

    getJsonStr((char *)sta_para.ssid, "ssid", sizeof(sta_para.ssid), json);
    getJsonStr((char *)sta_para.password, "password", sizeof(sta_para.password), json);

    general_add(NVS_STA_PARA, &sta_para);

    int work_mode_ate = Work_Mode_STA;
    general_add(NVS_WORK_MODE, &work_mode_ate);

    cJSON_Delete(json);
    return 0;
}
//----------------6-------------------//
char *protocol_ate_apget_handler(void)
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    // char buf[70] = {0};

    wifi_ap_para_t ap_para = {0};
    general_query(NVS_AP_PARA, &ap_para);

    cJSON_AddStringToObject(res, "ssid", (char *)ap_para.ssid);
    cJSON_AddStringToObject(res, "password", (char *)ap_para.password);
    // cJSON_AddNumberToObject(res, "typ", 1);

    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    return msg;
}
//-----------------7------------------//
int8_t protocol_ate_apset_handler(char *body)
{
    wifi_ap_para_t ap_para = {0};
    cJSON *json;
    ASW_LOGI("%s\n", body);
    json = cJSON_Parse(body);
    if (json == NULL)
        return -1;

    getJsonStr((char *)ap_para.ssid, "ssid", sizeof(ap_para.ssid), json);
    getJsonStr((char *)ap_para.password, "password", sizeof(ap_para.password), json);

    general_add(NVS_AP_PARA, &ap_para);

    cJSON_Delete(json);
    return 0;
}
//------------------8-----------------//
int8_t protocol_ate_paraset_handler(char *body)
{
    Setting_Para setting = {0};
    cJSON *json;
    printf("==== ate_paraset  handler: %s", body);
    json = cJSON_Parse(body);
    if (json == NULL)
        return -1;

    ASW_LOGI("%s\n", body);

    getJsonStr(setting.psn, "psn", sizeof(setting.psn), json);
    getJsonStr(setting.reg_key, "key", sizeof(setting.reg_key), json);
    wifi_ap_para_t ap_para = {0};

    /*
     [tgl add] if no value to set ap.ssid&ap.password
     to default value AISWEI-LANSTICK 12345678
     // 20220621
   */
    if (strlen(setting.psn) > 0)
    {
        sprintf((char *)ap_para.ssid, "%s%s", "AISWEI-", setting.psn + strlen(setting.psn) - 4);
    }
    else
    {
        memset(ap_para.ssid, 0, sizeof(ap_para.ssid));
    }

    if (strlen(setting.reg_key) > 0)
    {
        sprintf((char *)ap_para.password, "%s", setting.reg_key);
    }
    else
    {
        memset(ap_para.password, 0, sizeof(ap_para.password));
    }

    // sprintf((char *)ap_para.ssid, "%s%s", "AISWEI-", setting.psn + strlen(setting.psn) - 4);
    // sprintf((char *)ap_para.password, "%s", setting.reg_key);
    general_add(NVS_AP_PARA, &ap_para);
    // blufi_name_t name = {0};
    // sprintf(name, "%s%s", "AISWEI-", setting.psn + strlen(setting.psn) - 4);

    // if (legal_check(setting.reg_key) == -1 || legal_check(setting.psn) == -1)
    // {
    //     cJSON_Delete(json);
    //     return -1;
    // }
    // general_add(NVS_BLUFI_NAME, name);

    getJsonNum(&setting.typ, "typ", json);
    getJsonStr(setting.nam, "nam", sizeof(setting.nam), json);
    getJsonStr(setting.mod, "mod", sizeof(setting.mod), json);
    getJsonStr(setting.mfr, "muf", sizeof(setting.mfr), json);
    getJsonStr(setting.brw, "brd", sizeof(setting.brw), json);
    getJsonStr(setting.hw, "hw", sizeof(setting.hw), json);
    // getJsonStr(setting.sw, "sw", sizeof(setting.sw), json);
    getJsonStr(setting.wsw, "wsw", sizeof(setting.wsw), json);
    getJsonStr(setting.product_key, "pdk", sizeof(setting.product_key), json);
    getJsonStr(setting.device_secret, "ser", sizeof(setting.device_secret), json);

    getJsonStr(setting.host, "ali_ip", sizeof(setting.host), json);
    getJsonNum(&setting.port, "ali_port", json);
    getJsonNum(&setting.meter_en, "meter_en", json);
    getJsonNum(&setting.meter_add, "meter_add", json);
    getJsonNum(&setting.meter_mod, "meter_mod", json);

    general_add(NVS_ATE, &setting);

    if (legal_check(setting.psn) == ASW_OK)
    {
        xSemaphoreGive(g_semar_psn_ready);
    }

    // int elink = -1;
    // // int work_mode_ate = Work_Mode_NORMAL;
    // getJsonNum(&elink, "elink", json);

    // if (elink == 1)
    // {
    //     work_mode_ate = Work_Mode_SMART_PROV;
    // }
    // general_add(NVS_WORK_MODE, &work_mode_ate);

    cJSON_Delete(json);
    return 0;
}

//----------------9-------------------//
char *protocol_ate_bluget_handler(void)
{
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    // char buf[70] = {0};
    blufi_name_t name = {0};
    general_query(NVS_BLUFI_NAME, name);

    cJSON_AddStringToObject(res, "blufi_name", name);
    // cJSON_AddNumberToObject(res, "typ", 1);

    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    return msg;
}
//--------------10-------------------//
void protocol_ate_nvs_clear_handler(void)
{
    asw_nvs_clear();
    asw_db_open(); // [tgl update]新版文件 改动的函数
    ate_reboot_t msg = {0};
    memcpy(msg, "rebooting", strlen("rebooting"));
    general_add(NVS_ATE_REBOOT, msg);
    // esp_restart();
}

//------------------------------------//
char *protocol_ate_lan_get_handler(void)
{
    // oia,ip,msk,gtw,mac,oda,dns
    cJSON *res = cJSON_CreateObject();
    char *msg = NULL;
    // char buf[70] = {0};
    net_static_info_t st_net_lanInfo = {0};
    general_query(NVS_NET_STATIC_INFO, &st_net_lanInfo);

    char mIp[16] = {0};
    char mMsk[16] = {0};
    char mGtw[16] = {0};
    char mDns[16] = {0};
    char mMac[18] = {0};
    char mEnbale[8] = {0};

    sprintf(mEnbale, "%hd", st_net_lanInfo.enble);

    int mStaticEnabele = -1;

    if (st_net_lanInfo.enble && st_net_lanInfo.work_mode == g_stick_run_mode)
    {
        mStaticEnabele = 1;
    }
    else
    {
        mStaticEnabele = 0;
    }

    if (mStaticEnabele)
    {
        cJSON_AddStringToObject(res, "oia", "1");

        sprintf(mIp, "%hd.%hd.%hd.%hd", st_net_lanInfo.ip[0], st_net_lanInfo.ip[1], st_net_lanInfo.ip[2], st_net_lanInfo.ip[3]);
        sprintf(mMsk, "%hd.%hd.%hd.%hd", st_net_lanInfo.mask[0], st_net_lanInfo.mask[1], st_net_lanInfo.mask[2], st_net_lanInfo.mask[3]);
        sprintf(mGtw, "%hd.%hd.%hd.%hd", st_net_lanInfo.gateway[0], st_net_lanInfo.gateway[1], st_net_lanInfo.gateway[2], st_net_lanInfo.gateway[3]);
        sprintf(mDns, "%hd.%hd.%hd.%hd", st_net_lanInfo.maindns[0], st_net_lanInfo.maindns[1], st_net_lanInfo.maindns[2], st_net_lanInfo.maindns[3]);

        if (st_net_lanInfo.work_mode == Work_Mode_STA)
        {
            get_sta_mac(mMac);
        }
        else if (st_net_lanInfo.work_mode == Work_Mode_LAN)
        {
            get_eth_mac(mMac);
        }
    }
    else
    {
        cJSON_AddStringToObject(res, "oia", "0");

        if (g_stick_run_mode == Work_Mode_STA)
        {
            memcpy(mIp, my_sta_ip, sizeof(my_sta_ip));
            memcpy(mMsk, my_sta_mk, sizeof(my_sta_mk));
            memcpy(mGtw, my_sta_gw, sizeof(my_sta_gw));
            get_sta_mac(mMac);
            memcpy(mDns, my_sta_dns, sizeof(mDns));
        }
        else if (g_stick_run_mode == Work_Mode_LAN)
        {

            memcpy(mIp, my_eth_ip, sizeof(my_eth_ip));
            memcpy(mMsk, my_eth_mk, sizeof(my_eth_mk));
            memcpy(mGtw, my_eth_gw, sizeof(my_eth_gw));
            get_eth_mac(mMac);
            memcpy(mDns, my_eth_dns, sizeof(mDns));
        }
    }

    cJSON_AddStringToObject(res, "ip", mIp);
    cJSON_AddStringToObject(res, "gtw", mGtw);
    cJSON_AddStringToObject(res, "msk", mMsk);
    cJSON_AddStringToObject(res, "mac", mMac);

    cJSON_AddStringToObject(res, "oda", "1");
    cJSON_AddStringToObject(res, "dns", mDns);
    // cJSON_AddNumberToObject(res, "typ", 1);

    msg = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    return msg;
}
//---------------11----------------//
int8_t protocol_ate_update_handler(char *body)
{
    if (g_state_mqtt_connect != 0)
    {
        ASW_LOGE("Mqtt is no conncetd!!");
        return -1;
    }

    char url[300] = {0};
    cJSON *json;
    ASW_LOGI("%s\n", body);
    json = cJSON_Parse(body);
    if (json == NULL)
        return -1;

    getJsonStr(url, "url", sizeof(url), json);
    if (strstr(url, "http") == NULL) //[tgl mark] 这里添加检测是？？？
        return -1;
    update_firmware(url);

    cJSON_Delete(json);
    return 0;
}
//---------------12--------------------//

int8_t protocol_ate_network_handler(void)
{
    if (g_state_mqtt_connect != 0)
        return -1;
    else
        return 0;
}

//--------------13---------------------//
int8_t protocol_ate_elinkset_handler(char *body)
{
    // Setting_Para setting = {0};
    cJSON *json;
    ASW_LOGI("%s\n", body);
    json = cJSON_Parse(body);
    if (json == NULL)
        return -1;

    // int elink = -1;
    // int work_mode_ate = Work_Mode_NORMAL;
    // getJsonNum(&elink, "elink", json);

    // if (elink == 1)
    // {
    //     work_mode_ate = Work_Mode_SMART_PROV;
    // }
    // general_add(NVS_WORK_MODE, &work_mode_ate);

    cJSON_Delete(json);
    return 0;
}
