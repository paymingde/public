#include "update.h"

static const char *TAG = "update.c";
int update_inverter = 0;
//==================================//
void update_firmware(char *url)
{

    printf("update firmware begin...\n");

    asw_ota_start(url);
}
//================================//

//////////////////////////////////////////////////////////
/*                  gd32-lanstick-update.c              */
//////////////////////////////////////////////////////////
#if TRIPHASE_ARM_SUPPORT
#define DATA_FRAME_LEN 4096

void update_info_parse(Updata_Info_St *update_info);
void trans_file_to_gd32(char *inv_fw_path, uint8_t slave_id);
rst_code comm_write_data_combox(Inv_cmd cmd, const void *data_ptr, unsigned short len);
rst_code zs_write_start_frame_combox(unsigned char *data_ptr, unsigned short w_len);
rst_code zs_write_end_frame_combox(unsigned char *data_ptr, unsigned short w_len);
rst_code zs_write_data_frame_combox(unsigned char *data_ptr, unsigned short w_len);
#endif
//////////////////////////////////////////////////////////

#if !TRIPHASE_ARM_SUPPORT
void update_info_parse(Updata_Info_St *update_info)
{
    char *head_ptr = NULL;
    if (strstr(update_info->file_name, "master") != NULL)
    {
        head_ptr = update_info->file_name + strlen("master");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        // memset(update_info->file_path, 0, sizeof(update_info->file_path));
        // sprintf(update_info->file_path, "/data/asw/%s", update_info->file_name);

        update_info->file_type = 1;
    }
    else if (strstr(update_info->file_name, "safety") != NULL)
    {
        head_ptr = update_info->file_name + strlen("safety");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        // memset(update_info->file_path, 0, sizeof(update_info->file_path));
        // sprintf(update_info->file_path, "/data/asw/%s", update_info->file_name);

        update_info->file_type = 4;
    }
    else if (strstr(update_info->file_name, "slave") != NULL)
    {
        head_ptr = update_info->file_name + strlen("slave");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        // memset(update_info->file_path, 0, sizeof(update_info->file_path));
        // sprintf(update_info->file_path, "/data/asw/%s", update_info->file_name);

        update_info->file_type = 2;
    }
}
#endif
//----------------------------------//
// p2p
char p2p_upgrade_inv_enable = 0;
char inv_upgrade_file_type = 0;

rst_code zs_write_start_frame(uint8_t *data_ptr, uint32_t w_len)
{

    unsigned int sum = 0;
    unsigned short len = 0;
    unsigned int i = 0;
    data_ptr[0] = 0xAA;
    data_ptr[1] = 0x55;
    data_ptr[2] = 0x01; // 0x01
    data_ptr[3] = 0x00; // 0x00

    if (p2p_upgrade_inv_enable == 0) // broadcast dest addr
    {
        data_ptr[4] = 0x00;
        data_ptr[5] = 0xFF;
    }

    data_ptr[6] = 0x14; // control code
    data_ptr[7] = 0x01; // function code
    len = w_len + 1 + 8;

    for (i = 0; i < len; i++)
    {
        sum += data_ptr[i];
    }

    data_ptr[len++] = (unsigned char)((sum >> 8) & 0xff); // check sum high
    data_ptr[len++] = (unsigned char)(sum & 0xff);        // check sum low

    /////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send: scan start... ");

        for (uint8_t i = 0; i < len; i++)
        {
            printf("<%02X> ", data_ptr[i]);
        }
        printf("\n");
    }

    ////////////////////////////////////
    uart_write_bytes(UART_NUM_1, data_ptr, len);

    if (p2p_upgrade_inv_enable == 0)
        return 0;
    else
    {
        uint8_t res_buf[256] = {0};
        uint16_t res_len = 0;

        recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);
        //////////////////////////////////////
        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-R-", "receive scan...");
            for (uint8_t i = 0; i < res_len; i++)
            {
                printf("*%02X ", res_buf[i]);
            }
            printf("\n");
        }
        ////////////////////////////////////
        if (check_sum(res_buf, res_len) == 0 && res_len >= 13)
        {
            if (res_buf[6] == 0x14 && res_buf[7] == 0x81 && memcmp(res_buf + 2, data_ptr + 4, 2) == 0)
            {
                if (res_buf[8] >= 2)
                {
                    if (res_buf[9] == data_ptr[9] && res_buf[10] == 0x06)
                    {
                        printf("start ack ok\n");
                        return 0;
                    }
                }
            }
        }
        return -1;
    }
}
//-------------------------------//

rst_code zs_write_data_frame(uint8_t *data_ptr, uint32_t w_len)
{

    unsigned int sum = 0;
    unsigned short len = 0;
    unsigned int i = 0;
    data_ptr[0] = 0xAA;
    data_ptr[1] = 0x55;
    data_ptr[2] = 0x01;              // 0x01
    data_ptr[3] = 0x00;              // 0x00
    if (p2p_upgrade_inv_enable == 0) // broadcast dest addr
    {
        data_ptr[4] = 0x00;
        data_ptr[5] = 0xFF;
    }

    data_ptr[6] = 0x14; // control code
    data_ptr[7] = 0x02; // function code
    len = w_len + 1 + 8;
    for (i = 0; i < len; i++)
    {
        sum += data_ptr[i];
    }

    data_ptr[len++] = (unsigned char)((sum >> 8) & 0xff); // check sum high
    data_ptr[len++] = (unsigned char)(sum & 0xff);        // check sum low

    // #if DEBUG_PRINT_ENABLE
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send scan cmd...\n");
        for (i = 0; i < len; i++)
        {
            printf("<%02X>", data_ptr[i]);
        }
        printf("\n");
    }

    // #endif
    uart_write_bytes(UART_NUM_1, data_ptr, len);

    if (p2p_upgrade_inv_enable == 0)
        return 0;
    else
    {
        uint8_t res_buf[256] = {0};
        uint16_t res_len = 0;
        // int res ;
        recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);

        //////////////////////////////////////
        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-R-", "receive scan...\n");
            for (uint8_t i = 0; i < res_len; i++)
            {
                printf("*%02X ", res_buf[i]);
            }
            printf("\n");
        }
        ////////////////////////////////////
        if (check_sum(res_buf, res_len) == 0 && res_len >= 15)
        {
            if (res_buf[6] == 0x14 && res_buf[7] == 0x82 && memcmp(res_buf + 2, data_ptr + 4, 2) == 0)
            {
                if (res_buf[8] >= 4)
                {
                    if (res_buf[9] == data_ptr[9] && memcmp(res_buf + 10, data_ptr + 10, 2) == 0 && res_buf[12] == 0x06)
                    {
                        printf("data ack ok\n");
                        return 0;
                    }
                }
            }
        }
        return -1;
    }
}
//-------------------------------//
rst_code zs_write_end_frame(uint8_t *data_ptr, uint32_t w_len)
{
    unsigned int sum = 0;
    unsigned short len = 0;
    unsigned int i = 0;
    data_ptr[0] = 0xAA;
    data_ptr[1] = 0x55;
    data_ptr[2] = 0x01;              // 0x01
    data_ptr[3] = 0x00;              // 0x00
    if (p2p_upgrade_inv_enable == 0) // broadcast dest addr
    {
        data_ptr[4] = 0x00;
        data_ptr[5] = 0xFF;
    }
    data_ptr[6] = 0x14; // control code
    data_ptr[7] = 0x03; // function code
    len = w_len + 1 + 8;
    for (i = 0; i < len; i++)
    {
        sum += data_ptr[i];
    }

    data_ptr[len++] = (unsigned char)((sum >> 8) & 0xff); // check sum high
    data_ptr[len++] = (unsigned char)(sum & 0xff);        // check sum low

    // #if DEBUG_PRINT_ENABLE
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send scan getsn cmd...\n");
        for (i = 0; i < len; i++)
        {
            printf("<%02X>", data_ptr[i]);
        }
        printf("\n");
    }

    // #endif

    uart_write_bytes(UART_NUM_1, data_ptr, len);

    if (p2p_upgrade_inv_enable == 0)
        return 0;
    else
    {
        uint8_t res_buf[256] = {0};
        uint16_t res_len = 0;
        int res = recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);
        //////////////////////////////////////
        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-R-", "receive scan...\n");
            for (uint8_t i = 0; i < res_len; i++)
            {
                printf("*%02X ", res_buf[i]);
            }
            printf("\n");
        }
        ////////////////////////////////////
        switch (inv_upgrade_file_type)
        {
        case 1: // master
        {
            if (check_sum(res_buf, res_len) == 0 && res_len >= 15)
            {
                if (res_buf[6] == 0x14 && res_buf[7] == 0x83 && memcmp(res_buf + 2, data_ptr + 4, 2) == 0)
                {
                    if (res_buf[8] >= 4)
                    {
                        if (res_buf[9] == 0x06)
                        {
                            printf("end ack ok\n");
                            return 0;
                        }
                    }
                }
            }
        }
        break;
        case 2: // slave
        {
            if (check_sum(res_buf, res_len) == 0 && res_len >= 15)
            {
                if (res_buf[6] == 0x14 && res_buf[7] == 0x83 && memcmp(res_buf + 2, data_ptr + 4, 2) == 0)
                {
                    if (res_buf[8] >= 4)
                    {
                        if (res_buf[10] == 0x06)
                        {
                            printf("end ack ok\n");
                            return 0;
                        }
                    }
                }
            }
        }
        break;
        case 4: // safety
        {
            if (check_sum(res_buf, res_len) == 0 && res_len >= 15)
            {
                if (res_buf[6] == 0x14 && res_buf[7] == 0x83 && memcmp(res_buf + 2, data_ptr + 4, 2) == 0)
                {
                    if (res_buf[8] >= 4)
                    {
                        if (res_buf[12] == 0x06)
                        {
                            printf("end ack ok\n");
                            return 0;
                        }
                    }
                }
            }
        }
        break;

        default:
            break;
        }

        return -1;
    }
}
// p2p
rst_code zs_set_inv_baudrate(/*int fd,*/ unsigned char *data_ptr, unsigned short w_len)
{
    unsigned int sum = 0;
    unsigned short len = 0;
    unsigned int i = 0;
    data_ptr[0] = 0xAA;
    data_ptr[1] = 0x55;
    data_ptr[2] = 0x01;              // 0x01
    data_ptr[3] = 0x00;              // 0x00
    if (p2p_upgrade_inv_enable == 0) // broadcast dest addr
    {
        data_ptr[4] = 0x00;
        data_ptr[5] = 0xFF;
    }
    data_ptr[6] = 0x14; // control code
    data_ptr[7] = 0x04; // function code
    len = w_len + 8 + 1;
    for (i = 0; i < len; i++)
    {
        sum += data_ptr[i];
    }

    data_ptr[len++] = (unsigned char)((sum >> 8) & 0xff); // check sum high
    data_ptr[len++] = (unsigned char)(sum & 0xff);        // check sum low

    uart_write_bytes(UART_NUM_1, data_ptr, len);
    ////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send scan cmdEnd...:\n");
        for (i = 0; i < len; i++)
        {
            printf("<%02X>", data_ptr[i]);
        }
        printf("\n");
    }
    ////////////////////////////////////

    if (p2p_upgrade_inv_enable == 0)
        return 0;
    else
    {
        uint8_t res_buf[256] = {0};
        uint16_t res_len = 0;
        // int res ;
        recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);
        //////////////////////////////////////
        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-R-", "receive scan...\n");
            for (uint8_t i = 0; i < res_len; i++)
            {
                printf("*%02X ", res_buf[i]);
            }
            printf("\n");
        }
        ////////////////////////////////////

        if (check_sum(res_buf, res_len) == 0 && res_len >= 13)
        {
            if (res_buf[6] == 0x14 && res_buf[7] == 0x84 && memcmp(res_buf + 2, data_ptr + 4, 2) == 0)
            {
                if (res_buf[8] >= 2)
                {
                    if (data_ptr[9] == res_buf[9] && res_buf[10] == 0x06)
                    {
                        printf("baudrate set ack ok\n");
                        return 0;
                    }
                }
            }
        }

        return -1;
    }
}

//====================================//
int get_update_file_name(char *url, char *file_name)
{
    int i = 0;
    char savename[32] = {0};
    ESP_LOGI(TAG, "inverter update url %d \n", strlen(url));

    if (!strlen(url))
        return -1;
    for (i = strlen(url); i > 0; i--)
        if (url[i - 1] == '/')
        {
            ASW_LOGI("%d name %s \n", i, &url[i]);
            memcpy(savename, &url[i], strlen(url) - i);
            break;
        }
    sprintf(file_name, "/home/%s", savename);

    return 0;
}

//================================//
// [tgl mark] fd 没用到

rst_code comm_write_data(/*int fd,*/ Inv_cmd cmd, const void *data_ptr, uint32_t len)
{

    rst_code ret = ERR_BAD_DATA;

    switch (cmd)
    {
    case CMD_WRITE_START_FRAME: // 10
        ret = zs_write_start_frame(/*fd, */ (uint8_t *)data_ptr, len);
        break;

    case CMD_WRITE_DATA_FRAME: // 11
        ret = zs_write_data_frame(/*fd,*/ (uint8_t *)data_ptr, len);
        break;
    case CMD_READ_FRAME_RES: // 12
        ret = zs_write_end_frame(/*fd,*/ (uint8_t *)data_ptr, 0);
        break;
    case CMD_SET_INV_BAUDRATE:
        ret = zs_set_inv_baudrate(/*fd,*/ (uint8_t *)data_ptr, len);
    default:
        break;
    }
    return ret;
}

//===============p2p===

void update_inv_broadcast(char *inv_fw_path)
{
    printf("first enter into update inverter FW \n");
    unsigned char j, len = 0;
    unsigned short frame_num = 0;
    FILE *fp = NULL;
    Updata_Info_St info = {0};

    unsigned int r_size, send_ratio = 0;
    unsigned char buffer[256] = {0}; /*0~6:command area; 7~253:function code + length + data+ checksum; */
    unsigned int file_len = 0;
    unsigned int send_len = 0;
    // unsigned char up_err_cnt = 0;
    // int timeout_cnt = 0;
    // int fd = 1;

    update_inverter = 100;

    char inv_fw[32] = {0};
    uint8_t i = 0;
    for (i = strlen(inv_fw_path); i > 0; i--)
    {
        if (inv_fw_path[i - 1] == '/')
        {
            printf("%d version name %s \n", i, &inv_fw_path[i]);
            memcpy(inv_fw, &inv_fw_path[i], strlen(inv_fw_path) - i);
            break;
        }
    }

    memcpy(info.file_name, inv_fw, strlen(inv_fw));
    update_info_parse(&info);
    inv_upgrade_file_type = info.file_type;
    p2p_upgrade_inv_enable = 0;
    // memcpy(info.file_path, inv_fw, strlen(inv_fw));
    memcpy(info.file_path, inv_fw_path, strlen(inv_fw_path));

    printf("info.file_name ---%s--- %d \n", info.file_name, strlen(inv_fw));
    printf("info.file_path ---%s--- \n", info.file_path);
    printf("info.file_type %d \n", info.file_type);

    flush_serial_port(UART_NUM_1);

    memcpy(info.file_path, "/home/inv.bin", sizeof("/home/inv.bin"));
    fp = fopen(info.file_path, "rb+");
    if (fp != NULL)
    {
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        /*---------------------------start frame------------------------------------*/
        buffer[8] = 0x14;           // data length
        buffer[9] = info.file_type; // file type
        /* file length */
        buffer[10] = file_len >> 24 & 0XFF;
        buffer[11] = file_len >> 16 & 0XFF;
        buffer[12] = file_len >> 8 & 0XFF;
        buffer[13] = file_len & 0XFF;
        /* Start frame number */
        buffer[14] = 0x00;
        buffer[15] = 0x00;
        len = 0x14;
        /* MC value */

        memcpy(buffer + 16, info.part_num, 13); //+++++++++++++++++++++++

        for (j = 0; j < 3; j++)
        {
            comm_write_data(/*fd, */ CMD_WRITE_START_FRAME, buffer, len);
            sleep(1);
        }

        /*---------------------------data frame------------------------------------*/
        memset(buffer, 0x00, 256);
        frame_num = 0;
        while (send_len < file_len && !feof(fp))
        {
            /* Feed watchdog */

            /* File type */
            buffer[9] = info.file_type; // file type

            /* Start frame number */
            buffer[10] = (frame_num >> 8 & 0XFF);
            buffer[11] = (frame_num & 0XFF);
            frame_num++;
            r_size = fread(buffer + 12, 1, 228, fp);
            printf("data len: %d\n", r_size);
            if (r_size > 0)
            {
                send_len += r_size;
                buffer[8] = (r_size + 3); // data length
                comm_write_data(/*fd,*/ CMD_WRITE_DATA_FRAME, buffer, (r_size + 3));
                send_ratio = (send_len * 1000 / file_len);
                printf("send %s: %d.%d%%\r\n", info.file_name, send_ratio / 10, send_ratio % 10);
                usleep(360000);
            }
            // usleep(200);//1402
        }

        fclose(fp);
        remove(info.file_path);
        /* Send Brodcast message */
        usleep(4000);
        /*---------------------------end frame------------------------------------*/
        memset(buffer, 0x00, 256);
        for (j = 0; j < 10; j++)
        {
            comm_write_data(/*fd,*/ CMD_READ_FRAME_RES, buffer, 0);
            sleep(2);
        }
        // if (up_err_cnt >= 3)
        // {
        //     remove(info.file_path);
        //     up_err_cnt = 0;
        // }
        remove(info.file_path);

        if (info.file_type < 3)
        {
            ESP_LOGI("-- UpLoad Inv Finished ---","Upgrade inv end Wait 60S for INV to restart\r\n");
            sleep(60);
            esp_restart();
            //: TODO REBOOT 添加wifistick重启在这里
            // free_all(0);
            // exit(0);
        }
    }
    else
    {
        printf("Open upgrade inv FW faile:%s", info.file_path);
    }
    remove(info.file_path);
    printf("Upgrade inv FW ok %s exit\n", info.file_path);
    esp_restart();
}

void update_one_inv_with_ack(char *inv_fw_path, uint8_t slave_id)
{
    printf("first enter into update inverter FW \n");
    unsigned char j, len = 0;
    unsigned short frame_num = 0;
    FILE *fp = NULL;
    Updata_Info_St info = {0};

    unsigned int r_size, send_ratio = 0;
    unsigned char buffer[256] = {0}; /*0~6:command area; 7~253:function code + length + data+ checksum; */
    unsigned int file_len = 0;
    unsigned int send_len = 0;
    // unsigned char up_err_cnt = 0;
    // int timeout_cnt = 0;
    // int fd = 1;
    int res = 0;

    update_inverter = 100;

    char inv_fw[32] = {0};
    uint8_t i = 0;
    for (i = strlen(inv_fw_path); i > 0; i--)
    {
        if (inv_fw_path[i - 1] == '/')
        {
            printf("%d version name %s \n", i, &inv_fw_path[i]);
            memcpy(inv_fw, &inv_fw_path[i], strlen(inv_fw_path) - i); //[tgl mark]
            break;
        }
    }

    memcpy(info.file_name, inv_fw, strlen(inv_fw));
    update_info_parse(&info);
    inv_upgrade_file_type = info.file_type;
    p2p_upgrade_inv_enable = 1;
    // memcpy(info.file_path, inv_fw, strlen(inv_fw));
    memcpy(info.file_path, inv_fw_path, strlen(inv_fw_path));

    printf("info.file_name ---%s--- %d \n", info.file_name, strlen(inv_fw));
    printf("info.file_path ---%s--- \n", info.file_path);
    printf("info.file_type %d \n", info.file_type);

    flush_serial_port(UART_NUM_1);

    memcpy(info.file_path, "/home/inv.bin", sizeof("/home/inv.bin"));
    /* ----------------------------set baudrate --------------------------------*/
    /* dest addr */
    buffer[4] = 0x00;
    buffer[5] = slave_id;

    buffer[8] = 0x0E;           // data length
    buffer[9] = info.file_type; // file type

    len = 0x0E;
    /* MC value */

    memcpy(buffer + 10, info.part_num, 13);

    for (j = 0; j < 10; j++)
    {
        res = comm_write_data(/*fd,*/ CMD_SET_INV_BAUDRATE, buffer, len);
        sleep(1);
        if (res == 0)
        {
            if (g_p2p_mode == 1)
            {
                /** esp32 baudrate set to 38400 after send 1404 frame ***********************/
                if (uart_set_baudrate(UART_NUM_1, 38400) == ESP_OK)
                {
                    printf("esp32 baudrate set 38400 ok\n");
                    sleep(1);
                }
                else
                {
                    printf("esp32 baudrate set 38400 fail\n");
                    res = -1;
                }
                /** *************************************************************************/
            }
            break;
        }
    }
    if (res != 0)
    {
        printf("send baudrate lock frame 10 times fail\n");
        return;
    }
    /*---------------------------start frame------------------------------------*/
    fp = fopen(info.file_path, "rb+");
    if (fp != NULL)
    {
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        memset(buffer, 0x00, 256);
        /* dest addr */
        buffer[4] = 0x00;
        buffer[5] = slave_id;

        buffer[8] = 0x14;           // data length
        buffer[9] = info.file_type; // file type
        /* file length */
        buffer[10] = file_len >> 24 & 0XFF;
        buffer[11] = file_len >> 16 & 0XFF;
        buffer[12] = file_len >> 8 & 0XFF;
        buffer[13] = file_len & 0XFF;
        /* Start frame number */
        buffer[14] = 0x00;
        buffer[15] = 0x00;
        len = 0x14;
        /* MC value */

        memcpy(buffer + 16, info.part_num, 13);

        for (j = 0; j < 10; j++)
        {
            res = comm_write_data(/*fd,*/ CMD_WRITE_START_FRAME, buffer, len);
            sleep(1);
            if (res == 0)
                break;
        }
        if (res != 0)
        {
            printf("send start frame 10 times fail\n");
            return;
        }

        /*---------------------------data frame------------------------------------*/
        memset(buffer, 0x00, 256);
        frame_num = 0;
        while (send_len < file_len && !feof(fp))
        {
            /* dest addr */
            buffer[4] = 0x00;
            buffer[5] = slave_id;

            /* File type */
            buffer[9] = info.file_type; // file type

            /* Start frame number */
            buffer[10] = (frame_num >> 8 & 0XFF);
            buffer[11] = (frame_num & 0XFF);
            frame_num++;
            r_size = fread(buffer + 12, 1, 228, fp);
            printf("data len: %d\n", r_size);
            if (r_size > 0)
            {
                send_len += r_size;
                buffer[8] = (r_size + 3); // data length
                for (j = 0; j < 10; j++)
                {
                    res = comm_write_data(/*fd,*/ CMD_WRITE_DATA_FRAME, buffer, (r_size + 3));
                    send_ratio = (send_len * 1000 / file_len);
                    printf("send %s: %d.%d%%\r\n", info.file_name, send_ratio / 10, send_ratio % 10);
                    usleep(360000); // 360000
                    if (res == 0)
                        break;
                }
                if (res != 0)
                {
                    printf("send data frame 10 times fail\n");
                    return;
                }
            }
            // usleep(200);//1402
        }

        fclose(fp);
        remove(info.file_path);
        /* Send Brodcast message */
        usleep(4000);
        /*---------------------------end frame------------------------------------*/
        memset(buffer, 0x00, 256);
        for (j = 0; j < 10; j++)
        {
            /* dest addr */
            buffer[4] = 0x00;
            buffer[5] = slave_id;
            res = comm_write_data(/*fd,*/ CMD_READ_FRAME_RES, buffer, 0);
            sleep(2);
            if (res == 0)
                break;
        }
        if (res != 0)
        {
            printf("send end frame 10 times fail\n");
            return;
        }

        // if (up_err_cnt >= 3)
        // {
        //     remove(info.file_path);
        //     up_err_cnt = 0;
        // }
        remove(info.file_path);

        if (info.file_type < 3)
        {
            printf("Upgrade inv end Wait 60S for INV to restart\r\n");
            sleep(60);
            esp_restart();
            //: TODO REBOOT 添加wifistick重启在这里
            // free_all(0);
            // exit(0);
        }
    }
    else
    {
        printf("Open upgrade inv FW faile:%s", info.file_path);
    }
    remove(info.file_path);
    printf("Upgrade inv FW ok %s exit\n", info.file_path);
    esp_restart();
}

extern Inv_arr_t inv_arr;

#if !TRIPHASE_ARM_SUPPORT
void inv_update(char *inv_fw_path)
{
    printf("inv real num is: %d ******\n", g_num_real_inv);
    if (g_num_real_inv == 1 && g_p2p_mode > 0)
    {
        for (int j = 0; j < INV_NUM; j++)
        {
            uint8_t modbus_id = inv_arr[j].regInfo.modbus_id;
            if (modbus_id >= 3 && modbus_id <= 247)
            {
                update_one_inv_with_ack(inv_fw_path, modbus_id);

                esp_restart();
            }
        }
    }
    else
    {
        update_inv_broadcast(inv_fw_path);
    }
}
#else

void trans_file_to_gd32_broadcast(char *inv_fw_path, char slave_id)
{
    printf("first enter into update inverter FW \n");
    unsigned int j, len = 0;
    unsigned int frame_num = 0;
    FILE *fp = NULL;
    Updata_Info_St info = {0};

    unsigned int r_size, send_ratio = 0;

    unsigned char buffer[5000] = {0}; /*0~6:command area; 7~253:function code + length + data+ checksum; */

    unsigned int file_len = 0;
    unsigned int send_len = 0;
    unsigned int frame_len = 0;
    unsigned char up_err_cnt = 0;
    int timeout_cnt = 0;
    int fd = 1;
    int res = 0;
    uint16_t crc = 0;
    update_inverter = 100;

    char inv_fw[32] = {0};
    uint8_t i = 0;
    for (i = strlen(inv_fw_path); i > 0; i--)
    {
        if (inv_fw_path[i - 1] == '/')
        {
            printf("%d version name %s \n", i, &inv_fw_path[i]);
            memcpy(inv_fw, &inv_fw_path[i], strlen(inv_fw_path) - i);
            break;
        }
    }

    memcpy(info.file_name, inv_fw, strlen(inv_fw));
    update_info_parse(&info);

    memcpy(info.file_path, inv_fw_path, strlen(inv_fw_path));

    printf("info.file_name ---%s--- %d \n", info.file_name, strlen(inv_fw));
    printf("info.file_path ---%s--- \n", info.file_path);
    printf("info.file_type %d \n", info.file_type);

    flush_serial_port(UART_NUM_1);

    memcpy(info.file_path, "/home/inv.bin", sizeof("/home/inv.bin"));
    /*---------------------------start frame------------------------------------*/
    fp = fopen(info.file_path, "rb+");
    if (fp != NULL)
    {
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        // file_len = 4567;
        fseek(fp, 0, SEEK_SET);
        memset(buffer, 0x00, 5000);
        /* dest addr */
        buffer[0] = 0x66;
        buffer[1] = 0x99;
        buffer[2] = 0x0D;
        buffer[3] = 0x0A;
        buffer[4] = 0x66;
        buffer[5] = 0x99;
        buffer[6] = 0x0D;
        buffer[7] = 0x0A;
        /** fc */
        buffer[8] = slave_id;
        buffer[9] = 0x01;
        /** length */
        buffer[10] = 0x00;
        buffer[11] = 0x00;
        buffer[12] = 0x00;
        buffer[13] = 0x34; // 8+2+4+4+32+2=52
        /** file size*/
        buffer[14] = file_len >> 24 & 0XFF;
        buffer[15] = file_len >> 16 & 0XFF;
        buffer[16] = file_len >> 8 & 0XFF;
        buffer[17] = file_len & 0XFF;

        /** file name*/
        memcpy(&buffer[18], info.file_name, 32);
        // memcpy(&buffer[18], FILENAME, strlen(FILENAME));
        len = 50;
        crc = crc16_calc(buffer, len);
        buffer[len] = crc & 0xFF;
        len++;
        buffer[len] = (crc >> 8) & 0xFF;
        len++;

        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-S-", "send arm update start....\n");
            for (int i = 0; i < len; i++)
            {
                printf("<%02x> ", buffer[i]);
            }
            printf("\n");
        }

        uart_write_bytes(UART_NUM_1, buffer, len);
        sleep(1);

        // printf("send start frame for the ----------------- %dth time\n", j);

        /*---------------------------data frame------------------------------------*/

        frame_num = 1;
        while (send_len < file_len && !feof(fp))
        {
            memset(buffer, 0x00, 5000);
            /* dest addr */
            buffer[0] = 0x66;
            buffer[1] = 0x99;
            buffer[2] = 0x0D;
            buffer[3] = 0x0A;
            buffer[4] = 0x66;
            buffer[5] = 0x99;
            buffer[6] = 0x0D;
            buffer[7] = 0x0A;
            /** fc */
            buffer[8] = slave_id;
            buffer[9] = 0x02;

            r_size = fread(buffer + 22, 1, DATA_FRAME_LEN, fp);
            printf("data len: %d\n", r_size);
            if (r_size > 0)
            {
                send_len += r_size;
                frame_len = r_size + 8 + 2 + 4 + 4 + 4 + 2;

                /** length */
                buffer[10] = (frame_len >> 24 & 0xFF);
                buffer[11] = (frame_len >> 16 & 0xFF);
                buffer[12] = (frame_len >> 8 & 0xFF);
                buffer[13] = (frame_len >> 0 & 0xFF); // 8+2+4+4+4+4096+2=4120
                /** data component*/
                /** frame sq */
                buffer[14] = (frame_num >> 24 & 0XFF);
                buffer[15] = (frame_num >> 16 & 0XFF);
                buffer[16] = (frame_num >> 8 & 0XFF);
                buffer[17] = (frame_num & 0XFF);
                frame_num++;
                /** data len */
                buffer[18] = (r_size >> 24 & 0xFF);
                buffer[19] = (r_size >> 16 & 0xFF);
                buffer[20] = (r_size >> 8 & 0xFF);
                buffer[21] = (r_size >> 0 & 0xFF);
                /** data data */
                len = 22;
                len += r_size;
                /** crc */
                crc = crc16_calc(buffer, len);
                buffer[len] = crc & 0xff; // check sum high
                len++;
                buffer[len] = (crc >> 8) & 0xff; // check sum low
                len++;
                ASW_LOGI("total frame len:%d", len);

                if (g_asw_debug_enable == 1)
                {
                    ESP_LOGI("-S-", "send arm update data....\n");
                    for (int i = 0; i < len; i++)
                    {
                        printf("<%02x> ", buffer[i]);
                    }
                    printf("\n");
                }

                uart_write_bytes(UART_NUM_1, buffer, len);
                send_ratio = (send_len * 1000 / file_len);
                printf("send %s: %d.%d%%\r\n", info.file_name, send_ratio / 10, send_ratio % 10);
            }
            usleep(700000); // 1402
        }

        fclose(fp);
        remove(info.file_path);
        usleep(4000);
        /*---------------------------end frame------------------------------------*/
        memset(buffer, 0x00, 256);
        /* dest addr */
        buffer[0] = 0x66;
        buffer[1] = 0x99;
        buffer[2] = 0x0D;
        buffer[3] = 0x0A;
        buffer[4] = 0x66;
        buffer[5] = 0x99;
        buffer[6] = 0x0D;
        buffer[7] = 0x0A;
        /** fc */
        buffer[8] = slave_id;
        buffer[9] = 0x03;
        /** frame len */
        buffer[10] = 0x00; // 8+2+4+0+2
        buffer[11] = 0x00;
        buffer[12] = 0x00;
        buffer[13] = 0x10;

        /** crc */
        crc = crc16_calc(buffer, 14);
        buffer[14] = crc & 0xff;        // check sum high
        buffer[15] = (crc >> 8) & 0xff; // check sum low
        len = 16;

        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-S-", "send arm update finish....\n");
            for (int i = 0; i < len; i++)
            {
                printf("<%02x> ", buffer[i]);
            }
            printf("\n");
        }

        uart_write_bytes(UART_NUM_1, buffer, len);
        sleep(2);

        // ARM update wait(3min)
        if (info.file_type == 3)
        {

            printf("Upgrade arm end Wait 3min for INV to restart\r\n");
            for (uint8_t i = 0; i < 180; i++)
            {
                ESP_LOGI("update arm...", "%d %%", (uint16_t)(i * 100.0 / 180.0));
                sleep(1);
            }
        }
        else if (info.file_type < 4)
        {
            printf("Upgrade inv end Wait 12min for INV to restart\r\n");
            for (uint8_t i = 0; i < 720; i++)
            {
                ESP_LOGI("update inv...", "%d %%", (uint16_t)(i * 100.0 / 720.0));
                sleep(1);
            }
        }
    }
    else
    {
        ESP_LOGW("-- UpGrade ERROR --", "Open upgrade inv FW faile:%s\n", info.file_path);
    }
    remove(info.file_path);
    ASW_LOGI("Upgrade inv FW ok %s exit\n", info.file_path);
}
//////////////////////////////////////////////////
void inv_update(uint8_t modbus_id, char *inv_fw_path)
{
    printf("inv real num is: %d ******\n", g_num_real_inv);
    char mod_id = 0;

    if (g_num_real_inv == 1)
    {
        mod_id = 0x03;
    }
    else
    {
        mod_id = modbus_id;
    }

    if (g_p2p_mode == 1)
    {
        if (mod_id >= 3 && mod_id <= 247)
        {
            trans_file_to_gd32(inv_fw_path, mod_id);
            esp_restart();
        }
    }
    else
    {

        trans_file_to_gd32_broadcast(inv_fw_path, 0xFF);
        esp_restart();
    }
}

#endif

//------------------------------------------//
void download_task(void *pvParameters)
{
    update_url mqtt_url = {0};

    memcpy(&mqtt_url, (update_url *)pvParameters, sizeof(update_url));

    ESP_LOGI(TAG, "fff--%c--%d-\n", mqtt_url.down_url[0], mqtt_url.down_url[0] != 'h');
    if (mqtt_url.down_url[0] != 'h')
    {
        char str[240] = {0};
        memcpy(str, mqtt_url.down_url, strlen(mqtt_url.down_url) + 1); //[tgl change] 添加+1
        sprintf(mqtt_url.down_url, "%s%s", "http://", str);
    }
    ESP_LOGI(TAG, "new get download_url----->-%d--%s------- \n", strlen(mqtt_url.down_url), mqtt_url.down_url);

    if (strlen(mqtt_url.down_url) > 10)
    {
        long lSize;
        // char buf[100] = {0};
        char save_name[64] = {0};
        // char temp_name[64]={0};
        int file_len = 0;

        if (mqtt_url.update_type == 1)
            update_firmware(mqtt_url.down_url);

        if (mqtt_url.update_type == 2)
        {
            get_update_file_name(mqtt_url.down_url, save_name);
            ESP_LOGI(TAG, "save name %s \n", save_name);
            // memcmp(temp_name, save_name, sizeof(temp_name));

            http_get_file(mqtt_url.down_url, "/home/inv.bin", &file_len);
            FILE *fp = fopen("/home/inv.bin", "rb+");
            ESP_LOGI(TAG, "fp----------------------------%d \n", fp != NULL);
            if (fp != NULL)
            {
                // ESP_LOGI(TAG,"***************** read\n");
                fseek(fp, 0, SEEK_END);
                lSize = ftell(fp); //文件总字节数

                fclose(fp);
                ESP_LOGI(TAG, "httphead len %d  downloadfile len %ld \n", file_len, lSize);
                send_msg(3, save_name, strlen(save_name), NULL);
            }
        }
    }
    sleep(2);
    vTaskDelete(NULL);
}

////////////////////////////////////////////////////////////
/*                  gd32-arm-new update.c                 */
////////////////////////////////////////////////////////////

#if TRIPHASE_ARM_SUPPORT

// void inv_update(char *inv_fw_path)
// {
//     uint8_t modbus_id = 0;
//     printf("inv real num is: %d ******\n", g_num_real_inv);
//     if (g_num_real_inv == 1)
//     {
//         for (int j = 0; j < INV_NUM; j++)
//         {
//             modbus_id = inv_arr[j].regInfo.modbus_id;
//             // uint8_t modbus_id = 3;
//             if (modbus_id >= 3 && modbus_id <= 247)
//             {
//                 trans_file_to_gd32(inv_fw_path, modbus_id); /***********替换***/
//                 // esp_restart();
//             }
//         }
//     }
//     else // TGL MARK TODO add support multilInv
//     {
//         // update_inv_broadcast(inv_fw_path);
//         // printf("--- TODO ADD THE MultilInv to update ...\n");
//     }
// }

void trans_file_to_gd32(char *inv_fw_path, uint8_t slave_id)
{
    printf("first enter into update inverter FW \n");
    unsigned int j, len = 0;
    unsigned int frame_num = 0;
    FILE *fp = NULL;
    Updata_Info_St info = {0};

    unsigned int r_size, send_ratio = 0;
    // char *buffer = NULL;
    // buffer = malloc(16384);
    // if (buffer == NULL)
    // {
    //     printf("memory malloc error\n");
    //     return 0;
    // }

    unsigned char buffer[5000] = {0}; /*0~6:command area; 7~253:function code + length + data+ checksum; */

    unsigned int file_len = 0;
    unsigned int send_len = 0;
    unsigned int frame_len = 0;
    unsigned char up_err_cnt = 0;
    int timeout_cnt = 0;
    int fd = 1;
    int res = 0;
    uint16_t crc = 0;
    update_inverter = 100;

    char inv_fw[32] = {0};
    uint8_t i = 0;
    for (i = strlen(inv_fw_path); i > 0; i--)
        if (inv_fw_path[i - 1] == '/')
        {
            printf("%d version name %s \n", i, &inv_fw_path[i]);
            memcpy(inv_fw, &inv_fw_path[i], strlen(inv_fw_path) - i);
            break;
        }

    memcpy(info.file_name, inv_fw, strlen(inv_fw));
    update_info_parse(&info);
    inv_upgrade_file_type = info.file_type;
    p2p_upgrade_inv_enable = 1;
    // memcpy(info.file_path, inv_fw, strlen(inv_fw));
    memcpy(info.file_path, inv_fw_path, strlen(inv_fw_path));

    printf("info.file_name ---%s--- %d \n", info.file_name, strlen(inv_fw));
    printf("info.file_path ---%s--- \n", info.file_path);
    printf("info.file_type %d \n", info.file_type);

    flush_serial_port(UART_NUM_1);

    memcpy(info.file_path, "/home/inv.bin", sizeof("/home/inv.bin"));
    /*---------------------------start frame------------------------------------*/
    fp = fopen(info.file_path, "rb+");
    if (fp != NULL)
    {
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        // file_len = 4567;
        fseek(fp, 0, SEEK_SET);
        memset(buffer, 0x00, 5000);
        /* dest addr */
        buffer[0] = 0x66;
        buffer[1] = 0x99;
        buffer[2] = 0x0D;
        buffer[3] = 0x0A;
        buffer[4] = 0x66;
        buffer[5] = 0x99;
        buffer[6] = 0x0D;
        buffer[7] = 0x0A;
        /** fc */
        // buffer[8] = 0x00;
        buffer[8] = slave_id;

        buffer[9] = 0x01;
        /** length */
        buffer[10] = 0x00;
        buffer[11] = 0x00;
        buffer[12] = 0x00;
        buffer[13] = 0x34; // 8+2+4+4+32+2=52
        /** file size*/
        buffer[14] = file_len >> 24 & 0XFF;
        buffer[15] = file_len >> 16 & 0XFF;
        buffer[16] = file_len >> 8 & 0XFF;
        buffer[17] = file_len & 0XFF;

        /** file name*/
        memcpy(&buffer[18], info.file_name, 32);
        // memcpy(&buffer[18], FILENAME, strlen(FILENAME));
        len = 50;
        crc = crc16_calc(buffer, len);
        buffer[len] = crc & 0xFF;
        len++;
        buffer[len] = (crc >> 8) & 0xFF;
        len++;

        /////////////////////////////////////////
        if (g_asw_debug_enable == 1)
        {
            ESP_LOGI("-S-", "start sending............");

            for (int i = 0; i < len; i++)
            {
                printf("%02x ", buffer[i]);
            }
            printf("\n");
        }
        /////////////////////////////////////////////////
        for (uint8_t j = 0; j < 10; j++)
        {
            res = comm_write_data_combox(CMD_WRITE_START_FRAME, buffer, len);
            sleep(1);

            ASW_LOGI("send start frame for the ----------------- %dth time\n", j);
            if (res == 0)
                break;
        }
        if (res != 0)
        {
            ESP_LOGW(TAG, "send start frame 10 times fail\n");
            return;
        }

        /*---------------------------data frame------------------------------------*/

        frame_num = 1;
        while (send_len < file_len && !feof(fp))
        {
            memset(buffer, 0x00, 5000);
            /* dest addr */
            buffer[0] = 0x66;
            buffer[1] = 0x99;
            buffer[2] = 0x0D;
            buffer[3] = 0x0A;
            buffer[4] = 0x66;
            buffer[5] = 0x99;
            buffer[6] = 0x0D;
            buffer[7] = 0x0A;
            /** fc */
            buffer[8] = 0x00;
            buffer[9] = 0x02;

            r_size = fread(buffer + 22, 1, DATA_FRAME_LEN, fp);
            printf("data len: %d\n", r_size);
            if (r_size > 0)
            {
                send_len += r_size;
                frame_len = r_size + 8 + 2 + 4 + 4 + 4 + 2;

                /** length */
                buffer[10] = (frame_len >> 24 & 0xFF);
                buffer[11] = (frame_len >> 16 & 0xFF);
                buffer[12] = (frame_len >> 8 & 0xFF);
                buffer[13] = (frame_len >> 0 & 0xFF); // 8+2+4+4+4+4096+2=4120
                /** data component*/
                /** frame sq */
                buffer[14] = (frame_num >> 24 & 0XFF);
                buffer[15] = (frame_num >> 16 & 0XFF);
                buffer[16] = (frame_num >> 8 & 0XFF);
                buffer[17] = (frame_num & 0XFF);
                frame_num++;
                /** data len */
                buffer[18] = (r_size >> 24 & 0xFF);
                buffer[19] = (r_size >> 16 & 0xFF);
                buffer[20] = (r_size >> 8 & 0xFF);
                buffer[21] = (r_size >> 0 & 0xFF);
                /** data data */
                len = 22;
                len += r_size;
                /** crc */
                crc = crc16_calc(buffer, len);
                buffer[len] = crc & 0xff; // check sum high
                len++;
                buffer[len] = (crc >> 8) & 0xff; // check sum low
                len++;
                printf("total frame len:%d", len);
                for (j = 0; j < 10; j++)
                {
                    res = comm_write_data_combox(CMD_WRITE_DATA_FRAME, buffer, len);
                    send_ratio = (send_len * 1000 / file_len);
                    printf("send %s: %d.%d%%\r\n", info.file_name, send_ratio / 10, send_ratio % 10);
                    // usleep(10000); //360000
                    if (res == 0)
                        break;
                }
                if (res != 0)
                {
                    printf("send data frame 10 times fail\n");
                    return;
                }
            }
            // usleep(20000);//1402
        }

        fclose(fp);
        remove(info.file_path);
        usleep(4000);
        /*---------------------------end frame------------------------------------*/
        memset(buffer, 0x00, 256);
        /* dest addr */
        buffer[0] = 0x66;
        buffer[1] = 0x99;
        buffer[2] = 0x0D;
        buffer[3] = 0x0A;
        buffer[4] = 0x66;
        buffer[5] = 0x99;
        buffer[6] = 0x0D;
        buffer[7] = 0x0A;
        /** fc */
        buffer[8] = 0x00;
        buffer[9] = 0x03;
        /** frame len */
        buffer[10] = 0x00; // 8+2+4+0+2
        buffer[11] = 0x00;
        buffer[12] = 0x00;
        buffer[13] = 0x10;

        /** crc */
        crc = crc16_calc(buffer, 14);
        buffer[14] = crc & 0xff;        // check sum high
        buffer[15] = (crc >> 8) & 0xff; // check sum low
        len = 16;

        for (j = 0; j < 10; j++)
        {
            res = comm_write_data_combox(CMD_READ_FRAME_RES, buffer, len);
            sleep(2);
            if (res == 0)
                break;
        }
        if (res != 0)
        {
            printf("send end frame 10 times fail\n");
            return;
        }

        // if (up_err_cnt >= 3)
        // {
        //     remove(info.file_path);
        //     up_err_cnt = 0;
        // }
        /*remove(info.file_path);*/

        if (info.file_type < 4)
        {
            printf("Upgrade inv end Wait 600S for INV to restart\r\n");
            sleep(60 * 10);
            // esp_restart();
            //: TODO REBOOT 添加wifistick重启在这里
            // free_all(0);
            // exit(0);
        }
    }
    else
    {
        printf("Open upgrade inv FW faile:%s\n", info.file_path);
    }
    remove(info.file_path);
    printf("Upgrade inv FW ok %s exit\n", info.file_path);
    // esp_restart();
    //  free(buffer);
}

void update_info_parse(Updata_Info_St *update_info)
{
    char *head_ptr = NULL;
    if (strstr(update_info->file_name, "master") != NULL)
    {
        head_ptr = update_info->file_name + strlen("master");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        // memset(update_info->file_path, 0, sizeof(update_info->file_path));
        // sprintf(update_info->file_path, "/data/asw/%s", update_info->file_name);

        update_info->file_type = 1;
    }
    else if (strstr(update_info->file_name, "safety") != NULL)
    {
        head_ptr = update_info->file_name + strlen("safety");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        // memset(update_info->file_path, 0, sizeof(update_info->file_path));
        // sprintf(update_info->file_path, "/data/asw/%s", update_info->file_name);

        update_info->file_type = 4;
    }
    else if (strstr(update_info->file_name, "comm") != NULL)
    {
        update_info->file_type = 3;
    }
    else if (strstr(update_info->file_name, "slave") != NULL)
    {
        head_ptr = update_info->file_name + strlen("slave");
        memset(update_info->part_num, 0, sizeof(update_info->part_num));
        strncpy(update_info->part_num, head_ptr, 13);

        // memset(update_info->file_path, 0, sizeof(update_info->file_path));
        // sprintf(update_info->file_path, "/data/asw/%s", update_info->file_name);

        update_info->file_type = 2;
    }
}

rst_code comm_write_data_combox(Inv_cmd cmd, const void *data_ptr, unsigned short len)
{
    rst_code ret = ERR_BAD_DATA;

    switch (cmd)
    {
    case CMD_WRITE_START_FRAME:
        ret = zs_write_start_frame_combox((uint8_t *)data_ptr, len);
        break;
    case CMD_WRITE_DATA_FRAME: // 11
        ret = zs_write_data_frame_combox((uint8_t *)data_ptr, len);
        break;
    case CMD_READ_FRAME_RES: // 12
        ret = zs_write_end_frame_combox((uint8_t *)data_ptr, len);
        break;
    default:
        break;
    }
    return ret;
}

rst_code zs_write_end_frame_combox(uint8_t *data_ptr, unsigned short w_len)
{
    unsigned int sum = 0;
    unsigned short len = w_len;
    unsigned int i = 0;

    uart_write_bytes(UART_NUM_1, data_ptr, len);
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send broadcast update finish....");
        for (i = 0; i < len; i++)
        {
            printf("<%02X>", data_ptr[i]);
        }
        printf("\n");
    }

    uint8_t res_buf[256] = {0};
    uint16_t res_len = 0;
    int res = recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);
    //////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", "receive scan...");
        for (uint8_t i = 0; i < res_len; i++)
        {
            printf("*%02X ", res_buf[i]);
        }
        printf("\n");
    }
    ////////////////////////////////////
    if (res_len == 12 && crc16_calc(res_buf, res_len) == 0)
    {
        if (memcmp(res_buf, data_ptr, 8) == 0)
        {
            if (res_buf[8] == 0x00 && res_buf[9] == 0x03)
            {
                printf("end ack ok\n");
                return 0;
            }
        }
    }
    return -1;
}

rst_code zs_write_data_frame_combox(uint8_t *data_ptr, unsigned short w_len)
{
    unsigned int sum = 0;
    unsigned short len = w_len;
    unsigned int i = 0;

    //////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send broadcast update data....");
        for (i = 0; i < len; i++)
        {
            printf("<%02X>", data_ptr[i]);
        }
        printf("\n");
    }

    ////////////////////////////////////

    uart_write_bytes(UART_NUM_1, data_ptr, len);

    uint8_t res_buf[256] = {0};
    uint16_t res_len = 0;
    int res = recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);
    //////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", "receive scan...");
        for (uint8_t i = 0; i < res_len; i++)
        {
            printf("*%02X ", res_buf[i]);
        }
        printf("\n");
    }
    ////////////////////////////////////
    if (res_len == 12 && crc16_calc(res_buf, res_len) == 0)
    {
        if (memcmp(res_buf, data_ptr, 8) == 0)
        {
            if (res_buf[8] == 0x00 && res_buf[9] == 0x02)
            {
                printf("data ack ok\n");
                return 0;
            }
        }
    }
    return -1;
}

rst_code zs_write_start_frame_combox(uint8_t *data_ptr, unsigned short w_len)
{
    unsigned int sum = 0;
    unsigned short len = w_len;
    unsigned int i = 0;

    uart_write_bytes(UART_NUM_1, data_ptr, len);

    //////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-S-", "send broadcast update start....");
        for (i = 0; i < len; i++)
        {
            printf("<%02X>", data_ptr[i]);
        }
        printf("\n");
    }

    ////////////////////////////////////
    uint8_t res_buf[256] = {0};
    uint16_t res_len = 0;
    int res = recv_bytes_frame_waitting_nomd(UART_NUM_1, res_buf, &res_len);
    //////////////////////////////////////
    if (g_asw_debug_enable == 1)
    {
        ESP_LOGI("-R-", "receive scan...");
        for (uint8_t i = 0; i < res_len; i++)
        {
            printf("*%02X ", res_buf[i]);
        }
        printf("\n");
    }
    ////////////////////////////////////
    if (res_len == 12 && crc16_calc(res_buf, res_len) == 0)
    {
        if (memcmp(res_buf, data_ptr, 8) == 0)
        {
            if (res_buf[8] == 0x00 && res_buf[9] == 0x01)
            {
                ASW_LOGI("start ack ok\n");
                return 0;
            }
        }
    }
    return -1;
}
#endif