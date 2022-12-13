#include "estore_com.h"
#include "asw_mutex.h"

static const char *TAG = "estore_com.c";

// Bat_arr_t g_bat_arr = {0}; /// Lanstick-MultiInv +

int md_decode_bat_data(uint8_t *buf, int reg_num, Batt_data *bat_data_ptr)
{
    uint32_t temp = 0;
    uint16_t buffer[128] = {0};
    uint16_t mindex = 0;

    for (int j = 0; j < reg_num; j++)
    {
        buffer[j] = (buf[j * 2] << 8) + buf[j * 2 + 1];
    }

    uint16_t status_comm_batt = 0;
    status_comm_batt = buffer[6]; /// status of communication between dps and battery
    temp = (buffer[0] << 16);
    temp = (temp | buffer[1]);
    bat_data_ptr->ppv_batt = temp;

    temp = (buffer[2] << 16);
    temp = (temp | buffer[3]);
    bat_data_ptr->etoday_pv_batt = temp;

    temp = (buffer[4] << 16);
    temp = (temp | buffer[5]);
    bat_data_ptr->etotal_pv_batt = temp;

    bat_data_ptr->status_comm_batt = status_comm_batt;
    bat_data_ptr->status_batt = buffer[7];
    bat_data_ptr->error1_batt = buffer[8];
    bat_data_ptr->warn1_batt = buffer[12];

    bat_data_ptr->v_batt = buffer[16];
    bat_data_ptr->i_batt = (int16_t)buffer[17];

    temp = (buffer[18] << 16);
    temp = (temp | buffer[19]);
    bat_data_ptr->p_batt = (int32_t)temp;

    bat_data_ptr->t_batt = (int16_t)buffer[20];
    bat_data_ptr->SOC_batt = buffer[21];
    bat_data_ptr->SOH_batt = buffer[22];
    bat_data_ptr->i_in_limit_batt = buffer[23];
    bat_data_ptr->i_out_limit_batt = buffer[24];

    temp = (buffer[25] << 16);
    temp = (temp | buffer[26]);
    bat_data_ptr->etd_in_batt = temp;

    temp = (buffer[27] << 16);
    temp = (temp | buffer[28]);
    bat_data_ptr->etd_out_batt = temp;

    temp = (buffer[29] << 16);
    temp = (temp | buffer[30]);
    bat_data_ptr->etd_AC_used_batt = temp;

    temp = (buffer[31] << 16);
    temp = (temp | buffer[32]);
    bat_data_ptr->etd_AC_feed_batt = temp;

    bat_data_ptr->v_EPS_batt = buffer[33]; ///!< voltage of EPS
    bat_data_ptr->i_EPS_batt = buffer[34]; ///!< current of EPS
    bat_data_ptr->f_EPS_batt = buffer[35]; ///!< frequency of EPS

    temp = (buffer[36] << 16);
    temp = (temp | buffer[37]);
    bat_data_ptr->p_ac_EPS_batt = temp;

    temp = (buffer[38] << 16);
    temp = (temp | buffer[39]);
    bat_data_ptr->p_ra_EPS_batt = temp;

    temp = (buffer[40] << 16);
    temp = (temp | buffer[41]);
    bat_data_ptr->etd_EPS_batt = temp;

    temp = (buffer[42] << 16);
    temp = (temp | buffer[43]);
    bat_data_ptr->eto_EPS_batt = temp;

    if (status_comm_batt != 0x0A)
    {
        bat_data_ptr->v_batt = 0;
        bat_data_ptr->i_batt = 0;
        bat_data_ptr->p_batt = 0;
        bat_data_ptr->t_batt = 0;
    }

#if TRIPHASE_ARM_SUPPORT //增加三相内容
                         /// payload_3phase_u_i
    mindex = 44;
    uint8_t i;
    uint32_t tmp1 = 0, tmp2 = 0;
    for (i = 0; i < 3; i++)
    {
        bat_data_ptr->v_i_phase_ESP[i].iVol = buffer[mindex++]; // 31645
        bat_data_ptr->v_i_phase_ESP[i].iCur = buffer[mindex++]; // 31646
    }

    /// payload_3phase_p_q
    for (i = 0; i < 3; i++)
    {
        tmp1 = buffer[mindex++] << 16;
        tmp2 = buffer[mindex++];
        temp = tmp1 | tmp2;
        bat_data_ptr->p_q_phase_ESP[i].pac = temp; // 31645

        tmp1 = buffer[mindex++] << 16;
        tmp2 = buffer[mindex++];
        temp = tmp1 | tmp2;
        bat_data_ptr->p_q_phase_ESP[i].qac = temp; // 31646
    }

    // ac_3phase_u_i
    for (i = 0; i < 3; i++)
    {
        tmp1 = buffer[mindex++] << 16;
        tmp2 = buffer[mindex++];
        temp = tmp1 | tmp2;
        bat_data_ptr->p_q_phase_AC[i].pac = temp; // 31663

        tmp1 = buffer[mindex++] << 16;
        tmp2 = buffer[mindex++];
        temp = tmp1 | tmp2;
        bat_data_ptr->p_q_phase_AC[i].qac = temp; // 31665
    }

#endif

    return 0;
}

int md_read_battery_data(Batt_data *bat_data_ptr)
{
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};
    mb_req.fd = UART_NUM_1;

    mb_req.slave_id = 0x03;
    mb_req.start_addr = 0x0640;

#if TRIPHASE_ARM_SUPPORT
    mb_req.reg_num = 0x4A; // 0x2C=74
    mb_res.len = 74 * 2 + 5;

#else
    mb_req.reg_num = 0x2C; // 0x2C=44
    mb_res.len = 44 * 2 + 5;

#endif

    int res = asw_read_registers(&mb_req, &mb_res, 0x04);
    if (res < 0)
        return -1;

    md_decode_bat_data(&mb_res.frame[3], mb_req.reg_num, bat_data_ptr);

    return 0;
}

////////////// LanStick-MultilInv ///////////
int asw_md_read_battery_data(uint8_t modbus_addr, Batt_data *bat_data_ptr)
{
    mb_req_t mb_req = {0};
    mb_res_t mb_res = {0};
    mb_req.fd = UART_NUM_1;

    mb_req.slave_id = modbus_addr;
    mb_req.start_addr = 0x0640;
    mb_req.reg_num = 0x2C; // 0x2C=44

    mb_res.len = 44 * 2 + 5;
    int res = asw_read_registers(&mb_req, &mb_res, 0x04);
    if (res < 0)
        return -1;

    md_decode_bat_data(&mb_res.frame[3], mb_req.reg_num, bat_data_ptr);

    return 0;
}

/** 读取电池（如果有储能机）,地址3*/
#if 0 // tgl mark -
void read_bat_if_has_estore(void)
{
    if (is_com_has_estore() == 1)
    {
        Batt_data data = {0};
        int res = md_read_battery_data(&data);
        if (res == 0)
        {
            write_global_var(GLOBAL_BAT_DATA, &data);
        }
    }
}
#endif
/////////////   LanStick-MultilInv ////////////
//读取多台逆变器的电表数据
void asw_read_bat_arr_data(Inverter *inv_ptr)
{
    uint8_t m_inv_md_id = 0;
    // #if DEBUG_PRINT_ENABLE

    ASW_LOGI("read bat data -sn:%s-", inv_ptr->regInfo.sn);
    // #endif
    m_inv_md_id = inv_ptr->regInfo.modbus_id;

    if (inv_ptr->regInfo.mach_type <= 10 || inv_ptr->status != 1) // estore && online
    {
        ASW_LOGW(" the type or state is no right ,no data to read.");
        return;
    }

    Batt_data data = {0};
    /////////////////////////////////////////////////
    Bat_arr_t mBatsArryDatas = {0};
    read_global_var(GLOBAL_BATTERY_DATA, &mBatsArryDatas);

    int res = asw_md_read_battery_data(m_inv_md_id, &data);

    if (res != 0)
    {
        ESP_LOGW("-- Read Bat data Warn---", " return inv:%d-res:%d ",m_inv_md_id,res);
        return;
    }

    for (uint8_t k = 0; k < INV_NUM; k++)
    {

        // printf("g_bat_arr:%d:%s\n", k, mBatsArryDatas[k].sn);
        if (strcmp(mBatsArryDatas[k].sn, inv_ptr->regInfo.sn) == 0)
        {

            memcpy(&mBatsArryDatas[k].batdata, &data, sizeof(Batt_data));

            ASW_LOGI("Update data to g_bat_arr %s!!!", mBatsArryDatas[k].sn);

            break;
        }
    }

    write_global_var(GLOBAL_BATTERY_DATA, &mBatsArryDatas);
}
