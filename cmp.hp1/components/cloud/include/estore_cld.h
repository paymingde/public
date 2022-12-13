/**
 * @file estore_cld.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-09-08
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _ESTORE_CLD_H
#define _ESTORE_CLD_H

#include "Asw_global.h"
#include "cJSON.h"
#include "aiot_mqtt_sign.h"
#include "data_process.h"
#include "asw_mqueue.h"
#include "meter.h"


int get_estore_invinfo_payload(InvRegister devicedata, char *json_str);
int get_estore_meterinfo_payload(char *json_str);
void get_meter_mfr_and_model(char *mfr, char *model);
/** 储能机发布数据生成*/
int get_estore_invdata_payload(Inv_data *inv, char *json_str);

int write_battery_configuration(cJSON *setbattery);

int write_meter_configuration(cJSON *value);
int write_battery_define(cJSON *value);
// int write_battery_power(cJSON *value);
int read_battery_define(int day_idx, char *buffer);
// int read_battery_define(int mIndex, int day_idx, char *buffer);

int is_cld_has_estore(void);
int parse_estore_mqtt_msg_rrpc(char *rpc_topic, int rpc_len, void *payload, int data_len);
int get_cld_mach_type(void);


/////////// Lanstick-MultilInv ///////////
int asw_get_index_byPsn(char *psn);

int asw_write_inv_onoff(cJSON  *json); //多机setpower 
#endif