/**
 * @file estore_com.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-04-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _ESTORE_COM_H
#define _ESTORE_COM_H
#include "Asw_global.h"
#include "data_process.h"
#include "driver/uart.h"
#include "asw_mqueue.h"

#if 0
void read_bat_if_has_estore(void);
#endif

void asw_read_bat_arr_data(Inverter *inv_ptr);

#endif