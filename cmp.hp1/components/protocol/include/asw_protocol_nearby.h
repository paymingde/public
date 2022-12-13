/**
 * @file asw_protocol_nearby.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-25
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ASW_PROTOCOL_NEARBY_H
#define _ASW_PROTOCOL_NEARBY_H

#include "Asw_global.h"
#include "cJSON.h"


char *protocol_nearby_getdev_handler(uint16_t devecitType, uint16_t isSecret,char *psn);

char *protocol_nearby_getdevdata_handler(uint16_t devecitType, char *inv_sn);

char *protocol_nearby_wlanget_handler(uint16_t info);

int8_t protocol_nearby_setting_handler(uint16_t deviceType, char *action, cJSON *value);

int8_t protocol_nearby_wlanset_handler(char *action, cJSON *value);

char *protocol_nearby_fdbg_handler(cJSON *json);
void fresh_before_http_handler(void);


#endif
