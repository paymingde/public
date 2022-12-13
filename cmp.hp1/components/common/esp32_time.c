#include "esp32_time.h"
/************************************/
//----------------------------------//
int64_t get_msecond_sys_time(void)
{
    return (esp_timer_get_time() / 1000);
}

int64_t get_second_sys_time(void)
{
    return (esp_timer_get_time() / (1000 * 1000));
}

int8_t check_time_correct(void)
{
    struct tm currtime = {0};
    time_t t = time(NULL);

    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;

    if (currtime.tm_year >= 2020)
        return ASW_OK;
    return ASW_FAIL;
}
//--------------------------//
int time_legal_parse(void)
{
    struct tm currtime;
    time_t t = time(NULL);

    localtime_r(&t, &currtime);

    currtime.tm_year += 1900;

    if (currtime.tm_year >= 2020)
        return 0;
    else
        return -1;
}

