/**
 * @file http_client.h
 * @author tgl (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-03-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#include "Asw_global.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "time.h"


int http_get_time(char *time_url, struct tm *p_time);
int http_get_file(char *url, char *save_name, int * file_len);


#endif