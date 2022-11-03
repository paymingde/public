/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "wifi_sta_server.h"
#include "esp_http_server.h"

#include <esp_wifi.h>

#include <esp_netif.h>
#include <nvs.h>
#include <netdb.h>
#include "driver/gpio.h"

#include "esp_eth.h"
#include "inv_com.h"
#include "asw_job_http.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_net_event_group;

int8_t g_state_ethernet_connect = -1; //-1 初始状态; 0: 连接断开 ; 1: 连接成功 ; 2: 获取ip地址

#define WIFI_CONNECTED_BIT BIT0
#define ETH_CONNECTED_BIT BIT1

#define EXAMPLE_ESP_MAXIMUM_RETRY 5

#define CONFIG_APSTA_AP_SSID "AISWEI-8888"
#define CONFIG_APSTA_AP_PASSWORD "12345678"

#define CONFIG_APSTA_STA_SSID "11111111"          // 32
#define CONFIG_APSTA_STA_PASSWORD "2222222222222" // 32

//======== V2.0.0 add ===============//
#define DEFAULT_SCAN_LIST_SIZE 25

char wifiinfo[1024] = {0};
cJSON *scan_ap_res = NULL;
uint8_t s_ap_channel = 0;
static int s_retry_num = 0;
bool g_wifi_sta_stop_run = false;

EXT_RAM_ATTR char sta_save_ssid[32] = {0}; //记录当前连接路由器的名称

EXT_RAM_ATTR char my_sta_ip[16] = {0};
EXT_RAM_ATTR char my_sta_gw[16] = {0};
EXT_RAM_ATTR char my_sta_mk[16] = {0};
EXT_RAM_ATTR char my_sta_dns[16] = {0};
EXT_RAM_ATTR uint8_t my_sta_mac[6] = {0}; // ++

esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;
// esp_event_handler_instance_t instance_sc_event;

EventGroupHandle_t s_net_mode_group;

/***********************************/

//====== lan ethernet ============//
EXT_RAM_ATTR char my_eth_ip[16] = {0};
EXT_RAM_ATTR char my_eth_gw[16] = {0};
EXT_RAM_ATTR char my_eth_mk[16] = {0};
EXT_RAM_ATTR char my_eth_dns[16] = {0};

EXT_RAM_ATTR uint8_t my_eth_mac[6] = {0}; //++

static const char *TAG = "wifi_sta_server";

int g_scan_stop = 0;
wifi_sta_para_t hostcomm = {0};

static esp_netif_t *s_asw_esp_netif = NULL;
static esp_netif_t *netif_wifiAP = NULL;
static esp_netif_t *netif_wifiSTA = NULL;

esp_event_handler_instance_t instance_ip_event_any_id;
esp_event_handler_instance_t instance_wifi_event_any_id;
esp_event_handler_instance_t instance_eth_event_any_id;

//==============================================//
// esp_netif_t *get_asw_netif_from_desc(const char *desc);

/*   http_server */
static httpd_handle_t s_http_server = NULL;

static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_mac_t *s_mac = NULL;
static esp_eth_phy_t *s_phy = NULL;
static esp_eth_netif_glue_handle_t s_eth_glue = NULL;

//==============================================//

static wifi_scan_config_t scanres = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = 1};

bool g_flag_scan_done = false;
//----------------------------------
int get_rssi(void)
{
    wifi_ap_record_t ap_info = {0};
    esp_wifi_sta_get_ap_info(&ap_info);
    return ap_info.rssi;
}
//-------------------------------------
char *get_ssid(void)
{
    char *ssid = calloc(100, 1);
    wifi_ap_record_t ap_info = {0};
    esp_wifi_sta_get_ap_info(&ap_info);
    memcpy(ssid, ap_info.ssid, strlen((char *)ap_info.ssid));
    return ssid;
}

// //-------------------------
// esp_netif_t *get_asw_netif_from_desc(const char *desc)
// {
//     esp_netif_t *netif = NULL;
//     char *expected_desc;
//     asprintf(&expected_desc, "%s: %s", TAG, desc);
//     while ((netif = esp_netif_next(netif)) != NULL)
//     {
//         if (strcmp(esp_netif_get_desc(netif), expected_desc) == 0)
//         {
//             free(expected_desc);
//             return netif;
//         }
//     }
//     free(expected_desc);
//     return netif;
// }

//-----------------
// add wifi-sta mode v2.0.0
//---------------------------------------------//
int get_wifi_sta_connect_status(void)
{
    EventBits_t var;
    if (s_net_event_group == NULL)
        return -1;
    var = xEventGroupGetBits(s_net_event_group);
    if (var & WIFI_CONNECTED_BIT) //
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
//-----------------
// add wifi-sta mode v2.0.0
//--------------------------------------//
void wifi_sta_stop()
{
    EventBits_t var = xEventGroupGetBits(s_net_mode_group);
    if ((var & WIFI_STA_MODE_BIT) == 0)
    {
        ESP_LOGW(TAG, "wifi_sta stop is runned,nothing will to do");
        return;
    }

    ESP_LOGW(TAG, "WIFI Sta will Stop....");
    // esp_netif_t *wifi_netif = get_asw_netif_from_desc("sta");
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        ESP_LOGE(TAG, "esp_wifi_stop ERRO");
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());

    if (netif_wifiSTA != NULL)
    {
        ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(netif_wifiSTA));
        esp_netif_destroy(netif_wifiSTA);
        netif_wifiSTA = NULL;
    }

    // s_asw_esp_netif = NULL;
    xEventGroupClearBits(s_net_mode_group, WIFI_STA_MODE_BIT);
}

//---------------------------------------------//
int get_eth_connect_status(void)
{
    EventBits_t var;
    if (s_net_event_group == NULL)
        return -1;
    var = xEventGroupGetBits(s_net_event_group);
    if (var & ETH_CONNECTED_BIT) // [tgl lan]add ---ETH_CONNECTED_BIT
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

//---------------------------------------------//
int get_eth_ip(char *ip)
{
    if (get_eth_connect_status() == 0)
    {
        memcpy(ip, my_eth_ip, strlen(my_eth_ip));
        return 0;
    }
    else
    {
        return -1;
    }
}
//----------------------------------------------//
int get_sta_ip(char *ip)
{
    if (get_wifi_sta_connect_status() == 0)
    {
        memcpy(ip, my_sta_ip, strlen(my_sta_ip));
        return 0;
    }
    else
    {
        return -1;
    }
}
//------------------------------------------------//
int get_sta_mac(char *mac)
{
    if (get_wifi_sta_connect_status() == 0 && g_stick_run_mode == Work_Mode_STA)
    {
        char pMac[18];
        sprintf(pMac, "%02X:%02x:%02X:%02X:%02X:%02X", my_sta_mac[0], my_sta_mac[1],
                my_sta_mac[2], my_sta_mac[3], my_sta_mac[4], my_sta_mac[5]);
        memcpy(mac, pMac, strlen(pMac) + 1);

        return 0;
    }
    else
    {
        return -1;
    }
}
//-------------------------------------------//
int get_eth_mac(char *mac)
{
    if (get_eth_connect_status() == 0 && g_stick_run_mode == Work_Mode_LAN)
    {
        char pMac[18];
        sprintf(pMac, "%02X:%02x:%02X:%02X:%02X:%02X", my_eth_mac[0], my_eth_mac[1],
                my_eth_mac[2], my_eth_mac[3], my_eth_mac[4], my_eth_mac[5]);
        memcpy(mac, pMac, strlen(pMac) + 1);

        return 0;
    }
    else
    {
        return -1;
    }
}
//--------------------------------------------//

//-------------------------------------//
int parse_wifi_signal(int sig)
{
    if (sig > -50)
        return 5;
    else if (sig > -60)
        return 3;
    else if (sig > -70)
        return 1;
    else
        return 0;
}

//---------------------------------------//

static void display_scan_result(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE] = {0};
    uint16_t ap_count = 0;
    int index = 0;
    char *pCssid; // tgl add for ssid unit8 to char
    cJSON *ap_arr = NULL;
    cJSON *item = NULL;

    scan_ap_res = cJSON_CreateObject();

    memset(ap_info, 0, sizeof(ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (ap_count > DEFAULT_SCAN_LIST_SIZE)
    {
        ap_count = DEFAULT_SCAN_LIST_SIZE;
    }
    // ESP_LOGI(TAG, "Total APS scanned----------->= %u", ap_count);
    add_int_to_json(scan_ap_res, "num", ap_count);
    ap_arr = cJSON_AddArrayToObject(scan_ap_res, "wif");
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {

        pCssid = (char *)ap_info[i].ssid;

        item = cJSON_CreateObject();
        if (item != NULL)
        {
            if (strlen(pCssid))
            {
                add_str_to_json(item, "sid", pCssid);
                add_int_to_json(item, "srh", parse_wifi_signal(ap_info[i].rssi));
                add_int_to_json(item, "channel", ap_info[i].primary);
                cJSON_AddItemToArray(ap_arr, item);
            }
        }

        if (index + strlen(pCssid) < 1024)
        {
            // ESP_LOGI(TAG, "wifiinfo******* %s %d\n", wifiinfo, index);
            wifiinfo[index++] = '<';
            wifiinfo[index++] = 'p';
            wifiinfo[index++] = '>';
            memcpy(&wifiinfo[index], ap_info[i].ssid, strlen(pCssid));
            wifiinfo[index + strlen(pCssid)] = '<';
            wifiinfo[index + strlen(pCssid) + 1] = '/';
            wifiinfo[index + strlen(pCssid) + 2] = 'p';
            wifiinfo[index + strlen(pCssid) + 3] = '>';
            index += strlen(pCssid) + 4;
            // ESP_LOGI(TAG, "httpres \t\t%s %s %d %d", ap_info[i].ssid, wifiinfo, strlen(ap_info[i].ssid), index);
        }
    }
    // ESP_LOGI(TAG, "wifi info  %s   %d", wifiinfo, index);
}

//-------------------------------------//
void scan_ap_once(void)
{
    esp_wifi_scan_start(&scanres, true);

    if (g_flag_scan_done == true)
    {
        display_scan_result();
        g_flag_scan_done = false;
    }
}

//----------------------------------------//
int check_wifi_reconnect(void)
{
    if (get_wifi_sta_connect_status() < 0)
    {
        esp_wifi_connect();
    }
    return 0;
}

//-------------------------------------------//

int find_now_apname(void)
{

    wifi_sta_para_t sta_para = {0};

    general_query(NVS_STA_PARA, &sta_para);

    if (strlen((char *)sta_para.ssid) < 1)
        return -2;

    scan_ap_once();

    char *p = wifiinfo;

    cJSON_Delete(scan_ap_res);

    if (strstr(p, (const char *)sta_para.ssid) != NULL)
    {
        printf("\n found router %s \n", sta_save_ssid);
        return 0;
    }
    printf("\n cannot found router %s \n", sta_save_ssid);
    return -1;
}

////////////////////////  CONFIG STATIC IP INFO ////////////////
static esp_err_t asw_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE))
    {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}
//------------------------------------------------//
static void set_static_ip(esp_netif_t *netif, char *cip, char *cgtw, char *cmsk, char *dns)
{
    esp_netif_dhcp_status_t m_status;

    // esp_netif_dhcps_get_status(netif, &m_status);

    // printf("\n-----------------------   AAAAAAAAAAAAAAAAAAAAAAAAa  -[%s]--------------\n", cip);
    if (esp_netif_dhcpc_stop(netif) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
    }

    esp_netif_ip_info_t ip;
    memset(&ip, 0, sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(cip);
    ip.netmask.addr = ipaddr_addr(cmsk);
    ip.gw.addr = ipaddr_addr(cgtw);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }

    ESP_ERROR_CHECK(asw_set_dns_server(netif, ipaddr_addr(dns), ESP_NETIF_DNS_MAIN));
    ESP_ERROR_CHECK(asw_set_dns_server(netif, ipaddr_addr(cgtw), ESP_NETIF_DNS_BACKUP));

    ESP_LOGI(TAG, "Success to set static ip: %s, netmask: %s, gw: %s,dns:%s", cip, cmsk, cgtw, dns);
}
//----------------------------------------------------//
static void set_disable_static_ip(esp_netif_t *netif)
{
    if (esp_netif_dhcpc_start(netif) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start dhcp client");
    }
    return;
}
///////////////////////////////////////////////////////////////////
int config_net_static_set_with_config(esp_netif_t *netif, uint8_t stickRunMode)
{
    net_static_info_t st_net_lanInfo = {0};
    esp_err_t res_ap_get;
    char staticIpSet[16] = {0};
    char staticMskSet[16] = {0};
    char staticGtwSet[16] = {0};
    char staticDnsSet[16] = {0};
    /////////////// ESP_LOGW(TAG, "DHCP Ip COnfig....")///////////////
    // tcpip_adapter_ip_info_t local_ip;

    if (general_query(NVS_NET_STATIC_INFO, &st_net_lanInfo) == ASW_FAIL)
    {
        ESP_LOGW("-- config static ip WARN --", " noting to be done with query erro ");

        return ASW_FAIL;
    }
    printf("\n-----------config_net_static_set------\n");
    printf("\n--- set work mode:%d, stick run mode:%d----\n", st_net_lanInfo.work_mode, g_stick_run_mode);
    assert(netif != NULL);
    ///////////////////////////////////////////////////////////////

    if (esp_netif_dhcpc_stop(netif) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
    }

    if (st_net_lanInfo.enble && st_net_lanInfo.work_mode == stickRunMode)
    {
        /*write */
        sprintf(staticIpSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.ip[0], st_net_lanInfo.ip[1],
                st_net_lanInfo.ip[2], st_net_lanInfo.ip[3]);

        sprintf(staticMskSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.mask[0], st_net_lanInfo.mask[1],
                st_net_lanInfo.mask[2], st_net_lanInfo.mask[3]);

        sprintf(staticGtwSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.gateway[0], st_net_lanInfo.gateway[1],
                st_net_lanInfo.gateway[2], st_net_lanInfo.gateway[3]);

        sprintf(staticDnsSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.maindns[0], st_net_lanInfo.maindns[1],
                st_net_lanInfo.maindns[2], st_net_lanInfo.maindns[3]);

        set_static_ip(netif, staticIpSet, staticGtwSet, staticMskSet, staticDnsSet);

        printf("\n-----------config_net_static_set STA------\n");
        return ASW_OK;
    }

    else
    {
        if (esp_netif_dhcpc_start(netif) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start dhcp client");
            return ASW_FAIL;
        }

        return ASW_OK;
    }
}

/* 配置静态ip信息 */
int config_net_static_set(esp_netif_t *netif)
{
    net_static_info_t st_net_lanInfo = {0};
    esp_err_t res_ap_get;
    char staticIpSet[16] = {0};
    char staticMskSet[16] = {0};
    char staticGtwSet[16] = {0};
    char staticDnsSet[16] = {0};

    //////////// ESP_LOGW(TAG, "DHCP Ip COnfig....") /////////////////////
    // tcpip_adapter_ip_info_t local_ip;

    if (general_query(NVS_NET_STATIC_INFO, &st_net_lanInfo) == ASW_FAIL)
    {
        ESP_LOGW("-- config static ip WARN --", " noting to be done with query erro ");
        return ASW_FAIL;
    }

    printf("\n-----------config_net_static_set------\n");
    printf("\n--- set work mode:%d, stick run mode:%d----\n",
           st_net_lanInfo.work_mode, g_stick_run_mode);
    printf("\n-----------config_net_static_set------\n");

    ////////////////////////////////////////////////////////////////////

    if (st_net_lanInfo.enble)
    {
        /*write */
        sprintf(staticIpSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.ip[0], st_net_lanInfo.ip[1],
                st_net_lanInfo.ip[2], st_net_lanInfo.ip[3]);

        sprintf(staticMskSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.mask[0], st_net_lanInfo.mask[1],
                st_net_lanInfo.mask[2], st_net_lanInfo.mask[3]);

        sprintf(staticGtwSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.gateway[0], st_net_lanInfo.gateway[1],
                st_net_lanInfo.gateway[2], st_net_lanInfo.gateway[3]);

        sprintf(staticDnsSet, "%hd.%hd.%hd.%hd", st_net_lanInfo.maindns[0], st_net_lanInfo.maindns[1],
                st_net_lanInfo.maindns[2], st_net_lanInfo.maindns[3]);

        if (st_net_lanInfo.work_mode == Work_Mode_LAN && g_stick_run_mode == Work_Mode_LAN)
        {
            assert(s_asw_esp_netif != NULL);

            set_static_ip(s_asw_esp_netif, staticIpSet, staticGtwSet, staticMskSet, staticDnsSet);
            g_asw_static_ip_enable = 1;

            printf("\n-----------config_net_static_set LAN------\n");
        }
        else if (st_net_lanInfo.work_mode == Work_Mode_STA && g_stick_run_mode == Work_Mode_STA)
        {
            assert(netif_wifiSTA != NULL);
            set_static_ip(netif_wifiSTA, staticIpSet, staticGtwSet, staticMskSet, staticDnsSet);
            g_asw_static_ip_enable = 1;

            printf("\n-----------config_net_static_set STA------\n");
        }
        else
        {
            printf("\n----------DHCP IP------\n");
            if (netif != NULL)
                if (esp_netif_dhcpc_start(netif) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to start dhcp client");
                }
            g_asw_static_ip_enable = 0;
            ESP_LOGW("-- config static ip WARN --", " noting to be done with no fit mode ");
            return ASW_FAIL;
        }
    }

    return ASW_OK;
}
//---------------------------------------//

int config_net_static_diable_set()
{
    net_static_info_t st_net_lanInfo = {0};
    // esp_err_t res_ap_get;
    // tcpip_adapter_ip_info_t local_ip;

    if (general_query(NVS_NET_STATIC_INFO, &st_net_lanInfo) == ASW_FAIL)
    {
        ESP_LOGW("-- config static ip WARN --", " noting to be done with query erro ");

        return ASW_FAIL;
    }

    if (st_net_lanInfo.enble)
        return ASW_OK;

    if (st_net_lanInfo.work_mode == Work_Mode_LAN && g_stick_run_mode == Work_Mode_LAN)
    {
        assert(s_asw_esp_netif != NULL);
        set_disable_static_ip(s_asw_esp_netif);
        g_asw_static_ip_enable = 0;
    }
    else if (st_net_lanInfo.work_mode == Work_Mode_STA && g_stick_run_mode == Work_Mode_STA)
    {
        assert(netif_wifiSTA != NULL);

        set_disable_static_ip(netif_wifiSTA);
        g_asw_static_ip_enable = 0;
    }

    else
    {
        ESP_LOGW("-- config disable static ip WARN --", " noting to be done with no fit mode ");
        return ASW_FAIL;
    }

    // if (res_ap_get == ESP_OK)
    // {
    //     ESP_LOGI(TAG, "disable static get is success\n");
    // }
    // ESP_LOGI(TAG, "disable static  new_self_ip:" IPSTR "\n", IP2STR(&local_ip.ip));
    // ESP_LOGI(TAG, "disable static  new_self_netmask:" IPSTR "\n", IP2STR(&local_ip.netmask));
    // ESP_LOGI(TAG, "disable static  new_self_gw:" IPSTR "\n", IP2STR(&local_ip.gw));

    // //////////////////////////////////////

    return ASW_OK;
}
//-------------------
void asw_ip_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP)
    {
        ESP_LOGW("Got IPv4 event", "Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "ethernet-got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "ethernet-got gw  =" IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "ethernet-netmask     =" IPSTR, IP2STR(&event->ip_info.netmask));

        memset(my_eth_ip, 0, sizeof(my_eth_ip));
        strcpy(my_eth_ip, ip4addr_ntoa(&event->ip_info.ip));

        memset(my_eth_gw, 0, sizeof(my_eth_gw));
        strcpy(my_eth_gw, ip4addr_ntoa(&event->ip_info.gw));

        memset(my_eth_mk, 0, sizeof(my_eth_mk));
        strcpy(my_eth_mk, ip4addr_ntoa(&event->ip_info.netmask));

        assert(s_asw_esp_netif != NULL);

        ////////////////////  get eth DNS /////////////////

        esp_netif_dns_info_t mNetIfDns;

        ESP_ERROR_CHECK(esp_netif_get_dns_info(s_asw_esp_netif, ESP_NETIF_DNS_MAIN, &mNetIfDns));
        memset(my_eth_dns, 0, sizeof(my_eth_dns));

        // printf("\n ethernet dns get" IP2STR(&mNetIfDns.ip));

        strcpy(my_eth_dns, ip4addr_ntoa(&mNetIfDns.ip));

        ////////////////////  get eth mac /////////////////

        memset(my_sta_mac, 0, sizeof(my_eth_mac));
        // esp_netif_t *esp_netif = get_asw_netif_from_desc("eth");

        esp_netif_get_mac(s_asw_esp_netif, my_eth_mac);
        ESP_LOGI(TAG, " sta eth :");
        for (uint8_t i = 0; i < 6; i++)
            printf("%0x.", my_eth_mac[i]);
        printf("\n");

        ///////////////////////////////////////////////////

        xEventGroupSetBits(s_net_event_group, ETH_CONNECTED_BIT);

        g_state_ethernet_connect = 2;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_LOST_IP)
    {
        xEventGroupClearBits(s_net_event_group, ETH_CONNECTED_BIT);
        g_state_ethernet_connect = 0;

        ASW_LOGE("EThernet is lost!!!");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
    {
        ASW_LOGI("A state connected to the AP!!! ");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        //////////////////  FOR STATIC INFO SET ///////////////////

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got gw  =" IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "netmask     =" IPSTR, IP2STR(&event->ip_info.netmask));

        memset(my_sta_ip, 0, sizeof(my_sta_ip));
        strcpy(my_sta_ip, ip4addr_ntoa(&event->ip_info.ip));

        memset(my_sta_gw, 0, sizeof(my_sta_gw));
        strcpy(my_sta_gw, ip4addr_ntoa(&event->ip_info.gw));

        memset(my_sta_mk, 0, sizeof(my_sta_mk));
        strcpy(my_sta_mk, ip4addr_ntoa(&event->ip_info.netmask));
        assert(netif_wifiSTA != NULL);

        ////////////////////////  get dns ///////////////////

        esp_netif_dns_info_t mNetIfDns;

        ESP_ERROR_CHECK(esp_netif_get_dns_info(netif_wifiSTA, ESP_NETIF_DNS_MAIN, &mNetIfDns));
        memset(my_sta_dns, 0, sizeof(my_sta_dns));
        // printf("\n stat dns get" IP2STR(&mNetIfDns.ip));

        strcpy(my_sta_dns, ip4addr_ntoa(&mNetIfDns.ip));

        //////////////////  get mac ///////////////////////
        memset(my_sta_mac, 0, sizeof(my_sta_mac));

        // esp_netif_t *mp_esp_netif = get_asw_netif_from_desc("sta");
        esp_netif_get_mac(netif_wifiSTA, my_sta_mac);

        ESP_LOGI(TAG, " sta mac :");
        for (uint8_t i = 0; i < 6; i++)
            printf("%0x.", my_sta_mac[i]);
        printf("\n");

        //////////////////  get mac ///////////////////////

        s_retry_num = 0;
        xEventGroupSetBits(s_net_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_id == IP_EVENT_STA_LOST_IP)
    {
        xEventGroupClearBits(s_net_event_group, WIFI_CONNECTED_BIT);
        ASW_LOGE("WIFI EHT IP is lost!!!");
    }
}
//---------------------------------//
// V2.0.0 add

void asw_wifi_event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    ASW_LOGI("asw_wifi_event_handler !");
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        printf("asw_wifi_event_handler(void *arg, esp_event_base_t event_base \n");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the wifi");
        }
        else
        {
            xEventGroupClearBits(s_net_event_group, WIFI_CONNECTED_BIT);
        }
        ESP_LOGI(TAG, "connect to the wifi fail");
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE)
    {
        ASW_LOGI(" SCAN DONE !");
        g_flag_scan_done = true;
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ASW_LOGI("  WIFI_EVENT_STA_CONNECTED !");

#if STATIC_IP_SET_ENABLE
        if (g_stick_run_mode != Work_Mode_LAN)
            config_net_static_set(netif_wifiSTA);
        else
        {
            if (esp_netif_dhcpc_start(netif_wifiSTA) != ESP_OK)
            {
                ESP_LOGE(TAG, " netif_wifiSTA Failed to start dhcp client");
            }
        }
#endif
        // config_net_static_set_with_config(netif_wifiSTA, Work_Mode_STA);
    }
}

//---------------------
void asw_eth_event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ASW_LOGI("asw_eth_event_handler !");
    esp_netif_t *esp_netif = (esp_netif_t *)arg;
    if (event_id == ETHERNET_EVENT_CONNECTED)
    {
        ESP_LOGW(TAG, "ETH is connectted");
        g_state_ethernet_connect = 1;
#if STATIC_IP_SET_ENABLE
        g_stick_run_mode = Work_Mode_LAN; // tgl mark add +++ for static info set

        // config_net_static_set_with_config(s_asw_esp_netif, Work_Mode_LAN);

        //////////////////////  debug ///////////////////////

        {
            if (esp_netif_dhcpc_start(arg) != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to start dhcp client");
            }
        }
#endif
        ////////////////////////////////////////////////////

        //   asw_mqtt_client_start();
    }
    if (event_id == ETHERNET_EVENT_DISCONNECTED)
    {
        ESP_LOGW(TAG, "ETH is Disconnectted");
        g_state_ethernet_connect = 0;
        xEventGroupClearBits(s_net_event_group, ETH_CONNECTED_BIT);
        mqtt_client_destroy_free();

        // v2.0.0 change Lan 模式下，拔掉网口后，需重启或配网重启后才进入sta模式
        // if (get_wifi_sta_connect_status() != 0)
    }
}

//---------------------------------
void stop_http_server()
{
    if (s_http_server != NULL)
    {
        httpd_stop(s_http_server);
        s_http_server = NULL;

        ASW_LOGW(" HTTP Server Stop....");
    }
}

//---------------------------------------------//
void get_hostcomm(char *buff, char *puff)
{
    memcpy(buff, hostcomm.ssid, strlen((char *)hostcomm.ssid) + 1);
    memcpy(puff, hostcomm.password, strlen((char *)hostcomm.password) + 1);
}
//---------------------------------------------//
void get_stassid(char *buff)
{
    memcpy(buff, hostcomm.ssid, strlen((char *)hostcomm.ssid) + 1); // [tgl mark]
}

//------------------------------
void eth_net_stop(void)
{
    // esp_netif_t *eth_netif = get_asw_netif_from_desc("eth");
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, instance_eth_got_ip));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_LOST_IP, instance_eth_lost_ip));

    ESP_ERROR_CHECK(esp_eth_stop(s_eth_handle));
    ESP_ERROR_CHECK(esp_eth_del_netif_glue(s_eth_glue));
    ESP_ERROR_CHECK(esp_eth_driver_uninstall(s_eth_handle));
    ESP_ERROR_CHECK(s_phy->del(s_phy));
    ESP_ERROR_CHECK(s_mac->del(s_mac));

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, instance_ip_event_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_wifi_event_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, instance_eth_event_any_id));

    esp_event_handler_unregister(ETH_EVENT, ETHERNET_EVENT_CONNECTED, esp_netif_action_connected);
    esp_event_handler_unregister(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, esp_netif_action_disconnected);
    if (s_asw_esp_netif != NULL)
        esp_netif_destroy(s_asw_esp_netif);
    s_asw_esp_netif = NULL;
}

//-----------------------------------

int get_all_log(char *buf)
{
    char bufx[1024] = {0};
    char tmp[350] = {0};

    get_sys_log(tmp);

    strcat(bufx, tmp);

    memset(tmp, 0, 350);
    get_inv_log(tmp, 1);
    strcat(bufx, tmp);

    get_inv_log(tmp, 2);

    if ((strlen(bufx) + strlen(tmp)) < 1024)
        strcat(bufx, tmp);

    memset(tmp, 0, 350);
    get_inv_log(tmp, 3);

    if ((strlen(bufx) + strlen(tmp)) < 1024)
        strcat(bufx, tmp);

    char now_timex[30] = {0};
    char ipx[30] = {0};
    char buft[100] = {0};

    get_time(now_timex, sizeof(now_timex));

    /* v2.0.0 add change */
    if (g_stick_run_mode == Work_Mode_LAN)
    {
        get_eth_ip(ipx);
        sprintf(buft, "\n%s ip:%s ", now_timex, ipx);
    }
    else if (g_stick_run_mode == Work_Mode_STA)
    {
        get_sta_ip(ipx);
        sprintf(buft, "\n%s ip:%s ", now_timex, ipx);
    }
    else if (g_stick_run_mode == Work_Mode_AP_PROV)
    {
        sprintf(buft, "\n%s ip:%s ", now_timex, "160.190.0.1");
    }

    if ((strlen(bufx) + strlen(buft)) < 1024)
        strcat(bufx, buft);

    memcpy(buf, bufx, strlen(bufx));
    return 0;
}

/*************** 开启http服务器 ****************/
static void start_webserver(void)
{
    if (s_http_server != NULL)
    {
        ESP_LOGW(TAG, "http server is runing....");
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    //--------------

    //
    config.lru_purge_enable = false; // true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 4096 * 2;
    config.server_port = 8484;
    config.recv_wait_timeout = 30; /// tgl mark for W (xxxxx) httpd_txrx: httpd_sock_err: error in recv : 11” erro
    //-----------<<<
    // Start the httpd server
    ESP_LOGW(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&s_http_server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(s_http_server, &asw_get_callback);
        httpd_register_uri_handler(s_http_server, &asw_post_callback);

        return;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return;
}

//-----------------------------------//

static void eth_net_start(void)
{
    ASW_LOGW("ETH NET  start.....");
    char *desc;
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    // Prefix the interface description with the module TAG
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    asprintf(&desc, "%s: %s", TAG, esp_netif_config.if_desc);

    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 200; // Lan eth 优先级 > wifi-sta
    esp_netif_config_t netif_config = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
    s_asw_esp_netif = esp_netif_new(&netif_config);

    free(desc);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;

    mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
    s_mac = esp_eth_mac_new_esp32(&mac_config);

    s_phy = esp_eth_phy_new_ip101(&phy_config);

    // Install Ethernet driver
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(s_mac, s_phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));

    s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
    esp_netif_attach(s_asw_esp_netif, s_eth_glue);

    // Register user defined event handers

    esp_eth_start(s_eth_handle);

    xEventGroupSetBits(s_net_mode_group, ETH_MODE_BIT);

    int route_prio = esp_netif_get_route_prio(s_asw_esp_netif);
    // #if DEBUG_PRINT_ENABLE
    ASW_LOGI("====================== \n Lan ETH route prio:%d\n=====================\n", route_prio);
    // #endif
    ESP_LOGI(TAG, " ETH NET INIT Finished!!");
}

//-----------------------------------------//
static void dhcp_ip_config()
{
    ESP_LOGW(TAG, "DHCP Ip COnfig....");
    tcpip_adapter_ip_info_t local_ip;
    /*write */
    IP4_ADDR(&local_ip.ip, 160, 190, 0, 1);
    IP4_ADDR(&local_ip.gw, 160, 190, 0, 1);
    IP4_ADDR(&local_ip.netmask, 255, 255, 255, 0);
    /*stop dhcps*/
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    /*set new IP*/
    esp_err_t res_ap_set = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &local_ip);
    if (res_ap_set == ESP_OK)
    {
        ESP_LOGI(TAG, "set is success\n");
    }
    else
    {
        ESP_LOGI(TAG, "set is failed\n");
    }
    /*restart adapter dhcps*/
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);

    /*print new IP information*/
    esp_err_t res_ap_get = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &local_ip);
    if (res_ap_get == ESP_OK)
    {
        ESP_LOGI(TAG, "get is success\n");
    }
    ESP_LOGI(TAG, "new_self_ip:" IPSTR "\n", IP2STR(&local_ip.ip));
    ESP_LOGI(TAG, "new_self_netmask:" IPSTR "\n", IP2STR(&local_ip.netmask));
    ESP_LOGI(TAG, "new_self_gw:" IPSTR "\n", IP2STR(&local_ip.gw));
}

/***********************************************/

// V2.0.0 add
/**************************  wifi 调用 *********/

static void asw_wifi_init()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &asw_ip_event_handler, NULL, &instance_ip_event_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &asw_wifi_event_handler, NULL, &instance_wifi_event_any_id));

    // dhcp_ip_config(); //0721 -
}
//----------------------------------------//
void asw_wifi_stop()
{
    wifi_mode_t m_wifi_mode;
    esp_wifi_get_mode(&m_wifi_mode);
    if (m_wifi_mode == WIFI_MODE_NULL)
    {
        ESP_LOGW(TAG, "WiFi mode is NULL !! return");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}
//-----------------------------------//
static void asw_wifi_ap_mode()
{
    ESP_LOGW(TAG, " ASW WIFI AP MODE...");
    asw_wifi_stop();
    if (netif_wifiAP == NULL)
        netif_wifiAP = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = CONFIG_APSTA_AP_SSID,
            .ssid_len = strlen(CONFIG_APSTA_AP_SSID),
            .channel = s_ap_channel, // EXAMPLE_ESP_WIFI_CHANNEL,
            .password = CONFIG_APSTA_AP_PASSWORD,
            .max_connection = 4, // EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    if (strlen(CONFIG_APSTA_AP_PASSWORD) == 0)
    {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_ap_para_t ap_para = {0};
    general_query(NVS_AP_PARA, &ap_para);

    if (strlen((char *)ap_para.ssid) > 0)
    {

        memcpy(wifi_ap_config.ap.ssid, ap_para.ssid, strlen((char *)ap_para.ssid));
        memcpy(wifi_ap_config.ap.password, ap_para.password, strlen((char *)ap_para.password));
    }

    ESP_LOGE(TAG, "ap_config,ssid:%s,pawd:%s,channel:%d",
             (char *)wifi_ap_config.ap.ssid, (char *)wifi_ap_config.ap.password, wifi_ap_config.ap.channel);

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    esp_wifi_start();
}

//----------------------------------//

static void asw_wifi_sta_mode()
{
    ESP_LOGW(TAG, " ASW WIFI STA MODE...");
    asw_wifi_stop();
    if (netif_wifiSTA == NULL)
        netif_wifiSTA = esp_netif_create_default_wifi_sta();

    //------------------------
    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = CONFIG_APSTA_STA_SSID,         // DEFAULT_SSID,
            .password = CONFIG_APSTA_STA_PASSWORD, // DEFAULT_PWD,
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL
            // .threshold.rssi = DEFAULT_RSSI,
            // .threshold.authmode = DEFAULT_AUTHMODE
        }};

    wifi_sta_para_t sta_para = {0};

    general_query(NVS_STA_PARA, &sta_para);

    if (strlen((char *)sta_para.ssid) > 0) //[mark]会运行到这里吗？？？
    {
        memcpy(wifi_sta_config.sta.ssid, sta_para.ssid, sizeof(sta_para.ssid));             //[mark] strlen -> sizeof
        memcpy(wifi_sta_config.sta.password, sta_para.password, sizeof(sta_para.password)); //[mark] strlen -> sizeof
    }

    memcpy(sta_save_ssid, wifi_sta_config.sta.ssid, sizeof(wifi_sta_config.sta.ssid));
    memcpy(&hostcomm, &sta_para, sizeof(wifi_sta_para_t));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);

    ESP_LOGE(TAG, " WIFI STA,ssid:%s,password:%s \n", wifi_sta_config.sta.ssid, wifi_sta_config.sta.password);

    int route_prio = esp_netif_get_route_prio(netif_wifiSTA);

    ESP_LOGI(TAG, "\n======================= \n WIFI STA route prio:%d\n=====================\n", route_prio);
    esp_wifi_start();
}

//-------------------------------------------------//

static void asw_wifi_get_channel()
{
    ESP_LOGW(TAG, " ASW WIFI GET CHANNEL...");
    asw_wifi_stop();

    if (netif_wifiSTA == NULL)
        netif_wifiSTA = esp_netif_create_default_wifi_sta();
    uint8_t channel[14] = {0};

    uint16_t number = DEFAULT_SCAN_LIST_SIZE; // DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

    wifi_country_t mCtry = {0};
    esp_wifi_get_country(&mCtry);

    ESP_LOGW(TAG, "wifi country info,code:%s,start channnel:%d,total channel:%d ", mCtry.cc, mCtry.schan, mCtry.nchan);
    ESP_LOGW(TAG, "Task 剩余 stack = %d\n", uxTaskGetStackHighWaterMark(NULL));

    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {

        channel[ap_info[i].primary]++;
    }

    s_ap_channel = 1;
    uint8_t min = channel[1];
    for (int j = 1; j < 12; j++)
    {
        if (min > channel[j])
        {
            min = channel[j];
            s_ap_channel = j;
        }
    }
    printf("\n ======   scan result ========= \n channel:%d,num:%d", s_ap_channel, channel[s_ap_channel]);
    // printf("\n ============================== \n ");
}

void asw_wifi_apscan_mode()
{
    ESP_LOGW(TAG, " ASW WIFI AP SCAN MODE");
    asw_wifi_stop();
    if (netif_wifiAP == NULL)
        netif_wifiAP = esp_netif_create_default_wifi_ap();
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = CONFIG_APSTA_AP_SSID,
            .ssid_len = strlen(CONFIG_APSTA_AP_SSID),
            .channel = s_ap_channel, // EXAMPLE_ESP_WIFI_CHANNEL,
            .password = CONFIG_APSTA_AP_PASSWORD,
            .max_connection = 4, // EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    if (strlen(CONFIG_APSTA_AP_PASSWORD) == 0)
    {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_ap_para_t ap_para = {0};
    general_query(NVS_AP_PARA, &ap_para);

    if (strlen((char *)ap_para.ssid) > 0)
    {
        // printf("\n==========  ap scan job: ======\n ap_para.ssid:%s,ap_para.password:%s\n", (char *)ap_para.ssid, (char *)ap_para.password);

        memcpy(wifi_ap_config.ap.ssid, ap_para.ssid, sizeof(ap_para.ssid));
        memcpy(wifi_ap_config.ap.password, ap_para.password, sizeof(ap_para.password));
    }

    ESP_LOGE(TAG, "ap_config,ssid:%s,pawd:%s,channel:%d",
             (char *)wifi_ap_config.ap.ssid, (char *)wifi_ap_config.ap.password, wifi_ap_config.ap.channel);

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);

    if (netif_wifiSTA == NULL)
        netif_wifiSTA = esp_netif_create_default_wifi_sta();

    esp_wifi_set_mode(WIFI_MODE_APSTA);

    esp_wifi_start();
}

void asw_wifi_manager(enmWiFiJob mCmd)
{
    switch (mCmd)
    {
    case 0: /* scan get channel*/
        asw_wifi_get_channel();
        /* code */
        break;

    case 1: /* apscan mode*/
        asw_wifi_apscan_mode();
        break;

    case 2: /* ap  mode*/
        asw_wifi_ap_mode();
        break;

    case 3: /* sta  mode*/
        asw_wifi_sta_mode();
        break;

    default:
        break;
    }
    dhcp_ip_config();

    start_webserver();
}

void asw_net_start(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init()); // [ mark] tcpip_adapter_init（）->esp_netif_init
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &asw_eth_event_handler, NULL, &instance_eth_event_any_id));

    s_net_event_group = xEventGroupCreate();

    s_net_mode_group = xEventGroupCreate();

    asw_wifi_init();

    eth_net_start();

    usleep(500 * 1000);

    // if (get_eth_connect_status() != 0 )
    if (g_state_ethernet_connect < 1)
    {
        //启动wifi-eth
        // wifi_sta_start();

        printf("\n =====  Lan-ETH is not checked !!! ===== \n");

        switch (g_stick_run_mode)
        {
        case Work_Mode_AP_PROV:
        {
            asw_wifi_manager(WIFI_JOB_SCAN);
            sleep(1);
            asw_wifi_manager(WIFI_JOB_AP_SCAN);
        }
        break;

        case Work_Mode_STA:
        default:
            asw_wifi_manager(WIFI_JOB_STA);
            break;
        }
    }
    else // if (g_stick_run_mode != Work_Mode_LAN)
    {
        g_stick_run_mode = Work_Mode_LAN;
        printf("\n =====  Lan-ETH is checked !!! ===== \n");
        asw_wifi_manager(WIFI_JOB_SCAN);
        sleep(1);
        asw_wifi_manager(WIFI_JOB_AP_SCAN);
    }
}

void asw_net_stop(void)
{
    printf("\nTODO NEXT \n");
}

/////////////////////////////////////////////////////////////////////////////////////
/** sta_http_server_run()内含有以下初始化：
 * 1.tcp ini
 * 2.event loop start
 * 3.sta wifi connect
 * 4.sta websever start
 */
void net_http_server_start(void *pvParameters)
{

    xSemaphoreHandle *server_ready = pvParameters;
    asw_net_start();

    start_webserver();

    // ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &asw_ip_event_handler, NULL, &instance_ip_event_any_id));
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &asw_wifi_event_handler, NULL, &instance_wifi_event_any_id));
    // ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &asw_eth_event_handler, NULL, &instance_eth_event_any_id));

    int ret = esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, esp_netif_action_connected, s_asw_esp_netif);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error register  eth  connect!!");
    }

    ret = esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, esp_netif_action_disconnected, s_asw_esp_netif);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error register  eth disconnect!!");
    }

    xSemaphoreGive(*server_ready);
    ASW_LOGW("net http server start finished!!");
}
