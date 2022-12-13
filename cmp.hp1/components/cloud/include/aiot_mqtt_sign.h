/**
 * @file aiot_mqtt_sign.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-04-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _AIOT_MQTT_SIGN_H
#define _AIOT_MQTT_SIGN_H
#include "Asw_global.h"


#define IOTX_SDK_VERSION "3.0.1"
#define IOTX_ALINK_VERSION "20"
#define IOTX_FIRMWARE_VERSION_LEN (32)

#define IOTX_PRODUCT_KEY_LEN (20)
#define IOTX_DEVICE_NAME_LEN (32)
#define IOTX_DEVICE_SECRET_LEN (64)

#define IOTX_DEVICE_ID_LEN (64)
#define IOTX_PRODUCT_SECRET_LEN (64)
#define IOTX_PARTNER_ID_LEN (64)
#define IOTX_MODULE_ID_LEN (64)
#define IOTX_NETWORK_IF_LEN (160)
#define IOTX_FIRMWARE_VER_LEN (32)
#define IOTX_URI_MAX_LEN (135)
#define IOTX_DOMAIN_MAX_LEN (64)
#define IOTX_CUSTOMIZE_INFO_LEN (80)

//------------------------------------//
int aiotMqttSign(const char *productKey, const char *deviceName, const char *deviceSecret,
                 char clientId[150], char username[64], char password[65]);


#endif