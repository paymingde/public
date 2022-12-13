/**
 * @brief readme 关于时间 -- tgl add --
 *
 * If you need to obtain time with one second resolution, use the following method:

        time_t now;
        char strftime_buf[64];
        struct tm timeinfo;

        time(&now);
        // Set timezone to China Standard Time
        setenv("TZ", "CST-8", 1);
        tzset();

        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

* If you need to obtain time with one microsecond resolution, use the code snippet below:

        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
*
*/

#include "utility.h"
static const char *TAG = "utility.c";
/************************************/
//--------------------------------//
uint32_t get_time(char *p_date, int len)
{
    time_t t = time(0);
    // struct tm currtime = {0};
    strftime(p_date, len, "%Y-%m-%d %H:%M:%S", localtime(&t)); //
    // strftime(p_date, len, "%Y-%m-%d %H:%M:%S", localtime_r(&t, &currtime));
    return (uint32_t)t;
}

//-----------------------------------//
uint32_t get_time_by_sec_later(char *p_date, int len, int sec)
{
    time_t t = time(0) + sec;
    // struct tm currtime = {0};
    strftime(p_date, len, "%Y-%m-%d %H:%M:%S", localtime(&t)); //
    // strftime(p_date, len, "%Y-%m-%d %H:%M:%S", localtime_r(&t, &currtime));
    return (uint32_t)t;
}
//--------------------------------//
int get_current_days(void)
{
    struct tm currtime = {0};
    time_t t = time(NULL);

    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;
    currtime.tm_mon += 1;

    return currtime.tm_mday;
}
//--------------------------------//
int get_current_year(void)
{
    struct tm currtime = {0};
    time_t t = time(NULL);

    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;
    currtime.tm_mon += 1;

    return currtime.tm_year;
}

//----------------------------------//
void get_time_compact(char *p_date, int len)
{
    time_t t = time(0);
    struct tm currtime = {0};
    strftime(p_date, len, "%Y%m%d%H%M%S", localtime_r(&t, &currtime));
}
//--------------------------
int fileter_time(char *times, char *buf)
{
    int i = 0, j = 0;
    char new_time[64] = {0};
    for (; i < strlen(times); i++)
        if (times[i] != '-' && times[i] != ' ' && times[i] != ':')
            new_time[j++] = times[i];

    memcpy(buf, new_time, strlen(new_time));
    return 0;
}
//-----------------------------------------//
void set_time_cgi(char *str)
{
    struct tm tm = {0};
    struct timeval tv = {0};
    char buf[20] = {0};

    get_time(buf, 20);
    // printf("before set: %s\n", buf); ////recv time: 2020 10 22 14 08 13
    // strptime(str, "%+4Y%+2m%+2d%+2H%+2M%+2S", &tm);// sscanf( dtm, "%s %s %d  %d", weekday, month, &day, &year );
    // ESP_LOGE("parse set time", "%04d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    sscanf(str, "%4d", &tm.tm_year);
    sscanf(str + 4, "%2d", &tm.tm_mon);
    sscanf(str + 6, "%2d", &tm.tm_mday);
    sscanf(str + 8, "%2d", &tm.tm_hour);
    sscanf(str + 10, "%2d", &tm.tm_min);
    sscanf(str + 12, "%2d", &tm.tm_sec);
    if (tm.tm_year > 2019 && tm.tm_year < 2050)
    {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        // ESP_LOGE("parse set time", "%04d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (tm.tm_year > 119)
        {
            tv.tv_sec = mktime(&tm);
            settimeofday(&tv, NULL);
            memset(buf, 0, 20);
            get_time(buf, 20);
            ESP_LOGI(TAG, "after set: %s\n", buf);
        }
    }
}

//-----------------------------------//

int write_daylight_data(int tmz, char *tmdata)
{
    remove("/inv/timezone");
    char buf[16] = {0};
    sprintf(buf, "%d\r\n", tmz);

    ASW_LOGI("write timezone and summer time:%s--%s \n", buf, tmdata);

    if (strlen(buf) <= 0)
        return -1;

    FILE *fp = fopen("/inv/timezone", "w"); //"wb"
    if (fp == NULL)
    {
        ESP_LOGE("tmz", "Failed to open file for writing");
        return -2;
    }

    fseek(fp, 0, SEEK_SET);

    int res = -1;
    res = fwrite(buf, sizeof(char), strlen(buf), fp);
    res = fwrite(tmdata, sizeof(char), strlen(tmdata), fp);
    ASW_LOGI("tmz write %d \n", res);
    fclose(fp);
    return 0;
}
//-------------------------------------//
int set_time_from_string(const char *chGetTiem, int tmz)
{
    struct tm rtime;
    ASW_LOGI("GetTiem : %s \n", chGetTiem); //2021-03-22 08:25:00
    char chGetTemp[5];

    memset(chGetTemp, 0x00, 5);
    memcpy(chGetTemp, &chGetTiem[0], 4);
    if (atoi(chGetTemp) < 2019 || atoi(chGetTemp) > 2050)
    {
        return -1;
    }
    rtime.tm_year = atoi(chGetTemp);

    //month
    memset(chGetTemp, 0x00, 5);
    memcpy(chGetTemp, &chGetTiem[5], 2);
    rtime.tm_mon = atoi(chGetTemp);

    //day
    memset(chGetTemp, 0x00, 5);
    memcpy(chGetTemp, &chGetTiem[8], 2); //2021-03-22 08:35:36
    rtime.tm_mday = atoi(chGetTemp);
    //hour
    memset(chGetTemp, 0x00, 5);
    memcpy(chGetTemp, &chGetTiem[11], 2);
    rtime.tm_hour = atoi(chGetTemp);
    //minute
    memset(chGetTemp, 0x00, 5);
    memcpy(chGetTemp, &chGetTiem[14], 2);
    rtime.tm_min = atoi(chGetTemp);
    //second
    memset(chGetTemp, 0x00, 5);
    memcpy(chGetTemp, &chGetTiem[17], 2);
    rtime.tm_sec = atoi(chGetTemp);

    ASW_LOGI("Cloud time:%s  %02d-%02d-%02d %02d:%02d:%02d\n ", chGetTiem,
           rtime.tm_year, rtime.tm_mon, rtime.tm_mday, rtime.tm_hour, rtime.tm_min, rtime.tm_sec);

    int rmt_tmp = RTC_ConvertDatetimeToSeconds(&rtime);
    ASW_LOGI("cloud time seconds %d %d\r\n", rmt_tmp, rmt_tmp + tmz * 60);
    //rmt_tm_sec += tmz*60;
    //parse dst
    int rmt_tm_sec = read_timezone(rmt_tmp);
    ASW_LOGI("adjust time seconds %d \r\n", rmt_tm_sec);
    //
    struct tm changed_tm = {0};
    RTC_ConvertSecondsToDatetime(rmt_tm_sec, &changed_tm);
    ASW_LOGI("changed local time:%d  %02d-%02d-%02d %02d:%02d:%02d\n ", rmt_tm_sec,
           changed_tm.tm_year + 1900, changed_tm.tm_mon + 1, changed_tm.tm_mday, changed_tm.tm_hour, changed_tm.tm_min, changed_tm.tm_sec);
    rtime = changed_tm;

    //-----------------------------------------
    struct tm curr_time;
    time_t t = time(NULL);

    localtime_r(&t, &curr_time);
    // curr_time.tm_year += 1900;

    ASW_LOGI("get local time: %04d-%02d-%02d %02d:%02d:%02d \n",
           curr_time.tm_year + 1900, curr_time.tm_mon + 1, curr_time.tm_mday, curr_time.tm_hour, curr_time.tm_min, curr_time.tm_sec);

    if ((rtime.tm_year != curr_time.tm_year) || (rtime.tm_mon != curr_time.tm_mon) || (rtime.tm_mday != curr_time.tm_mday) || (rtime.tm_hour != curr_time.tm_hour) || (abs(rtime.tm_min - curr_time.tm_min) >= 2))
    {
        ASW_LOGI("Cloud current time > 2  minutes and set it \n");

        struct timeval stime = {0};
        stime.tv_sec = mktime(&rtime);

        settimeofday(&stime, NULL);
    }
    else
    {
        ASW_LOGI("Cloud current time < 2 minute, not set it\n");
    }
    return 0;
}
//----------------------------------------//
uint32_t RTC_ConvertDatetimeToSeconds(struct tm *datetime)
{
    struct timeval tv = {0};

    if (datetime->tm_year > 2019 && datetime->tm_year < 2050)
    {
        datetime->tm_year -= 1900;
        datetime->tm_mon -= 1;

        if (datetime->tm_year > 119)
        {
            tv.tv_sec = mktime(datetime);
            ASW_LOGI("data to second %ld\n", tv.tv_sec);
            return (tv.tv_sec);
        }
    }
    return -1;
}
//-------------------------------------//
int RTC_ConvertSecondsToDatetime(uint32_t seconds, struct tm *datetime)
{
    struct tm *info;

    info = localtime(&seconds);
    memcpy(datetime, info, sizeof(struct tm));
    return 0;
}
//------------------------------------//
int read_timezone(uint32_t utc_sec)
{
    int i = -1;
    FILE *file;
    int r_size = 0;
    char buf[512] = {0};
    int tm_off = 0;
    char time[100] = {0};
    uint32_t time_sec = utc_sec;

    FILE *fp = fopen("/inv/timezone", "rb");
    if (fp == NULL)
    {
        ESP_LOGE("TAG", "Failed to open timezone for reading");
        //remove("/inv/lost.db");
        return time_sec;
    }

    r_size = fread(buf, sizeof(char), sizeof(buf), fp);
    //if(FR_OK == f_read(&file, buf, sizeof(buf), (uint_32*)&r_size))
    ASW_LOGI("read %d timezone:%s \n", r_size, buf);
    if (r_size > 0)
    {
        char *p1 = strstr(buf, "\r\n"); //time offset
        if (p1)
        {
            memcpy(time, buf, p1 - buf);
            ASW_LOGI("read first %d %d line %s\n", strlen(time), p1 - buf, time);
            tm_off = atoi(time);
            memset(time, 0, sizeof(time));
            p1 += 2;
            ASW_LOGI("tm_off %d \n", tm_off);
        }
        time_sec = utc_sec + tm_off * 60;
        ASW_LOGI("utc+offset %d \n", time_sec);

        while (i++ < 10)
        {
            char *p2 = strstr(p1, "\r\n");
            if (p2 != NULL)
            {
                memcpy(time, p1, p2 - p1);
                ASW_LOGI("read %d line %d %d %s\n", i, strlen(time), p2 - p1, time);
                if (parse_dst(time, time_sec) == 0)
                {
                    if (tm_off == 630) //+10:30 = Lord Howe Island
                    {
                        ASW_LOGI("Lord Howe\n");
                        time_sec += 1800;
                    }
                    else
                    {
                        time_sec += 3600;
                    }

                    break;
                }
                p1 = p2 + 2;
            }
            else
            {
                printf("newline not found\n");
                break;
            }
        }
    }
    else
        printf("read timezone error\n");

    fclose(fp);
    ASW_LOGI("finally timesec %d \n", time_sec);
    return time_sec;
}
//------------------------------------//
void get_time_from_tmstp(char *p_date, int len, uint32_t tmstp)
{
    time_t t = tmstp;
    struct tm currtime = {0};
    strftime(p_date, len, "%Y-%m-%d %H:%M:%S", localtime_r(&t, &currtime));
}
//---------------------------------------//
int parse_dst(char *str, uint32_t time_sec)
{
    struct tm tm = {0};
    struct timeval tv = {0};
    char curr_date[20] = {0};
    char date1[20] = {0};
    char date2[20] = {0};

    get_time_from_tmstp(curr_date, sizeof(curr_date), time_sec);
    char *p = strstr(str, ",");
    memcpy(date1, str, 19);
    memcpy(date2, p + 1, 19);

    ASW_LOGI("curr date:%s\n", curr_date);
    ASW_LOGI("date1:%s\n", date1);
    ASW_LOGI("date2:%s\n", date2);

    if (strcmp(date2, date1) > 0)
    {
        if (strcmp(curr_date, date1) > 0 && strcmp(curr_date, date2) < 0)
        {
            return 0;
        }
    }

    return -1;
}


//----------------------------------//
void hex_print(const char *buf, int len)
{
    if (len > 0)
    {
        for (int i = 0; i < len; i++)
            ESP_LOGI(TAG, "<%02X> ", buf[i]);
    }
}

//-----------------------------------//
int StrToHex(unsigned char *pbDest, char *pbSrc, int srcLen)
{
    char h1, h2;
    unsigned char s1, s2;
    int i;

    for (i = 0; i < srcLen / 2; i++)
    {
        h1 = pbSrc[2 * i];
        h2 = pbSrc[2 * i + 1];

        s1 = toupper(h1) - 0x30;
        if (s1 > 9)
            s1 -= 7;

        s2 = toupper(h2) - 0x30;
        if (s2 > 9)
            s2 -= 7;
        pbDest[i] = s1 * 16 + s2;
    }
    return srcLen / 2;
}
//----------------------------------//
int HexToStr(unsigned char *pbDest, unsigned char *pbSrc, int srcLen)
{
    int i;
    char tmp[3] = {0};
    for (i = 0; i < srcLen; i++)
    {
        memset(tmp, 0, sizeof(tmp));
        // char tmp[2] = {0};
        sprintf(tmp, "%02x", pbSrc[i]);
        pbDest[i * 2] = tmp[0];
        pbDest[i * 2 + 1] = tmp[1];
    }
    return srcLen * 2;
}

void asw_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init_partition("my_nvs");
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase_partition("my_nvs"));
        ret = nvs_flash_init_partition("my_nvs");
    }
    ESP_ERROR_CHECK(ret);
}
//---------------------------------------------//
void asw_nvs_clear(void)
{
    nvs_flash_deinit_partition("my_nvs");
    ESP_ERROR_CHECK(nvs_flash_erase_partition("my_nvs"));
    asw_nvs_init();
}
//---------------------------------------------//
void factory_initial(void)
{
    clear_file_system();
    factory_reset_nvs();
    ESP_LOGW(TAG, " will restart by [facotry initial]");

  

    esp_restart();
}
//---------------------------------------------//query_meter_proc--->
void esp32_wifinvs_clear(void)
{
    printf("delet wifiphy data");
    nvs_flash_deinit_partition("nvs");
    ESP_ERROR_CHECK(nvs_flash_erase_partition("nvs"));
}
//----------------------------//
//从json中获取某字符串字段的值
int getJsonStr(char *dest, char *name, int size, cJSON *json)
{
    const cJSON *item = NULL;
    item = cJSON_GetObjectItemCaseSensitive(json, name);
    if (cJSON_IsString(item) && (item->valuestring != NULL))
    {
        strncpy(dest, item->valuestring, size);
        return 0;
    }
    return -1;
}
//---------------------------------------------//
//从json中获取某数字字段的值
int getJsonNum(int *dest, char *name, cJSON *json)
{
    const cJSON *item = NULL;
    item = cJSON_GetObjectItemCaseSensitive(json, name);
    if (cJSON_IsNumber(item))
    {
        *dest = item->valueint;
        return 0;

        // ASW_LOGW("---888***&&&& tgl debug print: get value :%d",item->valueint);
    }
    return -1;
}

/************************************************/
void int_2_str(int x, char *s, int s_len)
{
    memset(s, 0, s_len);
    sprintf(s, "%d", x);
}
//----------------------------------
void add_int_to_json(cJSON *res, char *tag_name, int num)
{
    char buf[300] = {0};
    int_2_str(num, buf, sizeof(buf));
    cJSON_AddStringToObject(res, tag_name, buf);
}
//----------------------------------
void add_str_to_json(cJSON *res, char *tag_name, char *val)
{
    cJSON_AddStringToObject(res, tag_name, val);
}
//----------------------------------//
