#include "inv_task.h"
#include "meter.h"
#include "asw_invtask_fun.h"
/*********************************************/
static const char *TAG = "inv_task.c";
/*********************************************/
//-------------------------------------------//
void inv_task(void *pvParameters)
{
    char trans_fun = 0;
    cloud_inv_msg p_trans_data = {0};
    SERIAL_STATE task_state = TASK_IDLE;

    ASW_LOGW("inv task start..");

    if (serialport_init(INV_UART) == 0)
    {
        ESP_LOGI(TAG, "inv uart ini ok ");
    }
    else
    {
        ESP_LOGE(TAG, "inv uart ini fail");
    }

    asw_inv_data_init(); //初始化逆变器数据

    while (1)
    {
        switch (task_state)
        {
        case TASK_IDLE:
            task_state = inv_task_schedule(&trans_fun, &p_trans_data);
            break;

        case TASK_QUERY_DATA: // send the query data command
            task_state = query_data_proc();
            break;

        case TASK_TRANS_UP_INV:
            task_state = upinv_transdata_proc(trans_fun, p_trans_data);
            break;

        case TASK_TRANS_SCAN_INV:
            task_state = scan_register(); //
            break;

        case CLEAR_METER_SET:
            task_state = clear_meter_setting(trans_fun);
            break;

        case TASK_QUERY_METER:
            task_state = query_meter_proc(0);
            break;

        case RESTART:
            task_state = upinv_transdata_proc(trans_fun, p_trans_data);
            break;

            // SSC
        case SEND_METER_DATA:
            task_state = ssc_send_meterdata_proc();
            break;

#if TRIPHASE_ARM_SUPPORT
        case TASK_ENERGE_METER_CONTROL:
            task_state = energy_meter_control_combox();
            break;
#endif
        default:
            task_state = TASK_IDLE;
            break;
        }

        //---- Eng.Stg.Mch ---//
        com_idle_work();
        setting_event_handler();

        vTaskDelay(10 / portTICK_PERIOD_MS); // usleep(10 * 1000);
    }
}
