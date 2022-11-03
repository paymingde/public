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
