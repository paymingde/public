
#include "datadecode.h"

static const char *TAG = "datadecode.c";

/*----------------------------------------------------------------------------*/
/*! \brief  This function decode the chanel data.
 *  \param  packet         [in], The received comm paket.
 *  \param  channel_table  [in],The real data decode table.
 *  \param  inv_data_ptr   [out],The real data decode table.
 *  \return none.
 *  \see
 *  \note
 */
/*----------------------------------------------------------------------------*/
//#define PRT_DATA
void md_decode_inv_data_content(uint8_t *packet, Inv_data *inv_data_ptr)
{
    uint8_t i = 0, j = 0, h = 0, k = 0;
    inv_data_ptr->grid_voltg = ((uint8_t)packet[0] << 8) | (uint8_t)packet[1];                                                          /// the grid voltage
    inv_data_ptr->e_today = ((uint8_t)packet[4] << 24) | (uint8_t)packet[5] << 16 | ((uint8_t)packet[6] << 8) | (uint8_t)packet[7];     ///< The accumulated kWh of that day
    inv_data_ptr->e_total = ((uint8_t)packet[8] << 24) | (uint8_t)packet[9] << 16 | ((uint8_t)packet[10] << 8) | (uint8_t)packet[11];   ///< Total energy to grid
    inv_data_ptr->h_total = ((uint8_t)packet[12] << 24) | (uint8_t)packet[13] << 16 | ((uint8_t)packet[14] << 8) | (uint8_t)packet[15]; ///< Total operation hours
    inv_data_ptr->status = (uint8_t)packet[17];                                                                                         ///< Inverter operation status
    inv_data_ptr->chk_tim = ((uint8_t)packet[18] << 8) | (uint8_t)packet[19];                                                           ///< the check time of inverter
    inv_data_ptr->col_temp = (packet[20] << 8) | packet[21];                                                                            ///< the cooling fin temperature
    inv_data_ptr->U_temp = (int16_t)(((uint8_t)packet[22] << 8) | (uint8_t)packet[23]);                                                 ///< the inverter U phase temperature
    inv_data_ptr->V_temp = (int16_t)(((uint8_t)packet[24] << 8) | (uint8_t)packet[25]);                                                 ///< the inverter V phase temperature
    inv_data_ptr->W_temp = (int16_t)(((uint8_t)packet[26] << 8) | (uint8_t)packet[27]);                                                 ///< the inverter W phase temperature
    inv_data_ptr->contrl_temp = (int16_t)(((uint8_t)packet[28] << 8) | (uint8_t)packet[29]);                                            ///< the inverter boost temperature
    inv_data_ptr->rsv_temp = (int16_t)(((uint8_t)packet[30] << 8) | (uint8_t)packet[31]);                                               ///< the reserve temperature
    inv_data_ptr->bus_voltg = ((uint8_t)packet[32] << 8) | (uint8_t)packet[33];                                                         ///< bus voltage
    inv_data_ptr->con_stu = ((uint8_t)packet[34] << 8) | (uint8_t)packet[35];                                                           ///< conntect status
#ifdef PRT_DATA
    printf("\r\ngrid_voltg:%d,e_today:%d,e_total:%d,\n"
           "h_total:%d,status:%d,chk_tim:%d,\n"
           "col_temp:%d,U_temp:%d,V_temp:%d,\n"
           "W_temp:%d,contrl_temp:%d,rsv_temp:%d,\n"
           "bus_voltg:%d,con_stu:%d\r\n",
           inv_data_ptr->grid_voltg, inv_data_ptr->e_today, inv_data_ptr->e_total,
           inv_data_ptr->h_total, inv_data_ptr->status, inv_data_ptr->chk_tim,
           inv_data_ptr->col_temp, inv_data_ptr->U_temp, inv_data_ptr->V_temp,
           inv_data_ptr->W_temp, inv_data_ptr->contrl_temp, inv_data_ptr->rsv_temp,
           inv_data_ptr->bus_voltg, inv_data_ptr->con_stu);
#endif
    // printf();
    /*10 mppt voltage and current*/

    for (i = 0; i < 10; i++)
    {
        inv_data_ptr->PV_cur_voltg[i].iVol = ((uint8_t)packet[36 + 4 * i] << 8) | (uint8_t)packet[36 + 1 + 4 * i];
        inv_data_ptr->PV_cur_voltg[i].iCur = ((uint8_t)packet[36 + 2 + 4 * i] << 8) | (uint8_t)packet[36 + 3 + 4 * i];

#ifdef PRT_DATA
        ASW_LOGI("PV_cur_voltg[%d].iVol:%d,PV_cur_voltg[%d].iCur:%d\r\n",
               i, inv_data_ptr->PV_cur_voltg[i].iVol, i, inv_data_ptr->PV_cur_voltg[i].iCur);
#endif
    }
    /*20 smart string currents*/
    for (j = 0; j < 20; j++)
    {
#ifdef PRT_DATA
        if ((j % 4) == 0)
        {
            printf("\r\n");
        }
#endif
        inv_data_ptr->istr[j] = ((uint8_t)packet[76 + 2 * j] << 8) | (uint8_t)packet[76 + 1 + 2 * j]; ///< string current 1~20
    }
#ifdef PRT_DATA
    printf("\r\n");
#endif
    /* AC voltage and current*/
    for (k = 0; k < 3; k++)
    {
        inv_data_ptr->AC_vol_cur[k].iVol = ((uint8_t)packet[116 + 4 * k] << 8) | (uint8_t)packet[116 + 1 + 4 * k];
        inv_data_ptr->AC_vol_cur[k].iCur = ((uint8_t)packet[116 + 2 + 4 * k] << 8) | (uint8_t)packet[116 + 3 + 4 * k];
#ifdef PRT_DATA
        printf("AC_vol_cur[%d].iVol:%d ,AC_vol_cur[%d].iCur:%d ,",
               k, inv_data_ptr->AC_vol_cur[k].iVol, k, inv_data_ptr->AC_vol_cur[k].iCur);
#endif
    }
    /*相电压*/
    inv_data_ptr->rs_voltg = ((uint8_t)packet[128] << 8) | (uint8_t)packet[129];
    inv_data_ptr->rt_voltg = ((uint8_t)packet[130] << 8) | (uint8_t)packet[131];
    inv_data_ptr->st_voltg = ((uint8_t)packet[132] << 8) | (uint8_t)packet[133];

    inv_data_ptr->fac = ((uint8_t)packet[134] << 8) | (uint8_t)packet[135];                                                             ///< AC frequence
    inv_data_ptr->sac = ((uint8_t)packet[136] << 24) | (uint8_t)packet[137] << 16 | ((uint8_t)packet[138] << 8) | (uint8_t)packet[139]; ///< Total power to grid(3Phase)
    inv_data_ptr->pac = ((uint8_t)packet[140] << 24) | (uint8_t)packet[141] << 16 | ((uint8_t)packet[142] << 8) | (uint8_t)packet[143]; ///< Total power to grid(3Phase)
    inv_data_ptr->qac = ((uint8_t)packet[144] << 24) | (uint8_t)packet[145] << 16 | ((uint8_t)packet[146] << 8) | (uint8_t)packet[147]; ///< Total power to grid(3Phase)
    inv_data_ptr->cosphi = (int8_t)packet[149];                                                                                         ///< the AC phase
    inv_data_ptr->pac_stu = (uint8_t)packet[151];                                                                                       ///< limit power status
    inv_data_ptr->err_stu = (uint8_t)packet[153];                                                                                       ///< error status
    inv_data_ptr->error = ((uint8_t)packet[154] << 8) | (uint8_t)packet[155];
    if (inv_data_ptr->error == 0xFFFF)
        inv_data_ptr->error = 0; ///< Failure description for status 'failure'
#ifdef PRT_DATA
    printf("\nrs_voltg:%d,rt_voltg:%d,st_voltg:%d,\n"
           "fac:%d,sac:%d,pac:%d,qac:%d,cosphi:%d,\n"
           "pac_stu:%d,err_stus:%d,error:%d\n",
           inv_data_ptr->rs_voltg, inv_data_ptr->rt_voltg, inv_data_ptr->st_voltg,
           inv_data_ptr->fac, inv_data_ptr->sac, inv_data_ptr->pac, inv_data_ptr->qac, inv_data_ptr->cosphi,
           inv_data_ptr->pac_stu, inv_data_ptr->err_stu, inv_data_ptr->error);
#endif

    /*10 warns*/
    for (h = 0; h < 10; h++)
    {
        inv_data_ptr->warn[h] = (uint8_t)packet[156 + 1 + 2 * h]; ///< Failure description for status 'failure'
        if (inv_data_ptr->warn[h] == 0xFF)
            inv_data_ptr->warn[h] = 0;
#ifdef PRT_DATA
        printf("warn[%d]:%d ", h, inv_data_ptr->warn[h]);
#endif
    }

#ifdef PRT_DATA
    printf("--encode end\r\n");
#endif
}

//---------------------------------------//
/*----------------------------------------------------------------------------*/
/*! \brief  This function decode the received buffer for register read ID_Info command.
 *  \param  packet_buffer   [in],the inverter's SN pointer.
 *  \param  len             [in],the buffer's length.
 *  \return int32_t         the decode result.
 *  \see
 *  \note   here must adding the inverter SN, because of there is no SN ,just has modbus address in default
 */
/*----------------------------------------------------------------------------*/
void md_decode_inv_Info(const uint8_t *data, InvRegister *inv_ptr)
{
    ASW_LOGI("start to decode the inverter info!!\r\n");

    // tyep (Byte 00~01)
    inv_ptr->type = data[1];

    // modbus address (Byte 02~03)
    inv_ptr->modbus_id = data[3];

    // Serial Number (Byte 4~35)
    memset(inv_ptr->sn, 0x00, sizeof(inv_ptr->sn));
    memcpy(inv_ptr->sn, data + 4, 32);
    // printf("****$$$$get inv sn:%d--%s\r\n",strlen(inv_ptr->device.sn),inv_ptr->device.sn);
    // mode name (Byte 36 ~ 51)
    memcpy(inv_ptr->mode_name, data + 36, 16);
    inv_ptr->mode_name[16] = '\0';
    ASW_LOGI("type:%02X,add:%d,sn:%s,mode_name:%s\r\n", inv_ptr->type, inv_ptr->modbus_id, inv_ptr->sn, inv_ptr->mode_name);

    // safety type(Byte 52 ~ 53)
    inv_ptr->safety_type = data[53];

    //////////////////////////

    if ((g_parallel_enable && g_host_modbus_id == inv_ptr->modbus_id) || (g_parallel_enable == 0 && g_host_modbus_id == 3))
    {
        if (inv_ptr->safety_type == 96 || inv_ptr->safety_type == 97 || inv_ptr->safety_type == 80)
        {
            g_safety_is_96_97 = 1;
        }
        else
        {
            g_safety_is_96_97 = 0;
        }

        ASW_LOGI(" g_safety_is_96_97: %d", g_safety_is_96_97);
    }

    /////////////////////////////////

    // VA rating (Byte 54~57)
    inv_ptr->rated_pwr = (uint32_t)((data[54] << 24) | (data[55] << 16) | (data[56] << 8) | (data[57]));
    ASW_LOGI("type:%d, add:%d, sn:%s, mode_name:%s, safety_type:%d, rated_pwr:%d; \r\n",
             inv_ptr->type, inv_ptr->modbus_id, inv_ptr->sn, inv_ptr->mode_name,
             inv_ptr->safety_type, inv_ptr->rated_pwr);

    // inverter master Firmware Ver (Byte 58~71)
    memcpy(inv_ptr->msw_ver, data + 58, 14);
    inv_ptr->msw_ver[14] = '\0';

    // inverter slave Firmware ver (Byte 72~85)
    memcpy(inv_ptr->ssw_ver, data + 72, 14);
    inv_ptr->ssw_ver[14] = '\0';

    // inverter safety Firmware Ver (Byte 86~99)
    memcpy(inv_ptr->tmc_ver, data + 86, 14);
    inv_ptr->tmc_ver[14] = '\0';

    // protocol version             (Byte 100~111)
    memcpy(inv_ptr->protocol_ver, data + 100, 12);
    inv_ptr->protocol_ver[12] = '\0';

    // Manufacturer (Byte 112~127)
    memcpy(inv_ptr->mfr, data + 112, 16);
    inv_ptr->mfr[16] = '\0';
    // Manufacturer (Byte 128~143)
    memcpy(inv_ptr->brand, data + 128, 16);
    inv_ptr->brand[16] = '\0';

    ASW_LOGI("msw_ver:%s, ssw_ver:%s, tmc_ver:%s, protocol_ver:%s, mfr:%s, brand:%s; \r\n",
             inv_ptr->msw_ver, inv_ptr->ssw_ver, inv_ptr->tmc_ver, inv_ptr->protocol_ver,
             inv_ptr->mfr, inv_ptr->brand);

    /**
     * mach_type:
     * 机种类型:
     * 1    单相机1-3KW
     * 2    单相机3-6KW
     * 3    三相机3-10KW
     * 4    三相机15-23KW
     * 5    三相机50-60KW
     * 11   单相储能1-3KW
     * 12   单相储能3-6KW
     * 13   三相纯3-10KW
     */
    inv_ptr->mach_type = data[145];

    // the inverter pv number (Byte 146~147)
    inv_ptr->pv_num = data[147] == 0xFF ? 0 : data[147];

    // the inverter string number (Byte 148~149)
    inv_ptr->str_num = data[149] == 0xFF ? 0 : data[149];
    /* ---------repetition---------------*/
    //        inv_ptr->status |= INV_SYNC_DEV_INDEX;
    //        inv_ptr->status |= INV_ONLINE_STATUS_INDEX;
    ASW_LOGI("mach_type:%d, pv_num:%d, str_NUM:%d\r\n", inv_ptr->mach_type, inv_ptr->pv_num, inv_ptr->str_num);
}
//---------------------------------------//
