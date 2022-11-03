/**
 * @file asw_mutex.h
 *       energy-storage machine supported
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-09-07
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ASW_MUTEX_H
#define _ASW_MUTEX_H

#include "Asw_global.h"

void asw_sema_ini(void);

void read_global_var(int type, void *p);

void write_global_var(int type, void *p);

void write_global_var_to_nvs(int type, void *p);

void load_global_var(void);

void inv_arr_memcpy(Inv_arr_t *des, Inv_arr_t *src);

void check_estore_become_online(void);

int is_cgi_has_estore(void);

int is_inv_has_estore(void);

int8_t is_has_inv_online(void);

int is_com_has_estore(void);

#endif
