#include "stdio.h"
#include "string.h"
#include "stdint.h"

float signal[] = {
    0.0f,
    1.53f,
    1.6f,
    1.675f,
    1.7975f,
    2.67875f,
    2.204375f,
    2.0671875f,
    2.79859375f,
    2.659296875f,
    3.744648438f,
    3.307324219f,
    4.033662109f,
    5.036831055f,
    6.013415527f,
    6.231707764f,
    6.835853882f,
    9.002926941f,
    10.01146347f,
    10.41073174f,
    11.87036587f,
    13.71518293f,
    15.35759147f,
    15.88379573f,
    15.70189787f,
    16.63094893f,
    18.26047447f,
    18.15023723f,
    19.40011862f,
    21.33005931f,
    22.15502965f,
    22.12751483f,
    22.87875741f,
    23.58437871f,
    25.18218935f,
    26.17109468f,
    28.37054734f,
    29.97527367f,
    30.55263683f,
    31.99131842f,
    32.53565921f,
    33.4828296f,
    33.7064148f,
    35.7182074f,
    36.1691037f,
    36.19955185f,
    38.34477593f,
    38.66238796f,
    39.71119398f,
    40.82059699f,
    41.0702985f,
    43.36014925f,
    43.85007462f,
    44.09003731f,
    44.78001866f,
    46.78000933f,
    47.77000466f,
    48.63000233f,
    50.45000117f,
    51.03000058f,
    52.11000029f,
    52.63000015f,
    53.47500007f,
    54.69750004f,
    55.80875002f,
    56.49937501f,
    58.3746875f,
    58.39234375f,
    60.30117188f,
    60.74058594f,
    45.51529297f,
    39.11764648f,
    35.83382324f,
    36.25191162f,
    36.58095581f,
    37.57547791f,
    36.92773895f,
    38.42386948f,
    40.02693474f,
    40.19346737f,
    40.40173368f,
    42.13586684f,
    43.99293342f,
    44.23646671f,
    45.58823336f,
    46.07911668f,
    46.14955834f,
    46.60977917f,
    48.62488958f,
    49.03244479f,
    51.5012224f,
    51.3106112f,
    51.7653056f,
    52.5126528f,
    54.7463264f,
    55.7331632f,
    55.9215816f,
    58.1157908f,
    58.4078954f,
    59.2339477f,
    61.29697385f,
    61.65348692f,
    63.51674346f,
    63.94337173f,
    65.79668587f,
    66.46334293f,
    66.83167147f,
    67.70583573f,
    67.95791787f,
    69.65895893f,
    70.31447947f,
    71.73223973f,
    72.56611987f,
    73.54305993f,
    74.19652997f,
    76.12826498f,
    77.05413249f,
    77.09706625f,
    79.08853312f,
    79.58426656f,
    80.36713328f,
    80.68856664f,
    83.12928332f,
    84.04964166f,
    84.13482083f,
    84.56741042f,
    86.39870521f,
    88.5943526f,
    89.6221763f,
    90.60108815f,
    91.29054408f};

typedef enum
{
    PEAK_FIND_INIT,
    PEAK_FIND_BIG,
    PEAK_FIND_SMALL,
} STEP_STATE;

typedef struct
{
    float val;
    int index;
    int polar;
} point_record_t;

typedef struct
{
    uint32_t deal_count;
    point_record_t record[10];
    int record_count;
    STEP_STATE step;
    float init_val;
    int polar;
    float max;
    float min;
    uint32_t max_idx;
    uint32_t min_idx;
} peak_ctl_t;

#define THRESHOLD 10

peak_ctl_t ctl;
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

void find_peak_init(void)
{
    memset(&ctl, 0, sizeof(ctl));
}

void push_to_record(float point, uint32_t index, int polar)
{
    ctl.record[ctl.record_count].val = point;
    ctl.record[ctl.record_count].index = index;
    ctl.record[ctl.record_count].polar = polar;
    ctl.record_count++;
}

void find_peak(float point)
{
    float delta;
    if (ctl.deal_count == 0)
    {
        ctl.init_val = point;
        push_to_record(point, ctl.deal_count, 0);
        ctl.deal_count++;
        return;
    }
    switch (ctl.step)
    {
    case PEAK_FIND_INIT:
        delta = point - ctl.init_val;
        if (my_abs(delta) > THRESHOLD)
        {
            ctl.polar = my_sign(delta);
            if (ctl.polar > 0)
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
        if (delta > THRESHOLD)
        {
            push_to_record(ctl.max, ctl.max_idx, 1);
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
        if (delta > THRESHOLD)
        {
            push_to_record(ctl.min, ctl.min_idx, -1);
            ctl.min = point;
            ctl.step = PEAK_FIND_BIG;
        }
        break;
    default:
        break;
    }
    ctl.deal_count++;
}

void main(void)
{
    for (int i = 0; i < sizeof(signal) / 4; i++)
    {
        find_peak(signal[i]);
    }

    for (int i = 0; i < ctl.record_count; i++)
    {
        point_record_t *t = &ctl.record[i];
        printf("%d %d %f\n", t->index, t->polar, t->val);
    }
}
