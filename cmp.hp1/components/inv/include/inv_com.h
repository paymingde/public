/**
 * @file inv_com.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-03-29
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef INV_COM_H
#define INV_COM_H

#include "Asw_global.h"
#include "driver/uart.h"

#include "driver/gpio.h"
#include "data_process.h"
#include "esp32_time.h"
#include "asw_nvs.h"
#include "asw_modbus.h"

#include "asw_fat.h"
// #include "log.h"
#include "utility.h"
#include "crc16.h"
#include "update.h"

// #define BATTERY_CHARGING_STOP 1 // stop charging/discharging
// #define BATTERY_CHARGING 2      // charging
// #define BATTERY_DISCHARGING 3   // discharging

//-----------------------------------//

typedef enum
{
    MAIN_UART = 0,
    INV_UART = 1,
    ATE_UART = 2
} UART_TYPE;

//-----------------------------------//

// void drm_handler(int drm); [tgl update] delete
// void com_idle_work(void);

int check_inv_states();

//-----------------------------------//

extern Inv_arr_t ipc_inv_arr;
// extern uint8_t g_num_real_inv;

extern Inv_arr_t cgi_inv_arr;
extern Inv_arr_t cld_inv_arr;

extern Inv_arr_t inv_arr;

// extern Bat_arr_t g_bat_arr; // LanStick-MultilInv  +

extern int inv_comm_error;

extern int inv_com_reboot_flg;


int last_com_data(unsigned int index);

int8_t serialport_init(UART_TYPE type);
int chk_msg(cloud_inv_msg *trans_data);

uint8_t load_reg_info(void);
void flush_serial_port(uint8_t);
int internal_inv_log(int typ);

int inv_com_log(int index, int err);
int8_t legal_check(char *text);

int8_t get_sum(uint8_t *buf, int len, uint8_t *high_byte, uint8_t *low_byte);
int8_t check_sum(uint8_t *buf, int len);
int8_t parse_sn(uint8_t *buf, int len, char *psn);
int8_t parse_modbus_addr(uint8_t *buf, int len, uint8_t *addr);
int8_t save_reg_info(void);
void reg_info_log(void);
int8_t clear_inv_list(void);
int8_t get_modbus_id(uint8_t *id);

int8_t recv_bytes_frame_transform(int fd, uint8_t *res_buf, uint16_t *res_len);

SERIAL_STATE inv_task_schedule(char *func, cloud_inv_msg *data);
SERIAL_STATE query_data_proc();
SERIAL_STATE upinv_transdata_proc(char func, cloud_inv_msg tans_data);
SERIAL_STATE scan_register(void);
SERIAL_STATE ssc_send_meterdata_proc(void);  //SSC 

int8_t recv_bytes_frame_waitting_nomd(int fd, uint8_t *res_buf, uint16_t *res_len);

//-----------------外部component调用------------------//

int get_inv_log(char *buf, int typ);
int recv_bytes_frame_meter(int fd, uint8_t *res_buf, uint16_t *res_len);
//---------------------------------------//

/////////// Energy-Storage ////////////
void com_idle_work(void);
int get_estore_inv_arr_first(void);

int is_safety_96_97(void);
void cld_idle_work(void);
int get_cld_inv_arr_first(void);

int asw_inv_data_init();

int asw_is_safety_96_97(void);

#endif
