/**
 * @file esp32_time.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-03-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _ESP32_TIME_H
#define _ESP32_TIME_H


#include "Asw_global.h"


int64_t  get_msecond_sys_time(void);
int64_t get_second_sys_time(void);
int8_t check_time_correct(void);

int time_legal_parse(void);
#endif