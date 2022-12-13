/**
 * @file asw_protocol_ate.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-27
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ASW_PROTOCOL_ATE_H
#define _ASW_PROTOCOL_ATE_H
#include "Asw_global.h"

#include "cJSON.h"

//----------------------------------/
char *protocol_ate_paraget_handler(void); // 1

char *protocol_ate_ifconfig_handler(void); // 2

void protocol_ate_reboot_handler(void); // 3

char *protocol_ate_staget_handler(void); // 4

int8_t protocol_ate_staset_handler(char *body); // 5

char *protocol_ate_apget_handler(void); // 6

int8_t protocol_ate_apset_handler(char *body); // 7

int8_t protocol_ate_paraset_handler(char *body); // 8

char *protocol_ate_bluget_handler(void); // 9

void protocol_ate_nvs_clear_handler(void); // 10

int8_t protocol_ate_update_handler(char *body); // 11

int8_t protocol_ate_network_handler(void); // 12

int8_t protocol_ate_elinkset_handler(char *body); // 13

char *protocol_ate_lan_get_handler(void); // get 11 lan.cgi

#endif