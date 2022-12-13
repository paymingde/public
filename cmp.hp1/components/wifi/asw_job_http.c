#include "asw_job_http.h"
#include "asw_protocol_ate.h"
#include "asw_protocol_nearby.h"
#include "esp_task_wdt.h"
#include "estore_cld.h"

//  CONFIG_ASW_NEARBY_BLE_ENABLE
//  CONFIG_ASW_ATE_HTTP_ENABLE
//  CONFIG_ASW_NEARBY_HTTP_ENABLE
static const char *TAG = "asw_job_http.c";

//---------------------------------------------//

/* An HTTP POST handler */
#define FORM_MATCHER "multipart/"
#define BOUNDARY_MATCHER "boundary="
#define NEWLINE_MATCHER "\r\n"
#define DOUBLELINE_MATCHER "\r\n\r\n"
#define FILENAME_MATCHER "filename=\""
#define FILENAME_END_MATCHER "\"\r\n"

const char *POST_RES_OK = "{\"dat\":\"ok\"}";
const char *POST_RES_ERR = "{\"dat\":\"err\"}";

const char *update_page = "<html>"
                          "<head>"
                          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
                          "<title>固件升级</title>"
                          "</head>"
                          "<body>"
                          "<h1>ESP32监控器固件升级</h1>"
                          "<form action=\"/update.cgi\" enctype=\"multipart/form-data\" method=\"POST\" id=\"from1\"><input type=\"file\" "
                          "name=\"esp32app\" id=\"Img1\"><br><input id=\"q\" type=\"submit\"></form>"
                          "</body>"
                          "</html>";

typedef enum MultPart_Stat_EM
{
    MULTIPART_WAIT_START,
    MULTIPART_WAIT_FINISH
};

typedef struct
{
    const char *cgi;
    void (*hook)(httpd_req_t *req, http_req_para_t *para);
} http_req_tab_t;
//-----------------------------------------//
static const int boundary_matcher_len = strlen(BOUNDARY_MATCHER);
static const int doubleline_matcher_len = strlen(DOUBLELINE_MATCHER);
static const int filename_matcher_len = strlen(FILENAME_MATCHER);

//-------------------5--------------------------//
static void web_page_handler(httpd_req_t *req, http_req_para_t *para)
{
    ASW_LOGW("******* web_page_handler handle..");

    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
    httpd_resp_send(req, update_page, strlen(update_page));
}

//------------------6------------------------//
void update_handler(httpd_req_t *req, http_req_para_t *para)
{
    ASW_LOGW("******* update_handler  handle..");
    assert(req != NULL);

    char *msg = "update ok";
    httpd_resp_send(req, msg, strlen(msg));
    return;
}

//***************************************//
// #if CONFIG_ASW_NEARBY_HTTP_ENABLE

//------------------1----------------------//
static void getdev_handler(httpd_req_t *req, http_req_para_t *para) //[tgl lan]delete wills
{

    char *msg = NULL;
    uint16_t deviceTyep = atoi(para->device);
    uint16_t stateSrt = atoi(para->secret);

    char psn[50] = {0};
    if (strlen(para->sn) > 0)
        memcpy(psn, para->sn, sizeof(para->sn));

    ASW_LOGW("------deviceTyep:%d,stateSrt:%d", deviceTyep, stateSrt);

    msg = protocol_nearby_getdev_handler(deviceTyep, stateSrt, psn);

    if (msg != NULL)
    {
        uint16_t m_len = strlen(msg);
        httpd_resp_send(req, msg, m_len);
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}

//------------------2-------------------------//

static void getdevdata_handler(httpd_req_t *req, http_req_para_t *para) //[tgl lan]delete will
{
    ASW_LOGW("******* getdevdata handle..");

    char *msg = NULL;
    uint16_t deviceTyep = atoi(para->device);

    msg = protocol_nearby_getdevdata_handler(deviceTyep, para->sn);

    if (msg != NULL)
    {
        uint16_t m_len = strlen(msg);
        httpd_resp_send(req, msg, m_len);
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}

//-----------------------------------------//
// 11- 获取debug info

static void get_debug_info_handler(httpd_req_t *req, http_req_para_t *para)
{
    ASW_LOGI("************get debug info ....");
    char *msgBuf = NULL;

    cJSON *res = cJSON_CreateObject();

    cJSON_AddNumberToObject(res, "debug_value", g_asw_debug_enable);

    msgBuf = cJSON_PrintUnformatted(res);

    cJSON_Delete(res);

    if (msgBuf != NULL)
    {
        uint16_t m_len = strlen(msgBuf);
        httpd_resp_send(req, msgBuf, m_len);
        free(msgBuf);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}
//-------------------3-----------------------//

static void wlanget_handler(httpd_req_t *req, http_req_para_t *para) //[tgl lan]delete will
{

    ASW_LOGW("******* wlanget_handler..");

    char *msg = NULL;
    uint16_t mParaInfo = atoi(para->info);

    msg = protocol_nearby_wlanget_handler(mParaInfo);
    // printf("\n test  wlanget : %s \n", msg);

    if (msg != NULL)
    {
        uint16_t m_len = strlen(msg);
        httpd_resp_send(req, msg, m_len);
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}
//---------------------------------------------//
//--------------------4----------------------//
static void wlanset_handler(httpd_req_t *req, http_req_para_t *para)
{

    ASW_LOGW("******* wlanset  handle..");

    int8_t m_res_cmdId = -1;

    char action[30] = {0};
    cJSON *json;
    const char *msg = "error : wlanset_handler";

    json = cJSON_Parse(para->body);
    if (json == NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        return;
    }
    getJsonStr(action, "action", sizeof(action), json);
    cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");

    m_res_cmdId = protocol_nearby_wlanset_handler(action, value);

    switch (m_res_cmdId)
    {
    case -1:
        httpd_resp_send(req, msg, strlen(msg));
        break;

    case 0:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        break;

    case CMD_REBOOT:
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        if (json != NULL)
            cJSON_Delete(json);
        ASW_LOGW("Restart will....");
        usleep(500 * 1000); // 500ms
        esp_restart();
    }
    break;
#if STATIC_IP_SET_ENABLE
    case CMD_STATIC_ENABLE:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        usleep(2000);
        config_net_static_set(NULL);

        break;

    case CMD_STATIC_DISABLE:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        usleep(2000);
        config_net_static_diable_set(NULL);

        break;
#endif
    case CMD_SYNC_REBOOT:
        /// TDODO  sync from send msg.

        xSemaphoreTake(g_semar_wrt_sync_reboot, portMAX_DELAY); // TODO

        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        usleep(1000);
        esp_restart();
        break;

    default:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        break;
    }

    if (json != NULL)
        cJSON_Delete(json);
    return;
}

//-------------------7-------------------------//

static void fdbg_handler(httpd_req_t *req, http_req_para_t *para)
{
    char *msg = NULL; /** 动态内存*/
    cJSON *json = cJSON_Parse(para->body);

    msg = protocol_nearby_fdbg_handler(json);

    if (json != NULL)
        cJSON_Delete(json);
    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
        return;
    }
    else
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
        return;
    }
}

//----------------8------------------------//
static void setting_handler(httpd_req_t *req, http_req_para_t *para) //[tgl lan]delete will
{

    ASW_LOGW("******* setting  handle..");
    int device = 0;
    char action[30] = {0};
    cJSON *json;
    const char *msg = "error : setting_handler";

    int16_t m_res_cmdId = -1;

    ASW_LOGW("setting handler:%d,%s\n", para->body_len, para->body);

    json = cJSON_Parse(para->body);
    if (json == NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        return;
    }

    ASW_LOGW("THe body len is %d\n", strlen(para->body));

    getJsonNum(&device, "device", json);
    getJsonStr(action, "action", sizeof(action), json);
    cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");

    printf("\n-------------setting_handler -------------\n");
    printf(" device:%d,action:%s", device, action);
    printf("\n-------------setting_handler -------------\n");

    m_res_cmdId = protocol_nearby_setting_handler((uint16_t)device, action, value);

    switch (m_res_cmdId)
    {
    case -1:
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
        break;

    case 0:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        break;

    case CMD_REBOOT:
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        if (json != NULL)
            cJSON_Delete(json);
        ASW_LOGW("Restart will....");
        usleep(500 * 1000); // 500ms
        esp_restart();
    }
    break;

    case CMD_RESET:
    {
        clear_file_system();
        factory_reset_nvs();
        esp32_wifinvs_clear();
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        ASW_LOGW("Reset will....");
        if (json != NULL)
            cJSON_Delete(json);
        usleep(500 * 1000); // 500ms
        esp_restart();
    }
    break;

    case CMD_FORMAT:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        clear_file_system();
        break;

    case CMD_SCAN:
    { /** 扫描逆变器*/
        char u8msg[8] = {0};
        unsigned char cnt = 15;
        int byteLen = 1;
        u8msg[0] = cnt;
        ASW_LOGI("cgi cnt int %d  %d\n", cnt, u8msg[0]);

#if TRIPHASE_ARM_SUPPORT
        send_cgi_msg(50, 0, u8msg, byteLen, NULL);
#else
        send_cgi_msg(50, u8msg, byteLen, NULL); //[tgl mark]？？？
#endif

        g_scan_stop = 0;

        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    break;

    case CMD_PARALLEL: //并机模式
    {
        /// have done in the func()
    }
    break;

    case CMD_SSC_SET: //使能光储充
    {
        /// have done in the func()
    }
    break;
///////// SET STATIC INFO ///////////
#if STATIC_IP_SET_ENABLE
    case CMD_STATIC_ENABLE:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        usleep(500);
        sleep(2);
        config_net_static_set(NULL);
        // esp_restart();

        break;

    case CMD_STATIC_DISABLE:
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
        usleep(500);
        sleep(2);
        config_net_static_diable_set(NULL);
        //  esp_restart();
        break;
        ///////////////// END //////////////
#endif
    default:
        httpd_resp_send(req, msg, strlen(msg));
        break;
    }

    if (json != NULL)
        cJSON_Delete(json);
    return;
}
//----------------------------------------//
static void debug_handler(httpd_req_t *req, http_req_para_t *para)
{
    ASW_LOGI("Debug handler...");

    int debug_print_value = -1;

    int device = -1;

    ASW_LOGW("debug handler:%d,%s\n", para->body_len, para->body);

    cJSON *json;
    json = cJSON_Parse(para->body);
    if (json == NULL)
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
        return;
    }

    getJsonNum(&device, "device", json);
    getJsonNum(&debug_print_value, "debug_value", json);

  

    if (device == 666 && debug_print_value >= 0)
    {
        g_asw_debug_enable = debug_print_value;

        ESP_LOGI("DEBUG-SET", " debug set value :%d  OK", g_asw_debug_enable);
    }
    else
    {
        ESP_LOGW(TAG, " debug set value Failed");
    }

    httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
}
//--------------------9----------------------//
static void innerfile_handler(httpd_req_t *req, http_req_para_t *para)
{

    ASW_LOGW("******* innerfile  handle..");

    char msg[500] = {0};
    char *netlog = "/home/netlog.txt";

    printf("LOG NET --------------------------------\r\n");
    if (strlen(para->filename) == 0)
    {
        get_file_list(msg);
        httpd_resp_send(req, msg, strlen(msg));
        return;
    }
    else
    {
        char path[100] = {0};
        sprintf(path, "/home/%s", para->filename);

        if (strcmp(path, netlog) == 0)
        {
            FILE *fp = NULL;
            fp = fopen(netlog, "r");
            if (fp != NULL)
            {
                char buf[100] = {0};
                while (!feof(fp)) // to read file
                {
                    memset(buf, 0, sizeof(buf));
                    int nread = fread(buf, 1, 100, fp);
                    httpd_resp_send_chunk(req, buf, nread);
                }
                httpd_resp_send_chunk(req, NULL, 0);

                fclose(fp);
                return;
            }
        }
    }
    httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
}

//------------------10-----------------------//
static void get_log_handler(httpd_req_t *req, http_req_para_t *para)
{
    ASW_LOGW("************get_log_handler....");
    char buff[1024] = {0};

    get_all_log(buff);
    printf("cgisyslog %d--%s \n", strlen(buff), buff);

    if (strlen(buff))
        httpd_resp_send_chunk(req, buff, strlen(buff));
    else
        httpd_resp_send_chunk(req, "no log", strlen("no log"));
}

//---------------------------------------*1
static void http_ate_reboot_cmd(httpd_req_t *req, http_req_para_t *para)
{
    protocol_ate_reboot_handler();
    httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    usleep(500 * 1000);
    esp_restart();
}

//---------------------------------------*2
static void http_ate_nvs_clear_cmd(httpd_req_t *req, http_req_para_t *para)
{
    protocol_ate_nvs_clear_handler();

    httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    usleep(500 * 1000);
    esp_restart();
}

//---------------------------------------*3
static void http_ate_paraset_cmd(httpd_req_t *req, http_req_para_t *para)
{
    int8_t res_code = -1;
    res_code = protocol_ate_paraset_handler(para->body); // 8

    if (res_code == 0)
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    else
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}

//---------------------------------------*4
static void http_ate_paraget_cmd(httpd_req_t *req, http_req_para_t *para)
{
    char *msg = NULL;                     /** 动态内存*/
    msg = protocol_ate_paraget_handler(); // 1
    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}

//---------------------------------------*5
static void http_ate_ifconfig_cmd(httpd_req_t *req, http_req_para_t *para)
{
    char *msg = NULL;                      /** 动态内存*/
    msg = protocol_ate_ifconfig_handler(); // 1

    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}

//---------------------------------------*6
static void http_ate_staget_cmd(httpd_req_t *req, http_req_para_t *para)
{
    char *msg = NULL;                    /** 动态内存*/
    msg = protocol_ate_staget_handler(); // 1

    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}

//---------------------------------------*7
static void http_ate_staset_cmd(httpd_req_t *req, http_req_para_t *para)
{
    int8_t res_code = -1;
    res_code = protocol_ate_staset_handler(para->body); // 8

    if (res_code == 0)
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    else
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}

//---------------------------------------*8
static void http_ate_apset_cmd(httpd_req_t *req, http_req_para_t *para)
{
    int8_t res_code = -1;
    res_code = protocol_ate_apset_handler(para->body); // 8

    if (res_code == 0)
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    else
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}

//---------------------------------------*9
static void http_ate_apget_cmd(httpd_req_t *req, http_req_para_t *para)
{
    char *msg = NULL;                   /** 动态内存*/
    msg = protocol_ate_apget_handler(); // 1

    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}
//---------------------------------------*10
static void http_ate_bluget_cmd(httpd_req_t *req, http_req_para_t *para) // blufi没用 [tgl mark]
{
    char *msg = NULL;                    /** 动态内存*/
    msg = protocol_ate_bluget_handler(); // 1

    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}
//---------------------------------------*11
static void http_ate_echo_cmd(httpd_req_t *req, http_req_para_t *para)
{
    httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
}

/// lan.cgi
//-------------------------------  11
static void get_lan_netinfo_handler(httpd_req_t *req, http_req_para_t *para)
{
    char *msg = NULL;                     /** 动态内存*/
    msg = protocol_ate_lan_get_handler(); // 1

    if (msg != NULL)
    {
        httpd_resp_send(req, msg, strlen(msg));
        free(msg);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
    }
}
//---------------------------------------//
void getdefine_handler(httpd_req_t *req, http_req_para_t *para)
{
    char msg[1536] = {0};

    // printf("\n----- getdefine_handler ---[%d]--------\n",para->date_day);
    read_battery_define(para->date_day, msg);
    printf("%.*s\n", strlen(msg), msg);
    httpd_resp_send(req, msg, strlen(msg));
}

//---------------------------------------*12
static void http_ate_network_cmd(httpd_req_t *req, http_req_para_t *para)
{
    int8_t res_code = -1;
    res_code = protocol_ate_network_handler(); // 8

    if (res_code == 0)
    {
        httpd_resp_send(req, "0", strlen("0"));
    }
    else
    {
        httpd_resp_send(req, "-1", strlen("-1"));
    }
}
//---------------------------------------*13
static void http_ate_elinkset_cmd(httpd_req_t *req, http_req_para_t *para)
{

    int8_t res_code = -1;
    res_code = protocol_ate_elinkset_handler(para->body); // 8

    if (res_code == 0)
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    else
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}

//---------------------------------------*14
static void http_ate_update_cmd(httpd_req_t *req, http_req_para_t *para)
{

    int8_t res_code = -1;
    res_code = protocol_ate_update_handler(para->body); // 8

    if (res_code == 0)
    {
        httpd_resp_send(req, POST_RES_OK, strlen(POST_RES_OK));
    }
    else
    {
        httpd_resp_send(req, POST_RES_ERR, strlen(POST_RES_ERR));
    }
}

//---------------------------------------*2
static const http_req_tab_t http_get_tab[] = {
    //------------Http ---ATE-----------//
    {"/reboot.cgi", http_ate_reboot_cmd},       //*1
    {"/nvs_clear.cgi", http_ate_nvs_clear_cmd}, //*2
    {"/paraget.cgi", http_ate_paraget_cmd},     //*4
    {"/ifconfig.cgi", http_ate_ifconfig_cmd},   //*5
    {"/staget.cgi", http_ate_staget_cmd},       //*6
    {"/apget.cgi", http_ate_apget_cmd},         //*9
    {"/bluget.cgi", http_ate_bluget_cmd},       //*10
    {"/echo.cgi", http_ate_echo_cmd},           //*11
    {"/netstate.cgi", http_ate_network_cmd},    //*12

    // {"/update.cgi", update_handler}         //-6-1};
    //--------------http---App-------//
    {"/getdev.cgi", getdev_handler},         //-1- ble
    {"/getdevdata.cgi", getdevdata_handler}, //-2- ble
    {"/wlanget.cgi", wlanget_handler},       //-3- ble
    {"/webpage.cgi", web_page_handler},      //-5-1 http
    {"/innerfile.cgi", innerfile_handler},   //-9-1 http
    {"/get_sys_log.cgi", get_log_handler},   //-10-

    //---------   add for lanstick-----
    {"/lan.cgi", get_lan_netinfo_handler}, //-11-
    {"/getdefine.cgi", getdefine_handler}, //-12-
    {"/getdebug.cgi", get_debug_info_handler}};

static const http_req_tab_t http_post_tab[] = {
    //------------Http ---ATE-----------//
    {"/paraset.cgi", http_ate_paraset_cmd},   // //*3 apn,aliyun:host,port,psn,keepalive_ms,
    {"/staset.cgi", http_ate_staset_cmd},     //*7
    {"/apset.cgi", http_ate_apset_cmd},       //*8
    {"/elinkset.cgi", http_ate_elinkset_cmd}, //*13
    {"/update_ate.cgi", http_ate_update_cmd}, //*14

    //--------------http---App-------//
    {"/wlanset.cgi", wlanset_handler}, //-4- ble
    {"/update.cgi", update_handler},   //-6-1 http
    {"/fdbg.cgi", fdbg_handler},       //-7- ble
    {"/setting.cgi", setting_handler}, //-8- ble
    {"/debug.cgi", debug_handler}};    //啓用打印調試信息};

//---------------------------------------------//
Update_p update_info = NULL;

#if !TRIPHASE_ARM_SUPPORT
void update_info_gen(void)
{
    char *head_ptr = NULL;
    if (update_info == NULL)
    {
        printf("err: update_info is null\n");
        return;
    }
    if (strstr(update_info->file_name, "master") != NULL)
    {
        head_ptr = update_info->file_name + strlen("master");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "/home/%s", update_info->file_name);

        update_info->file_type = 1;
    }
    else if (strstr(update_info->file_name, "safety") != NULL)
    {
        head_ptr = update_info->file_name + strlen("safety");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "/home/%s", update_info->file_name);

        update_info->file_type = 4;
    }
    else if (strstr(update_info->file_name, "slave") != NULL)
    {
        head_ptr = update_info->file_name + strlen("slave");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        update_info->file_type = 4;
    }
    else if (strstr(update_info->file_name, "update") != NULL)
    {
        head_ptr = update_info->file_name + strlen("update");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "%s", "/home/update.bin");

        update_info->file_type = 0;
    }
    else
    {
        update_info->file_type = -1;
    }
    printf("update_info->file_type: %d\n", update_info->file_type);
}
#else

/////////////////////////////////////////////
/*     lanstick - updat.c               */
////////////////////////////////////////////

// #if TRIPHASE_ARM_SUPPORT
void update_info_gen(void)
{
    char *head_ptr = NULL;
    if (update_info == NULL)
    {
        printf("err: update_info is null\n");
        return;
    }
    if (strstr(update_info->file_name, "master") != NULL)
    {
        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "/home/%s", update_info->file_name);

        update_info->file_type = 1;
    }
    else if (strstr(update_info->file_name, "safety") != NULL)
    {
        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "/home/%s", update_info->file_name);

        update_info->file_type = 4;
    }
    else if (strstr(update_info->file_name, "slave") != NULL)
    {
        update_info->file_type = 2;
    }
    else if (strstr(update_info->file_name, "update") != NULL)
    {
        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "%s", "/home/update.bin");

        update_info->file_type = 0;
    }
    else if (strstr(update_info->file_name, "comm") != NULL)
    {

        memset(update_info->file_path, 0, sizeof(update_info->file_path));
        sprintf(update_info->file_path, "/home/%s", update_info->file_name);

        update_info->file_type = 3;
    }
    else
    {
        update_info->file_type = -1;
    }
    printf("update_info->file_type: %d\n", update_info->file_type);
}

#endif
//================================================//
//-----------------------get_handler----------------//
static esp_err_t asw_get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    http_req_para_t req_para = {0};

    // Eng.Stg.Mch-lanstick +
    fresh_before_http_handler();

    /** 无效请求响应  */
    ESP_LOGE("http get req url", "%s", req->uri);
    ESP_LOGE("http get req len", "%d", req->content_len);

    /** 解析head中的键值对 */

    /** 解析url中的参数 */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) //[tgl mark]获取并打印信息，作为回调参数传递
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "sn", req_para.sn, sizeof(req_para.sn)) == ESP_OK)
            {
                ASW_LOGI( "Found => sn=%s", req_para.sn);
            }
            if (httpd_query_key_value(buf, "device", req_para.device, sizeof(req_para.device)) == ESP_OK)
            {
                ASW_LOGI( "Found => device=%s", req_para.device);
            }
            if (httpd_query_key_value(buf, "secret", req_para.secret, sizeof(req_para.secret)) == ESP_OK)
            {
                ASW_LOGI( "Found => secret=%s", req_para.secret);
            }
            if (httpd_query_key_value(buf, "info", req_para.info, sizeof(req_para.info)) == ESP_OK)
            {
                ASW_LOGI( "Found => info=%s", req_para.info);
            }
            if (httpd_query_key_value(buf, "filename", req_para.filename, sizeof(req_para.filename)) == ESP_OK)
            {
                ASW_LOGI( "Found => filename=%s", req_para.filename);
            }

            ////////////////////////////////////////////////////////////
            char this_buf[10] = {0};
            if (httpd_query_key_value(buf, "date_day", this_buf, sizeof(this_buf)) == ESP_OK)
            {
                ASW_LOGI( "Found => date_day=%s", this_buf);
                int num = -1;
                int parsed = 0;
                parsed = sscanf(this_buf, "%d", &num);
                if (parsed == 1)
                {
                    req_para.date_day = num;
                }
            }
            else
            {
                req_para.date_day = -1;
            }
            //////////////////////////////////////////////////
        }
        free(buf);
    }

    int i = 0;
    int req_num = sizeof(http_get_tab) / sizeof(http_req_tab_t);

    for (i = 0; i < req_num; i++)
    {
        if (strstr(req->uri, http_get_tab[i].cgi) != NULL)
            break;
        if (i == req_num - 1)
        {
            ASW_LOGE("[ err ] No this http request: %s\n", req->uri);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
            return ESP_OK;
        }
    }
    /** 响应头中加入自定义键值对 */
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    (*http_get_tab[i].hook)(req, &req_para);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */

    return ESP_OK;
}

//-----------------post handler-----------------------//
static esp_err_t asw_post_handler(httpd_req_t *req)
{
    char *data = NULL;
    char *decode_data = NULL;
    int data_len = 0;

    static uint32_t loop_count = 0; //文件过小 尝试次数  30*60*1000*10
    uint32_t MAX_LOOP_COUNT = 1000; // 18000000; //文件过小 尝试次数  30*60*1000*10

    bool flag_content_len = 0;

    // int decode_len = 0;
    int req_num;
    int is_cgi = 1;

    /** 接收文件所需*/
    char *content_type = NULL;
    char *boundary = NULL;
    char *finish_boundary = NULL;
    char *frame = NULL;
    char *last_frame_tail = NULL;
    char *search_box = NULL;
    int frame_len = 0;
    int last_frame_tail_len;
    int search_box_len;
    int content_type_len = 0;
    int boundary_len;
    // int start_boundary_len = 0;
    int finish_boundary_len = 0;
    char *p1;
    char *p2;
    char *p3;
    // char *p4;
    // char *p5;
    int len1;
    // int len2;
    int file_len = 0;
    int time_cnt_sec = 0;
    int nwrite;
    int multipart_state = MULTIPART_WAIT_START;
    FILE *update_file = NULL;
    int check;

    http_req_para_t req_para = {0};
    int i = 0;

    char buf[1024] = {0};
    int ret, remaining = req->content_len;
    // Eng.Stg.Mch-lanstick +
    fresh_before_http_handler();
    ESP_LOGE("http post req url", "%s", req->uri);

    ESP_LOGE("http post req len", "%d", req->content_len);

    float mFileSize = req->content_len;

    /** 检查点对点标识 */
    g_p2p_mode = 0;
    char *p2p_str = NULL;
    int p2p_len = httpd_req_get_hdr_value_len(req, "p2p");
    if (p2p_len > 0)
    {
        p2p_str = calloc(p2p_len + 1, 1);
        if (httpd_req_get_hdr_value_str(req, "p2p", p2p_str, p2p_len) == ESP_OK)
        {
            if (strcmp(p2p_str, "1") == 0)
            {
                g_p2p_mode = 1; // 38400
            }
            else if (strcmp(p2p_str, "2") == 0)
            {
                g_p2p_mode = 2; // 9600
            }
        }
        free(p2p_str);
    }

#if TRIPHASE_ARM_SUPPORT
    /** 检查逆变器序列号 */                                                        //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
    char *inv_sn = NULL;                                                           //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
    int isn_len = httpd_req_get_hdr_value_len(req, "inv_sn");                      //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
    if (isn_len > 0)                                                               //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
    {                                                                              //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
        inv_sn = calloc(isn_len + 1, 1);                                           //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
        if (httpd_req_get_hdr_value_str(req, "inv_sn", inv_sn, isn_len) == ESP_OK) //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
        {                                                                          //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
                                                                                   // TODO 																					//增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
        }                                                                          //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
        free(p2p_str);                                                             //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
    }                                                                              //增加http头的内容检查，检查点对点标识和逆变器序列号，这里cgi没给出，还要改，包括上面的函数，还要加一个获取指定的modbusid的方法
    /** 检查接收multipart文件*/
    content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type") + 1;
#endif

    /** 检查接收multipart文件*/

    content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type") + 1;

    /*  判断 升级文件大小 */

    if (content_type_len > 1)
    {

        content_type = malloc(content_type_len);
        memset(content_type, 0, content_type_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, content_type_len) == ESP_OK)
        {

            ESP_LOGE(TAG, "Found header => Content-Type: %s", content_type);
            /** 匹配multipart，提取分割线*/
            p1 = strstr(content_type, FORM_MATCHER); //判断是否是传送的文件
            if (p1 != NULL)
            {
                is_cgi = 0;
                update_info = (Update_p)malloc(sizeof(Update_t));
                memset(update_info, 0, sizeof(Update_t));
                req_para.update_info = update_info;
                p1 = strstr(content_type, BOUNDARY_MATCHER);
                if (p1 != NULL)
                {
                    p2 = p1 + boundary_matcher_len;
                    boundary_len = strlen(p2);
                    boundary = malloc(boundary_len);
                    if (boundary != NULL)
                    {
                        /** 获取到boundary*/
                        memcpy(boundary, p2, boundary_len);
                        printf("my boundary: %.*s\n", boundary_len, boundary);
                        /** 拼接结束分割线*/
                        finish_boundary = malloc(boundary_len + 6);
                        memcpy(finish_boundary, "\r\n--", strlen("\r\n--"));
                        memcpy(finish_boundary + 4, boundary, boundary_len);
                        memcpy(finish_boundary + 4 + boundary_len, "--", strlen("--"));
                        finish_boundary_len = boundary_len + 6;
                        last_frame_tail_len = finish_boundary_len - 1;

                        /** 计时器，控制超时*/
                        time_cnt_sec = get_second_sys_time();
                        while (1)
                        {

                            /* 文件大小及类型的判断，放在网页脚本代码进行判断 TODO */
                            if (flag_content_len && loop_count > MAX_LOOP_COUNT) //连续收到数据超过 1000 次（1s0且长度小于1024bytes，判断文件为空，退出
                            {
                                assert(req != NULL);
                                char *msg = "update erro :  file is empty or too little  !!";
                                httpd_resp_send(req, msg, strlen(msg));

                                goto OK;
                            }

                            // printf("time now1 %lld--------------------------------------------############ \n", esp_timer_get_time());
                            ret = httpd_req_recv(req, buf, sizeof(buf));
                            // printf("ffbuf %.*s \n", ret, buf);
                            // printf("recv ret %d--------------------------------------------############ \n", ret);
                            //  if (ret == HTTPD_SOCK_ERR_TIMEOUT || get_second_sys_time() - time_cnt_sec > 300)
                            if (get_second_sys_time() - time_cnt_sec > 1800) // 1800 //300
                            {
                                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "recv post file timeout, 404");
                                goto OK;
                            }
                            if (ret <= 0) // 1s 不会超时 3s喂狗
                            {
                                loop_count++;

                                usleep(1000);
                                continue;
                            }

                            loop_count = 0;

                            frame = realloc(frame, frame_len + ret);
                            memcpy(frame + frame_len, buf, ret);
                            frame_len = frame_len + ret;

                            // printf("time now22 %lld S----------------------------------############ \n", esp_timer_get_time() / (1000 * 1000));

                            // printf("recv ret %d-%lld,--------------------------------------------############ \n", ret, esp_timer_get_time());
                            // continue;

                            switch (multipart_state)
                            {
                            case MULTIPART_WAIT_START:
                                /** 文件至少1024字节*/
                                if (frame_len > 1024)
                                {
                                    p1 = memmem(frame, frame_len, FILENAME_MATCHER, filename_matcher_len);
                                    if (p1 != NULL)
                                    {
                                        p2 = memmem(p1, frame + frame_len - p1, FILENAME_END_MATCHER, strlen(FILENAME_END_MATCHER));
                                        if (p2 != NULL)
                                        {
                                            p1 = p1 + filename_matcher_len;

                                            printf("filename s%s e%s \n", p2, p1);
                                            memcpy(req_para.update_info->file_name, p1, p2 - p1);
                                            printf("ffname %s \n", req_para.update_info->file_name);
                                            update_info_gen();
                                            if (update_info->file_type == -1)
                                            {
                                            }

                                            /** 已获取到文件名*/
                                            printf("my filename: %s\n", req_para.update_info->file_name);

                                            p1 = memmem(p2, frame + frame_len - p2, DOUBLELINE_MATCHER, doubleline_matcher_len);
                                            if (p1 != NULL)
                                            {
                                                p2 = p1 + doubleline_matcher_len;
                                                len1 = frame + frame_len - p2;
                                                // p1 = memmem(p2, frame + frame_len - p2, finish_boundary, finish_boundary_len);
                                                // ESP_LOGE("RECV FILE START", "%.*s\n", len1, p2);
                                                printf("RECV FILE START %d \n", len1);
                                                /** 存到文件：pmu除外*/
                                                if (update_info->file_type >= 0)
                                                {
                                                    printf("write fopen ...%d %s\n", update_info->file_type, update_info->file_path);
                                                    if (update_info->file_type > 0)
                                                        update_file = fopen("/home/inv.bin", "wb"); // fopen(update_info->file_path, "wb");
                                                    else
                                                        update_file = fopen(update_info->file_path, "wb");

                                                    if (update_file == NULL)
                                                    {
                                                        printf("updatefile fopen fail \n");
                                                        force_flash_reformart("storage");
                                                    }
                                                    nwrite = fwrite(p2, 1, len1 - last_frame_tail_len, update_file);
                                                    if (nwrite != len1 - last_frame_tail_len)
                                                    {
                                                        printf("update write file error %d/%d\n", nwrite, len1);
                                                        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "write file header error, 505");
                                                        force_flash_reformart("storage");
                                                        sleep(2);
                                                        sent_newmsg();
                                                    }
                                                }
                                                else if (update_info->file_type == -1)
                                                {
                                                    /** 无效文件名*/
                                                    goto RESPONSE_PROCESSING;
                                                }
                                                file_len += len1 - last_frame_tail_len;
                                                printf("file len start: %d\n", file_len);
                                                /** 重新开始接收下一帧*/
                                                last_frame_tail = malloc(last_frame_tail_len);
                                                memcpy(last_frame_tail, p2 + len1 - last_frame_tail_len, last_frame_tail_len);

                                                frame_len = 0;
                                                multipart_state = MULTIPART_WAIT_FINISH;
                                            }
                                        }
                                    }

                                    flag_content_len = 0;
                                }
                                //防止文件过小，不断循环
                                else
                                {
                                    flag_content_len = 1;
                                }
                                break;
                            case MULTIPART_WAIT_FINISH:
                                // printf("time now3 %lld--------------------------------------------############ \n", esp_timer_get_time());
                                search_box_len = last_frame_tail_len + frame_len;
                                search_box = realloc(search_box, search_box_len);
                                memcpy(search_box, last_frame_tail, last_frame_tail_len);
                                memcpy(search_box + last_frame_tail_len, frame, frame_len);
                                /** 只需对新增的ret个进行匹配*/
                                p3 = search_box + search_box_len - finish_boundary_len - ret;
                                len1 = finish_boundary_len + ret;
                                // printf("------------------------len1 %d/%d\n", finish_boundary_len, ret);
                                p1 = memmem(p3, len1, finish_boundary, finish_boundary_len);
                                /** 发现结束分割线*/
                                if (p1 != NULL)
                                {
                                    p2 = search_box + last_frame_tail_len;
                                    len1 = p1 - search_box;
                                    // printf("RECV FILE FINISH: %.*s\n", len1, p2);
                                    printf("RECV FILE FINISH:  %d \n", len1);

                                    printf("\nUsed time:%lld S\n", get_second_sys_time() - time_cnt_sec);

                                    /** 存到文件*/
                                    if (update_file != NULL)
                                    {
                                        nwrite = fwrite(search_box, 1, len1, update_file);
                                        if (nwrite != len1)
                                        {
                                            printf("[ err ] write file bytes: %d/%d\n", nwrite, len1);
                                            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "write file end error, 505");
                                        }
                                        nwrite = fclose(update_file);

                                        if (nwrite != 0)
                                        {
                                            printf("[ err ] file close: %d\n", nwrite);
                                        }
                                        else
                                        {
                                            update_file = NULL; // tgl add to void to call fclose again.!!!
                                        }
                                    }

                                    file_len += len1;
                                    printf("file len finish: %d\n", file_len);

                                    /** 写入OTA分区，升级PMU*************************************************/
                                    if (update_info->file_type == 0)
                                    {
                                        printf("file path: %s\n", update_info->file_path);
                                        printf("file type: %d\n", update_info->file_type);
                                        check = sha256_file_check(update_info->file_path);
                                        if (check == 0)
                                        {
                                            httpd_resp_send(req, "update wifi stick begin", strlen("update wifi stick begin"));
                                            printf("native upgrade wifi stick start: ...\n");
                                            update_pmu();
                                            esp_restart();
                                        }
                                    }

#if TRIPHASE_ARM_SUPPORT
                                    /* 写入OTA分区，升级PMU  */
                                    //        下面的逆变器master、slave、safety以及新的arm文件全部合并成升级逆变器，不再做区分*/

                                    /** inv update*/
                                    else if (update_info->file_type > 0)
                                    {
                                        fdbg_msg_t fdbg_msg = {0};
                                        fdbg_msg.type = MSG_UPDATE_INV_MASTER;
                                        httpd_resp_send(req, "update inverter begin", strlen("update inverter begin"));
                                        // if (send_cgi_msg(30, update_info->file_path, strlen(update_info->file_path), NULL) == 0)
                                        if (send_cgi_msg(30, 0, update_info->file_path, strlen(update_info->file_path), NULL) == 0)

                                        {
                                            printf("[ ok ] MSG_UPDATE_INV send to inv\n");
                                        }
                                        else
                                        {
                                            printf("[ err ] MSG_UPDATE_INV send to inv\n");
                                        }
                                    }
#else
                                    /** master update*/
                                    else if (update_info->file_type == 1)
                                    {
                                        // fdbg_msg_t fdbg_msg = {0};
                                        // fdbg_msg.type = MSG_UPDATE_INV_MASTER; // [tgl mark]
                                        httpd_resp_send(req, "update inverter begin", strlen("update inverter begin"));
                                        if (send_cgi_msg(30, update_info->file_path, strlen(update_info->file_path), NULL) == 0)
                                        {
                                            printf("[ ok ] MSG_UPDATE_INV_MASTER send to inv\n");
                                        }
                                        else
                                        {
                                            printf("[ err ] MSG_UPDATE_INV_MASTER send to inv\n");
                                        }
                                    }
                                    /** safety update*/
                                    else if (update_info->file_type == 4)
                                    {
                                        // fdbg_msg_t fdbg_msg = {0};
                                        // fdbg_msg.type = MSG_UPDATE_INV_SAFETY; // [tgl mark]赋值没作用
                                        httpd_resp_send(req, "update inverter begin", strlen("update inverter begin"));
                                        if (send_cgi_msg(30, update_info->file_path, strlen(update_info->file_path), NULL) == 0)
                                        {
                                            printf("[ ok ] MSG_UPDATE_INV_SAFETY send to inv\n");
                                        }
                                        else
                                        {
                                            printf("[ err ] MSG_UPDATE_INV_SAFETY send to inv\n");
                                        }
                                    }

#endif

                                    multipart_state = MULTIPART_WAIT_START;
                                    goto RESPONSE_PROCESSING;
                                    break;
                                }
                                /** 纯内容，不含结束分割线*/
                                if (frame_len >= 1000)
                                {
                                    // printf("time now4 %lld--------------------------------------------############ \n", esp_timer_get_time());
                                    /** 非结束内容，保存到文件*/
                                    // ESP_LOGI("RECV FILE MIDDLE", "%.*s\n", frame_len, frame);
                                    printf("RECV FILE MIDDLE %d \n", frame_len);
                                    file_len += frame_len;
                                    // printf("file len:%d %d\n", frame_len, file_len);
                                    ESP_LOGI("Received:", "%.2f%% ,%d/%.2f", ((float)file_len / mFileSize) * 100.00, file_len, mFileSize);
                                    if (update_file != NULL)
                                    {
                                        // printf("time now8 %lld--------------------------------------------############ \n", esp_timer_get_time());
                                        nwrite = fwrite(search_box, 1, frame_len, update_file);
                                        // printf("time now9 %lld--------------------------------------------############ \n", esp_timer_get_time());
                                        if (nwrite != frame_len)
                                        {
                                            perror("write file recfile");
                                            printf("[ err ] write file bytes: %d/%d\n", nwrite, frame_len);
                                            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "write file content error, 505");
                                        }
                                    }
                                    /** 重新开始接收下一帧:
                                     * @记住上一帧的尾部， 下次匹配要用
                                     */
                                    p1 = frame + frame_len - last_frame_tail_len;
                                    memcpy(last_frame_tail, p1, last_frame_tail_len);
                                    // printf("time now5 %lld--------------------------------------------############ \n", esp_timer_get_time());
                                    frame_len = 0;
                                    continue;
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
                goto RESPONSE_PROCESSING;
            }
        }
    }

    /** 非文件CGI*/
    data = malloc(req->content_len + 1);
    decode_data = malloc(req->content_len + 1);
    memset(decode_data, 0, req->content_len + 1);
    memset(data, 0, req->content_len + 1);
    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request cgi timeout, 404");
                goto OK;
            }
            // usleep(10000);
            continue;
        }

        memcpy(data + data_len, buf, ret);
        data_len += ret;
        remaining -= ret;
    }
    ESP_LOGE(TAG, "===== RECEIVED POST BEGIN =====");
    printf("%.*s\n", data_len, data);
    ESP_LOGE(TAG, "===== RECEIVED POST END =======");

#ifdef ENCODE
    /** body解密*/
    Setting_Para setting = {0};
    general_query(NVS_ATE, &setting);

    char key[50] = {0};
    strncpy(key, setting.psn, sizeof(key));

    time_t t = time(NULL);
    struct tm currtime;
    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;
    currtime.tm_mon += 1;
    snprintf(key, sizeof(key), "%s%02d%02d", setting.psn, currtime.tm_mon, currtime.tm_mday);

    printf("Key=%s\n", key);
    G
        printf("Raw Len=%d\n", data_len);

    Custom_AES128_ECB_decrypt(data, data_len, decode_data, key);
    for (i = 0; i < data_len; i++)
    {
        printf("Raw body byte[%d]: %02X\n", i, *(uint8_t *)(&data[i]));
    }
    printf("Raw body: %.*s\n", (int)data_len, data);
    printf("Decode body: %.*s\n", (int)decode_len, decode_data);

    /** 传参，以待下一步处理*/
    req_para.body = decode_data;
    req_para.body_len = decode_len;
#else
    /** 传参，以待下一步处理*/
    req_para.body = data;
    req_para.body_len = data_len;
#endif

    /** 自定义响应，根据body内容 */
RESPONSE_PROCESSING:
    req_num = sizeof(http_post_tab) / sizeof(http_req_tab_t);

    // ASW_LOGW("*************** tgl debug A: %d", req_num);

    // printf("\n======= tgl update inv debug B ============\n");

    for (i = 0; i < req_num; i++)
    {
        // ASW_LOGW("*************** tgl debug B: %s,%s", req->uri, http_post_tab[i].cgi);
        if (strstr(req->uri, http_post_tab[i].cgi) != NULL)
        {
            // ASW_LOGW("*************** tgl debug C: %d", i);
            break;
        }

        if (i == req_num - 1)
        {
            printf("[ err ] No this http request: %s\n", req->uri);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "request not found, 404");
            goto OK;
        }
    }
    /** 响应头中加入自定义键值对 */
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    (*http_post_tab[i].hook)(req, &req_para);

OK:
    if (update_file != NULL)
        fclose(update_file);
    if (data != NULL)
        free(data);
    free(decode_data);
    free(content_type);
    free(boundary);
    free(finish_boundary);
    free(frame);
    free(last_frame_tail);
    free(search_box);
    free(update_info);
    /** 全局指针，释放后必须赋手动为NULL*/
    update_info = NULL;

    printf("mem left: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("post handle finished\n");
    return ESP_OK;
}
//---------------------------------//
const httpd_uri_t asw_get_callback = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = asw_get_handler,
    .user_ctx = NULL};
//------------------------------------//
const httpd_uri_t asw_post_callback = {
    .uri = "/*",
    .method = HTTP_POST,
    .handler = asw_post_handler,
    .user_ctx = NULL};

//-------------------------------------------------//
//*************************************************//
#if !TRIPHASE_ARM_SUPPORT
int send_cgi_msg(int type, char *buff, int lenthg, char *ws)
{
    cloud_inv_msg buf = {0};
    // int ret;
    ASW_LOGI("send cgi msg--------------------%d %d %d \n", type, buff[0], lenthg);

    if (lenthg)
        memcpy(buf.data, buff, lenthg);
    if (ws)
    {
        memcpy(&buf.data[lenthg], ws, strlen((char *)ws));
        printf("send cgi msg %d %d \n", buf.data[0], lenthg);
    }
    buf.type = type;
    buf.len = lenthg;

    printf("send cgi msg %d %d \n", buf.data[0], lenthg);

    if (to_inv_fdbg_mq != NULL)
    {
        xQueueSend(to_inv_fdbg_mq, (void *)&buf, (TickType_t)10); // 10 / portTICK_RATE_MS
        // printf("\n===============inv update debug C ============\n");
        printf("send cgi msg ok \n");
        return 0;
    }
    return -1;
}

#else
// int send_cgi_msg(int type, char *buff, int lenthg, char *ws)

int send_cgi_msg(int type, int sla_id, char *buff, int lenthg, char *ws)
{
    cloud_inv_msg buf = {0};
    // int ret;
    ASW_LOGI("send cgi msg--------------------%d %d %d \n", type, buff[0], lenthg);

    if (lenthg)
        memcpy(buf.data, buff, lenthg);
    if (ws)
    {
        memcpy(&buf.data[lenthg], ws, strlen((char *)ws));
        printf("send cgi msg %d %d \n", buf.data[0], lenthg);
    }
    buf.type = type;
    buf.len = lenthg;
    buf.slave_id = sla_id & 0xFF;

    printf("send cgi msg %d %d \n", buf.data[0], lenthg);

    if (to_inv_fdbg_mq != NULL)
    {
        xQueueSend(to_inv_fdbg_mq, (void *)&buf, (TickType_t)10); // 10 / portTICK_RATE_MS
        // printf("\n===============inv update debug C ============\n");
        printf("send cgi msg ok \n");
        return 0;
    }
    return -1;
}

#endif