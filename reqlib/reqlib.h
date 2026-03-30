#ifndef REQLIB_H
#define REQLIB_H

#include "stdint.h"
#include "stdbool.h"

/********************************* Definitions *********************************/
typedef enum
{
    REQLIB_EXE_SUCCESS,
    REQLIB_EXE_FAILURE,
    REQLIB_EXE_RUNNING,
} REQLIB_EXE_RESULT;

typedef void (*reqlib_void_fptr)(void);
typedef bool (*reqlib_bool_fptr)(void);

typedef struct
{
    // control part
    uint8_t step;
    uint32_t resend_tick;
    uint32_t send_tick;
    uint8_t timescnt;

    // input params
    uint16_t timeout;
    uint16_t resend_period;
    uint8_t times;
    reqlib_void_fptr request_cb;
    reqlib_bool_fptr never_response_cond;
    reqlib_bool_fptr success_cond;
    reqlib_void_fptr success_cb;
    reqlib_void_fptr failure_cb;
} reqlib_exe_ctl_t, *reqlib_exe_ctl_ptr;
/********************************* Variables ***********************************/

/************************************* Api *************************************/
#ifdef __cplusplus
extern "C"
{
#endif

uint8_t reqlib_step_exe(reqlib_exe_ctl_t *ctl);

#ifdef __cplusplus
}
#endif

#endif /* REQLIB_H */

/******************************* end of reqlib.h **************************/
