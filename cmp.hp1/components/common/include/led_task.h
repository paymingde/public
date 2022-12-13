/**
 * @file led_tash.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _LED_TASK_H
#define _LED_TASK_H
#include "Asw_global.h"

#include "driver/gpio.h"
#include "esp32_time.h"
#include "save_inv_data.h"
#include "wifi_sta_server.h"

// int set_ap_sts(void);
// void smartconfig_led(void *pvParameters);
void led_task(void *pvParameters);
void led_gpio_init();

#endif