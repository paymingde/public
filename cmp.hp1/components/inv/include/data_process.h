/**
 * @file data_process.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-03-30
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef DATA_PROCESS_H
#define DATA_PROCESS_H

#include "Asw_global.h"

#define INV_NUM 15
#define START_ADD 0x03
#define SAVE_TIMING 300

#define JSON_MSG_SIZE 1536
#define MAX_BATTERY_PLAN 6
#define METER_RECON_TIMES 30

typedef char ate_reboot_t[20]; //[tgl mark] 50->20
typedef char blufi_name_t[50];

extern int g_drm_value;

/** 事件标志位组*/
extern uint32_t task_cld_msg;

// extern uint32_t task_inv_msg;  LanStick-MultilInv
extern uint32_t task_inv_msg_arr[INV_NUM];
extern uint32_t task_inv_meter_msg;
extern uint16_t g_task_inv_broadcast_msg;

extern uint32_t task_other_msg;
extern uint32_t event_group_0;
//--------------------------//

typedef enum
{
    GLOBAL_INV_ARR = 0,
    PARA_CONFIG,
    PARA_SCHED_BAT,
    GLOBAL_BAT_DATA,
    GLOBAL_METER_DATA,
    METER_CONTROL_CONFIG, /// Lanstick-MultilInv
    GLOBAL_BATTERY_DATA
} GLOBAL_PARA_TYPES;

typedef enum
{
    MSG_FDBG,
    MSG_UPDATE_INV_MASTER,
    MSG_UPDATE_INV_SAFETY,
    MSG_SCAN_INVERTER
} ALL_MSG_TYPE;

//=============modbus===========//
typedef struct
{
    int fd;
    uint8_t slave_id;
    uint16_t start_addr;
    uint16_t reg_num;
} mb_req_t;

typedef struct
{
    uint8_t frame[500];
    uint16_t len;
} mb_res_t;

//============ update==========//
typedef struct
{
    char file_name[50];
    char file_path[80];
    char part_num[50];
    int file_type;
    int file_len;
} Update_t, *Update_p;

typedef struct
{
    char uri[150];

    char device[4]; //[tgl lan] device就是一个数字,改小了，
    char secret[2]; //[tgl lan] device就是一个标志,改小了，
    char sn[50];
    int date_day;
    char filename[50];
    char info[2]; //[tgl lan] info是一个数字,改小了

    char action[50];
    /** 由于post的body比较大，使用动态内存 */
    char *body;
    int body_len;
    Update_p update_info;
} http_req_para_t; //[tgl mark]

/** This struct describes the attributes of Cluster  */
typedef struct tagMonitor
{
    int8_t sn[16];
    int16_t type;
    int8_t name[13];
    int8_t model[13];
    int8_t mfr[16];
    int8_t brand[16];
    int8_t hw_ver[20];
    int8_t reg_key[17];
    int8_t prd_key[33];    // ProductKey
    int8_t dev_scrt[33];   // device screct len = 32
    int8_t alilyun_ip[33]; // alilyun ip
    int16_t alilyun_port;  // alilyun_port

    int8_t sw_ver[20];
    int8_t md_ver1[20];

    uint8_t inv_cunt; // 0: automatically collect inverter data,the modbus address is 3. 1:read inverte list from flashs
    int8_t meter_en;  // whether is enable the collect meter data
    int8_t meter_mod; // the meter module type
    int8_t meter_add; // default is 1, maybe not set
} Monitor, *Monitor_ptr;

typedef struct
{
    uint8_t en_pwr;
    uint8_t pwr_model;

    /* Fix cos model value, it's no use */
    uint8_t fix_cos;
    uint8_t fix_cd;

    /* varible cos model value, it's no use */
    uint8_t var_cp1;
    uint8_t var_cc1;
    uint8_t var_cd1;
    uint8_t var_cp2;
    uint8_t var_cc2;
    uint8_t var_cd2;
    uint8_t var_cp3;
    uint8_t var_cc3;
    uint8_t var_cd3;
    uint8_t var_cp4;
    uint8_t var_cc4;
    uint8_t var_cd4;
    uint8_t var_ct;

    /* Fix Q model value */
    uint8_t fix_qp; //!< Fix cos value it's used to ragulate the reactive power
    uint8_t fix_qd; //!< Fix cos phase it's no use

    /* varible Q model value it's no use*/
    uint8_t var_qu1;
    uint8_t var_qq1;
    uint8_t var_qd1;
    uint8_t var_qu2;
    uint8_t var_qq2;
    uint8_t var_qd2;
    uint8_t var_qu3;
    uint8_t var_qq3;
    uint8_t var_qd3;
    uint8_t var_qu4;
    uint8_t var_qq4;
    uint8_t var_qd4;
    uint8_t var_qt;
} RpwrPara, *RpwrPara_ptr;

typedef struct
{
    bool dhcp;
    bool auto_dns;
    uint32_t ip;
    uint32_t mask;
    uint32_t gateway;
    uint32_t dns_addr;
} Net_data, *Net_data_ptr;

typedef struct
{
    char psn[64]; // device name
    char product_key[64];
    char device_secret[64];

    char host[128];
    int port;

    char reg_key[51];
    char hw[51];
    char brw[51];
    char mfr[51];
    char nam[51];

    char md1[51];
    char md2[51];
    char cmd[51];
    int typ;
    char mod[51];
    char wsw[51];
    int meter_en;
    int meter_mod;
    int meter_add;
} Setting_Para, *Setting_Para_Ptr;

typedef struct
{
    uint8_t ssid[32];
    uint8_t password[64];
} wifi_ap_para_t;

typedef struct
{
    uint8_t ssid[32];
    // uint8_t password[64];
    uint8_t password[32];

} wifi_sta_para_t;

///////////////////
typedef struct
{
    uint8_t enble;
    uint8_t work_mode;
    uint16_t ip[4];
    uint16_t mask[4];
    uint16_t gateway[4];
    uint16_t maindns[4];
} net_static_info_t;
///////////////////
// int -->32 位有符号在esp32中
typedef struct
{
    bool en_ac; //!<  used to save the flag of enable/disable zero export. it's no used.
    int meter_day;
    int meter_enb;      //!< it's used to save the flag of enable/disable the meter monitor.
    int meter_mod;      //!<  smart meter model @@@METER
    int meter_target;   //!<  Output power according to meter
    int meter_regulate; //!< used to save the flag of enable/disable zero export
    int meter_enb_PF;
    int meter_target_PF;
    char meter_sn[50];
    char meter_mfr[20];
    uint8_t regulate_abs; // Eng.Stg.Mch-lanstick +

    uint32_t drm_ld_sp;      //!< used to record the timestamp of charging/discharging --Solar DC Capacity Change to DRMs load speed.
    int32_t regulate_offset; // Eng.Stg.Mch-lanstick  +

} AdvPara, *AdvPara_ptr;

///////////  LanStick-MultilInv ///////////////////
typedef struct tagBatteryInfo
{
    uint8_t active_mode; //!<  used to DRM control:mode =3 enable/disable
    uint8_t dc_per;      //!<  BEAST type
    uint8_t q_value;     //!<  (x)used in DRM, it's no use
    uint32_t fun_mode;   //!<  used for power regulation

    uint8_t freq_mode; //!< charging/discharging/stopping flag

    uint16_t fq_ld_sp; //!<  q value;

    uint16_t freq_stop; //!< whether in charging/discharging schedule 0:not in schedule; 1:in schedule
    uint16_t freq_back; //!< used to save the channle of wifi had connected to ap

    /*used to save the battery information set to inverter*/
    uint8_t uu1; //!< running mode
    uint8_t up1; //!< manufactory
    uint8_t uu2; //!< battery model
    uint8_t up2; //!< battery number
    uint8_t uu3; //!< offline for DRM
    uint8_t up3; //!< active power percent for DRM
    uint8_t uu4; //!< power on/off
    uint8_t up4; //!< first power on

    // RpwrPara ra_pwr; //!< Reactive power control value  //tgl mark 未用到 去掉了
    // uint16_t fq_t1; //!< charging power
    // uint16_t ov_t1; //!< discharging power
    uint16_t fq_t2; //!< DRM q value
    uint16_t ov_t2; //!< DRM setting value to inverter

    // add discharge_max/charge_max
    uint16_t dchg_max;
    uint16_t chg_max;
} Monitor_Battery_t, *Monitor_Batt_ptr;

///////////////////////////////////////////////////

// /** 调度电池***/

typedef struct
{
    uint8_t type;          /// 0x31: single phase inverter, 0x33 three phase inverter
    uint8_t modbus_id;     /// modbus address and zeversolar communication address
    char sn[33];           /// the inverter SN.
    char mode_name[17];    /// the inverter made name "ZL3000"
    uint8_t safety_type;   /// the inverter current safety
    uint32_t rated_pwr;    /// the inverter rate power
    char msw_ver[15];      /// master software version AISWEI料号: "V610-01037-00"
    char ssw_ver[15];      /// slave software version  AISWEI料号: "V610-01038-00"
    char tmc_ver[15];      /// safety software version AISWEI料号: "V610-10000-00"
    char protocol_ver[13]; /// the inverter communicate protocol version "MV2.00AV2.00"
    char mfr[17];          /// inverter manufacture "ZEVERSOLAR"
    char brand[17];        /// the inverter brand   "ZEVERSOLAR"
    uint8_t mach_type;     /// the inverter type 1-单相机1-3kw   2-单相机3-6kw
    uint8_t pv_num;        /// the inverter pv number
    uint8_t str_num;       /// the inverter string number
    uint8_t syn_time;      // 0: no synchronize time, 1: had synchronize yet

    uint16_t is_host; // add  1-主机，2-从机 99-正常模式  //暫未使用 mark

#if TRIPHASE_ARM_SUPPORT
    char csw_ver[32]; // arm softerware version	//增加一个数据，
                      //后面的cgi和cloud的部分的读数据也要相应增加，目前未定义，还没改
#endif
} InvRegister, *InvRegister_ptr;

typedef struct tagCur_Voltg
{
    uint16_t iVol;
    uint16_t iCur;
} Cur_Voltg, *Cur_Voltg_ptr;

typedef struct
{
    int8_t type;
    int8_t ws;
    uint8_t slave_id; // TRIPHASE_ARM_SUPPORT
    char data[512];
    uint8_t len;
} cloud_inv_msg;

typedef struct tagInv_data
{
    // char psn[50];  //LanStick MultilInv
    char psn[33];
    char time[64];
    uint16_t grid_voltg;        /// the grid voltage
    uint32_t e_today;           ///< The accumulated kWh of that day  ///meter E-Today(out)
    uint32_t e_total;           ///< Total energy to grid             ///meter e_total(in)
    uint32_t h_total;           ///< Total operation hours            ///meter E-Total( out) (old msg)
    uint8_t status;             ///< Inverter operation status
    uint16_t chk_tim;           ///< the check time of inverter
    int16_t col_temp;           ///< the cooling fin temperature
    int16_t U_temp;             ///< the inverter U phase temperature
    int16_t V_temp;             ///< the inverter V temperature
    int16_t W_temp;             ///< the inverter W temperature
    int16_t contrl_temp;        ///< the control board temperature
    int16_t rsv_temp;           ///< the cooling fin temperature
    int16_t bus_voltg;          ///< bus voltage                              ///meter Phase angle
    uint32_t con_stu;           ///< connect status   special for smart meter ///meter E-Today(in)
    Cur_Voltg PV_cur_voltg[10]; ///< the 10 voltage and current pair
    uint16_t istr[20];          ///< string current 1~20
    Cur_Voltg AC_vol_cur[3];    ///< L1,L2,L3 voltage and current
    uint16_t rs_voltg;          // rs line voltage
    uint16_t rt_voltg;          // rt line voltage
    uint16_t st_voltg;          // st line voltage
    uint16_t fac;               ///< AC frequence
    uint32_t sac;               ///< Total power to grid(3Phase)    ///meter SAC
    uint32_t pac;               ///< Total power to grid(3Phase)    ///meter pac
    uint32_t qac;               ///< Total power to grid(3Phase)    ///meter QAC
    int8_t cosphi;              ///< the AC phase                   ///meter   //与电表的大小不同，电表的是32位 [tgl marks]
    uint8_t pac_stu;            ///< limit power status 0: not limit power , 1:limit power
    uint8_t err_stu;            ///< the error status   0: there is not error accident, 1: there is an error accident
    uint16_t error;             ///< Failure description for status 'failure'
    char warn[10];              ///< Failure description for status 'failure'
    uint8_t dc_sample;          ///< the Zevercloud sample time
    uint32_t rtc_time;          ///< the occure time
} Inv_data, *Inv_data_ptr;

/** This struct describes the attributes of inverter  */
// total:static and dynamic part.don't save dynamic part.
typedef struct
{
    uint16_t last_error;
    uint16_t last_warn;
    InvRegister regInfo;
    uint8_t status;
    uint8_t status_first_running;
    uint32_t online_time;
    uint32_t sampnum;         // used to calc the average of pac
    int32_t pac_sum;          // used to calc the average of pac
    uint32_t fir_etotal;      // The current day's first E_Total value
    uint32_t fir_etotal_out;  // The current day's first E_Total value
    uint8_t private_data[64]; // TShe inverter's describe table
    Inv_data invdata;
    uint32_t last_save_time;

} Inverter, *Inverter_ptr;

/** 电表数据类型: 名称e打头以示区分 **/
typedef struct
{
    uint32_t e_in_today;
    uint32_t e_in_total;
    uint32_t e_out_today;
    uint32_t e_out_total;
    uint32_t e_today;
    uint32_t e_total;

    uint32_t vac[3];
    uint32_t iac[3];

    uint32_t fac;    ///< AC frequence
    uint32_t sac;    ///< Total power to grid(3Phase)    ///meter SAC
    uint32_t pac;    ///< Total power to grid(3Phase)    ///meter pac
    uint32_t qac;    ///< Total power to grid(3Phase)    ///meter QAC
    uint32_t cosphi; ///< the AC phase                   ///meter cosphi  //与电表的大小不同，电表的是32位 [tgl marks]

    uint8_t status;
    char time[64];
    char sn[40];
    char mfr[20];
    char model[20];
} mInv_data;

typedef struct
{
    mInv_data invdata;
    uint32_t e_in_day_begin;
    uint32_t e_out_day_begin;
} meter_data_t;

typedef struct
{
    int type;
    int len;
    char *data;
} fdbg_msg_t;

////////////////////** 储能参数配置*****///////////////////////////////////
#if TRIPHASE_ARM_SUPPORT
typedef struct
{
    uint32_t pac;
    int32_t qac;
} Pac_Qac;

#endif

typedef struct tagBatteryData
{
    uint32_t ppv_batt;         ///!< total power from PV
    uint32_t etoday_pv_batt;   ///!< energy today from PV
    uint32_t etotal_pv_batt;   ///!< energy total from pv
    int32_t p_batt;            // sint32_t ;           ///!< power from battery
    uint32_t etd_in_batt;      ///!< energy today in charging status from battery
    uint32_t etd_out_batt;     ///!< energy today in discharging status from battery
    uint32_t etd_AC_used_batt; ///!< energy today AC used
    uint32_t etd_AC_feed_batt; ///!< energy today AC fed
    uint32_t p_ac_EPS_batt;    ///!< active power of EPS
    uint32_t p_ra_EPS_batt;    ///!< reactive power of EPS
    uint32_t etd_EPS_batt;     ///!< energy today consumed by EPS
    uint32_t eto_EPS_batt;     ///!< energy total consumed by EPS

    uint16_t status_comm_batt; ///!< status of communication between dps and battery
    uint16_t status_batt;      ///!< status of battery
    uint16_t error1_batt;      ///!< error code from battery
    uint16_t warn1_batt;       ///!< warn code from battery
    uint16_t v_batt;           ///!< voltage from battery
    int16_t i_batt;            ///!< current from battery

    int16_t t_batt;            ///!< temperature from battery
    uint16_t SOC_batt;         ///!< SOC from battery
    uint16_t SOH_batt;         ///!< SOH from battery
    uint16_t i_in_limit_batt;  ///!< limited current in charging status from battery
    uint16_t i_out_limit_batt; ///!< limited current in discharging status from battery

    uint16_t v_EPS_batt; ///!< voltage of EPS
    uint16_t i_EPS_batt; ///!< current of EPS
    uint16_t f_EPS_batt; ///!< frequency of EPS

#if TRIPHASE_ARM_SUPPORT
    Cur_Voltg v_i_phase_ESP[3]; // L1,L2,L3 voltage and current of eps load		//增加三相内容
    Pac_Qac p_q_phase_ESP[3];   // L1,L2,L3 pac and qac of eps load              //增加三相内容
    Pac_Qac p_q_phase_AC[3];    // L1,L2,L3 pac and qac of AC                    //增加三相内容

#endif

} Batt_data, *Batt_data_ptr;

/********* 调度电池********/
typedef struct
{
    uint32_t time_sch[6]; //  time schedule in days: 0~1Bytes:begin time; 2Byte:running time; 3Byte:charging/discharging
                          // uint_16 flag[4];     //  flag of charging or discharging
} DaySchedule, *DaySchedule_ptr;

typedef struct
{
    uint16_t fq_t1; //!< charging power
    uint16_t ov_t1; //!< discharging power
    DaySchedule daySchedule[7];
} ScheduleBat, *ScheduleBat_ptr;

typedef struct
{
    // Net_data eth_para;  //!< network parameter from ethernet
    uint8_t inv_sample;  //!< monitor sampling time
    uint8_t dc_sample;   //!< uploading interval time to cloud
    uint8_t connect_cnt; //!< connect time
    uint8_t rule_count;  // 1:ATE mode other:normal
    /*1Byte:0/1 file exist/not exist
      4Bytes:   file size
      4Byres:   read size
      1Bytes:   reading 0/1
      0~9Bytes:  Sunday
      10~19Bytes:Monday
      20~29Bytes:Tursday
      30~39Bytes:Wensday
      40~49Bytes:Thursday
      50~59Bytes:Friday
      60~69Bytes:Saterday
    */
    AdvPara adv;

    uint8_t ssc_enable;  //光储充模式 1-开启，0-关闭
    uint8_t is_parallel; // 1：并联模式; 0:普通模式
    uint16_t host_adr;   //并联模式，主机地址
#if PARALLEL_HOST_SET_WITH_SN
    char host_psn[33]; //主机逆变器psn
#endif

} MonitorPara, *MonitorPara_ptr;

//////////////////////////////////

typedef struct
{
    uint16_t minutes_inday; // time minutes in one day. max:1440
    uint16_t duration;      // charge/discharge running time
    uint16_t charge_flag;   // flag of charging/discharging
} DayDetailSch, *DayDetailSch_ptr;

typedef struct
{
    /** Range from 1970 to 2099. */
    int16_t YEAR;
    /** Range from 1 to 12. */
    int16_t MONTH;
    /** Range from 1 to 31 (depending on month). */
    int16_t DAY;
    /** Range from 0 to 23. */
    int16_t HOUR;
    /** Range from 0 to 59. */
    int16_t MINUTE;
    /** Range from 0 to 59. */
    int16_t SECOND;
    /** Range from 0 to 999. */
    int16_t MILLISEC;
    /** Day of the week. Sunday is day 0. Range is from 0 to 6. */
    int16_t WDAY;
    /** Day of the year. Range is from 0 to 365. */
    int16_t YDAY;
} DATE_STRUCT, *DATE_STRUCT_PTR;
//*********************************************************//
/******************** broadcast inv msg *******************/
#define MSG_BRDCST_SET_BATTERY_INDEX 0X0001  // set battery information begin with running mode
#define MSG_BRDCST_SET_RUN_MODE_INDEX 0X0002 // Only set battery running mode
#define MSG_BRDCST_SET_POWER_INDEX 0X0004    // message for setting beast power on/off
#define MSG_BRDCST_GET_SAFETY_INDEX 0x0008   ///< uesed to inform inv_com task update the sechdule immediately
#define MSG_WRT_STA_INFO_INDEX 0x0010        ///< uesed to inform inv_com task update the sechdule immediately
#define MSG_WRT_SET_HOST_INDEX 0x0020        ///< uesed to inform inv_com task to set addr to host
#define MSG_BRDCST_DSP_ZV_CLD_INDEX 0x0040   ///< uesed to inform inv_com task to send cloud status to inv
#define MSG_BRDCST_CHARGE_INDEX 0x0080   ///< uesed to inform inv_com task to send charge/discharge battery
//----- inv register add ----------
#define INV_REG_ADDR_SSID_PSSWD 0X0484   // register address 41157 ssid
#define INV_REG_ADDR_SET_HOST 0X045C     // register address 41117 主从机设置
#define INV_REG_ADDR_SET_PARALLEL 0X045D // register address 41118 并联模式

/*status of monitor*/
#define SYNC_TIME_FROM_CLOUD_INDEX 0X01 // status means whether synchronize the time from cloud
/********************* message to inverter ***********************************/
#define MSG_PWR_ACTIVE_INDEX 0x00000001   ///< meter power control
#define MSG_PWR_INACTIVE_INDEX 0x00000002 ///< drm inactive power control
#define MSG_RPW_MODE_INDEX 0x00000004     ///< drm reactive mode mode control
#define MSG_INV_UPDATE_INDEX 0x00000008   ///< restart after update the firmware
#define MSG_INV_TIME_INDEX 0x00000010     ///< Send time to inverter
#define MSG_COMBOX_TIME_INDEX 0x00000020  ///< Read time from inverter ,and set this time to combox
#define MSG_SET_SAFETY_INDEX 0x00000040   ///< Send safety para to inverter flag--it's no use ---used to send battery information
#define MSG_GET_SAFETY_INDEX 0x00000080   ///< uesed to inform inv_com task update the sechdule immediately
#define MSG_SAFETY_TYPE_INDEX 0x00000100  ///< Send safety type to inverter flag--it's no use
#define MSG_DSP_HD_DG_INDEX 0X00000400    ///< half data transmission to inverter  flag -- it's no use
#define MSG_INV_SET_ADV_INDEX 0x00000200  ///<---used to send the meter status to inverter
#define MSG_DSP_FD_DG_INDEX 0X00000800
#define MSG_METER_TEST_INDEX 0x00001000    ///< Meter test message index
#define MSG_INV_ONOFF_INDEX 0x00002000     ///< Meter test message index
#define MSG_DRMS_PWR_INDEX 0x00004000      ///< The drms power load speed control
#define MSG_INV_FUN_INDEX 0x00008000       ///< The inverter function enable control--it's no use
#define MSG_OVER_FREQ_INDEX 0x00010000     ///< The inverter function enable control--it's no use
#define MSG_OVER_VOLT_INDEX 0x00020000     ///< The inverter function enable control--it's no use
#define MSG_LOAD_SPEED_INDEX 0x00040000    ///< read the fix q value from inverter
#define MSG_DSP_ZV_CLD_INDEX 0X00080000    ///< cloud status to inverter showing with LED
#define MSG_SET_BATTERY_INDEX 0X00100000   // set battery information begin with running mode
#define MSG_SET_RUN_MODE_INDEX 0X00200000  // Only set battery running mode
#define MSG_SET_CHARGE_INDEX 0X00400000    // message for setting charging/discharging/stopping flag to inverter,with
#define MSG_SET_FIRST_RUN_INDEX 0X00800000 // message for setting inverter first power on
#define MSG_SET_POWER_INDEX 0X01000000     // message for setting beast power on/off
#define MSG_DRED_DISABLE_INDEX 0X02000000  // message for dred disable
#define MSG_PMU_UPDATE_INDEX 0X00000008

#define INV_REG_ADDR_METER 0X0453  // register address 41107
#define INV_REG_DRM_0N_OFF 0x00C8  // register address 40201
#define INV_REG_ADDR_ACTIVE 0x151A // register address 45403
#define INV_REG_DRM_Q 0x158B       // register address 45516
#define INV_REG_RPW_MODE 0X157C    // register address 45501
#define INV_DRMs_Pval 0x154B       // register address 45452
/** inv registers****/
#define INV_REG_ADDR_FIRST_ON 0X044C             // register address 41101
#define INV_REG_ADDR_POWER_ON 0X044D             // register address 41102
#define INV_REG_ADDR_CHARGE 0X047F               // register address 41152
#define INV_REG_ADDR_BATTERY 0X044E              // register address 41103
#define INV_REG_ADDR_BATTERY_CHAGE_CONFIG 0X0481 // register address 41154

#define INV_REG_ADDR_POWER_ON 0X044D // register address 41102

#define METER_CONFIG_MASK 0x0001
#define NEW_CHARGE_SCHED_MASK 0x0002

#define INV_REG_ADDR_TIME 0X03E8     // register address 41001
#define INV_REG_ADDR_SAFETY 0X1450   // register address 45201
#define INV_REG_ADDR_REACTIVE 0X157C // register address 45501
#define INV_REG_ADDR_Q_PHASE 0X158B  // register address 45516
#define INV_REG_ADDR_ON_OFF 0X00C8   // register address 40201
#define INV_REG_CLD_STATUS 0X047E    // register address 41151

#define SYNC_TIME_FROM_INV_INDEX 0X02 // status means whether synchronize the time from inverter
#define MON_OTA_CMD_INDEX 0X04        // status means there is a inverter with error serial number.// unuseful
#define MON_OTA_INV_CMD_INDEX 0X08    // status means rtc time is error.// unuseful

#define INV_FOUND_METER_INDEX 0X10  // status means there is a smart meter in system.
#define INV_SCAN_STATE_INDEX 0X20   // status means whether monitor is in scanning
#define INV_REGIST_STATE_INDEX 0X40 // status means whether monitor has sent the registry command.

// cloud
#define INV_SYNC_DEV_INDEX 0X01      // status means whether synchronize the inverter information.
#define INV_CHECK_ONLINE_INDEX 0X02  // status means whether needs to check the inverter information; unused
#define INV_ONLINE_STATUS_INDEX 0X04 // status means whether the inverter is on line.
#define INV_SYNC_INV_STATE 0X08
#define INV_SYNC_PMU_STATE 0X10
#define START_ADD 0x03

//-----------Dang.Wang add ----------//

#define PWR_REG_SOON_MASK 0x0004
#define ESTORE_BECOME_ONLINE_MASK 0x0008 // edge trigger

#define CLD_GET_TIME_MASK 0x0010
#define INV_GET_TIME_MASK 0x0020
#define SYNC_ALL_ONCE 0x0040
#define SEND_INV_DATA_ONCE 0x0080

#define SAFE_96_97_READ_METER 0x0100
#define LOW_TIMEOUT_FOR_MODBUS 0x0200
//********************************************************//

typedef InvRegister Inv_reg_arr_t[INV_NUM]; // should save to database.

typedef Inverter Inv_arr_t[INV_NUM];

// typedef InvBat_data Bat_arr_t[INV_NUM];

//--- Eng.Stg.Mch  ---//
// extern MonitorPara g_monitor_para;
// extern ScheduleBat g_schedule_bat;

extern Batt_data g_bat_data;
// extern meter_data_t g_meter_data;  //改为局部变量

/* Lanstick-MultilInv */
#define MSG_INV_INDEX_GROUP (MSG_PWR_ACTIVE_INDEX | MSG_PWR_INACTIVE_INDEX | MSG_RPW_MODE_INDEX | MSG_INV_UPDATE_INDEX | MSG_INV_TIME_INDEX | MSG_COMBOX_TIME_INDEX | MSG_SET_SAFETY_INDEX | MSG_GET_SAFETY_INDEX | MSG_SAFETY_TYPE_INDEX | MSG_INV_SET_ADV_INDEX | MSG_DSP_HD_DG_INDEX | MSG_DSP_FD_DG_INDEX | MSG_METER_TEST_INDEX | MSG_INV_ONOFF_INDEX | MSG_DRMS_PWR_INDEX | MSG_INV_FUN_INDEX | MSG_OVER_FREQ_INDEX | MSG_OVER_VOLT_INDEX | MSG_LOAD_SPEED_INDEX | MSG_DSP_ZV_CLD_INDEX | MSG_SET_BATTERY_INDEX | MSG_SET_CHARGE_INDEX | MSG_SET_RUN_MODE_INDEX | MSG_SET_FIRST_RUN_INDEX | MSG_SET_POWER_INDEX | MSG_DRED_DISABLE_INDEX)

void set_current_date(DATE_STRUCT *date);
void get_current_date(DATE_STRUCT *date);

extern int netework_state;

////////////// Lanstick-MultilInv /////////////

typedef struct tagMonitorBatterys
{
    char sn[33]; /// the inverter SN.
    uint8_t modbus_id;
    Monitor_Battery_t batmonitor;
} MonitorBat_info;

typedef struct tagSchdleBatterys
{
    char sn[33]; /// the inverter SN.
    ScheduleBat batschdle;

} SchdlBat_info;

typedef struct tagInvBatteryData
{
    uint8_t modbus_id; /// modbus address and zeversolar communication address
    char sn[33];       /// the inverter SN.
    Batt_data batdata;
    // Monitor_Battery_t batmonitor;
    // ScheduleBat batschdle;

} InvBat_data;

typedef InvBat_data Bat_arr_t[INV_NUM];
typedef MonitorBat_info Bat_Monitor_arr_t[INV_NUM];
// typedef SchdlBat_info Bat_Schdle_arr_t[INV_NUM];  TGL MARK TODO广播 只有一份
///////////////////////////////////////////////
extern Bat_Monitor_arr_t g_monitor_para;
// extern Bat_Schdle_arr_t g_schedule_bat;
extern ScheduleBat g_schedule_bat;

void g_value_init(void);

#endif
