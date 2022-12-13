/**
 * @file asw_modbus.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-08
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ASW_MODBUS_H
#define _ASW_MODBUS_H

#include "Asw_global.h"
#include "data_process.h"
#include "inv_com.h"
#include "utility.h"
#include "meter.h"
#include "datadecode.h"
// #include "estore_com.h"
#include "asw_mqueue.h"
//============================//
#define DATA_REG_NUM 88
#define INFO_REG_NUM 75
#define TIME_REG_NUM 6
extern Inv_arr_t inv_dev_info;

typedef enum tagInverter_cmd
{
    CMD_CLR_DATA = 0,          ///< Clear data command.
    CMD_ACTIVE_PWR = 1,        ///< Active power limitation command.
    CMD_INACTIVE_PWR = 2,      ///< Inactive setpoint command.
    CMD_READ_INV_DATA = 3,     ///< Read inv data command.
    CMD_READ_SAFETY_TYPE = 4,  ///< Get inv Safety.
    CMD_WRITE_SAFETY_TYPE = 5, ///< Set inv Safety.
    CMD_READ_SAFETY_DATA = 6,  ///< Get inv Safety.
    CMD_WRITE_SAFETY_DATA = 7, ///< Set inv Safety.
    CMD_READ_ADV_PARA = 8,     ///< Get inv adv para.
    CMD_WRITE_ADV_PARA = 9,    ///< Set inv adv para.
    CMD_WRITE_START_FRAME = 10,
    CMD_WRITE_DATA_FRAME = 11,
    CMD_READ_FRAME_RES = 12,
    CMD_WRITE_IP = 13,            ///< Set ethernet ip to inv.
    CMD_WRITE_CLD_STATE = 14,     ///< Set Zevercloud state to inv.
    CMD_INV_HD_DEBUG = 15,        ///< read the debug information of the inverter
    CMD_INV_FD_DEBUG = 16,        ///< read the debug information of the inverter
    CMD_INV_ONOFF_CTRL = 17,      ///< control inverter power on or off
    CMD_WRITE_FUN_MODE = 18,      ///< control inverter function enable flag
    CMD_WRITE_DRMS_PWR = 19,      ///< Set inverter DRMS power cotrol
    CMD_WRITE_FREQ_CURE = 20,     ///< Set inverter over freq cure
    CMD_WRITE_VOLT_CURE = 21,     ///< Set inverter over freq cure
    CMD_INV_LOAD_SPEED = 22,      ///< Set inverter over freq load speed
    CMD_TRANSPARENT_CLD = 23,     ///< transparent zevercloud data and return the result
    CMD_TRANSPARENT_DBG_CLD = 24, ///< transparent zevercloud data and return the result
    CMD_READ_FREQ_CURE = 25,      ///< Get inverter over freq cure
    CMD_MD_READ_INV_INFO = 26,    // modbus protocol read the inverter information
    CMD_MD_METER_DATA = 27,
    CMD_MD_READ_INV_TIME = 28,  // read time from inverter
    CMD_MD_WRITE_INV_TIME = 29, // write time to inverter
    CMD_SET_INV_BAUDRATE = 30,
} Inv_cmd;
//=================================//

int8_t md_read_inv_time(Inverter *inv_ptr);
int8_t md_write_inv_time(Inverter *inv_ptr);

int8_t md_write_data(Inverter *inv_ptr, Inv_cmd cmd);
int8_t md_read_data(Inverter *inv_ptr, Inv_cmd cmd, unsigned int *time, uint16_t *error);

void save_inv_data(Inv_data p_data, unsigned int *time, uint16_t *error_code);

int8_t modbus_write_inv(uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint16_t *data);
int8_t modbus_holding_read_inv(uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint16_t *data);

int md_query_inv_data(Inverter *inv_ptr, unsigned int *time, uint16_t *error_code);
int8_t modbus_write_inv_bit8(uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint8_t *data);
int8_t md_query_inv_info(Inverter *inv_ptr, unsigned int *inv_index);

int8_t recv_bytes_frame_waitting(int fd, uint8_t *res_buf, uint16_t *res_len);
int8_t asw_read_registers(mb_req_t *mb_req, mb_res_t *mb_res, uint8_t funcode);

int asw_md_read_inv_reg(uint8_t modbus_id, uint16_t reg_adr, uint16_t len, uint8_t *data);

#if TRIPHASE_ARM_SUPPORT
int md_query_arm_info(Inverter *inv_ptr,unsigned int *inv_index);
#endif

#endif