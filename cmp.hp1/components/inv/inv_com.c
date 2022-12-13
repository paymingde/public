#include "inv_com.h"
#include "asw_mqueue.h"
#include "asw_mutex.h"

#define INV_UART_TXD (GPIO_NUM_32) // (GPIO_NUM_2)//
#define INV_UART_RXD (GPIO_NUM_33) //[(GPIO_NUM_15)tgl mark] [32-tx 33-rx[调试临时用]]

#define ALL_UART_RTS (UART_PIN_NO_CHANGE)
#define ALL_UART_CTS (UART_PIN_NO_CHANGE)

#define BUF_SIZE (1024)
static const char *TAG = "inv_com.c";

#if TRIPHASE_ARM_SUPPORT
#define SCAN_ABORT_COUNT 21
#else
#define SCAN_ABORT_COUNT 15
#endif

Inv_arr_t inv_arr = {0};

static int64_t request_data_time = 0, read_meter_time = 0, send_meter_time = 0;

uint8_t g_num_real_inv = 0; //[tgl mark] g_g_num_real_inv 建议改成
int inv_comm_error = 0;
unsigned int inv_index = 0;
uint8_t g_monitor_state = 0; //[tgl mark]

sys_log inv_log = {0};
sys_log com_log = {0};
sys_log last_comm = {0};

int inv_com_reboot_flg = 0;
int now_read_inv_index = 0;
cloud_inv_msg trans_data;

//-- Eng.Stg.Mch ---//
Inv_arr_t ipc_inv_arr = {0};
Inv_arr_t cgi_inv_arr = {0};
Inv_arr_t cld_inv_arr = {0};

/* register begin */
const uint8_t clear_register_frame[] = {0xAA, 0x55, 0x01, 0x00, 0x00, 0x00, 0x10, 0x04, 0x00, 0x01, 0x14};
const uint8_t check_sn_frame[] = {0xAA, 0x55, 0x01, 0x00, 0x00, 0x00, 0x10, 0x07, 0x00, 0x01, 0x17};
const uint8_t set_addr_header[] = {0xAA, 0x55, 0x01, 0x00, 0x00, 0x00, 0x10, 0x08, 0x21};
/***********************************************/

//--------------------------------------//
int check_inv_states()
{
    uint8_t i = 0, j = 0;
    for (i = 0; i < g_num_real_inv; i++)
    {
        if (inv_arr[i].status == 1)
            j++;
    }

    if (j >= g_num_real_inv)
        return 1; // led_green_on();

    if (j < 1)
        return -1; // led_green_off();

    if (j < g_num_real_inv && j > 0)
        return 0; // green_fast_blink();
    return j;
}

//----------------------------------------------//
int8_t serialport_init(UART_TYPE type)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    int baud_rate;
    int uart_port;
    int uart_tx;
    int uart_rx;
    int res = -1;

    switch (type)
    {
    case INV_UART:
    {
#if TRIPHASE_ARM_SUPPORT
        baud_rate = 115200; // 9600; // MAKR-TGL
#else
        baud_rate = 9600; // MAKR-TGL
#endif
        uart_port = UART_NUM_1;

        uart_tx = INV_UART_TXD; // [tgl mark] 引脚号需要根据最新的硬件修改
                                // （UART0 的引脚应该是固定的不能修改）
        uart_rx = INV_UART_RXD;
    }
    break;

    default:
    {
        ESP_LOGE(TAG, " the type is error ");
        return ASW_FAIL;
    }
    break;
    }

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_param_config(uart_port, &uart_config);
    uart_set_pin(uart_port, uart_tx, uart_rx, ALL_UART_RTS, ALL_UART_CTS);

    /* 消息 [tgl mark] */
    res = uart_driver_install(uart_port, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (res == ESP_OK)
        return ASW_OK;
    return ASW_FAIL;
}

//----------------------------------------------------//
uint8_t load_reg_info(void)
{
    Inv_reg_arr_t reg_info_arr = {0}; // nvs_invert_db
    int i;
    int ret;

    uint8_t flag_write_para = 0;

    printf("\n----------- Load Reg Info -----------------\n");

#if PARALLEL_HOST_SET_WITH_SN
    MonitorPara mt = {0};
    read_global_var(METER_CONTROL_CONFIG, &mt);

#endif

    ret = general_query(NVS_INVTER_DB, &reg_info_arr);

    if (ASW_FAIL == ret)
    {
        ESP_LOGE("--load_reg_info Erro --", "Failed to query NVS_INVTER_DB");
        return 0;
    }

    Bat_Monitor_arr_t m_bat_monitor_para = {0};
    // uint64_t mPeriod = 0;
    // int ret1 = general_query(NVS_CONFIG, &m_bat_monitor_para);
    // Bat_Monitor_arr_t monitor_para = {0};

    read_global_var(PARA_CONFIG, &m_bat_monitor_para);

    // mPeriod = get_msecond_sys_time();

    for (i = 0; i < INV_NUM; i++)
    {
        if (reg_info_arr[i].modbus_id < 3) // START_ADD
        {
            ESP_LOGW("--- load_reg_info WARN ---", "num:%d reg info arr modbus id < 3", i - 1);
            break;
        }
        memcpy(&inv_arr[i].regInfo, &reg_info_arr[i], sizeof(InvRegister));

        Bat_arr_t mBatsArryDatas = {0};
        read_global_var(GLOBAL_BATTERY_DATA, &mBatsArryDatas);

        memcpy(mBatsArryDatas[i].sn, reg_info_arr[i].sn, sizeof(reg_info_arr[i].sn));
        mBatsArryDatas[i].modbus_id = reg_info_arr[i].modbus_id;
        write_global_var(GLOBAL_BATTERY_DATA, &mBatsArryDatas);

//-------------------------------------------//
#if PARALLEL_HOST_SET_WITH_SN

        if (strcmp(reg_info_arr[i].sn, mt.host_psn) == 0)
        {
            g_host_modbus_id = reg_info_arr[i].modbus_id;
        }
#endif

        ////////////////////////////////////// for set battery inf0 //////////////////////////

        /*  FOR TEST MultilInv  */
        uint8_t flag_monitor_get = 0;

        // if (ret1 != ASW_FAIL)
        // {
        ASW_LOGI("\n---------  reg_info_arr[%d].sn:%s,  m_bat_monitor_para[%d].sn:%s  -----------  [%d]\n", i, reg_info_arr[i].sn, i,
                 m_bat_monitor_para[i].sn, m_bat_monitor_para[i].modbus_id);
        if (strcmp(reg_info_arr[i].sn, m_bat_monitor_para[i].sn) == 0)
        {
            flag_monitor_get = 1;
            ASW_LOGI("\n=========   dc_per:%d,uu1:%d ,num:%d,mod_b:%d,sta_b:%d===", m_bat_monitor_para[i].batmonitor.dc_per, m_bat_monitor_para[i].batmonitor.uu1,
                     m_bat_monitor_para[i].batmonitor.up2, m_bat_monitor_para[i].batmonitor.uu2, m_bat_monitor_para[i].batmonitor.uu4);
        }
        // }
        // }

        m_bat_monitor_para[i].modbus_id = reg_info_arr[i].modbus_id;

        if (flag_monitor_get == 0)
        {
            memcpy(m_bat_monitor_para[i].sn, reg_info_arr[i].sn, sizeof(reg_info_arr[i].sn));

            m_bat_monitor_para[i].batmonitor.dc_per = 0; //!<  BEAST type

            m_bat_monitor_para[i].batmonitor.uu1 = 0; //!< running mode
            m_bat_monitor_para[i].batmonitor.up1 = 0; //!< manufactory
            m_bat_monitor_para[i].batmonitor.uu2 = 0; //!< battery model
            m_bat_monitor_para[i].batmonitor.up2 = 0; //!< battery number

            /** inverter operation*/
            m_bat_monitor_para[i].batmonitor.uu3 = 0;    //!< offline for DRM
            m_bat_monitor_para[i].batmonitor.up3 = 0;    //!< active power percent for DRM
            m_bat_monitor_para[i].batmonitor.uu4 = 0;    //!< power on/off
            m_bat_monitor_para[i].batmonitor.up4 = 0xAF; //!< first power on

            /** inverter charging power*/
            m_bat_monitor_para[i].batmonitor.freq_mode = 1; //!< 1.2.3 stop/charging/discharging
            // tmp_bat_monitor_para[i].batmonitor.fq_t1 = 3000;  //!< charging power
            // tmp_bat_monitor_para[i].batmonitor.ov_t1 = 3000;  //!< discharging power

            /** DRM setting*/
            m_bat_monitor_para[i].batmonitor.active_mode = 0; // enable drm
            m_bat_monitor_para[i].batmonitor.fq_ld_sp = 0;    //!<  q value;

            m_bat_monitor_para[i].batmonitor.dchg_max = 100; // enable drm
            m_bat_monitor_para[i].batmonitor.chg_max = 10;   //!<  q value;
        }

        ASW_LOGI("load inv_arr modbus id: %02x, sn: %s ;  m_bat_monitor[%d].modbus_id[%d],sn[%s]\n",
                 inv_arr[i].regInfo.modbus_id, inv_arr[i].regInfo.sn,
                 i, m_bat_monitor_para[i].modbus_id, m_bat_monitor_para[i].sn);
    }

    //////////////////////////////////////////////////////////////

    // printf("\n---------------   TEST LOAD INV INFO PERIOD ----------------\n");
    // printf(" period:%lld", get_msecond_sys_time() - mPeriod);
    // printf("\n---------------   TEST LOAD INV INFO PERIOD ----------------\n");
    if (i > 1)
    {
        write_global_var_to_nvs(PARA_CONFIG, &m_bat_monitor_para);

        ASW_LOGI("--- Load inv info ro battery monitor ---\n");
    }

    // write_global_var_to_nvs(PARA_SCHED_BAT, &tmp_bat_schedule_bat);

    ESP_LOGI(TAG, "inv num %d \n", i);
    return i;
}
//-__---------------------------------------------------//
int8_t get_modbus_id(uint8_t *id)
{
    // int scan_id;
    uint8_t i;

    if (!inv_arr[0].regInfo.modbus_id)
    {
        *id = START_ADD;
        return ASW_OK;
    }

    for (i = 0; i < INV_NUM; i++)
    {
        if (inv_arr[i].regInfo.modbus_id == 0 && inv_arr[i - 1].regInfo.modbus_id >= START_ADD)
            break;
    }

    if (inv_arr[i - 1].regInfo.modbus_id < 246)
    {
        *id = inv_arr[i - 1].regInfo.modbus_id + 1;
        ASW_LOGI("next id %d \n", *id);
        return ASW_OK;
    }
    else
        return ASW_FAIL;
}

//--------------------------------------------------//
int8_t get_sum(uint8_t *buf, int len, uint8_t *high_byte, uint8_t *low_byte)
{
    uint16_t sum = 0;
    uint8_t i = 0;
    for (i = 0; i < len; i++)
        sum += buf[i];
    *high_byte = (sum >> 8) & 0xFF;
    *low_byte = sum & 0xFF;
    ASW_LOGI("high low: %02X %02X\n", *high_byte, *low_byte);
    return ASW_OK;
}
//--------------------------------------------------//
int8_t check_sum(uint8_t *buf, int len)
{
    uint8_t h_byte = 0;
    uint8_t l_byte = 0;
    get_sum(buf, len - 2, &h_byte, &l_byte);

    if (h_byte == buf[len - 2] && l_byte == buf[len - 1])
        return ASW_OK;
    else
    {
        return ASW_FAIL;
    }
}
//------------------------------------------//
int8_t parse_sn(uint8_t *buf, int len, char *psn)
{
    if (check_sum(buf, len) != 0 || len != 45)
    {
        ESP_LOGE(TAG, "check sum error\n");
        return ASW_FAIL;
    }
    memcpy(psn, buf + 9, 32);
    return ASW_OK;
}

//---------------------------------------------------//
int chk_msg(cloud_inv_msg *trans_data)
{
    cloud_inv_msg buf = {0};
    char ws[512] = {0};

    if (mq1 == NULL)
    {
        ESP_LOGE(TAG, "ERrro: mq1 is null");
        return -1;
    }

    if (xQueueReceive(mq1, &buf, 0) == pdFALSE)
    {

        if (to_inv_fdbg_mq != NULL)
        {

            if (xQueueReceive(to_inv_fdbg_mq, &buf, 0) == pdFALSE) // cgi和inv之间，通过2个消息队列实现双向通信*/
            {
                return -1;
            }
        }
    }
    ASW_LOGW("DEBUG_INFO: chk_msg recv buf have data type:%d", buf.type);

    if (2 == buf.type)
    {
        memcpy(trans_data->data, buf.data, sizeof(buf.data));
        trans_data->len = buf.len;

        memcpy(ws, &trans_data->data[trans_data->len], strlen(trans_data->data)); //[tgl mark] ???

        ASW_LOGI("recvmsg %d %d  %d  %d %s\n", sizeof(buf), trans_data->len,
                 strlen(trans_data->data), trans_data->len, ws);
        return 2;
    }
    else if (3 == buf.type)
    {

        memcpy(trans_data->data, buf.data, sizeof(buf.data));
        trans_data->len = buf.len;
        trans_data->ws = buf.len;

        ASW_LOGI("recv msg---%s--%d--\n", buf.data, buf.len);
        ASW_LOGI("mstyp %d --%s-- %c-- %d --%s--\n", buf.type,
                 buf.data, buf.ws, trans_data->len, trans_data->data);

        return 3;
    }
    else if (5 == buf.type)
    {
        trans_data->ws = buf.data[0];
        ASW_LOGI("mstyp %d %d %d %d %d\n", buf.type, buf.data[0], buf.data[1], buf.data[2], buf.ws);
        return 5;
    }
    else if (6 == buf.type)
    {
        ASW_LOGI("mstyp clear meter %d %s \n", buf.type, buf.data);

        return 6;
    }
    else if (99 == buf.type)
    {

        memcpy(trans_data->data, buf.data, sizeof(buf.data));
        trans_data->len = buf.len;
        ASW_LOGI("reboot monitor or inverter %d %d %s \n", buf.type, buf.data[0], &buf.data[1]);
        return 99;
    }
    else if (30 == buf.type)
    {
        memcpy(trans_data->data, buf.data, sizeof(buf.data));
        trans_data->len = buf.len;
        trans_data->ws = buf.len;
        ASW_LOGI("recv cgi msg update---%s--%d--\n", buf.data, buf.len);
        ASW_LOGI("mstyp %d --%s-- %c-- %d --%s--\n", buf.type, buf.data,
                 buf.ws, trans_data->len, trans_data->data);

        return 30;
    }
    else if (50 == buf.type)
    {

        memcpy(trans_data->data, buf.data, sizeof(buf.data));
        trans_data->len = buf.len;
        trans_data->ws = buf.len;
        ASW_LOGI("recv cgi msg scan---%s--%d--\n", buf.data, buf.len); //[tgl mark] data读取不到值
        ASW_LOGI("mstyp %d --%s-- %c-- %d --%s--\n", buf.type, buf.data,
                 buf.ws, trans_data->len, trans_data->data);

        return 50;
    }
    else if (20 == buf.type)
    {
        memcpy(trans_data->data, buf.data, sizeof(buf.data));
        trans_data->len = buf.len;
        memcpy(ws, &trans_data->data[trans_data->len], strlen(trans_data->data)); //-trans_data->len);

        ASW_LOGI("cgi trans recvmsg %d %d  %d  %d %s\n", sizeof(buf),
                 trans_data->len, strlen(trans_data->data), trans_data->len, ws);

        return 20;
    }
    else if (60 == buf.type)
    {
        ASW_LOGI("mstyp clear meter %d %s \n", buf.type, buf.data);

        return 60;
    }
    return -1;
}

//-----------------------------------------------------//
void reg_info_log(void)
{
    uint8_t i;
    ASW_LOGI("scan result:\n");
    for (i = 0; i < INV_NUM; i++)
    {
        if (inv_arr[i].regInfo.modbus_id != 0)
        {
            ASW_LOGI("modbus id: %02x, sn: %s\n",
                     inv_arr[i].regInfo.modbus_id, inv_arr[i].regInfo.sn);
        }
    }
}

//---------------------//
// [update] add
int recv_bytes_frame_meter(int fd, uint8_t *res_buf, uint16_t *res_len)
{
    char buf[500] = {0};
    int len = 0;
    int nread = 0;
    int timeout_cnt = 0;
    int i = 0;
    uint16_t max_len = *res_len;
    uint16_t crc = 0;
    ASW_LOGI("maxlen---------------------------------%d------\n", max_len);
    while (1)
    {
        usleep(50000);
        timeout_cnt++;
        if (timeout_cnt >= 60)
        {
            return -1;
        }
        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);

        if (nread > 0)
        {
            len += nread;
            // #if DEBUG_PRINT_ENABLE
            if (g_asw_debug_enable > 1)
            {
                for (i = 0; i < len; i++)
                    printf("*%02X* ", buf[i]);
            }
            // #endif
            break;
        }
    }

    if (len > 2)
    {
        crc = crc16_calc((uint8_t *)buf, len);
        ASW_LOGI("mmm crc %d \n", crc);
        if (crc == 0)
        {
            memcpy(res_buf, buf, len);
            *res_len = len;
            goto end_read;
        }
    }

    if (len == max_len)
    {
        crc = crc16_calc(res_buf, *res_len);
        ASW_LOGI("MMM crc %d \n", crc);
        if (crc == 0)
        {
            memcpy(res_buf, buf, len);
            *res_len = len;
            goto end_read;
        }
    }

    if (len > max_len + 5)
    {
        ASW_LOGI("maxlen-----------%d---%d---\n", max_len, len);
        return -1;
    }

    while (1)
    {
        usleep(50000);
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        if (nread > 0)
        {
            len += nread;
            // #if DEBUG_PRINT_ENABLE
            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X* ", buf[i]);
            }
            // #endif
        }
        else
        {
            if (len > max_len + 5)
            {
                ASW_LOGI("maxlen-----------%d---%d---\n", max_len, len);
                return -1;
            }
            // end_read:
            memcpy(res_buf, buf, len);
            *res_len = len;
            break;
            // return 0;
        }
    }
    crc = crc16_calc(res_buf, *res_len);

end_read:
    ASW_LOGI("recvv ok %x  %x %d \n", crc, ((res_buf[*res_len - 2] << 8) | res_buf[*res_len - 1]), len);
    if (len > 1 && 0 == crc) //((res_buf[*res_len-2]<<8)|res_buf[*res_len-1]) == crc)
        return 0;
    else
        return -1;
}
//----------------------------------------------------//
int8_t recv_bytes_frame_waitting_nomd(int fd, uint8_t *res_buf, uint16_t *res_len)
{
    char buf[500] = {0};
    int len = 0;
    int nread = 0;
    int timeout_cnt = 0;
    uint8_t i = 0;

    while (1)
    {
        usleep(30000);
        timeout_cnt++;
        if (timeout_cnt >= 50)
        {
            return ASW_FAIL;
        }
        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        if (nread > 0)
        {
            len += nread;
            // #if DEBUG_PRINT_ENABLE
            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X", buf[i]); //
            }

            // #endif
            break;
        }
    }

    while (1)
    {
        usleep(50000);
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        ASW_LOGI("recvvv ****************************%d \n", nread);
        if (nread > 0)
        {
            len += nread;
            // #if DEBUG_PRINT_ENABLE
            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X", buf[i]);
            }

            // #endif
        }
        else
        {
            ASW_LOGI("recvvv ***** \n");
            memcpy(res_buf, buf, len);
            *res_len = len;
            return ASW_OK;
        }
    }
    ASW_LOGI("recvvv -1 \n");
    *res_len = 0;
    return ASW_FAIL;
}
//------------------------------------------------//
int8_t legal_check(char *text)
{

    if (strlen(text) > 6)
        return ASW_OK;
    else
        return ASW_FAIL;
}

//--------------------------------------------------//
SERIAL_STATE inv_task_schedule(char *func, cloud_inv_msg *data)
{
    int64_t period = 0;
    int res;

    // Eng.Stg.Mch-Lanstick +
    static uint8_t new_sync_fsm = 0;

    /*query data from inverter every 10 seconds*/
    period = (get_second_sys_time() - request_data_time);

    res = chk_msg(data);

    if (2 == res)
    {
        *func = 2;
        return TASK_TRANS_UP_INV;
    }
    else if (3 == res)
    {
        *func = 3;
        return TASK_TRANS_UP_INV;
    }
    else if (5 == res)
    {
        *func = 5;
        return TASK_TRANS_SCAN_INV;
    }
    else if (6 == res)
    {
        *func = 6;
        return CLEAR_METER_SET;
    }
    else if (99 == res) // reboot
    {
        *func = 99;
        return RESTART;
    }
    else if (30 == res)
    {
        *func = 30;
        return TASK_TRANS_UP_INV;
    }
    else if (50 == res)
    {
        *func = 50;
        return TASK_TRANS_SCAN_INV;
    }
    else if (20 == res)
    {
        *func = 20;
        return TASK_TRANS_UP_INV;
    }
    else if (60 == res)
    {
        *func = 60;
        return CLEAR_METER_SET;
    }

    // Eng.Stg.Mch-lanstick  +
    int cycle = 10;     // 60
    int cycle_ssc = 15; // 60
    // if (strlen(inv_arr[0].regInfo.mode_name) <= 0)  //[MARK MultiInv -]
    //     cycle = 3;

    /**
     * @brief
     * 每10s,  调用TASK_QUERY_DATA
     * 每390ms,调用TASK_QUERY_METER  //没有来自云端的数据信息会到这里
     */

    if (period >= cycle) // period >= QUERY_TIMING * 1000000 || period < 0)
    {
        request_data_time = get_second_sys_time(); // esp_timer_get_time();
        ASW_LOGI("read inv data time sec: %lld\n", request_data_time);
        return TASK_QUERY_DATA;
    }

    // printf("\n-----  [tgl debug print] ----\n new_sync fsm  : %d \n", new_sync_fsm);

    // Eng.Stg.Mch-Lantick +
    switch (new_sync_fsm)
    {
    case 0:
        if (event_group_0 & SYNC_ALL_ONCE)
        {
            if (g_state_mqtt_connect == 0)
            {
                sleep(4);
                event_group_0 &= ~SYNC_ALL_ONCE;
                clear_all_sync_info();
                new_sync_fsm = 1;
            }
        }
        break;
    case 1:
        if (is_all_info_sync_ok() == 1)
        {
            event_group_0 |= SEND_INV_DATA_ONCE;
            request_data_time = get_second_sys_time();
            new_sync_fsm = 0;
            printf("\n------   debug TASK_QUERY_DATA------\n");
            return TASK_QUERY_DATA;
        }
        break;
    default:
        break;
    }

    if (event_group_0 & PWR_REG_SOON_MASK) /** immdiately once*/
    {
        event_group_0 &= ~PWR_REG_SOON_MASK;
        read_meter_time = get_msecond_sys_time();
#if TRIPHASE_ARM_SUPPORT
        //在cgi或cloud接收到设置电表的命令后，不再由query_meter处理，而是直接由energy_meter_control_combox()函数处理
        return TASK_ENERGE_METER_CONTROL;
#else
        return TASK_QUERY_METER;
#endif
    }
    else
    {
        if (get_msecond_sys_time() - read_meter_time > 390) // 390 ms
        {
            read_meter_time = get_msecond_sys_time();
            return TASK_QUERY_METER;
        }
    }

    ///////////////// SSC ENABLE ////////////////
    if (g_ssc_enable && get_second_sys_time() - send_meter_time > 15) // 15s
    {
        send_meter_time = get_second_sys_time();

        return SEND_METER_DATA;
    }

    return TASK_IDLE;
}

//----------------------------------------------------------------//
int inv_com_log(int index, int err)
{
    char buf[300] = {0};
    char sts[16] = {0};
    char tmp[160] = {0};
    // uint8_t j = 0;

    if (strlen(com_log.tag) == 0)
        memcpy(com_log.tag, "\r\nComm error\r\n", strlen("\r\nComm error\r\n"));

    if (err == -1)
        sprintf(sts, "%s", "err");

    if (err == -2)
        sprintf(sts, "%s", "out");

    if (strlen(inv_arr[index - 1].regInfo.sn) < 1)
        sprintf(buf, "%s,%s\r\n", "onyle one inverter", sts);
    else
        sprintf(buf, "%s,%s\r\n", inv_arr[index - 1].regInfo.sn, sts);

    if (com_log.index + strlen(buf) > 329)
    {

        if (com_log.index > 260)
            memcpy(tmp, &com_log.log[100], 160);
        else if (com_log.index > 100 && com_log.index < 261)
            memcpy(tmp, &com_log.log[100], com_log.index - 100);

        memset(com_log.log, 0, sizeof(com_log.log));
        com_log.index = 0;

        memcpy(com_log.log, tmp, strlen(tmp));
        com_log.log[160] = '\n';
        memcpy(&com_log.log[161], buf, strlen(buf));

        com_log.index += (161 + strlen(buf));
        // printf("========================new index---- %d \n", com_log.index);
    }
    else
    {
        memcpy(&com_log.log[com_log.index], buf, strlen(buf));
        com_log.index += strlen(buf);
    }

    if (com_log.index > 330)
    {
        memset(com_log.log, 0, sizeof(com_log.log));
        com_log.index = 0;
    }
    return 0;
}

//--------------------------------------------------------------------//
int internal_inv_log(int typ)
{
    char buf[300] = {0};
    int j = 0;

    if (strlen(inv_log.tag) == 0)
    {
        memcpy(inv_log.tag, "\r\nList invs\r\n", strlen("\r\nList invs\r\n"));
        if (typ > 0)
        {
            for (int i = 0; i < typ && j < 200; i++)
            {
                memcpy(&buf[j], inv_arr[i].regInfo.sn, strlen(inv_arr[i].regInfo.sn));
                j += strlen(inv_arr[i].regInfo.sn);
                buf[j] = ',';
                buf[j + 1] = inv_arr[i].regInfo.modbus_id + 0x30; //[tgl udpate]
                buf[j + 1 + 1] = '\n';
                j += 3;
                printf("buf%d %s \n", j, buf);
            }
        }
        else
        {
            if (inv_arr[0].regInfo.modbus_id == 0x03)
                memcpy(buf, "only one inverter modbus_id 3\n", strlen("only one inverter modbus_id 3\n"));
            else
            {
                memcpy(buf, "only one inverter modbus_id err", strlen("only one inverter modbus_id err"));
                buf[strlen("only one inverter modbus_id err")] = inv_arr[0].regInfo.modbus_id;
                buf[strlen("only one inverter modbus_id err") + 1] = '\n';
            }
        }
        inv_log.index = strlen(buf);
        memcpy(inv_log.log, buf, strlen(buf));
    }

    ASW_LOGI("inv_log_enable-- %s \n", inv_log.tag);
    return 0;
}
//--------------------------------------------------------------//
int last_com_data(unsigned int index)
{
    int datlen;
    uint8_t i;
    Inv_data invdata = {0};
    if (strlen(last_comm.tag) == 0)
        memcpy(last_comm.tag, "\r\nLast comm\r\n", strlen("\r\nLast comm\r\n"));

    char payload[330] = {0};

    memset(last_comm.log, 0, sizeof(last_comm.log));
    memcpy(&invdata, &cgi_inv_arr[index - 1].invdata, sizeof(Inv_data)); //[tgl update]

    datlen = sprintf(payload, "%s %s,%d,%d hz,%d w,%d err,%d pf,%d cf,",
                     invdata.time, invdata.psn, cgi_inv_arr[index - 1].regInfo.modbus_id,
                     invdata.fac, invdata.pac, invdata.error,
                     invdata.cosphi, invdata.col_temp);

    for (i = 0; i < 10; i++)
    {
        if ((invdata.PV_cur_voltg[i].iVol != 0xFFFF) && (invdata.PV_cur_voltg[i].iCur != 0xFFFF))
        {
            datlen += sprintf(payload + datlen, "v%d:%d,i%d:%d;",
                              i + 1, invdata.PV_cur_voltg[i].iVol,
                              i + 1, invdata.PV_cur_voltg[i].iCur);
        }
    }
    last_comm.index = strlen(payload);
    memcpy(last_comm.log, payload, strlen(payload));

    return 0;
}

/*----------------------------------------------------------------------------*/
/*! \brief  This function create the com comunication task.
 *  \param  initial_data      [in], The task's data.
 *  \return none.
 *  \see
 *  \note
----------------------------------------------------------------------------*/
SERIAL_STATE query_data_proc()
{

    struct tm currtime = {0};
    time_t t = time(NULL);
    unsigned int save_time = 0;

    int res = -1;

    Inverter *curr_inv_ptr = NULL;

    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;
    currtime.tm_mon += 1;

    if (!g_num_real_inv)
    {
        ASW_LOGI("-----------  g_num_inv is 0 --------------------\n");
        g_num_real_inv = load_reg_info();
        if (g_num_real_inv > 0)
            internal_inv_log(g_num_real_inv);
    }

    if (g_num_real_inv < 1)
    {
        ESP_LOGI(TAG, " only one inverter default add 3 \n");
        inv_arr[0].regInfo.modbus_id = 0x03;
        g_num_real_inv = 1;
        internal_inv_log(0);
    }

    if (inv_index >= g_num_real_inv)
        inv_index = 0;

    curr_inv_ptr = &inv_arr[inv_index++];

    flush_serial_port(UART_NUM_1);

    /****************************************************************
     * 1. @brief 从云端时间的同步完成后，g_monitor_state 的
     *        SYNC_TIME_FROM_CLOUD_INDEX置位，
     *        同时检测regInfo.syn_time状态是否为0，如两个条件满足，
     *        开始发送时间到inv *
     * @param
     ****************************************************************/
    if (g_monitor_state & SYNC_TIME_FROM_CLOUD_INDEX)
    {
        if (curr_inv_ptr->regInfo.syn_time == 0)
        {
            ASW_LOGW("DEBUG: Sync time from cloud");
            if (md_write_data(curr_inv_ptr, CMD_MD_WRITE_INV_TIME) == ASW_OK)
            {
                inv_arr[inv_index - 1].regInfo.syn_time = 1;
                ASW_LOGW("DEBUG: Sync time to inv %d OK!!", inv_index);
            }
        }
    }

    /****************************************************************
     * 2. @brief  g_monitor_state==0时调用（上电后第一次调用）
     *          从逆变器读取时间 *  stick与逆变器时间同步（上电会执行）
     * @param
     ****************************************************************/
    ASW_LOGI("read time A %d %d \n", g_monitor_state, curr_inv_ptr->status);
    if (!g_monitor_state)
    {
        ASW_LOGI("read time B  %d %d \n", g_monitor_state, curr_inv_ptr->status);
        if (curr_inv_ptr->status)
        {

            if (ASW_OK == (md_read_data(curr_inv_ptr, CMD_MD_READ_INV_TIME, NULL, NULL))) //
            {
                g_monitor_state |= SYNC_TIME_FROM_INV_INDEX;
            }
        }
    }
    /* read data from inverter*/ //&& g_monitor_state == SYNC_TIME_END
    ASW_LOGI("read inv data start... %d  %d %s\n",
             curr_inv_ptr->regInfo.modbus_id,
             inv_arr[inv_index - 1].last_save_time,
             inv_arr[inv_index - 1].regInfo.sn);

    // ESP_LOGW(TAG, "---TGL PRINT TODO update component...");

    /****************************************************************
     * 3. @brief  周期性调用（10s），获取逆变器数据
     *
     * @param
     ****************************************************************/
    res = md_read_data(curr_inv_ptr, CMD_READ_INV_DATA,
                       &inv_arr[inv_index - 1].last_save_time,
                       &inv_arr[inv_index - 1].last_error);

    // printf("\n-------------------- query data proc--------------\n");
    // printf("res:%d", res);
    // printf("\n-------------------- query data proc--------------\n");
    if (ASW_OK == res)
    {
        /* Notice:This is very important,if you want change the inv's real data */

        //如果读数据成功，则再次读出逆变器信息并标记“上线”和“同步”标志
        /*check the status of inverter online or offline*/
        inv_arr[inv_index - 1].status = 1; // curr_inv_ptr->status=1;

        save_time = inv_arr[inv_index - 1].regInfo.syn_time;

        ASW_LOGI("mode_name %s \n", inv_arr[inv_index - 1].regInfo.mode_name);

        /****************************************************************
         * @brief 信息错误时，重新读取info *
         ****************************************************************/
        // if (strlen(inv_arr[inv_index - 1].regInfo.mode_name) < 10)
        // if (strlen(inv_arr[inv_index - 1].regInfo.mode_name) < 1)
        if (strlen(inv_arr[inv_index - 1].regInfo.msw_ver) <= 1) // mar by tgl
        {

            if (ASW_OK == (md_read_data(curr_inv_ptr, CMD_MD_READ_INV_INFO, &inv_index, (uint16_t *)&save_time)))
            {
                ESP_LOGI(TAG, "INV INFO READ \n");
            }
            else
            {
                ESP_LOGE(TAG, "current inverter point is nULL or current inverter's serial number is Unknow!!\r\n!!");
            }
        }

        inv_arr[inv_index - 1].regInfo.syn_time = save_time;
        last_com_data(inv_index);
    }
    else
    {
        inv_com_log(inv_index, res);
        inv_comm_error++;
        inv_arr[inv_index - 1].status = 0; // curr_inv_ptr->status=0;
        // ESP_LOGE(TAG, "read inv data is error\r\n");
    }

    return TASK_IDLE;
}
//---------------------------------------------------//
int8_t recv_bytes_frame_transform(int fd, uint8_t *res_buf, uint16_t *res_len)
{
    uint8_t buf[500] = {0};
    int len = 0;
    int nread = 0;
    int timeout_cnt = 0;
    int i = 0;
    uint16_t max_len = *res_len;
    uint16_t crc = 0;
    ASW_LOGI("maxlen--%d---\n", max_len);

    while (1)
    {
        usleep(50000);
        timeout_cnt++;
        if (timeout_cnt >= 60)
        {
            return ASW_FAIL;
        }
        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);

        if (nread > 0)
        {
            len += nread;
            // #if DEBUG_PRINT_ENABLE
            if (g_asw_debug_enable > 1)
            {
                for (i = 0; i < len; i++)
                    printf("*%02X* ", buf[i]);
            }
            // #endif
            break;
        }
    }

    if (len > 2)
    {
        crc = crc16_calc(buf, len);
        ASW_LOGI("ffff crc %d \n", crc);
        if (crc == 0)
        {
            memcpy(res_buf, buf, len);
            *res_len = len;
            goto end_read;
        }
    }

    if (len == max_len)
    {
        crc = crc16_calc(res_buf, *res_len);
        ASW_LOGI("XXX crc %d \n", crc);
        if (crc == 0)
        {
            memcpy(res_buf, buf, len);
            *res_len = len;
            goto end_read;
        }
    }

    if (len > max_len + 5)
    {
        ASW_LOGI("maxlen------%d---%d---\n", max_len, len);
        return ASW_FAIL;
    }

    while (1)
    {
        usleep(50000);
        timeout_cnt++;
        if (timeout_cnt >= 60)
            break;

        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        if (nread > 0)
        {
            len += nread;
            break;
        }
    }

    if (len > max_len + 5)
    {
        ASW_LOGI("maxlen-----%d---%d---\n", max_len, len);
        return ASW_FAIL;
    }

    while (1)
    {
        usleep(50000);
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        if (nread > 0)
        {
            len += nread;
            // #if DEBUG_PRINT_ENABLE
            if (g_asw_debug_enable > 1)
            {

                for (; i < len; i++)
                    printf("*%02X* ", buf[i]);
            }
            // #endif
        }
        else
        {
            if (len > max_len + 5)
            {
                ASW_LOGI("maxlen----%d---%d---\n", max_len, len);
                return ASW_FAIL;
            }
            // end_read:
            memcpy(res_buf, buf, len);
            *res_len = len;
            break;
        }
    }
    crc = crc16_calc(res_buf, *res_len);

end_read:
    ASW_LOGI("recvv ok %x  %x %d ",
             crc, ((res_buf[*res_len - 2] << 8) | res_buf[*res_len - 1]), len);
    if (len > 1 && 0 == crc)
        return ASW_OK;
    else
        return ASW_FAIL;
}

//-----------------------------------------------------------------//
void flush_serial_port(uint8_t UART_NO)
{
    uart_wait_tx_done(UART_NO, 0);
    uart_flush(UART_NO); // uart_tx_flush(0); [tgl mark]
    uart_flush_input(UART_NO);
}

//-------------------------------------------------------------//
//-------------------------------------------------------------//

int8_t clear_inv_list(void)
{
    memset(&inv_arr, 0, sizeof(Inv_arr_t));
    clear_cloud_invinfo(); //

    g_num_real_inv = 0; // will reload register later  //tgl mark will need to del???
    inv_index = 0;
    memset(&inv_log, 0, sizeof(sys_log));
    memset(&com_log, 0, sizeof(sys_log));
    memset(&last_comm, 0, sizeof(sys_log));
    request_data_time = get_second_sys_time();
    read_meter_time = get_msecond_sys_time();
    return ASW_OK;
}

//----------------------------------------------------------------//
int8_t parse_modbus_addr(uint8_t *buf, int len, uint8_t *addr)
{
    if (check_sum(buf, len) != ASW_OK || len != 12)
        return ASW_FAIL;
    memcpy(addr, buf + 3, 1);
    *addr = buf[3];
    return ASW_OK;
}

//-----------------------------------------------------------------//

SERIAL_STATE upinv_transdata_proc(char func, cloud_inv_msg tans_data)
{
    /** 是否透传电表*/
    // int8_t is_fdbg_meter = 0;
    int i = 0;
    uint16_t data_len = 0;
    unsigned char read_data[300] = {0}, buff_data[300] = {0}, ws[65] = {0};
    int byteLen;
    fdbg_msg_t fdbg_msg;

    // #if DEBUG_PRINT_ENABLE
    if (g_asw_debug_enable > 1)
    {
        printf("org---->\n");
        for (i = 0; i < 32; i++)
            printf(" %d:<%0x2X> ", i, tans_data.data[i]);
        printf("\n");
    }

    // #endif
    if (strlen(tans_data.data) - tans_data.len > 64)
        memcpy(ws, &tans_data.data[tans_data.len], 64);
    else
        memcpy(ws, &tans_data.data[tans_data.len], strlen(tans_data.data) - tans_data.len);

    ASW_LOGI("rec uuu ws>  %d %d  %d  %d %s\n", func, tans_data.len, strlen(tans_data.data), tans_data.len, ws);

    uart_wait_tx_done(UART_NUM_1, 0);
    // uart_tx_flush(0); [tgl mark]  没有这个函数 替换为uart_flush(0)
    uart_flush(UART_NUM_1);
    uart_flush_input(UART_NUM_1);

    if (2 == func || 20 == func)
    {
        byteLen = StrToHex(read_data, tans_data.data, (int)tans_data.len);

        uart_write_bytes(UART_NUM_1, (const char *)read_data, byteLen);
        /////////////////////////////////////////
        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("--S--", "send to inv fbd ...\n");
            for (uint8_t i = 0; i < byteLen; i++)
                printf("<%02X> ", read_data[i]);
            printf("\n");
        }

        /////////////////////////////////////////////////
        ASW_LOGI("transdata ");
    }

    if (3 == func || 30 == func)
    {
        ESP_LOGW(TAG, "update inverter fw ---%s--- \n ", tans_data.data);
#if !TRIPHASE_ARM_SUPPORT
        inv_update(tans_data.data); //[tgl mark]

#else
        inv_update(trans_data.slave_id, tans_data.data);
#endif
        // ESP_LOGW(TAG, "---TGL DEBUG PRINTF inv_updata in update/update.c ");
    }

    if (99 == func)
    {
        ASW_LOGI("restart machine data ---%d--- \n ", tans_data.data[0]);
        sleep(2);
        if (tans_data.data[0] == 0 || tans_data.data[0] == 1)
        {
            inv_com_reboot_flg = 1; // reboot inverter
        }
    }

    /** 计算预期返回长度*/
    data_len = read_data[5] * 2 + 5; //[tgl mark ] ？？？如果 func不是2/20 的条件下，read_data=0;

    memset(read_data, 0, sizeof(read_data));
    //------------------------
    int res = recv_bytes_frame_transform(UART_NUM_1, buff_data, &data_len);
    ASW_LOGI("recvied respons data OK OK%s  %d \n ", buff_data, data_len);

    memset(trans_data.data, 0, sizeof(trans_data.data));
    //------------------------

    memcpy(&trans_data.data, buff_data, data_len);
    trans_data.len = data_len;

    if (2 == func)
        trans_resrrpc_pub(&trans_data, ws, data_len); // 
    ASW_LOGI("--- DEBUG MARK waiting clound component app_mqtt.c---");

    if (20 == func)
    {
        // fdbg_msg = {0 }; [tgl mark]
        fdbg_msg.data = calloc(data_len, sizeof(char)); //[tgl mark]注意free的调用
        fdbg_msg.len = data_len;
        memcpy(fdbg_msg.data, buff_data, data_len);

        if (to_cgi_fdbg_mq != NULL)
        {
            ASW_LOGI("cgidata %d \n ", data_len);
            for (i = 0; i < data_len; i++)
                printf("%02X ", fdbg_msg.data[i]);

            fdbg_msg.type = MSG_FDBG;
            if (to_cgi_fdbg_mq != NULL)
            {
                if (xQueueSend(to_cgi_fdbg_mq, (void *)&fdbg_msg, 10 / portTICK_RATE_MS) == pdPASS) //[tgl mark]注意free的调用
                    ASW_LOGI("transform to cgi \n");
            }
            else
            {
                ESP_LOGE(TAG, "ERROR to_cgi_fdbg_mq is null");
            }
        }
    }

    ASW_LOGI("recivedata ");

    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI(" --R--", " receive from inv fbd ...\n");
        for (uint8_t i = 0; i < data_len; i++)
            printf("*%02X ", buff_data[i]);
        printf("\n");
    }

    /////////////////////////////////////////

    ASW_LOGI("transdata OK OK return 485 ");

    return TASK_QUERY_DATA;
}
//-------------------------------------------//
SERIAL_STATE ssc_send_meterdata_proc()
{
    printf("\n--- TODO ADD the COde About to Send Meter to Modbus \n");
    return TASK_IDLE;
}

//--------------------------------------------//
int8_t save_reg_info(void)
{
    Inv_reg_arr_t info_arr = {0};

    uint8_t i;

    for (i = 0; i < INV_NUM; i++)
    {
        if (inv_arr[i].regInfo.modbus_id == 0)
            break;
        ASW_LOGI("save modbus id: %02x, sn: %s\n",
                 inv_arr[i].regInfo.modbus_id, inv_arr[i].regInfo.sn);

        memcpy(&info_arr[i], &inv_arr[i].regInfo, sizeof(InvRegister));
    }
    ////////////////////////////////////////
    ASW_LOGI("save info_arr modbus id: %02x, sn: %s\n",
             info_arr[i].modbus_id, info_arr[i].sn);

    // general_add(NVS_INVTER_DB, (void *)info_arr);
    int res = general_add(NVS_INVTER_DB, &info_arr);

    printf("\n-------------save_reg_info [res:%d]------------  \n", res);
    return ASW_OK;
}

//--------------------------------------------//
#if TRIPHASE_ARM_SUPPORT
//新增函数
int recv_bytes_scanonly(int fd, uint8_t *res_buf, int *res_len)
{
    char buf[500] = {0};
    int len = 0;
    int nread = 0;
    int timeout_cnt = 0;
    int i = 0;

    while (1)
    {
        usleep(50000);
        timeout_cnt++;
        if (timeout_cnt >= 60)
        {
            return -1;
        }
        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        if (nread > 0)
        {
            len += nread;

            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X* ", buf[i]);
                printf("\r\n");
            }

            break;
        }
    }

    while (1)
    {
        usleep(200000); //这个影响分包，为了保证扫描第一次接收粘包，增加等等待时间到200ms
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        // printf("recvvv ****************************%d \n", nread);
        if (nread > 0)
        {
            len += nread;
            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X* ", buf[i]);
                printf("\r\n");
            }
        }
        else
        {
            ASW_LOGI("recvvv ****************************888 \n");
            memcpy(res_buf, buf, len);
            *res_len = len;
            // break;
            return 0;
        }
    }
    ASW_LOGW("recvvv -1 \n");
    *res_len = 0;
    return -1;
}

#endif
//--------------------------------------------//

SERIAL_STATE scan_register(void)
{
    char psn[33] = {0};
    uint8_t i = 0;
    // static uint8_t mCount = 0;
    // int8_t res;

    uint8_t read_frame[500] = {0};
    uint8_t buf[500] = {0};
    uint8_t high_byte = 0;
    uint8_t low_byte = 0;
    uint8_t modbus_id = 0;
    uint8_t modbus_id_ack = 0;

    int set_addr_frame_len = 0;
    int reg_num = 0;
    // int time = 0;
    int abort_cnt = 0;

    // int inv_fd = -1;
    uint16_t len = 0;
    int64_t scan_start_time_sec = esp_timer_get_time(); // 返回为0 ???? [tgl mark]

    ASW_LOGW("********  debug printf: scan_start_time_sec %lld", scan_start_time_sec);

    { //这是一个串口发送指令流程 A
        ASW_LOGI("sacn# ");

        flush_serial_port(UART_NUM_1);
        memset(inv_arr, 0, sizeof(inv_arr)); // clear register

        //---Eng.Stg.Mch
        // inv_arr_memcpy(&ipc_inv_arr, &inv_arr);
        memset(&ipc_inv_arr, 0, sizeof(Inv_arr_t));

        // clear bat arr LanStick-MultilInv
        // memset(g_bat_arr, 0, sizeof(Bat_arr_t));

        for (i = 0; i < 3; i++)
        {
            ////////////////////////////////////////////////////
            if (g_asw_debug_enable == 1)
            {
                ESP_LOGI("-S-", "send  scan clean sn...");
                for (uint8_t i = 0; i < sizeof(clear_register_frame); i++)
                    printf("<%02X> ", clear_register_frame[i]);
                printf("\n");
            }
            /////////////////////////////////////////////////////
            uart_write_bytes(UART_NUM_1, (const char *)clear_register_frame, sizeof(clear_register_frame));
            sleep(1);
            // g_num_real_inv = 0;  //[lan-stick eng.stg mark 20220914 ++]
        }
    } //这是一个串口发送指令流程 A end

    memset(&cgi_inv_arr, 0, sizeof(Inv_arr_t));

    ///////////////////////////////////////
    // 扫描过程保存电池的配置信息到对应的sn逆变器
    Bat_Monitor_arr_t mBatCfgPara = {0};
    Bat_Monitor_arr_t mMidBatCfgPara = {0};
    read_global_var(PARA_CONFIG, &mBatCfgPara);
    int8_t flag_BatDataUpdate = -1;
    ///////////////////////////////////////

    while (1)
    {

        { //这是一个串口发送指令流程 B start
            flush_serial_port(UART_NUM_1);

            ////////////////////////////////////////////////////
            if (g_asw_debug_enable == 1)
            {
                ESP_LOGI("-S-", " send  scan read sn...");
                for (uint8_t i = 0; i < sizeof(check_sn_frame); i++)
                    printf("<%02X> ", check_sn_frame[i]);
                printf("\n");
            }
            /////////////////////////////////////////////////////
            uart_write_bytes(UART_NUM_1, check_sn_frame, sizeof(check_sn_frame));
            ASW_LOGI("read sn frame send\r\n");

            memset(read_frame, 0, sizeof(read_frame));

#if TRIPHASE_ARM_SUPPORT

            recv_bytes_scanonly(UART_NUM_1, read_frame, &len); //更换串口接收的函数，比原始函数recv_bytes_from_waiting_nomd等待时间加长，也可以在原本函数上改，
#else

            recv_bytes_frame_waitting_nomd(UART_NUM_1, read_frame, &len);
#endif
            //////////////////////////////////////
            if (g_asw_debug_enable == 1)
            {
                ESP_LOGI("-R-", "receive scan...");
                for (uint8_t i = 0; i < len; i++)
                {
                    printf("*%02X ", read_frame[i]);
                }
                printf("\n");
            }
            ////////////////////////////////////

            if (read_frame[0] != 0XAA && read_frame[1] != 0X55)
                len = 0;
            if (len == 0)
                abort_cnt++;
            else
                abort_cnt = 0;

            ASW_LOGI("end  %lld %lld %d \n", esp_timer_get_time(), scan_start_time_sec, abort_cnt);
            if (abort_cnt > SCAN_ABORT_COUNT || 1 == g_scan_stop)
            {
                printf("\n-------------scan_register--------------\n");
                printf("   abort_cnt:%d,g_scan_stop:%d", abort_cnt, g_scan_stop);
                printf("\n-------------scan_register--------------\n");

                goto FINISH_SCAN;
            }

            ASW_LOGI("recv frame\n");
            for (i = 0; i < len; i++)
            {
                printf("%02X ", read_frame[i]);
            }
        } //这是一个串口发送指令流程 B end

        {

            if (len > 0)
            {
                { //这是B串口数据解析流程
                    parse_sn(read_frame, len, psn);
                    if (legal_check(psn) != 0)
                        continue;
                    ASW_LOGI("parse get psn: %s\n", psn);
                    memset(buf, 0, sizeof(buf));
                    set_addr_frame_len = 0;

                    memcpy(buf, set_addr_header, sizeof(set_addr_header));
                    set_addr_frame_len += sizeof(set_addr_header);

                    memcpy(buf + set_addr_frame_len, psn, strlen(psn));
                    set_addr_frame_len += sizeof(psn);

                    if (get_modbus_id(&modbus_id) != 0)
                        continue;

                    memcpy(buf + set_addr_frame_len - 1, &modbus_id, 1);

                    get_sum(buf, set_addr_frame_len, &high_byte, &low_byte);
                    memcpy(buf + set_addr_frame_len, &high_byte, 1);
                    set_addr_frame_len += 1;
                    memcpy(buf + set_addr_frame_len, &low_byte, 1);
                    set_addr_frame_len += 1;
                    ASW_LOGI("send modbus assign addr frame\n");

                    // #if DEBUG_PRINT_ENABLE
                    if (g_asw_debug_enable > 1)
                    {
                        for (i = 0; i < set_addr_frame_len; i++)
                        {
                            printf("%02X ", buf[i]);
                        }
                    }

                    // #endif
                } //这是B串口数据解析流程 end

                { //这是串口数据发送流程 C
                    flush_serial_port(UART_NUM_1);
                    /////////////////////////////////////////
                    if (g_asw_debug_enable == 1)
                    {
                        ESP_LOGI("-S-", " send  scan set modbusId...");
                        for (uint8_t i = 0; i < set_addr_frame_len; i++)
                            printf("<%02X> ", buf[i]);
                        printf("\n");
                    }
                    /////////////////////////////////////////////////////
                    uart_write_bytes(UART_NUM_1, buf, set_addr_frame_len);
                    // usleep(20000);
                    memset(read_frame, 0, sizeof(read_frame));

                    // recv_bytes_scanonly(UART_NUM_1, read_frame, &len); //更换串口接收的函数，比原始函数recv_bytes_from_waiting_nomd等待时间加长，也可以在原本函数上改，
                    recv_bytes_frame_waitting_nomd(UART_NUM_1, read_frame, &len);

                    //////////////////////////////////////
                    if (g_asw_debug_enable == 1)
                    {
                        ESP_LOGI("-R-", "receive scan...");
                        for (uint8_t i = 0; i < len; i++)
                        {
                            printf("*%02X ", read_frame[i]);
                        }
                        printf("\n");
                    }
                    ////////////////////////////////////
                    if (read_frame[0] != 0XAA && read_frame[0] != 0X55)
                        len = 0;
                    ASW_LOGI("recv add frame\n");
                    // #if DEBUG_PRINT_ENABLE
                    if (g_asw_debug_enable > 1)
                    {
                        for (i = 0; i < len; i++)
                        {
                            printf("%02X ", read_frame[i]);
                        }
                    }
                    // #endif
                } //这是串口数据发送流程 C end

                { //这是串口数据解析流程 C
                    if (len > 0)
                    {
                        modbus_id_ack = 0;
                        parse_modbus_addr(read_frame, len, &modbus_id_ack);
                        ASW_LOGI("id idack: %02X %02X\n", modbus_id, modbus_id_ack);

                        ////////////////////////////////////
                        Bat_arr_t mBatArryData = {0};
                        read_global_var(GLOBAL_BATTERY_DATA, &mBatArryData);

                        //////////////////////////////

                        if (modbus_id != modbus_id_ack)
                            continue;
                        // register one inv ok, please save modbus_id, psn
                        for (i = 0; i < INV_NUM; i++)
                        {
                            ASW_LOGI("total add: %02X \n", inv_arr[i].regInfo.modbus_id);

                            if (inv_arr[i].regInfo.modbus_id == 0)
                            {
                                inv_arr[i].regInfo.modbus_id = modbus_id;

                                memset(inv_arr[i].regInfo.sn, 0, sizeof(inv_arr[i].regInfo.sn));
                                memcpy(inv_arr[i].regInfo.sn, psn, strlen(psn));
                                // memcpy(cgi_inv_arr[i].regInfo.sn, psn, strlen(psn));
                                // cgi_inv_arr[i].regInfo.modbus_id = modbus_id;

                                /////////////////////// LanStick-MultilInv //////////////////////
                                mBatArryData[i].modbus_id = modbus_id;
                                memset(mBatArryData[i].sn, 0, sizeof(mBatArryData[i].sn));
                                memcpy(mBatArryData[i].sn, psn, strlen(psn));
                                // #if DEBUG_PRINT_ENABLE
                                ASW_LOGI("\n--------scan_register----\n");
                                ASW_LOGI(" inv%d arr.type:%d , g_bat_arr ---  modbus id:%d,inv sn:%s",
                                         i, inv_arr[i].regInfo.mach_type, mBatArryData[i].modbus_id, mBatArryData[i].sn);
                                // printf("\n--------scan_register----\n");
                                // #endif
                                /////////////////////////////////////////////////////////////////

                                //---Eng.Stg.Mch---//
                                inv_arr_memcpy(&ipc_inv_arr, &inv_arr);
                                // printf("CGI SN: %s \n", inv_arr[i].regInfo.sn);
                                // 扫描过程保存电池的配置信息到对应的sn逆变器
                                for (uint8_t n = 0; n < g_num_real_inv; n++) // INV_NUM??? or g_num_real_inv
                                {
                                    if (strcmp(psn, mBatCfgPara[n].sn) == 0)
                                    {
                                        memcpy(mMidBatCfgPara[i].sn, psn, strlen(psn));
                                        mMidBatCfgPara[i].modbus_id = modbus_id;
                                        memcpy(&mMidBatCfgPara[i].batmonitor, &mBatCfgPara[n].batmonitor, sizeof(Monitor_Battery_t));
                                        flag_BatDataUpdate++;

                                        printf("\n---- have got the history about bat config info [sn:%s,%d]-num[%d]-\n", psn, modbus_id, flag_BatDataUpdate);

                                        break;
                                    }
                                }

                                ////////////////////////////////////////

                                reg_num++;
                                break;
                            }
                        }
                    }
                } //这是串口数据解析流程 C end
            }
            else
            {
                if (reg_num >= INV_NUM)
                    break;
            }
        }
    }

FINISH_SCAN:
    ASW_LOGI("end scan \n");
    if (reg_num >= 0)
    {
        ///////////////////////////
        if (flag_BatDataUpdate > 0)
        {
            //  write_global_var();
            write_global_var(PARA_CONFIG, &mMidBatCfgPara);
        }
        /////////////////////////////
        save_reg_info(); /** inv_arr存到另一个地方*/
        ESP_LOGI(TAG, "total register num: %d\n", reg_num);
        reg_info_log();
        ASW_LOGI("end **\n");
        usleep(100000);

        // g_num_real_inv = reg_num;

        // esp_restart();
        clear_inv_list(); /** 清空inv_arr, 重新加载; 此刻不需要清空ipc_inv_arr,从而保证cgi访问连续 */
        g_scan_stop = 0;

        /////////////////////  FOR DEBUG TEST  //////////////////////
        usleep(200000);
        g_num_real_inv = load_reg_info();
        if (g_num_real_inv > 0)
            internal_inv_log(g_num_real_inv);
        /////////////////////////////////////////////////////////////
    }

    return TASK_IDLE;
}
//-----------------------------------------//
//-----------------------------------------//
int get_inv_log(char *buf, int typ)
{
    if (typ == 1)
    {
        memcpy(buf, inv_log.tag, strlen(inv_log.tag));
        memcpy(&buf[strlen(inv_log.tag)], inv_log.log, inv_log.index);
    }

    if (typ == 2)
    {
        memcpy(buf, com_log.tag, strlen(com_log.tag));
        memcpy(&buf[strlen(com_log.tag)], com_log.log, com_log.index);
    }

    if (typ == 3)
    {
        memcpy(buf, last_comm.tag, strlen(last_comm.tag));
        memcpy(&buf[strlen(last_comm.tag)], last_comm.log, strlen(last_comm.log));
    }
    return 0;
}
///////////////  Energy-Storage /////////////////////
int get_first_inv_modbus_id(void)
{
    for (uint8_t j = 0; j < INV_NUM; j++)
    {
        int status = inv_arr[j].status;
        if (status == 1)
        {
            if (inv_arr[j].regInfo.modbus_id > 0)
                return inv_arr[j].regInfo.modbus_id;
        }
    }
    return 3;
}

/** For energy storage type inverter */
int read_inv_time_mannual(void)
{
    struct timeval stime = {0};
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};
    mb_req.fd = UART_NUM_1;
    mb_req.start_addr = 1000;
    mb_req.reg_num = TIME_REG_NUM;

    mb_req.slave_id = get_first_inv_modbus_id();
    mb_res.len = 6 * 2 + 5;
    int res = asw_read_registers(&mb_req, &mb_res, 0x03);

    if (res < 0)
        return -1;
#if DEBUG_PRINT_ENABLE
    printf("\n-----12h get time:\n");
    uint8_t len = 0;
    for (len = 0; len < 16; len++)
        printf("%d ", mb_res.frame[len]);
    printf("\n-----12h time end\n");
#endif
    if (mb_res.frame[2] != 12)
        return -1;
    int iyeartemp = (((uint8_t)mb_res.frame[3] << 8) | ((uint8_t)mb_res.frame[4]));
    if ((iyeartemp >= 2020) && (iyeartemp < 2070))
    {
        struct tm time = {0};
        time.tm_year = iyeartemp - 1900;
        time.tm_mon = (((uint8_t)mb_res.frame[5] << 8) | ((uint8_t)mb_res.frame[6])) - 1;
        time.tm_mday = (((uint8_t)mb_res.frame[7] << 8) | ((uint8_t)mb_res.frame[8]));
        time.tm_hour = (((uint8_t)mb_res.frame[9] << 8) | ((uint8_t)mb_res.frame[10]));
        time.tm_min = (((uint8_t)mb_res.frame[11] << 8) | ((uint8_t)mb_res.frame[12]));
        time.tm_sec = (((uint8_t)mb_res.frame[13] << 8) | ((uint8_t)mb_res.frame[14]));

        stime.tv_sec = mktime(&time);

        settimeofday(&stime, NULL);

        return 0;
    }
    else
    {
        return -1;
    }
}

//-------------------------------//

void inv_get_time_each_12h(void)
{
    // if ((event_group_0 & INV_GET_TIME_MASK )== 1)  [mark ]
    if ((event_group_0 & INV_GET_TIME_MASK) != 0)
    {
        event_group_0 &= ~INV_GET_TIME_MASK;
        read_inv_time_mannual();
    }
}

//----------------------//
int get_day_sec(void)
{
    DATE_STRUCT curr_date = {0};
    get_current_date(&curr_date);
    int sec = curr_date.HOUR * 3600 + curr_date.MINUTE * 60 + curr_date.SECOND;
    return sec;
}

//---------------------------//

void get_time_in_long_run(void)
{
    static int last_sec = 0;
    // static int rand_sec = -1;
    // int day_sec = get_day_sec();
    int now_sec = get_second_sys_time();

    /** 距上一次对时不到10小时，不动作*/
    if (now_sec - last_sec < 10 * 3600)
        return;

    // if (check_time_correct() == 0)
    {
        // if (rand_sec < 0)
        //     rand_sec = now_sec % 1800; // random
        // int refer_12h = 12 * 3600 + 1800 + rand_sec;
        // int refer_0h = 1800 + rand_sec;

        // if (day_sec > refer_0h && day_sec < refer_0h + 1800)
        // {
        last_sec = now_sec;
        event_group_0 |= CLD_GET_TIME_MASK;
        // rand_sec = -1;
        // }
        // else if (day_sec > refer_12h && day_sec < refer_12h + 1800)
        // {
        //     last_sec = now_sec;
        //     event_group_0 |= CLD_GET_TIME_MASK;
        //     rand_sec = -1;
        // }
    }
}

// Eng.Stg.Mch-lanstick
int get_cld_inv_arr_first(void)
{
    for (int j = 0; j < INV_NUM; j++)
    {
        if (cld_inv_arr[j].status == 1) // if online
        {
            return j;
        }
    }
    return 0;
}
void cld_get_time_each_12h(void)
{
    if (event_group_0 & CLD_GET_TIME_MASK)
    {
        event_group_0 &= ~CLD_GET_TIME_MASK;
        int res = get_time_manual();
        if (res != 0)
        {
            event_group_0 |= INV_GET_TIME_MASK;
        }
        else
        {
            inv_arr[0].regInfo.syn_time = 0; // TODO mutl-inv
        }
    }
}

//------------------------------//
void cld_idle_work(void)
{

    // if (memcmp(&cld_inv_arr, &ipc_inv_arr, sizeof(Inv_arr_t)) != 0) // tgl mark add
    inv_arr_memcpy(&cld_inv_arr, &ipc_inv_arr);
    cld_get_time_each_12h();
}
//-------------------------//
void com_idle_work(void)
{
    inv_arr_memcpy(&ipc_inv_arr, &inv_arr);
    check_estore_become_online();
    inv_get_time_each_12h();
    /** get time 0 and 12 hour*/
    get_time_in_long_run();
}

//--------------------------//

int get_estore_inv_arr_first(void)
{
    for (int j = 0; j < INV_NUM; j++)
    {
        int mach_type = inv_arr[j].regInfo.mach_type;
        int status = inv_arr[j].status;
        if (mach_type > 10 && status == 1) // estore && online
        {
            return j;
        }
    }
    return -1;
}

//-----------------------------//

int get_safety(void)
{
    for (uint8_t j = 0; j < INV_NUM; j++)
    {
        int mach_type = inv_arr[j].regInfo.mach_type;
        int status = inv_arr[j].status;
        if (mach_type > 10 && status == 1) // estore && online
        {
            return inv_arr[j].regInfo.safety_type;
        }
    }
    return -1;
}
//-----------------------------//

int is_safety_96_97(void)
{
    int safe = get_safety();
    if (safe == 96 || safe == 97 || safe == 80)
    {
        return 1;
    }
    return 0;
}
//----------------------------//

//并机模式下，只检测主机安规，否则检测第一台逆变器的安规

int asw_is_safety_96_97(void)
{
    uint8_t safe_num = 0;
    uint8_t mId = 0;
    if (g_parallel_enable)
    {
        for (uint8_t i = 0; i < g_num_real_inv; i++)
        {
            if (g_host_modbus_id == inv_arr[i].regInfo.modbus_id)
                mId = i;
        }
    }
    ASW_LOGI(" safty 96 97 get modbusId:%d", mId);

    if (inv_arr[mId].regInfo.mach_type > 10 && inv_arr[mId].status) // estore && online
    {
        safe_num = inv_arr[mId].regInfo.safety_type;
    }

    if (safe_num == 96 || safe_num == 97 || safe_num == 80)
    {
        return 1;
    }
    return 0;
}
//----------------------------//
//读取逆变器重放电深度寄存器
static int asw_readreg_battery_discharge_info()
{

    ESP_LOGI("---- Inv Task ---", " read inv reg with battery dischage/charge configure.\n");

    uint8_t buf[4];

    Bat_Monitor_arr_t monitor_para_arr = {0};

    read_global_var(PARA_CONFIG, &monitor_para_arr);
    uint16_t discharge_value = 0, charge_value = 0;

    for (uint8_t i = 0; i < g_num_real_inv; i++)
    {
        memset(buf, 0, 4);
        asw_md_read_inv_reg(inv_arr[i].regInfo.modbus_id, INV_REG_ADDR_BATTERY_CHAGE_CONFIG, 2, buf);

        discharge_value = ((buf[0] << 16) & 0xff00) | (buf[1] & 0xff);
        charge_value = ((buf[2] << 16) & 0xff00) | (buf[3] & 0xff);
        memcpy(monitor_para_arr[i].sn, inv_arr[i].regInfo.sn, sizeof(inv_arr[i].regInfo.sn));
        monitor_para_arr[i].modbus_id = inv_arr[i].regInfo.modbus_id;

        if (discharge_value > 100)
        {
            monitor_para_arr[i].batmonitor.dchg_max = discharge_value * 0.01;
        }
        if (charge_value > 100)
        {
            monitor_para_arr[i].batmonitor.chg_max = charge_value * 0.01;
        }

        if (monitor_para_arr[i].batmonitor.dchg_max < 0 || monitor_para_arr[i].batmonitor.dchg_max > 100)
        {
            monitor_para_arr[i].batmonitor.dchg_max = 10;
        }

        if (monitor_para_arr[i].batmonitor.chg_max < 0 || monitor_para_arr[i].batmonitor.chg_max > 100)
        {
            monitor_para_arr[i].batmonitor.chg_max = 100;
        }

        ASW_LOGI("load inv:%d-%s reg charge/discharge %d,%d:",
                 monitor_para_arr[i].modbus_id, monitor_para_arr[i].sn,
                 monitor_para_arr[i].batmonitor.chg_max, monitor_para_arr[i].batmonitor.dchg_max);
    }

    write_global_var(PARA_CONFIG, &monitor_para_arr);

    return 0;
}

//---------------------------//
int asw_inv_data_init()
{
    ESP_LOGI("--- Inv Task ---", " inv data init ....");
    g_num_real_inv = load_reg_info();
    usleep(20000);

    asw_readreg_battery_discharge_info();

    usleep(20000);

    /////读取逆变器充放电深度信息
    return ASW_OK;
}