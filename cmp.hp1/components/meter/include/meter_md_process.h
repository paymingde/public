/**
 * @file meter_md_process.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-09
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _METER_MD_PROCESS_H
#define _METER_MD_PROCESS_H
#include "Asw_global.h"
#include "data_process.h"
#include "asw_mutex.h"
#include "asw_modbus.h"
#include "inv_com.h"
#include "utility.h"

//------------------------------//
extern int total_rate_power;
extern int total_curt_power;
extern meter_data_t g_meter_inv_meter;

extern int16_t meter_real_factory;
//------------------------------//
/** Aimeter电表类型,从7开始。7以下的是之前的老电表*/
// typedef enum
// {
//     METER_IREADER_IM1281B = 7,
//     METER_ACREL_ADL200 = 100
// } aimeter_types;

/** Aimeter电表类型,从7开始。7以下的是之前的老电表*/

// Eng.Stg.Mch-lanstick 20220908
typedef enum
{
    METER_ESTRON_230 = 2,
    METER_MAIGE_ODM = 6,
    METER_IREADER_IM1281B = 7,
    METER_ACREL_ADL200 = 100
} aimeter_types;

typedef struct
{
    int e_in_total;
    int e_out_total;
    char day;
} meter_gendata;


// meter_data_t m_inv_meter = {0}; //[tgl mark]为了项目全局访问，在没有特殊的作用下，去掉了static修饰。 同事 inv_meter -> g_inv_meter

/** 电表类型（注意aimeter的电表不在此表中）*/
#if TRIPHASE_ARM_SUPPORT
extern uint16_t meter_pro[8][8];

#else
extern uint16_t meter_pro[7][8];

#endif
//==============================//
uint32_t get_u32_from_res_frame_4B_reg(int addr, uint8_t *buf);
int32_t get_i32_from_res_frame_4B_reg(int addr, uint8_t *buf);
uint32_t get_u32_from_res_frame(int addr, uint8_t *buf);
uint16_t get_u16_from_res_frame(int addr, uint8_t *buf);
int16_t get_i16_from_res_frame(int addr, uint8_t *buf);
int parse_sync_time(void);
float u32_to_float(uint32_t stored_data);

int is_time_valid(void);

// int8_t md_decode_meter_pack(char is_fast, uint8_t type, uint8_t *buf, uint16_t len);
int8_t md_decode_meter_pack(char is_fast, uint8_t type, uint8_t *buf, uint16_t len,uint8_t frame_order);

// meter_data_t *get_static_inv_meter(void);
//--------------------------------------//

#endif