/**
 * @file save_inv_data.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-04-12
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _SAVE_DATA_H
#define _SAVE_DATA_H

#include "Asw_global.h"
#include <dirent.h>
#include "data_process.h"
#include "asw_fat.h"

#include "esp32_time.h"
#include "app_mqtt.h"


extern int net_com_reboot_flg;  

int write_change_apconfig(void);
int recive_invdata(int * flag, Inv_data * inv_data, int * lost_flag);
// void set_resetnet_reboot(void);
void check_resetnet_reboot(void);


//Eng.Stg.Mch-lanstick + 
int read_hist_msg(char *msg, int *msg_len);
int hist_offset_next(void);
int read_hist_payload(char *msg);
int read_instant_payload(char *msg);

#endif
