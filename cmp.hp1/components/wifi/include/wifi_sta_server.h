/**
 * @file wifi_sta_server.h
 * @author tgl (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-03-29
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef WIFI_STA_SERVER_H
#define WIFI_STA_SERVER_H

#include "Asw_global.h"
// #include "asw_http_hook.h"

#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_eth.h>
#include "protocol_examples_common.h"

#include "cJSON.h"

#include "asw_mqueue.h"
// #include "common.h"
#include "asw_modbus.h"
#include "meter.h"
#include "cJSON_Utils.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
// #include "asw_ap_prov.h"
#include "save_inv_data.h"
#include "asw_mqtt.h"
#include "update.h"
#include "native_ota.h"
#include "sha256.h"

// extern char sta_save_ssid[32];

// extern  char my_sta_ip[16];
// extern  char my_sta_gw[16];
// extern  char my_sta_mk[16];

//======[tgl lan]
extern char my_eth_ip[16];
extern char my_eth_gw[16];
extern char my_eth_mk[16];
extern char my_eth_dns[16];

extern char my_sta_ip[16];
extern char my_sta_gw[16];
extern char my_sta_mk[16];
extern char my_sta_dns[16];

void get_stassid(char *buff);
void get_hostcomm(char *buff, char *puff);

int get_rssi(void);

int get_all_log(char *buf);
int get_eth_connect_status(void);
int get_eth_ip(char *ip);

int get_sta_ip(char *ip); // v2.0.0 add

// void set_external_reboot(void);

char *get_ssid(void);
void wifi_sta_stop();

//[tgl lan]add
void asw_net_start(void);
void asw_net_stop(void);

void stop_http_server();

void net_http_server_start(void *pvParameters);

//===  add wifi-sta mode v2.0.0 ===//
#define WIFI_STA_MODE_BIT BIT0
#define WIFI_AP_MODE_BIT BIT1
#define ETH_MODE_BIT BIT2

int get_wifi_sta_connect_status(void);
void scan_ap_once(void);

void wifi_sta_stop();

void asw_wifi_manager(enmWiFiJob mCmd);

extern bool g_flag_scan_done;
extern cJSON *scan_ap_res;
extern char sta_save_ssid[32];
//================================//
int find_now_apname(void);
int check_wifi_reconnect(void);

///////////////////////////////////
int get_eth_mac(char *mac);
int get_sta_mac(char *mac);
// int config_net_static_set();
int config_net_static_set(esp_netif_t *netif);
int config_net_static_diable_set();

/////////////////////////////////
#endif