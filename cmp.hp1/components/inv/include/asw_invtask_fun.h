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

int  handleMsg_setAdv_fun(void);
int handleMsg_pwrActive_fun(MonitorPara *p_monitor_para, meter_data_t *p_inv_meter);

#endif