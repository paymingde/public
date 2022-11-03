#include "meter_md_process.h"

static const char *TAG = "meter_md_processs.c";
uint16_t meter_pro[][8] = {
    /**
     * 0 --- start addr
     * 1 --- reg num
     * 2 --- pac
     * 3 --- e_in
     * 4 --- e_out
     * 5 --- qac
     * 6 --- sac
     * 7 --- pf
     * */
    /** 0,   1,  2, 3,  4,  5, 6, 7*/
    // {0x0034, 70, 0, 20, 22, 4, 2, 6},           // 0 estron 630 ct
    // {0x0034, 70, 0, 20, 22, 4, 2, 6},           // 1 estron 630 dc
    // {0x000C, 70, 0, 60, 62, 4, 2, 6},           // 2 estron 230
    // {0x000C, 70, 0, 60, 62, 4, 2, 6},           // 3 estron 220
    // {0x000C, 70, 0, 60, 62, 4, 2, 6},           // 4 estron 120
    // {0x0001, 20, 4, 10, 12, 0x00FF, 0x00FF, 6}, // 5 hu tai
    // {0x2000, 10, 2, 6, 8, 3, 4, 5}              // 6 ACTEL

    {0x0034, 70, 0, 20, 22, 8, 4, 10},          // 0 estron 630 ct
    {0x0034, 70, 0, 20, 22, 8, 4, 10},          // 1 estron 630 dc
    {0x000C, 70, 0, 60, 62, 12, 6, 18},         // 2 estron 230 ----------- confirmed
    {0x000C, 70, 0, 60, 62, 12, 6, 18},         // 3 estron 220
    {0x000C, 70, 0, 60, 62, 12, 6, 18},         // 4 estron 120
    {0x0001, 20, 4, 10, 12, 0x00FF, 0x00FF, 6}, // 5 hu tai
    {0x2000, 10, 2, 6, 8, 3, 4, 5},             // 6 ACTEL

#if TRIPHASE_ARM_SUPPORT
                                    //增加一个类型，第七行，如果行数不对，前后的内容中的下标都要同步更改
    {0x1921, 17, 0, 9, 11, 4, 2, 6}, // 11 AISWEI-INV-METER
#endif
};
int total_rate_power = 0;
int total_curt_power = 0;
int16_t meter_real_factory = 0;

uint32_t m_fast_read_pac = 0;

static int g_start_addr = 0x0;  /** 解析电表数据的modbus起始地址，是协议文档里的绝对地址*/
meter_data_t m_inv_meter = {0}; //[tgl mark]为了项目全局访问，在没有特殊的作用下，去掉了static修饰。 同事 inv_meter -> g_inv_meter

/** 每个寄存器4字节的，例如艾瑞达，不符合标准Modbus的*/
uint32_t get_u32_from_res_frame_4B_reg(int addr, uint8_t *buf)
{
    uint32_t var = 0;
    int idx = (addr - g_start_addr) * 4 + 3;
    var = (buf[idx] << 24) + (buf[idx + 1] << 16) + (buf[idx + 2] << 8) + (buf[idx + 3]);
    return var;
}
/** 每个寄存器4字节的，例如艾瑞达，不符合标准Modbus的*/
int32_t get_i32_from_res_frame_4B_reg(int addr, uint8_t *buf)
{
    uint32_t var = 0;
    int idx = (addr - g_start_addr) * 4 + 3;
    var = (buf[idx] << 24) + (buf[idx + 1] << 16) + (buf[idx + 2] << 8) + (buf[idx + 3]);
    return (int32_t)var;
}
/** 从响应帧提取数据值，地址是文档绝对地址，标准Modbus*/
uint32_t get_u32_from_res_frame(int addr, uint8_t *buf)
{
    uint32_t var = 0;
    int idx = (addr - g_start_addr) * 2 + 3;
    var = (buf[idx] << 24) + (buf[idx + 1] << 16) + (buf[idx + 2] << 8) + (buf[idx + 3]);
    return var;
}
/** 从响应帧提取数据值，地址是文档绝对地址，标准Modbus*/
uint16_t get_u16_from_res_frame(int addr, uint8_t *buf)
{
    uint16_t var = 0;
    int idx = (addr - g_start_addr) * 2 + 3;
    var = (buf[idx] << 8) + (buf[idx + 1]);
    return var;
}
/** 从响应帧提取数据值，地址是文档绝对地址，标准Modbus*/
int16_t get_i16_from_res_frame(int addr, uint8_t *buf)
{
    uint16_t var = 0;
    int idx = (addr - g_start_addr) * 2 + 3;
    var = (buf[idx] << 8) + (buf[idx + 1]);
    return (int16_t)var;
}

//--- Eng.Stg.Mch-lanstick 20220908 + ----//

float get_f32_from_u16(uint16_t *buf, int idx)
{
    float f = 0;
    uint32_t u = (buf[idx] << 16) + buf[idx + 1];
    f = *(float *)&u;
    return f;
}

#if TRIPHASE_ARM_SUPPORT

uint32_t get_u32_from_u8arry(uint8_t *buf)
{
    uint32_t u = 0;
    u = ((((uint32_t)*buf) << 24) & 0xFF000000) + ((((uint32_t) * (buf + 1) << 16)) & 0x00FF0000) + ((((uint32_t) * (buf + 2)) << 8) & 0x0000FF00) + (((uint32_t) * (buf + 3)) & 0x000000FF);
    return u;
}
//增加新函数
uint32_t get_u16_from_u8arry(uint8_t *buf)
{
    uint32_t u = 0;
    u = ((((uint32_t)*buf) << 8) & 0x0000FF00) + (((uint32_t) * (buf + 1)) & 0x000000FF);
    return u;
}

#endif

//=====================================//

float u32_to_float(uint32_t stored_data)
{
    float f;
    uint32_t u = stored_data;
    f = *(float *)&u;
    return f;
}
//---------------------------------//
int is_time_valid(void)
{
    int year = get_current_year();
    if (year > 2020 && year < 2100)
    {
        return 1;
    }

    else
    {
        return 0;
    }
}

//-------------------------------------//
int parse_sync_time(void)
{
    return 0; //
}
//---------------------------------//

int8_t pack_SDM_meter_fun(int type, uint8_t *buf, uint16_t len)
{
    /** modbus长度错误检查 */
    int reg_num = meter_pro[type][1];
    uint32_t tmp = 0;
    // float f_tmp = 0.0;
    uint16_t input_reg[128] = {0};
    uint8_t j;

    /* Eng.Stg.Mch-lanstick 20220908 - */
    // if (len != 5 + 2 * reg_num || buf[2] != 2 * reg_num)
    // {
    //     return ASW_FAIL;
    // }

    for (j = 0; j < reg_num; j++)
    {
        input_reg[j] = (buf[3 + 2 * j] << 8) + buf[4 + 2 * j];
    }

    uint16_t pac_idx = meter_pro[type][2];    // index of the ac power
    uint16_t etl_in_idx = meter_pro[type][3]; // index of the total energy input
    uint16_t etl_ou_idx = meter_pro[type][4]; // index of the total energy output
    uint16_t qac_idx = meter_pro[type][5];    // index of the reactive power
    uint16_t sac_idx = meter_pro[type][6];    // index of the apparent power
    uint16_t pf_idx = meter_pro[type][7];     // index of the power factor

    /* Eng.Stg.Mch-lanstick 20220908 +- */
    // if (type != 6)
    if (type < 6)
    {
        m_inv_meter.invdata.pac = get_f32_from_u16(input_reg, pac_idx);                  // W for cloud
        m_fast_read_pac = get_f32_from_u16(input_reg, pac_idx);                          // W for regulate
        m_inv_meter.invdata.e_in_total = get_f32_from_u16(input_reg, etl_in_idx) * 100;  // 0.01kwh
        m_inv_meter.invdata.e_out_total = get_f32_from_u16(input_reg, etl_ou_idx) * 100; // 0.01kwh
        m_inv_meter.invdata.qac = get_f32_from_u16(input_reg, qac_idx);
        m_inv_meter.invdata.sac = get_f32_from_u16(input_reg, sac_idx);
        m_inv_meter.invdata.cosphi = get_f32_from_u16(input_reg, pf_idx) * 100; // 0.01

        if (type >= 2 && type <= 4)
            m_inv_meter.invdata.fac = get_f32_from_u16(input_reg, 58) * 100; // 0.01 Hz
        else if (type >= 0 && type <= 1)
        {
            m_inv_meter.invdata.fac = get_f32_from_u16(input_reg, 18) * 100; // 0.01 Hz
        }

        meter_real_factory = get_f32_from_u16(input_reg, pf_idx) * 10000; // 0.0001,pf
                                                                          // m_inv_meter.invdata.fac = get_f32_from_u16(input_reg, 58) * 100;        // 0.01 Hz
                                                                          // meter_real_factory = get_f32_from_u16(input_reg, pf_idx) * 10000;       // 0.0001,pf
                                                                          // #if DEBUG_PRINT_ENABLE
        ASW_LOGI("\n-----[tgl debug print]--------\n  -2- fast read pac:%d\n", m_fast_read_pac);
        // #endif
    }

    if (type == 6)
    {
        /** Total system power */
        m_inv_meter.invdata.pac = (int16_t)input_reg[pac_idx]; // W
        tmp = input_reg[etl_in_idx] + input_reg[etl_in_idx + 1];
        m_inv_meter.invdata.e_in_total = (uint32_t)tmp;
        tmp = input_reg[etl_ou_idx] + input_reg[etl_ou_idx + 1];
        m_inv_meter.invdata.e_out_total = (uint32_t)tmp;
        m_inv_meter.invdata.qac = (int16_t)input_reg[qac_idx];
        m_inv_meter.invdata.sac = (int16_t)input_reg[sac_idx];
        m_inv_meter.invdata.cosphi = (int16_t)input_reg[pf_idx];
    }
    // meter_real_factory = (int16_t)m_inv_meter.invdata.cosphi; // 0.0001, pf -------------- regulate

    return ASW_OK;
}
//-----------------------------------//
void pack_irdIM1281B_meter_fun(uint8_t *buf, uint16_t len)
{
    if (len == 45)
    {
        g_start_addr = 0x48;
        //[tgl mark] 获取的数据的位数和符号类型不匹配
        m_inv_meter.invdata.vac[0] = get_u32_from_res_frame_4B_reg(0x48, buf) / 1000;       // 0.1V
        m_inv_meter.invdata.iac[0] = get_i32_from_res_frame_4B_reg(0x49, buf) / 100;        // 0.01A
        m_inv_meter.invdata.pac = get_i32_from_res_frame_4B_reg(0x4A, buf) / 10000;         // W ----------------- regulate
        m_inv_meter.invdata.e_total = get_u32_from_res_frame_4B_reg(0x4B, buf) / 10000;     // kWh
        m_inv_meter.invdata.cosphi = get_i32_from_res_frame_4B_reg(0x4C, buf) / 10;         // 0.01 --cosphi
        m_inv_meter.invdata.fac = get_u32_from_res_frame_4B_reg(0x4F, buf);                 // 0.01Hz
        m_inv_meter.invdata.e_in_total = get_u32_from_res_frame_4B_reg(0x50, buf) / 10000;  // positive kWh
        m_inv_meter.invdata.e_out_total = get_u32_from_res_frame_4B_reg(0x51, buf) / 10000; // negative kWh

        ASW_LOGI("volt %f [V]\n", (float)(m_inv_meter.invdata.vac[0] * 0.1));
        ASW_LOGI("curr %f [A]\n", (float)((int)m_inv_meter.invdata.iac[0] * 0.01));
        ASW_LOGI("pac %d [W]\n", (int)m_inv_meter.invdata.pac);
        ASW_LOGI("e %u [kWh]\n", m_inv_meter.invdata.e_total);
        ASW_LOGI("pf %f\n", (float)((int)m_inv_meter.invdata.cosphi * 0.01));
        ASW_LOGI("freq %f [Hz]\n", (float)(m_inv_meter.invdata.fac * 0.01));
        ASW_LOGI("e+ %u [kWh]\n", m_inv_meter.invdata.e_in_total);
        ASW_LOGI("e- %u [kWh]\n", m_inv_meter.invdata.e_out_total);

        meter_real_factory = get_i32_from_res_frame_4B_reg(0x4C, buf) * 10; // 0.0001, pf -------------- regulate
    }
}

//--------------------------------//
int8_t md_decode_meter_pack(char is_fast, uint8_t type, uint8_t *buf, uint16_t len, uint8_t frame_order)
{
    /** modbus crc16错误检查*/
    uint16_t crc16 = crc16_calc(buf, len);
    if (crc16 != 0)
        return ASW_FAIL;

    int8_t res = ASW_FAIL;

    static int meter_data_time = -600; /** -600 sec 保证第一次发送*/
    static uint8_t last_day = 0;
    meter_gendata gendata = {0};

    if (is_fast == 0)
    {
        if (type <= 6)
        {
            res = pack_SDM_meter_fun(type, buf, len);
            if (res != ASW_OK)
                return res;
        }

#if TRIPHASE_ARM_SUPPORT
        else if (type == 11) //
        {
            uint16_t pac_idx = 3 + 2 * meter_pro[7][2];                                    // index of the ac power
            uint16_t etl_in_idx = 3 + 2 * meter_pro[7][3];                                 // index of the total energy input
            uint16_t etl_ou_idx = 3 + 2 * meter_pro[7][4];                                 // index of the total energy output
            uint16_t qac_idx = 3 + 2 * meter_pro[7][5];                                    // index of the reactive power
            uint16_t sac_idx = 3 + 2 * meter_pro[7][6];                                    // index of the apparent power
            uint16_t pf_idx = 3 + 2 * meter_pro[7][7];                                     // index of the power factor
                                                                                           //
            m_inv_meter.invdata.pac = get_u32_from_u8arry(&buf[pac_idx]);                  // W for cloud
            m_fast_read_pac = get_u32_from_u8arry(&buf[pac_idx]);                          // W for regulate
            m_inv_meter.invdata.e_in_total = get_u32_from_u8arry(&buf[etl_in_idx]) * 100;  // 0.01kwh  //
            m_inv_meter.invdata.e_out_total = get_u32_from_u8arry(&buf[etl_ou_idx]) * 100; // 0.01kwh //
            m_inv_meter.invdata.qac = get_u32_from_u8arry(&buf[qac_idx]);                  //
            m_inv_meter.invdata.sac = get_u32_from_u8arry(&buf[sac_idx]);                  //
            m_inv_meter.invdata.cosphi = get_u16_from_u8arry(&buf[pf_idx]) * 100;          // 0.01
            m_inv_meter.invdata.fac = get_u16_from_u8arry(&buf[19]) * 100;                 // 0.01 Hz
            meter_real_factory = get_u16_from_u8arry(&buf[pf_idx]) * 10000;                // 0.0001,pf

            ASW_LOGI(" pac:%d,e_total:%d,e_out_total:%d,qac:%d,sac:%d,cos:%d,fac:%d,\n",
                     m_fast_read_pac, m_inv_meter.invdata.e_in_total, m_inv_meter.invdata.e_out_total,
                     m_inv_meter.invdata.qac, m_inv_meter.invdata.sac, m_inv_meter.invdata.cosphi, m_inv_meter.invdata.fac);
        }

#endif
    }
    /** 快速读取：只读功率*/
    else
    {
        // Eng.Stg.Mch-lanstick 20220908 +
#if TRIPHASE_ARM_SUPPORT
        if (type <= 6 || type == 11) //

#else
        if (type <= 6)

#endif
        {
            int reg_num = 2; // only pac
            uint16_t input_reg[4] = {0};
            for (int j = 0; j < reg_num; j++)
            {
                input_reg[j] = (buf[3 + 2 * j] << 8) + buf[4 + 2 * j];
            }

            uint8_t pac_idx = 0; // index of the ac power

            if (type != 6)
            {
                /** pac */
                // inv_meter.invdata.pac = get_f32_from_u16(input_reg, pac_idx); //W
                m_fast_read_pac = get_f32_from_u16(input_reg, pac_idx); // W
                m_inv_meter.invdata.pac = m_fast_read_pac;
                // #if DEBUG_PRINT_ENABLE
                ASW_LOGI("----[tgl debug print]--------  -1- fast read pac:%d\n", m_fast_read_pac);
                // #endif
            }
            else if (type == 6)
            {
                /** Total system power */
                m_inv_meter.invdata.pac = (int16_t)input_reg[pac_idx]; // W ----- regulate
                m_fast_read_pac = m_inv_meter.invdata.pac;
            }
        }
        if (type == METER_IREADER_IM1281B)
        {
            g_start_addr = 0x4A;
            m_inv_meter.invdata.pac = get_i32_from_res_frame_4B_reg(0x4A, buf) / 10000; // W ----------------- regulate
        }
        if (type == METER_ACREL_ADL200)
        {
            g_start_addr = 0x0D;
            m_inv_meter.invdata.pac = (int)get_i16_from_res_frame(0x0D, buf); // W ----------------- regulate
        }
        ASW_LOGI("pac: %d [W]\n", (int)m_inv_meter.invdata.pac);
    }

    //----------------------------------------------------//
    //电表数据读取时间
    char timeRdMeter[32] = {0};
    get_time(timeRdMeter, sizeof(timeRdMeter));
    fileter_time(timeRdMeter, m_inv_meter.invdata.time);  

    /************************* 计算当日买卖电量 ****************************************/

    write_global_var(GLOBAL_METER_DATA, &m_inv_meter);

    if (is_fast == 1) /** 如果没有读到总买卖电量*/
    {
        return ASW_OK;
    }
    if (is_time_valid() == 0) /** 系统时间不对，不计算当日发电量*/
    {
        return ASW_OK;
    }

    general_query(NVS_METER_GEN, (void *)&gendata);

    if (!last_day) /** 第一次，仅一次*/
    {
        if (get_current_days() != gendata.day) /** day发生变化*/
        {
            memset(&gendata, 0, sizeof(meter_gendata));
            gendata.e_in_total = m_inv_meter.invdata.e_in_total;
            gendata.e_out_total = m_inv_meter.invdata.e_out_total;
            gendata.day = get_current_days();
            general_add(NVS_METER_GEN, (void *)&gendata);
            m_inv_meter.e_in_day_begin = m_inv_meter.invdata.e_in_total;
            m_inv_meter.e_out_day_begin = m_inv_meter.invdata.e_out_total;
        }
        else /** day未发生变化*/
        {
            m_inv_meter.e_in_day_begin = gendata.e_in_total;
            m_inv_meter.e_out_day_begin = gendata.e_out_total;
        }
        last_day = get_current_days();
    }
    else if (get_current_days() != gendata.day)
    {

        memset(&gendata, 0, sizeof(meter_gendata));
        gendata.e_in_total = m_inv_meter.invdata.e_in_total;
        gendata.e_out_total = m_inv_meter.invdata.e_out_total;
        gendata.day = get_current_days();
        general_add(NVS_METER_GEN, (void *)&gendata);

        m_inv_meter.e_in_day_begin = m_inv_meter.invdata.e_in_total;
        m_inv_meter.e_out_day_begin = m_inv_meter.invdata.e_out_total;
        last_day = get_current_days();
    }

    m_inv_meter.invdata.e_in_today = (m_inv_meter.invdata.e_in_total - m_inv_meter.e_in_day_begin); // 0.01 kwh
    m_inv_meter.invdata.e_out_today = (m_inv_meter.invdata.e_out_total - m_inv_meter.e_out_day_begin);

    if ((int)m_inv_meter.invdata.e_in_today < 0 || (int)m_inv_meter.invdata.e_out_today < 0)
    {
        m_inv_meter.invdata.e_in_today = 0;
        m_inv_meter.invdata.e_out_today = 0;

        memset(&gendata, 0, sizeof(meter_gendata));
        gendata.e_in_total = m_inv_meter.invdata.e_in_total;
        gendata.e_out_total = m_inv_meter.invdata.e_out_total;
        gendata.day = get_current_days();
        general_add(NVS_METER_GEN, (void *)&gendata);

        m_inv_meter.e_in_day_begin = m_inv_meter.invdata.e_in_total;
        m_inv_meter.e_out_day_begin = m_inv_meter.invdata.e_out_total;
        last_day = get_current_days();
    }

    ASW_LOGI("e_in_today: %u [0.01kwh]\n", m_inv_meter.invdata.e_in_today);
    ASW_LOGI("e_out_today: %u [0.01kwh]\n", m_inv_meter.invdata.e_out_today);
    ASW_LOGI("e_in_day_begin: %u [0.01kwh]\n", m_inv_meter.e_in_day_begin);
    ASW_LOGI("e_out_day_begin: %u [0.01kwh]\n", m_inv_meter.e_out_day_begin);

    write_global_var(GLOBAL_METER_DATA, &m_inv_meter);

    if (get_second_sys_time() - meter_data_time >= 300)
    {
        meter_data_time = get_second_sys_time();
        if (mq2 != NULL)
        {
            xQueueSend(mq2, (void *)&m_inv_meter, (TickType_t)0);
            ASW_LOGI("meter data send cld ok\n");
        }
        // msgsnd(meter_msg_id, &trans_meter_cloud, sizeof(trans_meter_cloud.data), IPC_NOWAIT);
        return ASW_OK;
    }

    else
        return ASW_OK;
}
