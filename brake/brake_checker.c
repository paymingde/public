#include "brake_check.h"

peak_ctl_t ctl;
static int peak_find_start_event;

float my_abs(float val)
{
    if (val > 0)
        return val;
    else
        return -val;
}

int my_sign(float val)
{
    if (val > 0)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

void find_peak_clear(void)
{
    peak_find_start_event = 1;
}

void find_peak_reset(void)
{
    memset(&ctl, 0, sizeof(ctl));
}

peak_ctl_t *get_peak_finder(void)
{
    return &ctl;
}

void append_record(float point, uint32_t index, int polar)
{
    if (ctl.record_count >= PREAK_RECORD_MAX_NUM - 1)
        return;
    ctl.record[ctl.record_count].val = point;
    ctl.record[ctl.record_count].index = index;
    ctl.record[ctl.record_count].polar = polar;
    ctl.record[ctl.record_count].timestamp_us = 0; // TODO save systime by us
    ctl.record_count++;
}

void find_peak(float point)
{
    float delta;
    if (peak_find_start_event)
    {
        peak_find_start_event = 0;
        find_peak_reset();
    }
    switch (ctl.step)
    {
    case PEAK_FIND_INIT:
        ctl.init_val = point;
        ctl.step = PEAK_FIND_START;
        break;
    case PEAK_FIND_START:
        delta = point - ctl.init_val;
        if (my_abs(delta) > FIRST_THRESHOLD)
        {
            ctl.init_val = point;
            append_record(point, ctl.deal_count, 0);
            ctl.step = PEAK_FIND_TREND;
        }
        break;
    case PEAK_FIND_TREND:
        delta = point - ctl.init_val;
        if (my_abs(delta) > PEAK_THRESHOLD)
        {
            if (my_sign(delta) > 0)
            {
                ctl.max = point;
                ctl.max_idx = ctl.deal_count;
                ctl.step = PEAK_FIND_BIG;
            }
            else
            {
                ctl.min = point;
                ctl.min_idx = ctl.deal_count;
                ctl.step = PEAK_FIND_SMALL;
            }
        }
        break;
    case PEAK_FIND_BIG:
        if (point > ctl.max)
        {
            ctl.max = point;
            ctl.max_idx = ctl.deal_count;
        }
        delta = ctl.max - point;
        if (delta > PEAK_THRESHOLD)
        {
            append_record(ctl.max, ctl.max_idx, 1);
            ctl.min = point;
            ctl.step = PEAK_FIND_SMALL;
        }
        break;

    case PEAK_FIND_SMALL:
        if (point < ctl.min)
        {
            ctl.min = point;
            ctl.min_idx = ctl.deal_count;
        }
        delta = point - ctl.min;
        if (delta > PEAK_THRESHOLD)
        {
            append_record(ctl.min, ctl.min_idx, -1);
            ctl.max = point;
            ctl.step = PEAK_FIND_BIG;
        }
        break;
    default:
        break;
    }
    ctl.deal_count++;
}
