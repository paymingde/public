#include "asw_modbus.h"
#include "estore_com.h"
/*****************************************/
static const char *TAG = "asw_modbus.c";

Inv_arr_t inv_dev_info = {0};
//----------------------------------//
int8_t md_write_inv_time(Inverter *inv_ptr)
{
    //[32][10][03][E8][00][06][0C][07][E4][00][07][00][0F][00][0A][00][27][00][03][B9][2A]
    uint16_t i = 0;
    struct tm currtime = {0};
    time_t t = time(NULL);
    uint16_t crc = 0;
    int8_t res;

    localtime_r(&t, &currtime);
    currtime.tm_year += 1900;
    currtime.tm_mon += 1;
    ASW_LOGI("----data-------- %d %d %d %d %d %d \n", currtime.tm_year, currtime.tm_mon,
             currtime.tm_mday, currtime.tm_hour, currtime.tm_min, currtime.tm_sec);

    unsigned char set_time[21] = {0x32, 0x10, 0x03, 0xE8, 0x00, 0x06, 0x0C, 0x07, 0xE4, 0x00, 0x07, 0x00,
                                  0x0F, 0x00, 0x0A, 0x00, 0x27, 0x00, 0x03, 0xB9, 0x2A};
    set_time[0] = inv_ptr->regInfo.modbus_id;
    set_time[7] = (currtime.tm_year >> 8) & 0XFF;
    set_time[8] = (currtime.tm_year) & 0XFF;

    set_time[9] = (currtime.tm_mon >> 8) & 0XFF;
    set_time[10] = (currtime.tm_mon) & 0XFF;

    set_time[11] = (currtime.tm_mday >> 8) & 0XFF;
    set_time[12] = (currtime.tm_mday) & 0XFF;

    set_time[13] = (currtime.tm_hour >> 8) & 0XFF;
    set_time[14] = (currtime.tm_hour) & 0XFF;

    set_time[15] = (currtime.tm_min >> 8) & 0XFF;
    set_time[16] = (currtime.tm_min) & 0XFF;

    set_time[17] = (currtime.tm_sec >> 8) & 0XFF;
    set_time[18] = (currtime.tm_sec) & 0XFF;

    crc = crc16_calc(set_time, 19);
    set_time[19] = crc & 0xFF;
    set_time[20] = (crc & 0xFF00) >> 8;

    ASW_LOGI("send set time >>>>>>>>>>>>>\n");

    ///////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", " send to inv time...");
        for (uint8_t ij = 0; ij < 21; ij++)
        {
            printf("<%02X> ", set_time[ij]);
        }
        printf("\n");
    }
    ////////////////////////////////////

    uart_write_bytes(UART_NUM_1, (const char *)set_time, 21);
    memset(set_time, 0, 21);
    i = 8;

    ASW_LOGI("recv data write inv time ---,size:%d", i);
    res = recv_bytes_frame_waitting(UART_NUM_1, set_time, &i);
    ///////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", "receive from inv time...");
        for (uint8_t ij = 0; ij < i; ij++)
        {
            printf("*%02X  ", set_time[ij]);
        }
        printf("\n");
    }
    ////////////////////////////////////

    ASW_LOGI("recv data result,res: %d,len:%d", res, i);

    // len = modbus_write_registers(ctx, 1000, TIME_REG_NUM, inv_time_buf);
    if (i <= 0 || res != ASW_OK)
    {
        ESP_LOGE(TAG, "setting time error \n");
        return ASW_FAIL;
    }
    else
    {
        return ASW_OK;
    }
}

//----------------------------------------//

int8_t md_write_data(Inverter *inv_ptr, Inv_cmd cmd)
{

    int8_t ret = ASW_FAIL;
    sleep(1);

    ASW_LOGI("----------11111111111[%d]---------\n", cmd);

    switch (cmd)
    {
    case CMD_MD_WRITE_INV_TIME:

        ret = md_write_inv_time(inv_ptr);
        break;
    /*for upgrade inverter firmware*/
    default:
        break;
    }
    return ret;
}

//-----------------------------------------//
int8_t recv_bytes_frame_waitting(int fd, uint8_t *res_buf, uint16_t *res_len)
{
    uint8_t buf[500] = {0};
    // uint8_t buf[300] = {0}; //[tgl mark] 减少大小
    int len = 0;
    int nread = 0;
    int timeout_cnt = 0;
    uint16_t crc;
    int i = 0;
    uint16_t max_len = *res_len;
    *res_len = 0;
    //////////////// lanstick-Eng.Stg.Mch //////////////////
    int limit_cnt = 60; // first wait

    if (is_safety_96_97() && (event_group_0 & SAFE_96_97_READ_METER))
    {
        limit_cnt = 10;
        event_group_0 &= ~SAFE_96_97_READ_METER;
    }

    if (event_group_0 & LOW_TIMEOUT_FOR_MODBUS)
    {
        event_group_0 &= ~LOW_TIMEOUT_FOR_MODBUS;
        limit_cnt = 6;
    }

    ASW_LOGI("maxlen------%d---limit_cn:%d---\n", max_len, limit_cnt);

    ///////// ////////////////////////////////////

    /** 第一次读到数据*/
    while (1)
    {
        usleep(50000); // [tgl mark]
        timeout_cnt++;
        if (timeout_cnt > limit_cnt)
        {
            // ASW_LOGE(" ---  recv inv data time out ");
            return ASW_FAIL;
        }
        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);

        if (nread > 0)
        {
            len += nread;

            //////////////////////////////////////
            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X*", buf[i]);

                printf("\n");
            }
            //////////////////////////////////////////
            if (len >= max_len)
                goto AAA;
            break;
        }
    }
    ASW_LOGI("\nfirst recv data maxlen:%d,len:%d", max_len, len);

    if (len > max_len)
    {
        ESP_LOGW(TAG, "recv len is erro:A maxlen--------%d  ----%d \n", max_len, len);
        return ASW_FAIL;
    }

    /** 判断长度是否足够，直至超时*/
    while (1)
    {
        usleep(50000); //
        timeout_cnt++;
        if (timeout_cnt >= 60)
            break;

        // 20 / portTICK_RATE_MS
        nread = uart_read_bytes(fd, buf + len, sizeof(buf) - len, 0);
        if (nread > 0)
        {
            len += nread;
            ///////////////////////////////////////////////////////////
            if (g_asw_debug_enable > 1)
            {
                for (; i < len; i++)
                    printf("*%02X*", buf[i]);
                printf("\n");
            }
            //////////////////////////////////////////////////////
            if (len >= max_len)
                goto AAA;
        }
    }

AAA:
    if (len != max_len)
    {
        ESP_LOGW(TAG, "maxlen---------------------------------%d---%d---\n", max_len, len);
        return ASW_FAIL;
    }
    crc = crc16_calc(buf, max_len);

    ASW_LOGI("recvv ok %x  %x %d \n", crc, ((buf[max_len - 2] << 8) | buf[max_len - 1]), len);

    if (len > 1 && 0 == crc)
    {
        memcpy(res_buf, buf, len);
        *res_len = len;
        return ASW_OK;
    }
    else
        return ASW_FAIL;
}

//--------------------------------------------------//

int8_t asw_read_registers(mb_req_t *mb_req, mb_res_t *mb_res, uint8_t funcode)
{
    uint8_t req_frame[256] = {0};
    uint16_t crc;

    req_frame[0] = mb_req->slave_id;
    req_frame[1] = funcode; // 0x04;
    req_frame[2] = (mb_req->start_addr & 0xFF00) >> 8;
    req_frame[3] = mb_req->start_addr & 0xFF;
    req_frame[4] = (mb_req->reg_num & 0xFF00) >> 8;
    req_frame[5] = mb_req->reg_num & 0xFF;
    crc = crc16_calc(req_frame, 6);
    req_frame[6] = crc & 0xFF;
    req_frame[7] = (crc & 0xFF00) >> 8;

    /////////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", " send to modbus read ...");
        for (uint8_t i = 0; i < 8; i++)
        {
            printf("<%02X> ", req_frame[i]);
        }

        printf("\n");
        printf("\n");
    }
    /////////////////////////////////////////
    ASW_LOGI("-------%d \n", mb_res->len);

    uart_write_bytes(mb_req->fd, (const char *)req_frame, 8);

    int res = recv_bytes_frame_waitting(mb_req->fd, mb_res->frame, &mb_res->len);
    /////////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", " Receive from modbus ...");
        for (uint8_t k = 0; k < mb_res->len; k++)
        {
            printf("*%02X ", mb_res->frame[k]);
        }

        printf("\n");
        printf("\n");
    }
    /////////////////////////////////////////

    return res;
}
//--------------------------------------------------//

void save_inv_data(Inv_data p_data, unsigned int *time, uint16_t *error_code)
{

    Inv_data _data = p_data;
    // save data every five minute
    struct timeval tv;
    // static uint32_t save_data_time = 0; // the time for sampling inverter data
    int period = 0;

    ASW_LOGI("*******save_inv_data----->>>> %d %d \n", *error_code, p_data.error);
    if ((*error_code != p_data.error) && (p_data.error != 0XFFFF))
    {
        *error_code = p_data.error;
        ESP_LOGE(TAG, "find error----->>>> %d %d \n", *error_code, p_data.error);
        if (p_data.error != 0)
        {
            goto sent_error;
        }
    }
    /*save data every 1 minte*/
    gettimeofday(&tv, NULL);

    period = (tv.tv_sec - *time);

    if (period >= SAVE_TIMING || period < 0) // 5min 一次  往消息队列 mq0中发送一次数据
    {
        *time = tv.tv_sec;
    }
    else
    {
        // ESP_LOGW("--- save_inv_data Erro---", " 5m is not come ");
        return;
    }

    if ((((_data.time[0] - 0x30) * 10 + (_data.time[1] - 0x30)) * 100 +
         (_data.time[2] - 0x30) * 10 + (_data.time[3] - 0x30)) < 2020)
    {
        ESP_LOGW("--- save_inv_data Erro---", " the send data time is not right! ");
        return;
    }

sent_error:
    ASW_LOGI("read** %s %s %d %d %d\n", _data.psn, _data.time,
             (_data.time[0] - 0x30) * 10 + (_data.time[1] - 0x30), (_data.time[2] - 0x30) * 10 + (_data.time[3] - 0x30),
             (((_data.time[0] - 0x30) * 10 + (_data.time[1] - 0x30)) * 100 + (_data.time[2] - 0x30) * 10 + (_data.time[3] - 0x30)));

    if (mq0 != NULL)
    {
        xQueueSend(mq0, (void *)&_data, (TickType_t)0);

        //逆变器上传数据 和 电表数据上传同步
        g_meter_inv_synupload_flag = 1;
        g_uint_meter_data_time = get_second_sys_time();

        ASW_LOGW("----->>>>>mq0 send ok<<<<<-------\n");
    }
    return;
}
//--------------------------------------------------//
/*! \brief  This function check the comm receive packet whether is right.*/
int md_query_inv_data(Inverter *inv_ptr, unsigned int *time, uint16_t *error_code)
{
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};
    mb_req.fd = UART_NUM_1;
    mb_req.start_addr = 1300;
    mb_req.reg_num = DATA_REG_NUM;
    Inv_data inv_data = {0};
    char now_time[32] = {0};
    uint8_t k = 0;
    ASW_LOGI("-----------------read inv data :%d", inv_ptr->regInfo.modbus_id);
    mb_req.slave_id = inv_ptr->regInfo.modbus_id;
    mb_res.len = 88 * 2 + 5; // mb_res.len = 88 * 2 + 5; [tgl update]change

    // ASW_LOGW("-----TGL DEBUG, md-read-data --->md_query_inv_data,cmd-adr:  %d", mb_req.start_addr);
    int8_t res = asw_read_registers(&mb_req, &mb_res, 0x04);
    // ASW_LOGW("-----TGL DEBUG, md-read-data --->md_query_inv_data,return code %d", res);

    if (res != ASW_OK)
    {
        uint8_t i = 0;

        for (i = 0; i < 170; i++)
            if (mb_res.frame[i])
                return -1;

        if (i >= 168)
            return -2;
    }

    md_decode_inv_data_content(&mb_res.frame[3], &inv_data);

    total_curt_power = inv_data.pac;

    inv_data.rtc_time = get_time(now_time, sizeof(now_time)) - 8 * 3600;
    strncpy(inv_data.time, now_time, sizeof(inv_data.time));

    memcpy(inv_data.psn, inv_ptr->regInfo.sn, sizeof(inv_ptr->regInfo.sn));

    for (k = 0; k < INV_NUM; k++)
    {
        if (inv_arr[k].regInfo.modbus_id == inv_ptr->regInfo.modbus_id)
        {

            ASW_LOGI(" copy invdata with time to inv:%s_arr[%d]", inv_arr[k].regInfo.sn, k);

            memcpy(&inv_arr[k].invdata, &inv_data, sizeof(Inv_data));
            break;
        }
    }
    ASW_LOGI("read data sn %s %s %d %d\n", inv_data.psn, inv_data.time,
             (inv_data.time[0] - 0x30) * 10 + (inv_data.time[1] - 0x30),
             (inv_data.time[2] - 0x30) * 10 + (inv_data.time[3] - 0x30));
    if (strlen(inv_data.psn) > 10                                    // get_work_mode() == 0
        && get_second_sys_time() > 180 && get_device_sn() == ASW_OK) //检测序列号为空，则不上传云端 [tgl add 20220622]
    {
        save_inv_data(inv_data, time, error_code);
        ASW_LOGW("############# SAVE INV DATA update to clound");
    }

    return res;
}

//----------------------------------------------//
// 读取逆变器寄存器数据
int asw_md_read_inv_reg(uint8_t modbus_id, uint16_t reg_adr, uint16_t len, uint8_t *data)
{
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};
    mb_req.fd = UART_NUM_1;
    mb_req.start_addr = reg_adr;
    mb_req.reg_num = len;

    ESP_LOGI("-- read inv reg ---", "--------- read inv[%d] reg[%d] len[%d] ----", modbus_id, reg_adr, len);
    mb_req.slave_id = modbus_id;
    mb_res.len = len * 2 + 5; // mb_res.len = 88 * 2 + 5; [tgl update]change

    int8_t res = asw_read_registers(&mb_req, &mb_res, 0x03);

    if (res != ASW_OK)
    {

        return -1;
    }
    else
    {
        memcpy(data, &mb_res.frame[3], len * 2);
        return mb_res.len - 5;
    }
}
//-------------------------------------------------//

int8_t md_query_inv_info(Inverter *inv_ptr, unsigned int *inv_index)
{
    //[31][04][03][E8][00][4B][35][BD]
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};

    static int old_total_rate_power = 0;

    mb_req.fd = UART_NUM_1;
    mb_req.start_addr = 1000;
    mb_req.reg_num = INFO_REG_NUM;
    InvRegister inv_device = {0};
    InvRegister_ptr p_device = &inv_device;
    mb_req.slave_id = inv_ptr->regInfo.modbus_id;

    ASW_LOGI("  slave id:%d", mb_req.slave_id);

    mb_res.len = 75 * 2 + 5; // mb_res.len = 75 * 2 + 5; [tgl update]change
    int8_t res = asw_read_registers(&mb_req, &mb_res, 0x04);

    if (res != ASW_OK)
        return ASW_FAIL;
    md_decode_inv_Info(&mb_res.frame[3], p_device);

    total_rate_power += p_device->rated_pwr;
    ASW_LOGI("total_rate_power %d \n", total_rate_power);

#if TRIPHASE_ARM_SUPPORT

    if (old_total_rate_power != total_rate_power)
    {
        old_total_rate_power = total_rate_power;
        event_group_0 |= PWR_REG_SOON_MASK;

        ASW_LOGI("======  TEST TOTAL INV POWER IS CHANGE %d =======", total_rate_power);
    }

#endif

    memcpy(&(inv_arr[*inv_index - 1].regInfo.mode_name), p_device->mode_name, sizeof(p_device->mode_name));
    memcpy(&(inv_arr[*inv_index - 1].regInfo.sn), p_device->sn, sizeof(p_device->sn));

    /* Eng.Stg.Mch-lanstick 20220909 + */
    memcpy(&(inv_arr[*inv_index - 1].regInfo), p_device, sizeof(InvRegister));

    /* Eng.Stg.Mch-lanstick 20220907 - */
    // memcpy(&(cgi_inv_arr[*inv_index - 1].regInfo), p_device, sizeof(InvRegister));
    // memcpy(&(inv_dev_info[*inv_index - 1].regInfo), p_device, sizeof(InvRegister));

    return ASW_OK;
}
/* \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  */
#if TRIPHASE_ARM_SUPPORT

//新增函数,读arm板信息，目前只有一个软件版本的信息
#define PROTOCOL_COM_HEAD protocol_com_head
const uint8_t protocol_com_head[8] = {0x66, 0x99, 0x0D, 0x0A, 0x66, 0x99, 0x0D, 0x0A};
int md_query_arm_info(Inverter *inv_ptr, unsigned int *inv_index)
{
    uint8_t buf[64] = {0};
    uint8_t recvbuf[256] = {0};
    uint16_t recvlen = 0;
    uint16_t crc = 0;
    int len = 0;

    memcpy(buf, PROTOCOL_COM_HEAD, 8);
    len += 8;
    buf[len] = inv_ptr->regInfo.modbus_id;
    buf[++len] = 0x05;

    buf[++len] = 0; // 8+1+1+4+0+2
    buf[++len] = 0;
    buf[++len] = 0;
    buf[++len] = 0x10;

    crc = crc16_calc(buf, ++len);
    buf[len] = crc & 0xFF;
    buf[++len] = (crc >> 8) & 0xFF;
    len++;
    int res = -1;

    ASW_LOGI("read commbox info........\n");
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send to arm version cmd...");
        for (int i = 0; i < len; i++)
        {
            printf("<%02x>", buf[i]);
        }
        printf("\n");
    }

    // for (int j = 0; j < 3; j++)
    // {
    uart_write_bytes(UART_NUM_1, buf, len);

    res = recv_bytes_frame_waitting_nomd(UART_NUM_1, recvbuf, &recvlen);
    //////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", "receive  from  arm ...");
        for (uint8_t i = 0; i < recvlen; i++)
        {
            printf("*%02X ", recvbuf[i]);
        }
        printf("\n");
    }
    ////////////////////////////////////
    if (recvlen == 44 && crc16_calc(recvbuf, recvlen) == 0)
    {
        if (memcmp(recvbuf, buf, 8) == 0)
        {
            if (recvbuf[9] == 0x05)
            {
                ASW_LOGI("recv commbox info ok\n");
                res = 0;
            }
            else
                res = -1;
        }
    }
    // }

    if (res == 0)
    {
        memcpy(&(inv_arr[*inv_index - 1].regInfo.csw_ver), recvbuf + 10, 32); //存到的inv_arr里，因此，inv_arr.regInfo这个结构体中增加一个数据
        ASW_LOGI("csw_ver:%s\n", inv_arr[*inv_index - 1].regInfo.csw_ver);
    }

    return res;
}

#endif
/* \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\  */

//--------------------------------------------------//
/*----------------------------------------------------------------------------*/
/*! \brief  This function query rtc time from inverter.
 *  \param  md_addr        [in],modbus address of inverter
 *  \param  buff           [out],rtc time from invertr
 *  \return bool            The result of query;True:Success,False:faile
 *  \see  从逆变器获取时间信息后，更新到stick系统时间。
 *  \note
 */
/*----------------------------------------------------------------------------*/
int8_t md_read_inv_time(Inverter *inv_ptr)
{
    time_t nowt = time(NULL);
    struct timeval stime;
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};
    mb_req.fd = UART_NUM_1;
    mb_req.start_addr = 1000;
    mb_req.reg_num = TIME_REG_NUM;
    // Inv_data inv_data = {0};
    // char now_time[32] = {0};

    mb_req.slave_id = inv_ptr->regInfo.modbus_id;
    mb_res.len = 6 * 2 + 5; // mb_res.len = 6 * 2 + 5; [tgl update]change
    int8_t res = asw_read_registers(&mb_req, &mb_res, 0x03);

    if (res != ASW_OK)
    {
        ESP_LOGW(TAG, "read inv time fun return is not OK!!!\n");
        return ASW_FAIL;
    }

    ASW_LOGI("read inv time stat \n");
    uint8_t len = 0;

    /////////////////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        for (len = 0; len < 16; len++)
            printf("%d ", mb_res.frame[len]);
        ASW_LOGI("time end\n ");
    }
    ///////////////////////////////////////////////

    if (mb_res.frame[2] != 12)
        return ASW_FAIL;
    int iyeartemp = (((uint8_t)mb_res.frame[3] << 8) | ((uint8_t)mb_res.frame[4]));
    if ((iyeartemp >= 2020) && (iyeartemp < 2050))
    {
        struct tm time = {0};
        time.tm_year = iyeartemp - 1900;
        time.tm_mon = (((uint8_t)mb_res.frame[5] << 8) | ((uint8_t)mb_res.frame[6])) - 1;
        time.tm_mday = (((uint8_t)mb_res.frame[7] << 8) | ((uint8_t)mb_res.frame[8]));
        time.tm_hour = (((uint8_t)mb_res.frame[9] << 8) | ((uint8_t)mb_res.frame[10]));
        time.tm_min = (((uint8_t)mb_res.frame[11] << 8) | ((uint8_t)mb_res.frame[12]));
        time.tm_sec = (((uint8_t)mb_res.frame[13] << 8) | ((uint8_t)mb_res.frame[14]));

        ASW_LOGI("read local time %ld \n", nowt);

        stime.tv_sec = mktime(&time);
        ASW_LOGI("##data-------- %d %d %d %d %d %d \n", time.tm_year, time.tm_mon,
                 time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
        ASW_LOGI("read local time----- %ld  %ld \n", nowt, stime.tv_sec);

        if (g_monitor_state == 0)
        {
            if (stime.tv_sec > nowt)
                settimeofday(&stime, NULL); //更新时间信息到系统时间变量
        }
        return ASW_OK;
    }
    else
    {
        ESP_LOGW(TAG, "get time year %d err from inv\r\n", iyeartemp);
        return ASW_FAIL;
    }
}

//-------------------------------------------------//

int8_t md_read_data(Inverter *inv_ptr, Inv_cmd cmd, unsigned int *time, uint16_t *error)
{
    int8_t ret = ASW_FAIL;
    /** some type inverter need sleep, like storm*/
    // sleep(1); // return md_query_inv_data(inv_ptr);  // Eng.Stg.Mch-Lanstick 20220907 -

    switch (cmd)
    {
    case CMD_READ_INV_DATA:
        // Eng.Stg.Mch-Lanstick 20220907 +

        query_meter_proc(1); /** 读电表数据 */
        // read_bat_if_has_estore();                   /** 读电池数据*/
        asw_read_bat_arr_data(inv_ptr);
        ret = md_query_inv_data(inv_ptr, time, error); /** 读逆变器数据*/
        break;

    case CMD_MD_READ_INV_INFO:

        ret = md_query_inv_info(inv_ptr, time);
#if TRIPHASE_ARM_SUPPORT
        md_query_arm_info(inv_ptr, time);
#endif

        break;
    case CMD_MD_READ_INV_TIME:
        ret = md_read_inv_time(inv_ptr);
        break;
    default:
        ret = ASW_FAIL;
        break;
    }
    return ret;
}
//-----------------------------------------//
static int8_t modbus_write_broadcast(int fd, uint16_t start_addr, uint16_t reg_num, uint16_t *data)
{
    uint8_t buf[257] = {0};
    buf[0] = 0;
    uint16_t len = 0;
    int res = 0;
    uint16_t crc = 0;
    if (reg_num == 1)
    {
        buf[1] = 6;
        buf[2] = (start_addr >> 8) & 0xFF;
        buf[3] = start_addr & 0xFF;
        buf[4] = (data[0] >> 8) & 0xFF;
        buf[5] = data[0] & 0xFF;
        crc = crc16_calc(buf, 6);
        buf[6] = crc & 0xFF;
        buf[7] = (crc >> 8) & 0xFF;
        len = 8;
    }
    else
    {
        buf[1] = 16;
        buf[2] = (start_addr >> 8) & 0xFF;
        buf[3] = start_addr & 0xFF;
        buf[4] = (reg_num >> 8) & 0xFF;
        buf[5] = reg_num & 0xFF;
        buf[6] = (uint8_t)(reg_num * 2);
        for (int j = 0; j < reg_num; j++)
        {
            buf[7 + 2 * j] = (data[j] >> 8) & 0xFF;
            buf[8 + 2 * j] = data[j] & 0xFF;
        }
        len = 7 + 2 * reg_num;
        crc = crc16_calc(buf, len);
        buf[len] = crc & 0xFF;
        buf[len + 1] = (crc >> 8) & 0xFF;
        len += 2;
    }
    ASW_LOGI("----modbus_write_broadcast: send buf: ------- ");
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send to inv ....");
        for (uint8_t i = 0; i < len; i++)
        {
            printf("<%02X> ", buf[i]);
        }
        printf("\n");
    }

    uart_write_bytes(fd, (const char *)buf, len);
    ASW_LOGI("E-MODBUS Broadcast WRITE:\n"); // hex_print(buf, len);

    return ASW_OK;
}

//-----------------------------------------//
static int8_t modbus_write(int fd, uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint16_t *data)
{
    uint8_t buf[257] = {0};
    buf[0] = slave_id;
    uint16_t len = 0;
    int res = 0;
    uint16_t crc = 0;
    if (reg_num == 1)
    {
        buf[1] = 6;
        buf[2] = (start_addr >> 8) & 0xFF;
        buf[3] = start_addr & 0xFF;
        buf[4] = (data[0] >> 8) & 0xFF;
        buf[5] = data[0] & 0xFF;
        crc = crc16_calc(buf, 6);
        buf[6] = crc & 0xFF;
        buf[7] = (crc >> 8) & 0xFF;
        len = 8;
    }
    else
    {
        buf[1] = 16;
        buf[2] = (start_addr >> 8) & 0xFF;
        buf[3] = start_addr & 0xFF;
        buf[4] = (reg_num >> 8) & 0xFF;
        buf[5] = reg_num & 0xFF;
        buf[6] = (uint8_t)(reg_num * 2);
        for (int j = 0; j < reg_num; j++)
        {
            buf[7 + 2 * j] = (data[j] >> 8) & 0xFF;
            buf[8 + 2 * j] = data[j] & 0xFF;
        }
        len = 7 + 2 * reg_num;
        crc = crc16_calc(buf, len);
        buf[len] = crc & 0xFF;
        buf[len + 1] = (crc >> 8) & 0xFF;
        len += 2;
    }

    if (g_asw_debug_enable == 1)
    {

        ESP_LOGI("-S-", " send to inv write cmd....");
        for (uint8_t i = 0; i < len; i++)
        {
            printf("<%02X>", buf[i]);
        }
        printf("\n");
        printf("\n");
    }

    uart_write_bytes(fd, (const char *)buf, len);
    ASW_LOGI("E-MODBUS WRITE:\n");
    // hex_print(buf, len);

    memset(buf, 0, sizeof(buf));
    len = 8;
    ASW_LOGW("recv data modbus write AAAA.....");
    res = recv_bytes_frame_waitting(fd, buf, &len);

    if (g_asw_debug_enable == 1)
    {

        ESP_LOGI("-R-", "receive  from inv write cmd....");
        for (uint8_t i = 0; i < len; i++)
        {
            printf("*%02X ", buf[i]);
        }
        printf("\n");
        printf("\n");
    }

    if (res == 0)
    {
        if (reg_num == 1)
        {
            if (len == 8)
            {
                if (buf[1] == 6)
                {
                    return ASW_OK;
                }
            }
        }
        else
        {
            if (len == 8)
            {
                if (buf[1] == 16)
                {
                    return ASW_OK;
                }
            }
        }
    }
   ESP_LOGW(TAG," modbus write Error!!");
    return ASW_FAIL;
}

////////////////////////////////////////////
static int8_t modbus_write_bit8(int fd, uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint8_t *data)
{
    uint8_t buf[257] = {0};
    buf[0] = slave_id;
    uint16_t len = 0;
    int res = 0;
    uint16_t crc = 0;
    if (reg_num == 1)
    {
        buf[1] = 6;
        buf[2] = (start_addr >> 8) & 0xFF;
        buf[3] = start_addr & 0xFF;
        buf[4] = data[0] & 0xFF;
        buf[5] = data[1] & 0xFF;
        crc = crc16_calc(buf, 6);
        buf[6] = crc & 0xFF;
        buf[7] = (crc >> 8) & 0xFF;
        len = 8;
    }
    else
    {
        buf[1] = 16;
        buf[2] = (start_addr >> 8) & 0xFF;
        buf[3] = start_addr & 0xFF;
        buf[4] = (reg_num >> 8) & 0xFF;
        buf[5] = reg_num & 0xFF;
        buf[6] = (uint8_t)(reg_num * 2);
        for (int j = 0; j < buf[6]; j++)
        {
            buf[7 + j] = data[j] & 0xFF;
        }
        len = 7 + 2 * reg_num;
        crc = crc16_calc(buf, len);
        buf[len] = crc & 0xFF;
        buf[len + 1] = (crc >> 8) & 0xFF;
        len += 2;
    }

    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send  to inv ....");
        for (uint8_t i = 0; i < len; i++)
        {
            printf("<%02X> ", buf[i]);
        }
        printf("\n");
        printf("\n");
    }

    uart_write_bytes(fd, (const char *)buf, len);
    ASW_LOGI("E-MODBUS WRITE:\n");
    // hex_print(buf, len);

    memset(buf, 0, sizeof(buf));
    len = 8;
    ASW_LOGI("recv data modbus write BBBB.....");
    res = recv_bytes_frame_waitting(fd, buf, &len);

    /////////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", "receive from inv ...");
        for (uint8_t k = 0; k < len; k++)
        {
            printf("*%02X ", buf[k]);
        }

        printf("\n");
        printf("\n");
    }
    /////////////////////////////////////////
    if (res == 0)
    {
        if (reg_num == 1)
        {
            if (len == 8)
            {
                if (buf[1] == 6)
                {
                    return ASW_OK;
                }
            }
        }
        else
        {
            if (len == 8)
            {
                if (buf[1] == 16)
                {
                    return ASW_OK;
                }
            }
        }
    }

    return ASW_FAIL;
}
//---------------------------------------------------//
/** *********************
 * fc: 0x03 or 0x04
 * modbus function code
 * read input or holding registers
 */
static int8_t modbus_read(int fd, uint8_t slave_id, uint8_t fc, uint16_t start_addr, uint16_t reg_num, uint16_t *res_data)
{
    uint8_t buf[257] = {0};
    uint16_t len = 0;
    int res = 0;
    uint16_t crc = 0;

    buf[0] = slave_id;
    buf[1] = fc;
    buf[2] = (start_addr >> 8) & 0xFF;
    buf[3] = start_addr & 0xFF;
    buf[4] = (reg_num >> 8) & 0xFF;
    buf[5] = reg_num & 0xFF;
    crc = crc16_calc(buf, 6);
    buf[6] = crc & 0xFF;
    buf[7] = (crc >> 8) & 0xFF;

    if (g_asw_debug_enable == 1)
    {

        ESP_LOGI("-S-", "send  to inv  readcmd....");
        for (uint8_t i = 0; i < 8; i++)
        {
            printf("<%02X> ", buf[i]);
        }
        printf("\n");
    }

    uart_write_bytes(fd, (const char *)buf, 8);
    // hex_print(buf, 8);
    memset(buf, 0, sizeof(buf));
    len = reg_num * 2 + 5;

    ASW_LOGI("recv data modbus read BBBB.....");
    res = recv_bytes_frame_waitting(fd, buf, &len);
    /////////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", " receive from inv...");
        for (uint8_t k = 0; k < len; k++)
        {
            printf("*%02X ", buf[k]);
        }

        printf("\n");
    }
    /////////////////////////////////////////

    if (res == ASW_OK)
    {
        if (buf[0] == slave_id && buf[1] == fc)
        {
            if (len == 2 + 1 + 2 * reg_num + 2 && buf[2] == 2 * reg_num)
            {
                for (int j = 0; j < reg_num; j++)
                {
                    res_data[j] = (buf[3 + 2 * j] << 8) + buf[4 + 2 * j];
                }
                return ASW_OK;
            }
        }
    }

    return ASW_FAIL;
}
//------------------------------------------------------------------//
int8_t modbus_write_inv(uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint16_t *data)
{
    if (slave_id == 0)
        return modbus_write_broadcast(UART_NUM_1, start_addr, reg_num, data);
    else
        return modbus_write(UART_NUM_1, slave_id, start_addr, reg_num, data);
}

int8_t modbus_write_inv_bit8(uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint8_t *data)
{

    return modbus_write_bit8(UART_NUM_1, slave_id, start_addr, reg_num, data);
}

//-----------------------------------------------------------------//
int8_t modbus_holding_read_inv(uint8_t slave_id, uint16_t start_addr, uint16_t reg_num, uint16_t *data)
{
    return modbus_read(UART_NUM_1, slave_id, 0x03, start_addr, reg_num, data);
}
