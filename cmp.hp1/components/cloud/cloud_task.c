#include "cloud_task.h"

/*this thread is mqtt connect and upload*/

void send_inv_cld_status_check(void)
{
    static int8_t last_status = 0;
    int8_t curr_status = 0;

    if (g_num_real_inv <= 0)
    {
        printf("\n-------send_inv_cld_status_check------\n  ");
        printf(" the inv num is 0 .\n");
        return;
    }

    if (g_state_mqtt_connect == 0)
    {
        curr_status = 1;
    }

    if (last_status != curr_status)
    {
        g_task_inv_broadcast_msg |= MSG_BRDCST_DSP_ZV_CLD_INDEX;
        last_status = curr_status;
    }
}

void cloud_task(void *pvParameters)
{
    TDCEvent mqtt_state = dcIdle;
    // int lost_data_flag = 0;
    // int get_new_invdata = 0;
    // Inv_data inv_data = {0};

    // int tmp_state_publish_old = -1;

    uint8_t m_mqtt_connect_enable = -1;
    char json_msg[JSON_MSG_SIZE] = {0};

    // static uint8_t failed_num = 0;

    cloud_setup();

    sleep(10); // sleep(180);

    while (1)
    {
        if (g_stick_run_mode == Work_Mode_AP_PROV || (g_stick_run_mode == Work_Mode_LAN && get_eth_connect_status() != 0) || (g_stick_run_mode == Work_Mode_STA && get_wifi_sta_connect_status() != 0))
        {
            m_mqtt_connect_enable = 0;
            // failed_num = 0;

            mqtt_state = dcIdle;
        }
        else
        {
            m_mqtt_connect_enable = 1;

            ////// 当网络连接正常，服务器链接异常时，重新进行连接mqtt服务器 //////////  Mark  20220919+ //////////////////////
            // if (g_state_mqtt_connect == -1)
            // {
            //     if (failed_num++ > 30)
            //     {
            //         failed_num = 0;
            //         mqtt_state = dcalilyun_authenticate;
            //         printf("\n========================   clound task ============   mqtt connected failed reconnect to mqtt \n");
            //     }
            // }
            //////////////////////////////////////////////////////////////////////////////////////////////////////////
        }

        if (!update_inverter && m_mqtt_connect_enable) // v2.0.0 add   TODOs
        {

            switch (mqtt_state)
            {
            case dcIdle:
                mqtt_state = get_net_state();
                break;
            case dcalilyun_authenticate:
                mqtt_state = mqtt_connect();
                break;
            case dcalilyun_publish:
                // mqtt_state = mqtt_publish(get_new_invdata, inv_data, &lost_data_flag);
                mqtt_state = mqtt_publish(json_msg);
                break;
            default:
                break;
            }
        }

        /* Eng.Stg.Mch-Lanstick +- */
        /** 读取实时数据 **
         * 网络异常则存为历史数据(这里不读取历史数据)*/
        read_instant_payload(json_msg);
        cld_idle_work();
        send_inv_cld_status_check(); // for estore only

        vTaskDelay(10 / portTICK_PERIOD_MS); // usleep(50 * 1000);  [tgl change]
    }
}
