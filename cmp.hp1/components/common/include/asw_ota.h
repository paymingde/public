/**
 * @file asw_ota.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-04-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _ASW_OTA_H
#define _ASW_OTA_H

#include "Asw_global.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"



void asw_ota_start(void *pvParameter);

#endif