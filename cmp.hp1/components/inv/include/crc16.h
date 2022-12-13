/**
 * @file crc16.h
 * @author tgl (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-03-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef CRC16_H
#define CRC16_H

#include "Asw_global.h"

uint16_t crc16_calc(uint8_t* pchMsg, uint16_t wDataLen);

#endif