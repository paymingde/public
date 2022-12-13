/**
 * @file asw_job_http.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _ASW_JOB_HTTTP_H
#define _ASW_JOB_HTTP_H
#include "Asw_global.h"
#include "data_process.h"
#include "esp_http_server.h"

#include "utility.h"

extern const httpd_uri_t asw_get_callback;
extern const httpd_uri_t asw_post_callback; //[tgl mark]add

#if TRIPHASE_ARM_SUPPORT

int send_cgi_msg(int type, int sla_id, char *buff, int lenthg,  char *ws);

#else

int send_cgi_msg(int type, char *buff, int lenthg, char *ws);

#endif

#endif