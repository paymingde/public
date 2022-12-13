/**
 * @file update.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-03-29
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _UPDATE_H
#define _UPDATE_H

#include "Asw_global.h"
#include "data_process.h"
#include "inv_com.h"
#include "http_client.h"
#include "asw_ota.h"
#include "asw_mqtt.h"
//----------------------------//
typedef struct
{
    char file_name[128];
    char file_path[128];
    char part_num[50];
    int file_type;
    int file_len;
} Updata_Info_St; // update_info;  //Updata_Info_St [tgl mark] change for erro with update_info

typedef enum tagRst_code
{
    RST_YES = 0, //!< Success

    ERR_TIMEOUT_S = -3,    //!< Operation timed out// ERR_TIMEOUT has been defined in err.h line 69
    ERR_BAD_DATA = -4,     //!< Data integrity check failed
    ERR_PROTOCOL = -5,     //!< Protocol error
    ERR_NO_MEMORY = -7,    //!< Insufficient memory
    ERR_NO_DATA = -8,      //!< Insufficient memory
    ERR_INVALID_ARG = -9,  //!< Invalid argument
    ERR_BAD_ADDRESS = -10, //!< Bad address
    ERR_BUSY = -11,        //!< Resource is busy
    ERR_BAD_FORMAT = -12,  //!< Data format not recognized
    ERR_NO_TIMER = -13,    //!< No timer available

    ERR_FILE_NAME = -13,  //!< Error file name
    ERR_FILE_SIZE = -14,  //!< Error file size
    ERR_FILE_CHECK = -15, //!< Error file crc
    ERR_HW_CHECK = -16,   //!< File content HW check error
    ERR_SW_CHECK = -17,   //!< File content SW check error
    ERR_FILE_RULE = -18,  //!< File upate rule error

    ERR_ILLEGAL_FUNC = -20,
    ERR_INVALID_ADDR = -21,
    ERR_INVALID_VALUE = -22,
    ERR_DEVICE_BUSY = -23,
    ERR_PARITY_ERROR = -24,

    ERR_RTCS_INIT = -25,
    ERR_CREATE_SOCKET = -26,
    ERR_TCP_CONNCET = -27,
    ERR_TCP_SEND = -28,
    ERR_TCP_BIND = -29,
    ERR_LOGIN_SLD = -30,
    ERR_DNS_SERVER = -31,
    ERR_CN_ROUTER = -32,

    ERR_WIFI_SEND = -35,
    ERR_WIFI_HEAD = -36,
    ERR_WIFI_DATA = -37,
    ERR_WIFI_LENGTH = -38,

    ERR_CREATE_WEB_SERVER_TASK = -40,
    ERR_CREATE_TCP_SERVER_TASK = -41,
    ERR_CREATE_TCP_CLIENT_TASK = -42,
    ERR_CREATE_UDP_SERVER_TASK = -43,
    ERR_CREATE_UDP_CLIENT_TASK = -44,
    ERR_CREATE_FTP_SERVER_TASK = -45,
    ERR_CREATE_FTP_CLIENT_TASK = -46,

    ERR_NET_LINK = -50,
    EER_NET_INIT = -51,

    ERR_SN_EMPTY = -52,
    ERR_SN_INVALID = -53,
    ERR_SN_SPACE = -54,
    ERR_SN_LEN = -55,

    ERR_NO_UPDATE = -59,
    ERR_NO_SUPPORT = -60,

    ERR_CRC = -61,
    ERR_DEV_ADDR = -62,
    ERR_FUN_CODE = -63,
    ERR_REG_ADDR = -64,
    ERR_DATA = -65,
} rst_code;

typedef struct
{
    char down_url[256];
    char update_type;
} update_url;

extern int update_inverter;

//------------------------------//

#if !TRIPHASE_ARM_SUPPORT
void inv_update(char *inv_fw_path);
#else
void inv_update(uint8_t modbus_id, char *inv_fw_path);
#endif
void download_task(void *pvParameters);

void update_firmware(char *url);

// void update_firmware(char *url);

#endif