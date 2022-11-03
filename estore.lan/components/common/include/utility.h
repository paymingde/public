/**
 * @file utility.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-08
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _UTILITY_H
#define _UTILITY_H
#include "Asw_global.h"

#include <ctype.h>
#include "cJSON.h"
#include "asw_nvs.h"

/*--- string ------*/
int StrToHex(unsigned char *pbDest, char *pbSrc, int srcLen);
int HexToStr(unsigned char *pbDest, unsigned char *pbSrc, int srcLen);
void hex_print(const char *buf, int len);

/*---  json ------*/
int getJsonStr(char *dest, char *name, int size, cJSON *json);
int getJsonNum(int *dest, char *name, cJSON *json);
void add_int_to_json(cJSON *res, char *tag_name, int num);
void add_str_to_json(cJSON *res, char *tag_name, char *val);

/*---  time ------*/
uint32_t get_time(char *p_date, int len);
int get_current_days(void);
int get_current_year(void);
void set_time_cgi(char *str);
void get_time_compact(char *p_date, int len);
int fileter_time(char *times, char *buf);

/*---  nvs------*/
void esp32_wifinvs_clear(void);
void factory_initial(void);
void asw_nvs_init(void);
void asw_nvs_clear(void);

#endif
