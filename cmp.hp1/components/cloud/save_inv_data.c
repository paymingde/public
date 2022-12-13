#include "save_inv_data.h"
#include "estore_cld.h"

#define MAX_HIST_DATA_SIZE 1024 * 2000 /** 2000 KB*/

static const char * TAG="save_in_data.c";
int net_com_reboot_flg = 0;
/********************************/

// void set_resetnet_reboot(void)
// {
//     net_com_reboot_flg =1;
// }

// int get_mqtt_status(void)  //[tgl mark]去掉 改为全局
// {
//     return g_state_mqtt_connect;
// }
//-------------------------------//
void check_resetnet_reboot(void)
{
    if (net_com_reboot_flg == 1)
    {
        ESP_LOGW("ESP_RESTART", " will restart by [check reset net] ");
        sleep(2);
        esp_restart();
    }
}
//--------------------------------//
int write_change_apconfig(void)
{
    if (access("/inv/configap", 0) != 0)
    {
        ESP_LOGE("write_lost_index", "configap not existed \n");

        FILE *fp = fopen("/inv/configap", "w");
        if (fp == NULL)
        {
            ESP_LOGE("ff", "Failed to open /inv/configap file");
            return -1;
        }
        fclose(fp);
    }
    return 0;
}

//--------------------------------
void check_external_reboot(void)
{
    if (inv_com_reboot_flg == 1)
    {
        ESP_LOGW("ESP_RESTART", " will restart by [check external] ");
        sleep(2);
        esp_restart();
    }
}

//-------------------------------//
int recive_invdata(int *flag, Inv_data *inv_data, int *lost_flag)
{
    Inv_data invdata = {0};
    static int lost_line = 0;
    static Inv_data tmp_invdata = {0};
    static int read_lost_time = 0;
    int res = 0;
    static int last_time_log = -5; //-5 means ini

    check_external_reboot();

    if (3 == *lost_flag)
    {
        if (0 == time_legal_parse())
        {
            if (write_lost_data(inv_data) < -1)
                force_flash_reformart("invdata");
        }
        *lost_flag = 0;

        check_resetnet_reboot();
    }

    if (mq0 != NULL)
    {

        //  ESP_LOGW("77777777","---------->>>>>>>>>>>>..recive_invdata,mq0 is not NULL");
        // if (xQueueReceive(mq0, &invdata, (TickType_t)10) == pdPASS)
        if (xQueueReceive(mq0, &invdata, (TickType_t)10) == pdPASS)
        {
            // ESP_LOGW("88888888:","mq0 get data ready to publish...");
            if ((g_state_mqtt_connect == 0) && (!netework_state || (g_monitor_state & INV_SYNC_PMU_STATE))) // net ok
            {

                *flag = 1;
                *inv_data = invdata;

                if ((last_time_log != 0 || last_time_log == -5))
                {
                    network_log(",[ok] MQTT CONNECT");
                    last_time_log = 0;
                }
            }
            else // no net ,save
            {
                if (0 == time_legal_parse())
                {
                    if (write_lost_data(&invdata) < -1)
                        force_flash_reformart("invdata");
                    else
                    {
                        if (check_lost_data() < -1)
                            if (check_inv_pv(&invdata) == 0)
                                force_flash_reformart("invdata");
                    }
                }

                if ((last_time_log == 0 || last_time_log == -5))
                {
                    network_log(",[err] MQTT CONNECT");
                    last_time_log = -10;
                }

                if ((g_monitor_state & INV_SYNC_PMU_STATE) || get_connect_stage() > 0) // v2.0.0
                {
                    static int mq_er_time = 0;
                    if (!mq_er_time)
                        mq_er_time = get_second_sys_time(); // esp_timer_get_time();

                    if (get_second_sys_time() - mq_er_time > 600 * 3) // v2.0.0 600->600*3    esp_timer_get_time() - publish_fail_time > 300 * 1000000)
                    {
                        ESP_LOGW("ESP_RESTART", " will restart by [recive_invdata time out 600*3s]");
                        sleep(1);
                        esp_restart();
                    }
                }

                check_resetnet_reboot();
            }
            return 0;
        }
        else
        {
            *flag = 0;
        }
    }

    if (g_state_mqtt_connect == 0 && g_stick_run_mode != Work_Mode_AP_PROV                             /* v2.0.0 add */
        && (!netework_state || (g_monitor_state & INV_SYNC_PMU_STATE)) && get_second_sys_time() > 300) // query data
    {
        *flag = 0;
        if ((get_second_sys_time() - read_lost_time) < 1)
            return -1;
        read_lost_time = get_second_sys_time();

        if (0 == *lost_flag)
            lost_line = read_lost_index();

        if (*lost_flag != 1)
        {
            write_lost_index(lost_line);
            // ASW_LOGI("read next line %d \n", *lost_flag);
            if ((res = read_lost_data(&invdata, &lost_line)) >= 0)
            {
                ASW_LOGI("read %d st line \n", lost_line);
                *flag = 1;
                *lost_flag = 1;
                *inv_data = invdata;
                tmp_invdata = invdata;
            }
            if (res == -2)
            {
                ASW_LOGI("read lost data crc error , contiue loop\n");
                *flag = 0;
                *lost_flag = 2;
                // write_lost_index(lost_line);
                memset(inv_data, 0, sizeof(Inv_data));
                return -2;
            }
            if (res == -1)
            {
                *flag = 0;
                *lost_flag = 0;
                // write_lost_index(lost_line);
                memset(inv_data, 0, sizeof(Inv_data));
                return -1;
            }
        }
        else
        {
            ASW_LOGI("copy preline %d line \n", lost_line);
            *inv_data = tmp_invdata;
            *flag = 1;
            *lost_flag = 1;
        }
    }
    return 0;
}

////////////////////////////////////////////
//---- Eng.Stg.Mch-lanstick + --------//
/** *****************************************************************************************************
 * 数据存入/inv/lost.db：
 * 每条格式： 长度+数据+CRC16
 *
 * 读取地址存入：/inv/index.db
 * 格式: 读取位置+CRC16
 *********************************************************************************************************/
static int get_hist_file_size(void)
{
    if (access("/inv/lost.db", 0) != 0)
    {
        // ESP_LOGE("get_hist_file_size", "access fail");
        return -1;
    }
    FILE *fp = fopen("/inv/lost.db", "ab+"); //"wb"
    if (fp == NULL)
    {
        return -2;
    }

    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    fclose(fp);
    return file_size;
}
static int write_hist_offset(int offset)
{
    int nwrite = 0;
    if (access("/inv/index.db", 0) != 0)
    {
        ESP_LOGE("write_lost_index", "access fail");
        // return -1;
    }

    FILE *fp = fopen("/inv/index.db", "wb"); //"wb"
    if (fp == NULL)
    {
        ESP_LOGE("write_lost_index", "open fail");
        return -2;
    }

    uint8_t buff[32] = {0};
    memcpy(buff, &offset, sizeof(int));
    uint16_t crc = crc16_calc(buff, 4);
    memcpy(buff + 4, &crc, 2);

    ESP_LOGE("write offset", "%d", offset);
    nwrite = fwrite(buff, sizeof(char), 6, fp);
    fclose(fp);
    if (nwrite == 6)
        return 0;
    else
    {
        return -2;
    }
}

static int read_hist_offset(void)
{
    int offset = 0;
    int nread = 0;
    if (access("/inv/index.db", 0) != 0)
    {
        ESP_LOGE("read_hist_offset", "access fail");
        return -1;
    }

    FILE *fp = fopen("/inv/index.db", "rb");
    if (fp == NULL)
    {
        ESP_LOGE("read_hist_offset", "open fail");
        return -2;
    }

    uint8_t buff[32] = {0};
    nread = fread(buff, sizeof(char), 6, fp);
    fclose(fp);
    if (nread == 6)
    {
        uint16_t recv_crc;
        memcpy(&recv_crc, buff + 4, 2);
        uint16_t crc = crc16_calc(buff, 4);
        if (crc == recv_crc)
        {
            memcpy(&offset, buff, sizeof(int));
            return offset;
        }
        else
        {
            remove("/inv/lost.db");
            write_hist_offset(0); /** 读取偏移复位*/
            return -1;
        }
    }
    return -1;
}

int write_hist_msg(char *msg, int olen)
{
    if (get_second_sys_time() < 240) // not save since start 4min
        return 0;
    uint16_t msg_len = olen;
    int nwrite = 0;
    if (access("/inv/lost.db", 0) != 0)
    {
        ESP_LOGE("write_hist_msg", "access fail");
        // return -1;
    }
    FILE *fp = fopen("/inv/lost.db", "ab+"); //"wb"
    if (fp == NULL)
    {
        ESP_LOGE("write_hist_msg", "open fail");
        return -2;
    }

    fseek(fp, 0, SEEK_END);
    ESP_LOGE("write_hist_msg", "hist file len  %ld \n", ftell(fp));

    /** 超出范围，清空*/
    if (ftell(fp) > MAX_HIST_DATA_SIZE) // 5 invs 7day 1827000
    {
        ESP_LOGE("write_hist_msg", "file overflow");
        fclose(fp);

        remove("/inv/lost.db");
        write_hist_offset(0); /** 读取偏移复位*/

        ESP_LOGE("write_hist_msg", "rm lost.db");

        fp = fopen("/inv/lost.db", "ab+"); //"wb"
        if (fp == NULL)
        {
            ESP_LOGE("write_hist_msg", "open 2rd fail");
            return -3;
        }
    }

    /** ab+强制末尾写入，写入不受fseek影响*/
    uint8_t buff[1536] = {0};
    uint16_t len = 0;
    memcpy(buff + len, &msg_len, 2);
    len += 2;
    memcpy(buff + len, msg, msg_len);
    len += msg_len;
    uint16_t crc = crc16_calc(buff, len);
    memcpy(buff + len, &crc, 2);
    len += 2;

    nwrite = fwrite(buff, sizeof(char), len, fp);

    fclose(fp);
    if (nwrite == len)
        return 0;
    return -1;
}

int read_hist_msg(char *msg, int *msg_len)
{
    int nread = 0;
    uint8_t buff[1536] = {0};
    uint16_t item_size = 0;
    uint16_t len = 0;

    int offset = read_hist_offset();
    int curr_file_size = get_hist_file_size();

    /** 如果文件不为空，但偏移不合法，则复位偏移*/
    if (offset < 0)
    {
        write_hist_offset(0);
        offset = read_hist_offset();
        if (offset < 0)
            return offset;
    }

    if (curr_file_size <= 0)
    {
        return curr_file_size == 0 ? -1 : curr_file_size;
    }

    /** 读取超出范围*/
    if (offset > curr_file_size - 1)
    {
        remove("/inv/lost.db");
        remove("/inv/index.db");
        return -1;
    }

    if (access("/inv/lost.db", 0) != 0)
    {
        ESP_LOGE("read_lost_data", "file don't existed \n");
        return -1;
    }

    FILE *fp = fopen("/inv/lost.db", "rb"); // read only
    if (fp == NULL)
    {
        ESP_LOGE("TAG", "Failed to open file for reading");
        // remove("/inv/lost.db");
        return -2;
    }

    /** 读取长度,数据本身长度*/
    fseek(fp, offset, SEEK_SET);
    nread = fread(&len, sizeof(char), 2, fp);
    if (nread != 2)
    {
        fclose(fp);
        return -1;
    }

    /** 读取：数据长度+数据内容+CRC*/
    fseek(fp, offset, SEEK_SET);
    nread = fread(buff, sizeof(char), 4 + len, fp); /** len+data+crc, len is data length*/
    if (nread != 4 + len)
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    uint16_t crc = crc16_calc(buff, len + 2);
    uint16_t crc_read = 0;
    memcpy(&crc_read, buff + 2 + len, 2);
    if (crc == crc_read)
    {
        if (msg != NULL)
        {
            memcpy(msg, buff + 2, len);
        }
        if (msg_len != NULL)
        {
            *msg_len = len;
        }
        return 0;
    }
    else
    {
        remove("/inv/lost.db");
        write_hist_offset(0); /** 读取偏移复位*/
    }
    return -1;
}

int is_hist_exist(void)
{
    int res = read_hist_msg(NULL, NULL);
    return res;
}

int hist_offset_next(void)
{
    int curr_file_size = 0;
    int res = 0;
    int offset = 0;

    int msg_len = 0;
    curr_file_size = get_hist_file_size();
    offset = read_hist_offset();
    if (offset < 0)
    {
        return offset;
    }
    if (curr_file_size <= 0)
        return curr_file_size == 0 ? -1 : curr_file_size;
    /** 读取当前这条的内容长度*/
    res = read_hist_msg(NULL, &msg_len);
    if (res < 0)
        return res;
    offset += (msg_len + 4); // 4: LEN+CRC
    /** 超出范围, 清空*/
    if (offset > curr_file_size - 1 && curr_file_size > 0 && offset > 0)
    {
        remove("/inv/lost.db");
        remove("/inv/index.db");
        return -1;
    }

    if (offset <= curr_file_size - 1 && curr_file_size > 0 && offset >= 0)
    {

        write_hist_offset(offset);
    }
    return 0;
}

int read_hist_payload(char *msg)
{
    int msg_len;
    int res = read_hist_msg(msg, &msg_len);
    if (res < -1)
    {
        force_flash_reformart("invdata");
        return -1;
    }
    return res;
}

/** invdata paylaod, non estore*/
int get_invdata_payload(Inv_data invdata, char *payload)
{
    uint8_t i;
    int datlen = sprintf(payload, "{\"Action\":\"UpdateInvData\",\"psn\":\"%s\",\"sn\":\"%s\","
                                  "\"mty\":%d,\"tmstp\":%d000,"
                                  "\"csq\":%d,"
                                  "\"smp\":5,\"tim\":\"%s\","
                                  "\"fac\":%d,\"pac\":%d,\"er\":%d,"
                                  "\"pf\":%d,\"cf\":%d,\"eto\":%d,\"etd\":%d,\"hto\":%d,",
                         DEMO_DEVICE_NAME, invdata.psn,
                         get_cld_mach_type(), invdata.rtc_time,
                         get_rssi(),
                         invdata.time,
                         invdata.fac, invdata.pac, invdata.error,
                         invdata.cosphi, invdata.col_temp, invdata.e_total, invdata.e_today, invdata.h_total);

    for (i = 0; i < 10; i++)
    {

        if ((invdata.PV_cur_voltg[i].iVol != 0xFFFF) && (invdata.PV_cur_voltg[i].iCur != 0xFFFF))
        {
            datlen += sprintf(payload + datlen, "\"v%d\":%d,\"i%d\":%d,",
                              i + 1, invdata.PV_cur_voltg[i].iVol,
                              i + 1, invdata.PV_cur_voltg[i].iCur);
        }
    }
    /* check 1~3 ac parameters*/
    for (i = 0; i < 3; i++)
    {
        if ((invdata.AC_vol_cur[i].iVol != 0xFFFF) && (invdata.AC_vol_cur[i].iCur != 0xFFFF))
        {
            datlen += sprintf(payload + datlen, "\"va%d\":%d,\"ia%d\":%d,",
                              i + 1, invdata.AC_vol_cur[i].iVol,
                              i + 1, invdata.AC_vol_cur[i].iCur);
        }
    }
    /* check 1~20 string current parameters*/
    for (i = 0; i < 20; i++)
    {
        if ((invdata.istr[i] != 0xFFFF))
        {

            datlen += sprintf(payload + datlen, "\"s%d\":%d,", i + 1, invdata.istr[i]);
        }
    }
    // Check_upload_unit16
    Check_upload_unit32("\"prc\":%d,", invdata.qac, payload, &datlen);
    Check_upload_unit32("\"sac\":%d,", invdata.sac, payload, &datlen);
    Check_upload_sint16("\"tu\":%d,", invdata.U_temp, payload, &datlen);
    Check_upload_sint16("\"tv\":%d,", invdata.V_temp, payload, &datlen);
    Check_upload_sint16("\"tw\":%d,", invdata.W_temp, payload, &datlen);
    Check_upload_sint16("\"cb\":%d,", invdata.contrl_temp, payload, &datlen);
    Check_upload_sint16("\"bv\":%d,", invdata.bus_voltg, payload, &datlen);
    // printf("[ payload 5 ]\n%s\n", payload);
    /*check 1~10 warn*/
    for (i = 0; i < 10; i++)
    {
        if (invdata.warn[i] != 0xFF)
        {
            datlen += sprintf(payload + datlen, "\"wn%d\":%d,", i + 1, invdata.warn[i]);
        }
    }
    /* itv field as the end of the body*/
    datlen += sprintf(payload + datlen, "\"itv\":%d}", GetMinute1((invdata.time[11] - 0x30) * 10 + invdata.time[12] - 0x30, (invdata.time[14] - 0x30) * 10 + invdata.time[15] - 0x30, 0));
    // printf("payload   %s\n", payload);
    return datlen;
}
/**
 * 即时数据处理:
 * 此输入参数兼有表示上次发布的结果之义:
 * 1.为空表示发布成功
 * 2.否则表示失败并需要存储*/
int read_instant_payload(char *msg)
{
    Inv_data invdata = {0};
    int res = 0;

    /** AP开启1小时后，重启以再次连接路由器*/
    check_external_reboot();

    /** 存储上一次未发成功的*/
    int last_len = strlen(msg);
    if (last_len > 0)
    {
        res = write_hist_msg(msg, last_len);
        memset(msg, 0, JSON_MSG_SIZE);
        if (res < -1)
        {
            force_flash_reformart("invdata");
        }
    }

    /** 接收新的数据*/
    if (mq0 != NULL)
    {
        if (xQueueReceive(mq0, &invdata, (TickType_t)10) == pdPASS)
        {
            ASW_LOGI("recv invdata stru ok %s\n", invdata.psn);
            char buf[1536] = {0};
            if (is_cld_has_estore() == 1)
            {
                get_estore_invdata_payload(&invdata, buf);
            }
            else
            {
                // get_invdata_payload(invdata, buf);
                ESP_LOGW(TAG, "--------  no support to Grid-connected inverter !\n");
            }

            int len = strlen(buf);
            if ((g_state_mqtt_connect == 0) && (!netework_state || (g_monitor_state && INV_SYNC_PMU_STATE))) // net ok
            {
                ASW_LOGI("net ok, send it\n");
                /** net ok, generate json, return it*/
                memset(msg, 0, JSON_MSG_SIZE);
                memcpy(msg, buf, len);
                return 0;
            }
            else
            {
                ESP_LOGW(TAG, "net error, save it\n");
                /** no net ,generate json, store, return -1*/
                if (0 == check_time_correct())
                {
                    res = write_hist_msg((char *)buf, strlen((char *)buf));
                    if (res < -1)
                    {
                        force_flash_reformart("invdata");
                    }
                }
            }
        }
    }

    return -1;
}
