/**
 * @file meter.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-03-30
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef METER_H
#define METER_H
#include "meter_md_process.h"

typedef struct tagMD_RTU_WR_MTP_REQ_Packet
{
    uint16_t reg_addr;
    uint16_t reg_num;
    uint8_t len;
    uint8_t data[256];
} TMD_RTU_WR_MTP_REQ_Packet;

// typedef struct _meter_data
// {
//     unsigned int type;
//     Inv_data data;
// } meter_cloud_data;

//-------------------------------//

SERIAL_STATE clear_meter_setting(char msg);
// SERIAL_STATE query_meter_proc(void);
SERIAL_STATE query_meter_proc(int is_for_cloud);

void get_meter_info(char mod, char *meterbran, char *metername);
#if TRIPHASE_ARM_SUPPORT
SERIAL_STATE energy_meter_control_combox();

#endif

#endif