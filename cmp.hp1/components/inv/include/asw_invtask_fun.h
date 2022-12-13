/**
 * @file asw_invtask_fun.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-08
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ASW_INVTASK_FUN_H
#define _ASW_INVTASK_FUN_H
#include "Asw_global.h"
#include "inv_com.h"
#include "data_process.h"
#include "asw_modbus.h"

int8_t setting_event_handler(void);

void handleMsg_setAdv_fun(void);
#endif