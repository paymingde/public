/**
 * @file cloud_task.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-04-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _CLOUD_TASK_H
#define _CLOUD_TASK_H

#include "Asw_global.h"
#include "app_mqtt.h"
#include "update.h"
#include "save_inv_data.h"

void cloud_task(void *pvParameters);

#endif