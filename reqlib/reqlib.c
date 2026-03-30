#include "reqlib.h"
#include "my_tick.h"
/******************************** Definitions **********************************/

/********************************* Variables ***********************************/

/*********************************** Code **************************************/
/*!
    \name     reqlib_get_time_ms
    \brief    get time by millisecond
    \param [in & out]
        \arg  none
    \retval   time(ms) value
*/
static uint32_t reqlib_get_time_ms(void)
{
    return get_my_tick();
}

/*!
    \name     reqlib_passed_delay
    \brief    check if current time passed start time by expected time
    \param [in & out]
        \arg  start_time: start time, ms
        \arg  ms_delay: a time interval we expected, ms
    \retval   0: false 1:true
*/
static uint8_t reqlib_passed_delay(uint32_t start_time, uint32_t ms_delay)
{
    uint32_t stoptime = start_time + ms_delay;
    uint32_t current_time = reqlib_get_time_ms();

    if ((int32_t)current_time - (int32_t)stoptime >= 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*!
    \name     reqlib_step_exe
    \brief    request with retry times and timeout
    \param [in & out]
        \arg  ctl: control parameters
    \retval   execution result
*/
uint8_t reqlib_step_exe(reqlib_exe_ctl_t *ctl)
{
    if (ctl->step == 0)
    {
        // do request
        if (NULL != ctl->request_cb)
            ctl->request_cb();

        ctl->send_tick = reqlib_get_time_ms();
        ctl->resend_tick = ctl->send_tick;
        ctl->step = 1;
        return REQLIB_EXE_RUNNING;
    }
    else if (ctl->step == 1)
    {
        // resend for response never start. for long time response only like 1KB can data's receive
        if (ctl->never_response_cond != NULL)
        {
            if (ctl->never_response_cond())
            {
                if (reqlib_passed_delay(ctl->resend_tick, ctl->resend_period) &&
                    !reqlib_passed_delay(ctl->send_tick, ctl->timeout - ctl->resend_period))
                {
                    ctl->request_cb();
                    ctl->resend_tick = reqlib_get_time_ms();
                }
            }
        }
        // check success
        if (NULL != ctl->success_cond)
        {
            if (ctl->success_cond()) // responsed and result ok
            {
                ctl->step = 0;
                ctl->timescnt = 0;
                // success callback
                if (NULL != ctl->success_cb)
                    ctl->success_cb();
                return REQLIB_EXE_SUCCESS;
            }
        }

        // check single request finish's timeout or not
        // resend on single request finish timeout
        // total timeout limited by given times
        if (reqlib_passed_delay(ctl->send_tick, ctl->timeout))
        {
            ctl->timescnt++;
            if (ctl->timescnt >= ctl->times)
            {
                ctl->step = 0;
                ctl->timescnt = 0;
                // failure callback
                if (NULL != ctl->failure_cb)
                    ctl->failure_cb();
                return REQLIB_EXE_FAILURE; // total timeout
            }
            ctl->step = 0;
            return REQLIB_EXE_RUNNING; // each request's timeout
        }
        else
        {
            return REQLIB_EXE_RUNNING; // each request's wait
        }
    }

    // exception only, normally not run this
    ctl->step = 0;
    ctl->timescnt = 0;
    // failure callback
    if (NULL != ctl->failure_cb)
        ctl->failure_cb();
    return REQLIB_EXE_FAILURE;
}

/******************************* end of reqlib.c **************************/
