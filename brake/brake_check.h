#ifndef BRAKE_CHECK_H
#define BRAKE_CHECK_H

#include "stdio.h"
#include "string.h"
#include "stdint.h"

#define FIRST_THRESHOLD 4
#define PEAK_THRESHOLD 10
#define PREAK_RECORD_MAX_NUM 10

typedef enum
{
    PEAK_FIND_INIT,
    PEAK_FIND_START,
    PEAK_FIND_TREND,
    PEAK_FIND_BIG,
    PEAK_FIND_SMALL,
} PEAK_FINDER_STATE;

typedef struct
{
    float val;
    int index;
    int polar;
    uint32_t timestamp_us;
} point_record_t;

typedef struct
{
    uint32_t deal_count;
    point_record_t record[PREAK_RECORD_MAX_NUM];
    int record_count;
    PEAK_FINDER_STATE step;
    float init_val;
    float max;
    float min;
    uint32_t max_idx;
    uint32_t min_idx;
} peak_ctl_t;

void find_peak_clear(void);
void find_peak(float point);
peak_ctl_t *get_peak_finder(void);

#endif
