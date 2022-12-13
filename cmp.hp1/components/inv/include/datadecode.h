/**
 * @file datadecode.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-03-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef DATADECODE_H
#define DATADECODE_H
#include "Asw_global.h"
#include "data_process.h"

void md_decode_inv_data_content(uint8_t *packet, Inv_data *inv_data_ptr);
void md_decode_inv_Info(const uint8_t *data, InvRegister *inv_ptr);
#if 0

#include "data_process.h"
void Check_upload_unit32(const char *field, uint_8 value, char *body, uint_16 *datalen);
void Check_upload_sint16(const char *field, sint_16 value, char *body, uint_16 *datalen);
#endif
#endif