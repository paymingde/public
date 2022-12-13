/**
 * @file asw_fat.h
 * @author tgl (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-03-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef ASW_FAT_H
#define ASW_FAT_H

#include "Asw_global.h"

#include <cJSON.h>
#include <dirent.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "vfs_fat_internal.h"
#include "utility.h"
#include "crc16.h"

#include <diskio_impl.h>
#include <vfs_fat_internal.h>
#include <ff.h>  //[tgl mark] add for fats 


#define OLD_DB_PATH "/inv/lost.db"
#define NEW_DB_PATH "/inv/new.db"


//-----------------------------//
typedef struct
{
    char tag[16];
    char log[332];
    int  index;
} sys_log;

//---------------------------
int check_inv_pv(Inv_data * inv_data);
int check_file_exist(char * path);
int find_new_days(FILE *fp, char * day);
int find_next_day(FILE *fp, char * day, int lines);
int transfer_another_file(void);
int read_lost_index(void);
int read_lost_data(Inv_data * inv_data, int * line);
int check_lost_data(void);
int write_lost_index(int index);
int write_lost_data(Inv_data * inv_data);
//--------------------------//
int8_t asw_fat_mount(void);
int8_t inv_fat_mount(void);
void clear_file_system(void);
int get_sys_log(char *buf);
void get_file_list(char *msg);
int network_log(char *msg);

int force_format(char *partname);
int force_flash_reformart(char *partion);
#if 0
int network_log(char *msg);

int read_temp_file(char *netlogx);
void get_file_list(char *msg);

void clear_file_system(void);

int get_sys_log(char *buf);
#endif
#endif