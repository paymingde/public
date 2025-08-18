#include "motor_control.h"
#include "app_time.h"
#include "bsp_rtc.h"
#include "systick.h"

#define MOTOR_CONTROL_TASK_CYCLE 10  //10ms
#define CHARGE_GO_BACK_TIME 15000 //15s
#define MAX_SPEED_WHEN_STOP 27
#define BACK_SPEED_LIMIT -1200

#define WHEEL_DISTANCE 432.2f
typedef struct m_speed_time_data_rec
{
    int32_t ls:12;
    int32_t rs:12;
    int32_t time:8;
    uint16_t accl;
    uint16_t accr;
}m_speed_time_data_rec_t,*m_speed_time_data_rec_ptr;

typedef struct m_speed_time_data_send
{
    int32_t ls:12;
    int32_t rs:12;
    int32_t time:8;
    uint16_t time_rec_ms:10;
    uint16_t time_rec_s:6;
    uint16_t time_send_ms:10;
    uint16_t time_send_s:6;
}m_speed_time_data_send_t,*m_speed_time_data_send_ptr;

typedef struct motor_work_status
{
  uint8_t status_l,status_r;//0: 1:run 2 stop
  uint8_t target_spd_l, target_spd_r;//0: 1:run 2 stop
  uint8_t cur_spd_l,cur_spd_r;//0: 1:run 2 stop
}motor_work_status_t,*motor_work_status_ptr;

uint16_t MotorPrintCnt = 0;
bool bAsistanceStatus_t = false;
int16_t iSetTorqueL = 0;
int16_t iSetTorqueR = 0;
uint8_t uMStatusMode = M_STATUS_MODE_SPEED_RUNNING;//M_STATUS_MODE_TORQUE_ENABLE;//;
uint8_t uAsisStatus = 0;
uint8_t uAsisRunningStep = 0;
float fAsisTargetValueL = 500;
float fAsisTargetValueR = 500;
uint8_t uAsisStopCnt = 0;
uint8_t uRobotMovMode = ROBOT_MOVE_NULL;
uint8_t uMotorSwitch = 0;
uint8_t uMotorMode = 0;//0:PV 1:PT
uint16_t uGZXTestCnt = 0;

uint8_t uRobotMovingStatus = 0;
uint8_t uParamCheckFlg = 0;
uint8_t uParamCheckFlg2 = 0;
uint16_t uCalParamCheckFlg = 0;
uint32_t g_motor_msg_cnt = 0;
bool g_motor_init_flg = true;
bool g_motor_reset_flg = false;
uint8_t g_motor_overload_flg = 255;

static int32_t MileageL_t,MileageR_t;
static uint8_t air_warn_force_flag = 0;//0:close 1:open
static motor_work_status_t motor_work_status;

PID_PARAM_t AsistancePidL = 
{
  0.70, 0.01, 1.60,
  0, 0, 0,
  0, 
  0,
  0,
  0
};

PID_PARAM_t AsistancePidR = 
{
  0.70, 0.01, 1.60,
  0, 0, 0,
  0,
  0,
  0,
  0
};

PID_PARAM_t AsistancePidTurnL = 
{
  0.70, 0.01, 1.60,
  0, 0, 0,
  0,
  0,
  0,
  0
};

PID_PARAM_t AsistancePidTurnR = 
{
  0.70, 0.01, 1.60,
  0, 0, 0,
  0,
  0,
  0,
  0
};

KALMAN_FILTER_PARAMETER kfp_ramp =
{
    0.02,  //上次估算协方差
    0,  //当前估算协方差
    0,  //输出
    0,  //卡尔曼增益
    0.01,  //过噪声协方差,小滑
    0.5   //观测噪声协方差，小近
};

/**
 * @brief get motor run status
 * 
 * @param status_ptr 
 * @param cur_spd_l left wheel current speed mm/s
 * @param cur_spd_r right wheel current speed mm/s
 * @param tar_spd_l left wheel target speed mm/s
 * @param tar_spd_r right wheel target speed mm/s
 */
static void check_motor_work_status(motor_work_status_ptr status_ptr,int16_t cur_spd_l, 
    int16_t cur_spd_r, int16_t tar_spd_l, int16_t tar_spd_r)
{

    status_ptr->cur_spd_l = (abs(cur_spd_l) <= MAX_SPEED_WHEN_STOP)?1:3;
    status_ptr->cur_spd_r = (abs(cur_spd_r) <= MAX_SPEED_WHEN_STOP)?1:3;
    status_ptr->target_spd_l = (0 == tar_spd_l)?1:2;
    status_ptr->target_spd_r = (0 == tar_spd_r)?1:2;

    status_ptr->status_l = (1 == status_ptr->cur_spd_l && 1 == status_ptr->target_spd_l)?2:1;
    status_ptr->status_r = (1 == status_ptr->cur_spd_r && 1 == status_ptr->target_spd_r)?2:1;
    return;
}

/**
 * @brief report force control status
 * 
 * @return uint8_t none
 */
static uint8_t force_control_stat_report(void)
{
    uint8_t data = (0 == air_warn_force_flag)? 0:1;
    can_pakage(CAN_DEV_RK,CAN_MSG_FORCE_CTRL_STAS,1,&data);
    return 0;
}

static uint8_t charge_mode_manage(void)
{
    static uint16_t charge_time = 0;
    if(!bChargeStatus)//not charge
    {
        charge_time = 0;
        MotorCurrentSet(SYNTRON_CURRENT_MAX_A*100);
        return 0;
    }

    MotorCurrentSet(SYNTRON.max_current_set_l);
    if((0 == SYNTRON.iSpeedL) && (0 == SYNTRON.iSpeedR) && (ROBOT_MOVE_PUSH_FROM_RK != uRobotMovMode))
    {
      charge_time += MOTOR_CONTROL_TASK_CYCLE;
    }
    else
    {
      charge_time = 0;
      return 0;
    }

    if(charge_time >= CHARGE_GO_BACK_TIME)
    {
      charge_time = CHARGE_GO_BACK_TIME;
      return 1;
    }
    else
    {
      return 0;
    }
    
}

/**
 * @brief limit speed when motor run back in assistant
 * 
 * @param control 0: clear param in function. else number: none
 * @param val limit speed (mm/s),must be little than 0. example: -1200
 */
static void back_speed_limit(uint8_t control, int16_t val)
{
    static uint32_t unlock_cnt = 0;
    int16_t torqueL = 0;
    int16_t torqueR = 0;
    static uint32_t limit_level_cnt_l = 0;
    static uint32_t limit_level_cnt_r = 0;
    static int16_t limit_level_l = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
    static int16_t limit_level_r = STRAIGHT_LINE_LIMIT_PERIOD_INIT;

    if(0 == control)
    {
        unlock_cnt = 0;
        limit_level_cnt_l = 0;
        limit_level_cnt_r = 0;
        limit_level_l = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
        limit_level_r = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
        return;
    }
      
    if(val >= 0)
    {
      return;
    }

    if((SYNTRON.SpeedBackLineL >= val) && (SYNTRON.SpeedBackLineR >= val))
    {
        unlock_cnt += 10;//ms
    }
    else
    {
        unlock_cnt = 0;
    }
    
    if(unlock_cnt >= 1000)//when cnt >= 1000ms,limit speed is finished
    {
        printf("exit SpeedLimit!\r\n");
        unlock_cnt = 0;
        uMStatusMode = M_STATUS_MODE_TORQUE_OVER_RUNNING;
        limit_level_cnt_l = 0;
        limit_level_cnt_r = 0;
        limit_level_l = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
        limit_level_r = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
        return;
    }
    else//need Speed Limit
    {
        if(SYNTRON.SpeedBackLineL <= -600)
        {
            torqueL = 100*limit_level_l;
            limit_level_cnt_l += 10;//10ms
            if(limit_level_cnt_l >= 500)
            {
                limit_level_cnt_l = 0;
                limit_level_l = (limit_level_l>=10) ? 10 : (limit_level_l+1);
            }
        }
        if(SYNTRON.SpeedBackLineR <= -600)
        {
            torqueR = 100*limit_level_r;
            limit_level_cnt_r += 10;//10ms
            if(limit_level_cnt_r >= 500)
            {
                limit_level_cnt_r = 0;
                limit_level_r = (limit_level_r>=10) ? 10 : (limit_level_r+1);
            }
        }
    }
    syntron_motor_torque_set(torqueL,torqueR);
}

static void err_speed_ctrl(const bool assis)
{
    static uint8_t delay = 0;
    if(true == assis)//enter assistant mode
    {
        delay = 0;
        syntron_set_overspeed_level(1);
    }
    else if(false == assis)//quit assistant mode
    {
        if(delay < 80)//800ms
        {
            delay++;
        }
        else
        {
            syntron_set_overspeed_level(0);
        }
    }
}

static void motor_init_task(void)
{
    if(g_motor_init_flg)
    {
        syntron_motor_init();
    }
    if(SyntronMotorInitStep == SYN_MOTOR_STEP_ENABLE)
    {//motor enable
        SyntronMotorEnableInMainWhile();
    }
    if(SyntronMotorInitStep == SYN_MOTOR_STEP_OK)
    {//param set if needed
      if(g_motor_init_flg != false)
      {
          SyntronMotorParamSet();
      }

      g_motor_init_flg = false;
      g_motor_reset_flg = false;
    }
}

void motor_control_task(void *param)
{
  static uint8_t position_time_flag;
  static bool bRKStatus_old = true;
  bool assis_work_flag = false;
  watch_dog_feed(MotorControlTaskBit);

#ifdef MOTOR_INIT_IN_MAIN_WHILE
  motor_init_task();
#endif

  if(otalib_is_slave_during())//station is ota
  {
    syntron_motor_PV_confirm();
    motor_brake();
    uRobotMovingStatus = 9;
    return;
  }

  position_time_flag = charge_mode_manage();

  motor_data_report();
  MotorCANDetection();

  if(SRGST.bEmergencyStopKey)
  {
    syntron_motor_PV_confirm();
    motor_brake();
    uRobotMovingStatus = 5;
  }
  else if(bRKStatus == false)
  {
    syntron_motor_PV_confirm();
    motor_brake();
    uRobotMovingStatus = 0x0a;

    if(true == bRKStatus_old)
    {
      uint8_t data[7] = {0};
      uRobotMovingStatus = 0x0b;
      data[0] = uRobotMovingStatus;
      data[1] = (uint8_t)((((uint16_t)SYNTRON.iSpeedL)>>0)&0xff);//target speed
      data[2] = (uint8_t)((((uint16_t)SYNTRON.iSpeedL)>>8)&0xff);
      data[3] = (uint8_t)((((uint16_t)SYNTRON.iSpeedR)>>0)&0xff);
      data[4] = (uint8_t)((((uint16_t)SYNTRON.iSpeedR)>>8)&0xff);
      key_log_move_motor(data);
    }
  }
  else if(g_motor_overload_flg != 255)
  {
    motor_overload_warning_action();
  }
  else
  {
    if(uRobotMovMode == ROBOT_MOVE_PUSH_FROM_RK)//push move by people
    {
      bGeoBrakeStatus = false;
      if(true == SRGST.EasyPush)
      {
          assis_work_flag = true;
      }
      motor_ctrl(SRGST.EasyPush);
      uRobotMovingStatus = 1;
    }
    else
    {
      if(bAirCollideBrakeFlg)
      {
        if(1 == air_warn_force_flag)//open force control
        {
            if(1 == get_air_real_stats()) //air collide real status
            {
              air_warn_force_flag = 0;
            }
            else
            {
              syntron_motor_speed_set(SYNTRON.iSpeedL,SYNTRON.iSpeedR,SYNTRON.uAccL,SYNTRON.uAccR);
              uRobotMovingStatus = 0x0c;
            }
        }

        if(0 == air_warn_force_flag)
        {
            motor_brake();
            uRobotMovingStatus = 6;
        }

      }
      else if(bChargeStatus == true)//if charging cannot draw back
      {
          if((0 == SYNTRON.iSpeedL) && (0 == SYNTRON.iSpeedR))
          {
              if(1 == position_time_flag)
              {
                  syntron_motor_position_confirm();
                  uRobotMovingStatus = 0x15;
              }
              else
              {
                  motor_brake();
                  uRobotMovingStatus = 2;
              }
          }
          else
          {
              syntron_motor_speed_set(SYNTRON.iSpeedL,SYNTRON.iSpeedR,SYNTRON.uAccL,SYNTRON.uAccR);
              uRobotMovingStatus = 2;
          }

          bGeoBrakeStatus = false;
      }
      else if(bGeoBrake)
      {
        //uRobotMovingStatus = 3;
        if(bGeoBrakeSwitch)//switch open 
        {
          motor_brake();
          bGeoBrakeStatus = true;
          uRobotMovingStatus = 7;
        }
        else              //switch close
        {
          syntron_motor_speed_set(SYNTRON.iSpeedL,SYNTRON.iSpeedR,SYNTRON.uAccL,SYNTRON.uAccR);
          bGeoBrakeStatus = false;
          uRobotMovingStatus = 8;
        }
      }
      else                    //normal running
      {
        uRobotMovingStatus = 4;
        syntron_motor_speed_set(SYNTRON.iSpeedL,SYNTRON.iSpeedR,SYNTRON.uAccL,SYNTRON.uAccR);
        bGeoBrakeStatus = false;
//        printf("uAcc: %d %d\r\n",SYNTRON.uAccL,SYNTRON.uAccR);
      }
    }
  }

  err_speed_ctrl(assis_work_flag);
  bRKStatus_old = bRKStatus;
}

void motor_control_init(void)
{
  SYNTRON.uAccL = 0;
  SYNTRON.uAccR = 0;
  SyntronMotorInitStep = SYN_MOTOR_STEP_ACTIVE;
  
  SYNTRON.max_current_set_l = 4000;
  SYNTRON.max_current_set_r = 4000;
  
  while(SyntronMotorInitStep != SYN_MOTOR_STEP_ENABLE)
  {
    syntron_motor_init();
    delay_1ms(5);
  }
  tpdo2_cfg();
  delay_1ms(5);
  
  syntron_motor_enable();
}

void motor_param_check(void)
{
  uint8_t data[8] = {0};
  //params are different or never checked
  if((uParamCheckFlg&0x03) != 0x03)//0000 0011
  {
    //check time of watchdog in sytron
    data[0] = 0x40;
    data[1] = 0x0b;
    data[2] = 0x45;
    can_pakage(1,0x601,8,data);
    data[0] = 0x40;
    data[1] = 0x0c;
    data[2] = 0x45;
    can_pakage(1,0x601,8,data);
  }
  if((uParamCheckFlg&0x0c) != 0x0c)//0000 1100
  {
    //release voltage
    data[0] = 0x40;
    data[1] = 0x05;
    data[2] = 0x44;
    can_pakage(1,0x601,8,data);
    data[0] = 0x40;
    data[1] = 0x06;
    data[2] = 0x44;
    can_pakage(1,0x601,8,data);
  }
  if(uSytronSoftCheckFlg == 0x3f)//sytron soft ware check OK.
  {
      if((SYNTRON.version.uYearL >= 2023) && (SYNTRON.version.uMonth_DayL >= 215))//hardware brake check
      {
          if((uParamCheckFlg&0x30) != 0x30)//0011 0000
          {
              //hardware brake
              data[0] = 0x40;
              data[1] = 0x00;
              data[2] = 0x41;
              can_pakage(1,0x601,8,data);
          }
      }
  }
  if((uParamCheckFlg&0xc0) != 0xc0)//1100 0000
  {
      //low power protection threshold ->5V
      data[0] = 0x40;
      data[1] = 0x04;
      data[2] = 0x44;
      can_pakage(1,0x601,8,data);
  }
  if((uParamCheckFlg2&0x03) != 0x03)//0000 0011
  {
    data[0] = 0x40;
    data[1] = 0x2e;
    data[2] = 0x24;
    can_pakage(1,0x601,8,data);
    data[0] = 0x40;
    data[1] = 0x2e;
    data[2] = 0x34;
    can_pakage(1,0x601,8,data);
  }
}

void motor_caculation_param_check(void)
{
    uint8_t data[8] = {0};
    
    data[0] = 0x40;
    
    if((uCalParamCheckFlg&0x0003) != 0x0003)//0000 0000 0000 0011
    {
        data[1] = 0x00;
        data[2] = 0x23;
        can_pakage(1,0x601,8,data);//pid_1 p left
        data[1] = 0x00;
        data[2] = 0x33;
        can_pakage(1,0x602,8,data);//pid_1 p right
    }
    else if((uCalParamCheckFlg&0x000c) != 0x000c)//0000 0000 0000 1100
    {
        data[1] = 0x01;
        data[2] = 0x23;
        can_pakage(1,0x601,8,data);//pid_1 i left
        data[1] = 0x01;
        data[2] = 0x33;
        can_pakage(1,0x602,8,data);//pid_1 i right
    }
    else if((uCalParamCheckFlg&0x0030) != 0x0030)//0000 0000 0011 0000
    {
        data[1] = 0x03;
        data[2] = 0x23;
        can_pakage(1,0x601,8,data);//pid_2 p left
        data[1] = 0x03;
        data[2] = 0x33;
        can_pakage(1,0x602,8,data);//pid_2 p right
    }
    else if((uCalParamCheckFlg&0x00c0) != 0x00c0)//0000 0000 1100 0000
    {
        data[1] = 0x04;
        data[2] = 0x23;
        can_pakage(1,0x601,8,data);//pid_2 i left
        data[1] = 0x04;
        data[2] = 0x33;
        can_pakage(1,0x602,8,data);//pid_2 i right
    }
    else if((uCalParamCheckFlg&0x0300) != 0x0300)//0000 0011 0000 0000
    {
        data[1] = 0x0d;
        data[2] = 0x23;
        can_pakage(1,0x601,8,data);//convert speed left
        data[1] = 0x0d;
        data[2] = 0x33;
        can_pakage(1,0x602,8,data);//convert speed right
    }
    else if((uCalParamCheckFlg&0x0c00) != 0x0c00)//0000 1100 0000 0000
    {
        data[1] = 0x01;
        data[2] = 0x24;
        can_pakage(1,0x601,8,data);//current pid i left
        data[1] = 0x01;
        data[2] = 0x34;
        can_pakage(1,0x602,8,data);//current pid i right
    }
}

void motor_data_report(void)
{
  uint8_t data_t[8] = {0};
  static int32_t TestMileageL_t = 0;
  static int32_t TestMileageR_t = 0;
  
  SYNTRON.SpeedBackLineL = syntron_motor_line_speed_calculation(SYNTRON.speedBackL);
  SYNTRON.SpeedBackLineR = syntron_motor_line_speed_calculation(SYNTRON.speedBackR);

  check_motor_work_status(&motor_work_status,SYNTRON.SpeedBackLineL,SYNTRON.SpeedBackLineR,
    SYNTRON.iSpeedL,SYNTRON.iSpeedR);
  
  MileageL_t = SyntronMotorMileageCalculation(0);
  MileageR_t = SyntronMotorMileageCalculation(1);
  // printf("fortest mile 0x%X 0x%X\n", (uint32_t)SYNTRON.MileageBackL, (uint32_t)SYNTRON.MileageBackR);
  
  if(((abs(MileageL_t - TestMileageL_t)) >= 50) || ((abs(MileageR_t - TestMileageR_t)) >= 50))
  {
    uint8_t MileageErr[7] = {0};
    
    key_log_move_mileagel_err(MileageErr);
    log_print("ERR","\r\n %d %d %d %d\r\n",MileageL_t,TestMileageL_t,MileageR_t,TestMileageR_t);
  }
  
  TestMileageL_t = MileageL_t;
  TestMileageR_t = MileageR_t;

  if((MotorPrintCnt%70) == 0)//700ms
  {
      syntron_get_overspeed_level(SMO_SEND,SMD_NONE,(uint16_t*)0);
  }
  if((MotorPrintCnt%100) == 0)//1s
  {
    uint16_t uElcCurrentL,uElcCurrentR;
    uElcCurrentL = SYNTRON.uMomentDutyL*SYNTRON_CURRENT_MAX_A;
    uElcCurrentR = SYNTRON.uMomentDutyR*SYNTRON_CURRENT_MAX_A;
    data_t[0] = 7;
    data_t[1] = 2;
    data_t[2] = (uint8_t)((uElcCurrentL>>0)&0xff);
    data_t[3] = (uint8_t)((uElcCurrentL>>8)&0xff);
    data_t[4] = (uint8_t)((uElcCurrentR>>0)&0xff);
    data_t[5] = (uint8_t)((uElcCurrentR>>8)&0xff);
    can_pakage(CAN_DEV_RK,CAN_MSG_MOTOR_ELECTRIC_CURRENT,6,data_t);
    
    syntron_motor_err_code();
    motor_param_check();
    syntron_motor_soft_version_check();
    MotorCurrentAsk();
    key_log_motor_max_current(0);
    MotorTemperatureAsk();
    motor_caculation_param_check();
    SytronCurrentResetAsk();
    SytronOverloadTankAsk();
    force_control_stat_report();
    log_print("SYTRON","version %d %d %d %d %d %d\r\n",SYNTRON.version.uYearL,SYNTRON.version.uYearR,
                                                           SYNTRON.version.uMonth_DayL,SYNTRON.version.uMonth_DayR,
                                                           SYNTRON.version.uSerialNumberL,SYNTRON.version.uSerialNumberR);
    log_print("SYTRON","overload_tank: %d %d %d %d\r\n",SYNTRON.overload_tank_l,SYNTRON.overload_tank_r,SYNTRON.current_reset_cnt_l,SYNTRON.current_reset_cnt_r);
  }
  
  if(MotorPrintCnt++ >= 300)
  {
    MotorPrintCnt =1;
    log_print("INFO","speed: Taget:%d %d Det:%d %d mileage: %d %d SYNTRON.Acc: %d %d\r\n",SYNTRON.iSpeedL,SYNTRON.iSpeedR,SYNTRON.SpeedBackLineL,SYNTRON.SpeedBackLineR,MileageL_t,MileageR_t,SYNTRON.uAccL,SYNTRON.uAccR);
//    printf("-------------SYNTRON.Acc: %d  %d\r\n",SYNTRON.uAccL,SYNTRON.uAccR);
    uint8_t data[7] = {0};
    
    data[0] = uRobotMovingStatus;
    data[1] = (uint8_t)((((uint16_t)SYNTRON.iSpeedL)>>0)&0xff);//target speed
    data[2] = (uint8_t)((((uint16_t)SYNTRON.iSpeedL)>>8)&0xff);
    data[3] = (uint8_t)((((uint16_t)SYNTRON.iSpeedR)>>0)&0xff);
    data[4] = (uint8_t)((((uint16_t)SYNTRON.iSpeedR)>>8)&0xff);
    key_log_move_motor(data);
    ParamSytronCurrentInChargingAsk();
  }

  SYNTRON.uTimeStampL = time_timestamp_get();
  data_t[0] = 1;//left
  data_t[1] = (uint8_t)((SYNTRON.uTimeStampL>>0)&0xff);
  data_t[2] = (uint8_t)((SYNTRON.uTimeStampL>>8)&0xff);
  data_t[3] = (uint8_t)((SYNTRON.uTimeStampL>>16)&0xff);
  data_t[4] = (uint8_t)((SYNTRON.uTimeStampL>>24)&0xff);
  data_t[5] = (uint8_t)((SYNTRON.uMomentDutyL>>0)&0xff);
  data_t[6] = (uint8_t)((SYNTRON.uMomentDutyL>>8)&0xff);
  data_t[7] = (uint8_t)(SYNTRON.overload_tank_l/10);
  can_pakage(CAN_DEV_RK,CAN_MSG_SYNTRON_TIMESTAMP,8,data_t);
  data_t[0] = 1;//left
  data_t[1] = (uint8_t)((((uint16_t)SYNTRON.SpeedBackLineL)>>0)&0xff);
  data_t[2] = (uint8_t)((((uint16_t)SYNTRON.SpeedBackLineL)>>8)&0xff);
  data_t[3] = (uint8_t)((((uint32_t)MileageL_t)>>0)&0xff);
  data_t[4] = (uint8_t)((((uint32_t)MileageL_t)>>8)&0xff);
  data_t[5] = (uint8_t)((((uint32_t)MileageL_t)>>16)&0xff);
  data_t[6] = (uint8_t)((((uint32_t)MileageL_t)>>24)&0xff);
  data_t[7] = (motor_work_status.status_l & 0x03) |
              (((uint8_t)(motor_work_status.target_spd_l & 0x03)) << 2) | 
              (((uint8_t)(motor_work_status.cur_spd_l & 0x03)) << 4);
  can_pakage(CAN_DEV_RK,CAN_MSG_SYNTRON_SPEED_MILEAGE,8,data_t);
   
  SYNTRON.uTimeStampR = time_timestamp_get();
  data_t[0] = 2;//right
  data_t[1] = (uint8_t)((SYNTRON.uTimeStampR>>0)&0xff);
  data_t[2] = (uint8_t)((SYNTRON.uTimeStampR>>8)&0xff);
  data_t[3] = (uint8_t)((SYNTRON.uTimeStampR>>16)&0xff);
  data_t[4] = (uint8_t)((SYNTRON.uTimeStampR>>24)&0xff);
  data_t[5] = (uint8_t)((SYNTRON.uMomentDutyR>>0)&0xff);
  data_t[6] = (uint8_t)((SYNTRON.uMomentDutyR>>8)&0xff);
  data_t[7] = (uint8_t)(SYNTRON.overload_tank_r/10);
  can_pakage(CAN_DEV_RK,CAN_MSG_SYNTRON_TIMESTAMP,8,data_t);
  data_t[0] = 2;//right
  data_t[1] = (uint8_t)((((uint16_t)SYNTRON.SpeedBackLineR)>>0)&0xff);
  data_t[2] = (uint8_t)((((uint16_t)SYNTRON.SpeedBackLineR)>>8)&0xff);
  data_t[3] = (uint8_t)((((uint32_t)MileageR_t)>>0)&0xff);
  data_t[4] = (uint8_t)((((uint32_t)MileageR_t)>>8)&0xff);
  data_t[5] = (uint8_t)((((uint32_t)MileageR_t)>>16)&0xff);
  data_t[6] = (uint8_t)((((uint32_t)MileageR_t)>>24)&0xff);
  data_t[7] = (motor_work_status.status_r & 0x03) |
              (((uint8_t)(motor_work_status.target_spd_r & 0x03)) << 2) | 
              (((uint8_t)(motor_work_status.cur_spd_r & 0x03)) << 4);

  can_pakage(CAN_DEV_RK,CAN_MSG_SYNTRON_SPEED_MILEAGE,8,data_t);

}

void MotorCANDetection(void)
{
    uint8_t data[8] = {0};
    static uint8_t motor_can_det_cnt = 0;
    
    motor_can_det_cnt++;
    if(motor_can_det_cnt >= 100)//1s
    {
        motor_can_det_cnt = 0;
    }
    else
    {
        return;
    }
    
    if(g_motor_msg_cnt == 0)//msg lost
    {
        data[2] = 1;
    }
    
    data[0] = SYNTRON.uErrCodeL;
    data[1] = SYNTRON.uErrCodeR;
    can_pakage(CAN_DEV_RK,CAN_MSG_MOTOR_ERR_CODE,3,data); 
    g_motor_msg_cnt = 0;//clear counts
}

void motor_mov_mode_handle(uint8_t *data)
{  
  if(data[0] == 0)//change into ROBOT_MOVE_AUTO
  {
    uRobotMovMode = ROBOT_MOVE_AUTO;   
    printf("uRobotMovMode change into ROBOT_MOVE_AUTO\r\n");
    bGeoBrake = false;
  }
  else if(data[0] == 1)
  {
      if(SRGST.EasyPush == true)//enter asistance mode
      {
          if(uRobotMovMode != ROBOT_MOVE_PUSH_FROM_RK)
          {
              uMStatusMode = M_STATUS_MODE_TORQUE_ENABLE;
          }
          printf("1");
      }
      else if(SRGST.EasyPush == false)//exit asistance mode
      {
          uMStatusMode = M_STATUS_MODE_TORQUE_DISABLE;
          printf("2");
      }
      uRobotMovMode = ROBOT_MOVE_PUSH_FROM_RK;
      printf("uRobotMovMode change into ROBOT_MOVE_PUSH_FROM_RK\r\n");
  }

  data[0] = uRobotMovMode;
  can_pakage(CAN_DEV_RK,CAN_MSG_MOV_MODE_CTRL_ACK,1,data);
  
  uint8_t uKeyData[7] = {0};
  uKeyData[0] = data[0];
  keylog_report_currently(KEYLOG_MOV_MODE_CHECK,uKeyData);
}

//kfp_ramp
int16_t ramp_angle_test = 0;
uint8_t ramp_detection(void)
{
  static uint8_t ret = 0;
  static uint32_t ramp_flat_cnt = 0;
  static int16_t ramp_angle_cnt = 0;

  static uint64_t time_old = 0; 
  int16_t ramp_angle = 0;

  if(get_sys_time() - time_old >= 2000)
  {
      ramp_angle_cnt = ramp_flat_cnt = 0;
      ret = 0;
  }
  time_old = get_sys_time();

  IMU_DATA_t *imu = GetImuData();
  ramp_angle = imu->iPich;
  //ramp check
  if((ramp_angle < IMU_UPHILL_THRESHOLD) && (ramp_angle > IMU_DOWNHILL_THRESHOLD)) //flat
  {
    ramp_angle_cnt = ramp_flat_cnt = 0;
    ret = 0;
  }
  else if(ramp_angle >= (IMU_UPHILL_THRESHOLD+100))//up
  {
      //uphill slope
      ramp_angle_cnt += 10;
      ramp_flat_cnt = 0;
  }
  else if(ramp_angle <= (IMU_DOWNHILL_THRESHOLD-100))//down
  {
      //downhill slope
      ramp_angle_cnt -= 10;
      ramp_flat_cnt = 0;
  }
  
  // time check
  if(ramp_angle_cnt >= 1000)//1s
  {
      ramp_angle_cnt = 1000;
      ret = 1;
  }
  else if(ramp_angle_cnt <= (-1000))//1s
  {
      ramp_angle_cnt = -1000;
      ret = 2;
  }
  else if(ramp_flat_cnt >= 1000)
  {
      ramp_flat_cnt = 1000;
      ramp_angle_cnt = 0;
      ret = 0;
  }
  return ret;
}

double get_turn_radius(void)
{
    double v1 = (double)SYNTRON.SpeedBackLineL;
    double v2 = (double)SYNTRON.SpeedBackLineR;

    double v_diff = v1-v2;
    if(v_diff < 100)//  mm/s
    {
        return 1000000;//1km
    }

    double v = (v1+v2)/2;
    double w = v_diff/WHEEL_DISTANCE;
    double r = v / w;
    if(r < 0)
    {
        r = -r;
    }
    return r;
}

bool is_robot_spinning(void)
{
    if(get_turn_radius()<2000)
        return true;
    else 
        return false;
}

/**
 * @brief motor_ctrl
 * 
 * @param status:
 *    true: assistant button is pushed
 *    false: assistant button is not pushed
 */

void reset_turn_pid() 
{
  AsistancePidTurnL.SetValue = 0;
  AsistancePidTurnL.Error = 0;
  AsistancePidTurnL.S_Error = 0;
  AsistancePidTurnL.SS_Error = 0;
  AsistancePidTurnL.Inc = 0;
  AsistancePidTurnL.Output = 0;
  
  AsistancePidTurnR.SetValue = 0;
  AsistancePidTurnR.Error = 0;
  AsistancePidTurnR.S_Error = 0;
  AsistancePidTurnR.SS_Error = 0;
  AsistancePidTurnR.Inc = 0;
  AsistancePidTurnR.Output = 0;
}

void reset_line_pid() 
{
  AsistancePidL.SetValue = 0;
  AsistancePidL.Error = 0;
  AsistancePidL.S_Error = 0;
  AsistancePidL.SS_Error = 0;
  AsistancePidL.Inc = 0;
  AsistancePidL.Output = 0;
  
  AsistancePidR.SetValue = 0;
  AsistancePidR.Error = 0;
  AsistancePidR.S_Error = 0;
  AsistancePidR.SS_Error = 0;
  AsistancePidR.Inc = 0;
  AsistancePidR.Output = 0;
}

void motor_ctrl(bool status)
{
  uint8_t uRampStatus = 0;
  static uint8_t ramp_status_t = 0;
  bool motor_key = false;
/* uRampStatus
   0 : flat ground
   1 : uphill slope
   2 : downhill slope
*/
  uRampStatus = ramp_detection();
  
  if((bAsistanceStatus_t != status)  // assistant button status has changed
  || (uRampStatus != ramp_status_t)) // ramp status has changed
  {
    motor_key = true;
  }

  if((uRampStatus == 2) || (uRampStatus == 1))
  {
    AisstantInHill(status,motor_key);
  }
  else if(uRampStatus == 0)
  {
      static uint32_t USuspendDelayCnt = 0;
      //mode judement
      if((bAsistanceStatus_t != status) || (uRampStatus != ramp_status_t))//key status changed
      {
        //clear speed and torque
        SYNTRON.iSpeedL = 0;
        SYNTRON.iSpeedR = 0;
        iSetTorqueL = 0;
        iSetTorqueR = 0;
        
        if(status == true)//enter asistance mode
        {
          uMStatusMode = M_STATUS_MODE_TORQUE_ENABLE;// 1
        }
        else if(status == false)//exit asistance mode
        {
          uMStatusMode = M_STATUS_MODE_TORQUE_DISABLE;
        }
      }
      else if(status == false)//no power
      {
        uMStatusMode = M_STATUS_MODE_SUSPEND;
      }
      else if(status == true)//running in PT
      {

      }

      //action rely on uMStatusMode
      switch(uMStatusMode)
      {
      /* enter mode PT */
        case M_STATUS_MODE_TORQUE_ENABLE://2
          iSetTorqueL = 0;
          iSetTorqueR = 0;
          syntron_motor_torque_set(iSetTorqueL,iSetTorqueR);
          back_speed_limit(0,0);
          uAsisStatus = 0;

          syntron_motor_control_switch(0);//disable motor
          syntron_motor_mode_switch(1);
          syntron_motor_control_switch(1);//enable motor          
          uMStatusMode = M_STATUS_MODE_TORQUE_DET;
          printf(">> start\r\n");
          break;
          
      /*
      enter mode PV, but still in ROBOT_MOVE_PUSH_FROM_RK status(do not need 2s key).
      */
        case M_STATUS_MODE_TORQUE_DISABLE:
          back_speed_limit(0,0);
          uAsisStatus = 0;
          USuspendDelayCnt = 0;
          break;
          
      /* key released,  */
        case M_STATUS_MODE_SUSPEND:
          //stop 1000ms delay
          if(USuspendDelayCnt < 990)//reduce speed in torque
          {
            //if robot still run without pushing, provide resistance.
            iSetTorqueL = (int16_t)(0.8*(-SYNTRON.SpeedBackLineL));
            iSetTorqueR = (int16_t)(0.8*(-SYNTRON.SpeedBackLineR));
            syntron_motor_torque_set(iSetTorqueL,iSetTorqueR);
          }
          else if(USuspendDelayCnt == 990)//brake
          {
            SYNTRON.iSpeedL = 0;
            SYNTRON.iSpeedR = 0;
            iSetTorqueL = 0;
            iSetTorqueR = 0;
            syntron_motor_control_switch(0);
            syntron_motor_mode_switch(0);
            syntron_motor_control_switch(1);
          }

          USuspendDelayCnt += 10;//10ms

          if((SYNTRON.SpeedBackLineL <= 0) && (SYNTRON.SpeedBackLineR <= 0))
          {
            if(USuspendDelayCnt < 990)
            {
                syntron_motor_control_switch(0);
                syntron_motor_mode_switch(0);
                syntron_motor_control_switch(1);
                USuspendDelayCnt = 1000; 
            }
          }

          if(USuspendDelayCnt >= 1000)//1s
          {
            USuspendDelayCnt = 1000;
            motor_brake();
          }
          break;
          
      /* easy push running. */
        case M_STATUS_MODE_TORQUE_RUNNING:
          if(abs(iSetTorqueL-iSetTorqueR) >= 50 || is_robot_spinning()) //&& ((iSetTorqueL*iSetTorqueR)<0)                               //turn right/left
          {
            if(uAsisStatus != 1)//uAsisStatus changed
            {
              iSetTorqueL = 0;
              iSetTorqueR = 0;
              
              //clear pid param
              AsistancePidTurnL.SetValue = 0;
              AsistancePidTurnL.Error = 0;
              AsistancePidTurnL.S_Error = 0;
              AsistancePidTurnL.SS_Error = 0;
              AsistancePidTurnL.Inc = 0;
              AsistancePidTurnL.Output = 0;
              
              AsistancePidTurnR.SetValue = 0;
              AsistancePidTurnR.Error = 0;
              AsistancePidTurnR.S_Error = 0;
              AsistancePidTurnR.SS_Error = 0;
              AsistancePidTurnR.Inc = 0;
              AsistancePidTurnR.Output = 0;
            }
            motor_torque_calculation_turn();
            uAsisStatus = 1;//when assistant end or start, this param need to be cleared!
          }
          else//go straight on
          {
            if(uAsisStatus != 2)//uAsisStatus changed
            {
              iSetTorqueL = 0;
              iSetTorqueR = 0;
              
              //clear pid param
              AsistancePidL.SetValue = fAsisTargetValueL;
              AsistancePidL.Error = 0;
              AsistancePidL.S_Error = 0;
              AsistancePidL.SS_Error = 0;
              AsistancePidL.Inc = 0;
              AsistancePidL.Output = 0;
              
              AsistancePidR.SetValue = fAsisTargetValueR;
              AsistancePidR.Error = 0;
              AsistancePidR.S_Error = 0;
              AsistancePidR.SS_Error = 0;
              AsistancePidR.Inc = 0;
              AsistancePidR.Output = 0;
            }
            motor_torque_calculation();
            uAsisStatus = 2;//when assistant end or start, this param need to be cleared!
          }
          iSetTorqueL = (abs(iSetTorqueL)>SYN_MOTOR_TORQUE_MAX) ? (((iSetTorqueL>0)?1:-1)*SYN_MOTOR_TORQUE_MAX) : iSetTorqueL;
          iSetTorqueR = (abs(iSetTorqueR)>SYN_MOTOR_TORQUE_MAX) ? (((iSetTorqueR>0)?1:-1)*SYN_MOTOR_TORQUE_MAX) : iSetTorqueR;
          if(   ((abs(SYNTRON.SpeedBackLineL)>1000) && (abs(SYNTRON.SpeedBackLineR)>1000))
              &&((SYNTRON.SpeedBackLineL*SYNTRON.SpeedBackLineR)<0) 
              &&((SYNTRON.uMomentDutyL>300)&&(SYNTRON.uMomentDutyR>300))
            )
          {
            iSetTorqueL = 0;
            iSetTorqueR = 0;
            printf("aaaaaa\r\n");
          }
          syntron_motor_torque_set(iSetTorqueL,iSetTorqueR);
          break;
        case M_STATUS_MODE_TORQUE_BACK:
          iSetTorqueL = -50;
          iSetTorqueR = -50;
          syntron_motor_torque_set(iSetTorqueL,iSetTorqueR);
          if((SYNTRON.SpeedBackLineL <= BACK_SPEED_LIMIT) || (SYNTRON.SpeedBackLineR <= BACK_SPEED_LIMIT))
          {
              uMStatusMode = M_STATUS_MODE_TORQUE_BACKSPEED_LIMIT;
          }
          break;
        case M_STATUS_MODE_TORQUE_OVER_RUNNING:
          if((abs(SYNTRON.SpeedBackLineL) <= 500) && (abs(SYNTRON.SpeedBackLineR) <= 500))
          {
            iSetTorqueL = 0;
            iSetTorqueR = 0;
            uMStatusMode = M_STATUS_MODE_TORQUE_DET;
            syntron_motor_torque_set(50,50);
          }
          else if((SYNTRON.SpeedBackLineL > 1400) || (SYNTRON.SpeedBackLineR > 1400))//over speed
          {
              uMStatusMode = M_STATUS_MODE_TORQUE_SPEED_LIMIT;
              syntron_motor_torque_set(50,50);
              printf("enter M_STATUS_MODE_TORQUE_SPEED_LIMIT!!!\r\n");
          }
          else if((SYNTRON.SpeedBackLineL < BACK_SPEED_LIMIT) || (SYNTRON.SpeedBackLineR < BACK_SPEED_LIMIT))//over speed
          {
              uMStatusMode = M_STATUS_MODE_TORQUE_BACKSPEED_LIMIT;
              syntron_motor_torque_set(0,0);
          }
          else
          {
              syntron_motor_torque_set(0,0);
          }
          break;
        case M_STATUS_MODE_TORQUE_BACKSPEED_LIMIT:
          back_speed_limit(1, BACK_SPEED_LIMIT - 200);
          break;
        case M_STATUS_MODE_TORQUE_SPEED_LIMIT:
          SpeedLimit(1500);
          break;
        case M_STATUS_MODE_TORQUE_DET: //3
          iSetTorqueL = 0;
          iSetTorqueR = 0;
          uAsisStatus = 0;
          back_speed_limit(0,0);
          if((SYNTRON.SpeedBackLineL>=200) && (SYNTRON.SpeedBackLineR>=200))
          {
            uMStatusMode = M_STATUS_MODE_TORQUE_RUNNING;
            iSetTorqueL = 0;
            iSetTorqueR = 0;
            uAsisRunningStep = 0;
            fAsisTargetValueL = 500;
            fAsisTargetValueR = 500;
            uAsisStopCnt = 0;
            
            //clear pid param
            AsistancePidL.SetValue = fAsisTargetValueL;
            AsistancePidL.Error = 0;
            AsistancePidL.S_Error = 0;
            AsistancePidL.SS_Error = 0;
            AsistancePidL.Inc = 0;
            AsistancePidL.Output = 0;
            
            AsistancePidR.SetValue = fAsisTargetValueR;
            AsistancePidR.Error = 0;
            AsistancePidR.S_Error = 0;
            AsistancePidR.SS_Error = 0;
            AsistancePidR.Inc = 0;
            AsistancePidR.Output = 0;
          }
          else if((SYNTRON.SpeedBackLineL<=-200) && (SYNTRON.SpeedBackLineR<=-200))
          {
            uMStatusMode = M_STATUS_MODE_TORQUE_BACK;
            iSetTorqueL = -50;
            iSetTorqueR = -50;
          }
          else
          {
              if((abs(SYNTRON.SpeedBackLineL - SYNTRON.SpeedBackLineR)) > 100)//take a turn in place
              {
                  TurnInPlace();
              }
              else
              {
                  syntron_motor_torque_set(0,0);
              }
          }
          break;
        default:
          break;
      } 
  }
  
  bAsistanceStatus_t = status;
  ramp_status_t = uRampStatus;
}

void motor_torque_calculation(void)
{
  AsistancePidL.ActualValue = (float)SYNTRON.SpeedBackLineL;
  AsistancePidR.ActualValue = (float)SYNTRON.SpeedBackLineR;
  iSetTorqueL = (int16_t)asistance_pid_calculation(&AsistancePidL);
  iSetTorqueR = (int16_t)asistance_pid_calculation(&AsistancePidR);

  if((iSetTorqueL < -200) && (iSetTorqueR < -200)
  && (SYNTRON.SpeedBackLineL >= 400) && (SYNTRON.SpeedBackLineR >= 400))
  {
    iSetTorqueL = 50;
    iSetTorqueR = 50;
    uMStatusMode = M_STATUS_MODE_TORQUE_OVER_RUNNING;
  }
/******************************** left release ********************************/
  static uint16_t uMomentDutyL_t[10] = {0};
  uint16_t uMomentDutyLTmp[9];
  
  memcpy(uMomentDutyLTmp,&uMomentDutyL_t[1],18);
  memcpy(uMomentDutyL_t,uMomentDutyLTmp,18);
  uMomentDutyL_t[9] = SYNTRON.uMomentDutyL;//转矩

  uint8_t uRiseCntL = 0;
  uint16_t uDeltaAccumulationL = 0;
  for(uint8_t i=0;i<9;i++)
  {
    if( (((int16_t)uMomentDutyL_t[i+1])-((int16_t)uMomentDutyL_t[i])) > 0  )
    {
      uRiseCntL++;
      uDeltaAccumulationL += ((int16_t)uMomentDutyL_t[i+1])-((int16_t)uMomentDutyL_t[i]);
    }
  }
/******************************** right release ********************************/
  static uint16_t uMomentDutyR_t[10] = {0};
  uint16_t uMomentDutyRTmp[9];
  
  memcpy(uMomentDutyRTmp,&uMomentDutyR_t[1],18);
  memcpy(uMomentDutyR_t,uMomentDutyRTmp,18);
  uMomentDutyR_t[9] = SYNTRON.uMomentDutyR;
  
  uint8_t uRiseCntR = 0;
  uint16_t uDeltaAccumulationR = 0;
  for(uint8_t i=0;i<9;i++)
  {
    if(  (((int16_t)uMomentDutyR_t[i+1])-((int16_t)uMomentDutyR_t[i])) > 0  )
    {
      uRiseCntR++;
      uDeltaAccumulationR += ((int16_t)uMomentDutyR_t[i+1])-((int16_t)uMomentDutyR_t[i]);
    }
  }

//output set zero
  if((iSetTorqueL>0) && (iSetTorqueR>0))//go forward
  {
    if((uRiseCntL >= 7) && (uRiseCntR >= 7))//rising output
    {
      if((uDeltaAccumulationL >= 150) && (uDeltaAccumulationR >= 150))
      {
        iSetTorqueL = 0;
        iSetTorqueR = 0;
        uMStatusMode = M_STATUS_MODE_TORQUE_DET;
      }
    }
    
    if( ((((int16_t)uMomentDutyL_t[9])-((int16_t)uMomentDutyL_t[0])) > 50) 
      &&((((int16_t)uMomentDutyR_t[9])-((int16_t)uMomentDutyR_t[0])) > 50)
      )
    {
      uAsisStopCnt++;
      printf(" uAsisStopCnt %d\r\n",uAsisStopCnt);
      if(uAsisStopCnt > 15)
      {
        iSetTorqueL = 0;
        iSetTorqueR = 0;
        uMStatusMode = M_STATUS_MODE_TORQUE_DET;
      }
    }
  }
}

void motor_torque_calculation_turn(void)
{
  static uint8_t limit_turn_flg = 0;
  static uint8_t limit_turn_flg_t = 0;
  int16_t iMomentDutyL,iMomentDutyR;
  
  if((SYNTRON.SpeedBackLineL > 600) || (SYNTRON.SpeedBackLineR > 600))
  {
      if(SYNTRON.SpeedBackLineL >= SYNTRON.SpeedBackLineR)
      {
          limit_turn_flg = 1;//turn right over speed
      }
      else if(SYNTRON.SpeedBackLineL < SYNTRON.SpeedBackLineR)
      {
          limit_turn_flg = 2;//turn left over speed
      }
  }
  else//speed under threshold
  {
      limit_turn_flg = 0;
  }
  
  if(limit_turn_flg != limit_turn_flg_t)
  {
      //clear pid param
      AsistancePidTurnL.SetValue = 0;
      AsistancePidTurnL.Error = 0;
      AsistancePidTurnL.S_Error = 0;
      AsistancePidTurnL.SS_Error = 0;
      AsistancePidTurnL.Inc = 0;
      AsistancePidTurnL.Output = 0;
      
      AsistancePidTurnR.SetValue = 0;
      AsistancePidTurnR.Error = 0;
      AsistancePidTurnR.S_Error = 0;
      AsistancePidTurnR.SS_Error = 0;
      AsistancePidTurnR.Inc = 0;
      AsistancePidTurnR.Output = 0;
      
      limit_turn_flg_t = limit_turn_flg;
  }
  
  if(limit_turn_flg == 1)
  {
      AsistancePidTurnL.ActualValue = SYNTRON.SpeedBackLineL;
      AsistancePidTurnL.SetValue = 300;
      
      AsistancePidTurnR.ActualValue = SYNTRON.SpeedBackLineR;
      AsistancePidTurnR.SetValue = -300;
      
      printf(">turn right over speed.\r\n");
  }
  else if(limit_turn_flg == 2)
  {
      AsistancePidTurnL.ActualValue = SYNTRON.SpeedBackLineL;
      AsistancePidTurnL.SetValue = -300;
      
      AsistancePidTurnR.ActualValue = SYNTRON.SpeedBackLineR;
      AsistancePidTurnR.SetValue = 300;
      
      printf(">turn left over speed.\r\n");
  }
  else if(limit_turn_flg == 0)//normal
  {
    iMomentDutyL = (int16_t)SYNTRON.uMomentDutyL;
    iMomentDutyR = (int16_t)SYNTRON.uMomentDutyR;
    AsistancePidTurnL.ActualValue = iMomentDutyL - iMomentDutyR;
    AsistancePidTurnR.ActualValue = iMomentDutyR - iMomentDutyL;
    
    printf(">speed under threshold.\r\n");
  }
  
  iSetTorqueL = (int16_t)asistance_pid_calculation(&AsistancePidTurnL);
  iSetTorqueR = (int16_t)asistance_pid_calculation(&AsistancePidTurnR);
}

float asistance_pid_calculation(PID_PARAM_t *pid)
{
  pid->SS_Error = pid->S_Error;
  pid->S_Error = pid->Error;
  pid->Error = pid->SetValue - pid->ActualValue;
  
  pid->Inc = pid->Kp * (pid->Error - pid->S_Error) + 
             pid->Ki *  pid->Error + 
             pid->Kd * (pid->Error - 2*pid->S_Error + pid->SS_Error);
  
  pid->Output += pid->Inc;
  
  return pid->Output;
}

void SpeedLimit(int16_t val)
{
    static uint32_t unlock_cnt = 0;
    int16_t torqueL = 0;
    int16_t torqueR = 0;
    static uint32_t limit_level_cnt_l = 0;
    static uint32_t limit_level_cnt_r = 0;
    static int16_t limit_level_l = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
    static int16_t limit_level_r = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
    
    if((SYNTRON.SpeedBackLineL <= val) && (SYNTRON.SpeedBackLineR <= val))
    {
        unlock_cnt += 10;//ms
        printf("speed slow down...\r\n");
    }
    else
    {
        unlock_cnt = 0;
    }
    
    if(unlock_cnt >= 1000)//1s
    {
        printf("exit SpeedLimit!\r\n");
        unlock_cnt = 0;
        uMStatusMode = M_STATUS_MODE_TORQUE_OVER_RUNNING;
        limit_level_cnt_l = 0;
        limit_level_cnt_r = 0;
        limit_level_l = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
        limit_level_r = STRAIGHT_LINE_LIMIT_PERIOD_INIT;
        return;
    }
    else//need Speed Limit
    {
        printf("\r\nSpeed Limit. %d | %d\r\n",limit_level_l,limit_level_r);
        if(SYNTRON.SpeedBackLineL >= (600))
        {
            torqueL = (-100)*limit_level_l;
            limit_level_cnt_l += 10;//10ms
            if(limit_level_cnt_l >= 500)
            {
                limit_level_cnt_l = 0;
                limit_level_l = (limit_level_l>=10) ? 10 : (limit_level_l+1);
            }
        }
        if(SYNTRON.SpeedBackLineR >= (600))
        {
            torqueR = (-100)*limit_level_r;
            limit_level_cnt_r += 10;//10ms
            if(limit_level_cnt_r >= 500)
            {
                limit_level_cnt_r = 0;
                limit_level_r = (limit_level_r>=10) ? 10 : (limit_level_r+1);
            }
        }
    }
    
    syntron_motor_torque_set(torqueL,torqueR);
}

void key_log_move_motor(uint8_t *data)
{
  uint8_t uKeyData[7] = {0};
  
  memcpy(uKeyData,data,7);
  
  keylog_report_currently(KEYLOG_MOTOR_RUNNING_STATUS,uKeyData);
}

void key_log_move_mileagel_err(uint8_t *data)
{
  uint8_t uKeyData[7] = {0};
  
  memcpy(uKeyData,data,7);
  
  keylog_report_currently(KEYLOG_MOTOR_MILEAGEL_ERR,uKeyData);
}

int16_t kalmanfilter(KALMAN_FILTER_PARAMETER *kfp,float input)
{
	kfp->P_now = kfp->P_last + kfp->Q;
	kfp->K = kfp->P_now/(kfp->P_now + kfp->R);
	kfp->X_out = kfp->X_out + kfp->K*(input - kfp->X_out);
	kfp->P_last = (1 - kfp->K)*kfp->P_now;
	
	return (int16_t)kfp->X_out;
}

void TurnInPlace(void)
{
    static uint32_t limit_level_cnt_l = 0;
    static uint32_t limit_level_cnt_r = 0;
    static int16_t limit_level_l = 1;
    static int16_t limit_level_r = 1;
    
    if(SYNTRON.SpeedBackLineL > 600)
    {
        iSetTorqueL = -100 * limit_level_l;
        iSetTorqueR = 100 * limit_level_l;
        limit_level_cnt_l += 10;//10ms
        if(limit_level_cnt_l >= 500)
        {
            limit_level_cnt_l = 0;
            limit_level_l = (limit_level_l>=10) ? 10 : (limit_level_l+1);
        }
        
        printf(">turn right limit. %d\r\n",limit_level_l);
    }
    else if(SYNTRON.SpeedBackLineR > 600)
    {
        iSetTorqueR = -100 * limit_level_r;
        iSetTorqueL = 100 * limit_level_r;
        limit_level_cnt_r += 10;//10ms
        if(limit_level_cnt_r >= 500)
        {
            limit_level_cnt_r = 0;
            limit_level_r = (limit_level_r>=10) ? 10 : (limit_level_r+1);
        }
        
        printf(">turn left limit. %d\r\n",limit_level_r);
    }
    else
    {
        iSetTorqueL = 0;
        iSetTorqueR = 0;
        limit_level_l = 1;
        limit_level_r = 1;
        limit_level_cnt_l = 0;
        limit_level_cnt_r = 0;
    }
    
    syntron_motor_torque_set(iSetTorqueL,iSetTorqueR);
}

/**
 * @brief AisstantInHill
 * 
 * @param assis_stas 
 *  true: assistant button pushed
 *  false: assistant button is not pushed
 * @param stas_change_flag 
 *  true: ramp status changed, or assistant button status changed
 *  false: ramp status and assistant button status are not changed
 */
void AisstantInHill(bool assis_stas, bool stas_change_flag)
{
    static int8_t uphill_dir = 0; 
    IMU_DATA_t *imu = GetImuData();
    if(stas_change_flag == true)
    {
      //printf("imu->iPich:%d uphill_dir:%d status:%d\r\n",imu->iPich,uphill_dir,assis_stas);
      if(assis_stas == true)
      {
        if((imu->iPich >= 0) && (uphill_dir >= 0))//up hill
        {
          syntron_motor_mode_switch(1);//PT
        }
        else//down hill
        {
          syntron_motor_mode_switch(0);//PV
        }
      }
      else if(assis_stas == false)//assistant button released, brake
      {
        syntron_motor_mode_switch(0);//PV
      }
    }
    
    if(assis_stas == true)//in asistance mode
    {
        if(imu->iPich < 0)//down hill
        {
            syntron_motor_speed_set(300,300,6000,6000);
        }
        else //up hill
        { 
            if((SYNTRON.SpeedBackLineL < (-200)) && (SYNTRON.SpeedBackLineR < (-200)))
            {
                if(uphill_dir >= 0)
                {
                    syntron_motor_mode_switch(0);//PV
                    uphill_dir = -1;
                }
            }
            else
            {
              uphill_dir = 1;
            }

            if(uphill_dir >= 0) 
            {
                syntron_motor_torque_set(300,300);
            }
            else if(uphill_dir < 0)
            {
                syntron_motor_speed_set(-100,-100,6000,6000);
            } 
        }
    }
    else if(assis_stas == false)//exit asistance mode
    {
      uphill_dir = 0;
      motor_brake();
    }
}

void MotorCurrentSet(uint16_t val)
{
    uint8_t data[8] = {0};
    static uint8_t print_cnt = 0;
    
    if((SYNTRON.max_current_l == val) && (SYNTRON.max_current_r == val))
    {
        if(print_cnt++ >= 200)
        {
            print_cnt = 0;
           log_print("MOTOR","max_current is already %d.\r\n",val); 
        }      
        return;
    }
// if need to write in flash 
//    data[0] = 0x23;
//    data[1] = 0x0a;
//    data[2] = 0x45;
//    data[3] = 0x00;
//    data[4] = 0x01;
//    can_pakage(1,0x601,8,data);
//    can_pakage(1,0x602,8,data);
    
    data[0] = 0x2b;
    data[1] = 0x2a;
    data[2] = 0x24;
    data[3] = 0x00;
    data[4] = (uint8_t)(val&0xff);
    data[5] = (uint8_t)((val>>8)&0xff);
    can_pakage(1,0x601,8,data);
    data[0] = 0x2b;
    data[1] = 0x2a;
    data[2] = 0x34;
    data[3] = 0x00;
    data[4] = (uint8_t)(val&0xff);
    data[5] = (uint8_t)((val>>8)&0xff);
    can_pakage(1,0x602,8,data);

// if need to write in flash 
//    data[0] = 0x23;
//    data[1] = 0x0a;
//    data[2] = 0x45;
//    data[3] = 0x00;
//    data[4] = 0x00;
//    data[5] = 0x00;
//    can_pakage(1,0x601,8,data);
//    can_pakage(1,0x602,8,data);
}

void MotorCurrentAsk(void)
{
    uint8_t data[8] = {0};
    
    data[0] = 0x40;
    data[1] = 0x2a;
    data[2] = 0x24;
    data[3] = 0x00;
    can_pakage(1,0x601,8,data);
    data[2] = 0x34;
    can_pakage(1,0x602,8,data);
}

void key_log_motor_max_current(uint8_t category)
{
  uint8_t uKeyData[7] = {0};
  
  uKeyData[0] = category;
  if(category == 0)
  {
      uKeyData[1] = (uint8_t)((SYNTRON.max_current_l>>0)&0xff);
      uKeyData[2] = (uint8_t)((SYNTRON.max_current_l>>8)&0xff);
      uKeyData[3] = (uint8_t)((SYNTRON.max_current_r>>0)&0xff);
      uKeyData[4] = (uint8_t)((SYNTRON.max_current_r>>8)&0xff);
  }
  else if(category == 1)
  {
      uKeyData[1] = (uint8_t)((SYNTRON.max_current_set_l>>0)&0xff);
      uKeyData[2] = (uint8_t)((SYNTRON.max_current_set_l>>8)&0xff);
      uKeyData[3] = (uint8_t)((SYNTRON.max_current_set_r>>0)&0xff);
      uKeyData[4] = (uint8_t)((SYNTRON.max_current_set_r>>8)&0xff);
  }

  
  keylog_report_currently(KEYLOG_MOTOR_MAX_CURRENT,uKeyData);
}

void MotorTemperatureAsk(void)
{
    uint8_t data[8] = {0};
    
    data[0] = 0x40;
    data[1] = 0x1b;
    data[2] = 0x50;
    data[3] = 0x00;
    can_pakage(1,0x601,8,data);
    data[2] = 0x51;
    can_pakage(1,0x602,8,data);
    log_print("MOTOR","motor temperature : %d %d\r\n",SYNTRON.temperature_l,SYNTRON.temperature_r);
    
    data[0] = (((uint16_t)SYNTRON.temperature_l)>>0)&0xff;
    data[1] = (((uint16_t)SYNTRON.temperature_l)>>8)&0xff;
    data[2] = (((uint16_t)SYNTRON.temperature_r)>>0)&0xff;
    data[3] = (((uint16_t)SYNTRON.temperature_r)>>8)&0xff;
    can_pakage(CAN_DEV_RK,CAN_MSG_MOTOR_TEMPERATURE_REPORT,4,data);

    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    keylog_report_currently(KEYLOG_MOTOR_TEMPERATURE,data);
}

void ParamSytronCurrentInChargingAsk(void)
{
    uint8_t data[8] = {0};
    
    data[0] = 6;//sytron motor
    data[1] = 1;//current in charging
    
    can_pakage(CAN_DEV_RK,CAN_MSG_THRESHOLD_MSG_ASK,2,data);
}

void ParamSytronCurrentInChargingRec(uint8_t *data)
{
    if(data[0] == 6)//sytron motor
    {
        if(data[1] == 1)//current in charging
        {
            SYNTRON.max_current_set_l = ((uint16_t)data[2])*100;
            SYNTRON.max_current_set_r = ((uint16_t)data[2])*100;
            log_print("SYTRON","receive setting current in charging.\r\n");
            key_log_motor_max_current(1);
            if(SYNTRON.max_current_set_l >= 5000)
            {
                SYNTRON.max_current_set_l = 5000;
            }
            if(SYNTRON.max_current_set_r >= 5000)
            {
                SYNTRON.max_current_set_r = 5000;
            }
        }
    }
}

void SytronCurrentResetAsk(void)
{
    uint8_t data[8] = {0};

    //report
    data[0] = 0x40;
    data[1] = 0x1c;
    data[2] = 0x50;
    data[3] = 0x00;
    can_pakage(1,0x601,8,data);
    data[2] = 0x51;
    can_pakage(1,0x602,8,data);
}

void SytronOverloadTankAsk(void)
{
    uint8_t data[8] = {0};
    
    //report
    
    data[0] = 0x40;
    data[1] = 0x06;
    data[2] = 0x50;
    data[3] = 0x00;
    can_pakage(1,0x601,8,data);
    data[2] = 0x51;
    can_pakage(1,0x602,8,data);
}

void key_log_motor_wakeup(uint8_t sel)
{
    uint8_t uKeyData[7] = {0};
    
    uKeyData[0] = sel;
    keylog_report_currently(KEYLOG_MOTOR_WAKEUP,uKeyData);

    g_motor_init_flg = true;
    SyntronMotorInitStep = SYN_MOTOR_STEP_ACTIVE;
    uParamCheckFlg = 0;
    uParamCheckFlg2 = 0;
    uCalParamCheckFlg = 0;
    motor_ack_status[0] = 0;
    motor_ack_status[1] = 0;
}

void key_log_motor_acc_changed(uint8_t *data)
{
    uint8_t uKeyData[7] = {0};
    
    memcpy(uKeyData,data,7);
    keylog_report_currently(KEYLOG_MOTOR_CALCULATION,uKeyData);
}

void motor_overload_warning_rec_handle(uint8_t *data)
{
  if(g_motor_overload_flg != 255)//action in opration, waiting
  {
    motor_overload_warning_ack(data[0],2);
    key_log_motor_overload_action(0);
    return;
  }

  if(0 == data[0])//overload pre-warning
  {
    g_motor_overload_flg = 0;
    key_log_motor_overload_action(1);
  }
  else if(1 == data[0])//need to reset power
  {
    g_motor_overload_flg = 1;
    g_motor_reset_flg = true;

    mile_ctl_reboot_enable();

    key_log_motor_overload_action(2);
  }
  else if(2 == data[0])//power off
  {
    SYNTRON.uMomentDutyL = 0;
    SYNTRON.uMomentDutyR = 0;
    g_motor_overload_flg = 255;
    g_motor_reset_flg = false;
    key_log_motor_overload_action(10);
    motor_overload_warning_ack(data[0],1);
  }
  else//err
  {
    motor_overload_warning_ack(data[0],3);
    key_log_motor_overload_action(3);
  }
}

void motor_overload_warning_ack(uint8_t level, uint8_t ret)
{
  uint8_t data_t[8] = {0};

  data_t[0] = level;
  data_t[1] = 0;//mcu id
  data_t[2] = ret;//action result
  can_pakage(CAN_DEV_RK,CAN_MSG_MOTOR_RESTORE_ACK,3,data_t);
}

void motor_overload_warning_action(void)
{
  static uint32_t time_cnt = 0;

  time_cnt += 10;

  if(g_motor_overload_flg == 0)//overload pre-warning
  {
    if(time_cnt <= 10)//overload pre-warning occurred
    {
      syntron_motor_mode_switch(1);//change into PT
      syntron_motor_torque_set(0,0);
      key_log_motor_overload_action(4);
    }
    else if(time_cnt < MOTOR_PRE_OVERLOAD_RECOVER_TIME)
    {
      syntron_motor_torque_set(0,0);
    }
    else if(time_cnt >= MOTOR_PRE_OVERLOAD_RECOVER_TIME)//500ms
    {
      time_cnt = 0;
      g_motor_overload_flg = 255;
      syntron_motor_mode_switch(0);//change into PV
      motor_brake();
      if((SYNTRON.uErrCodeL == 0) && (SYNTRON.uErrCodeR == 0))
      {
        motor_overload_warning_ack(0,1);//success
      }
      else
      {
        motor_overload_warning_ack(0,0);//fail
      }
      key_log_motor_overload_action(5);
    }
  }
  else if(g_motor_overload_flg == 1)//motor power reset
  {
    if(g_motor_reset_flg == false)
    {
      time_cnt = 0;
      g_motor_overload_flg = 255;
      motor_overload_warning_ack(1,1);//success
      key_log_motor_overload_action(6);
    }
    else if(time_cnt >= 10000)//10s
    {
      time_cnt = 0;
      g_motor_overload_flg = 255;
      g_motor_reset_flg = false;
      motor_overload_warning_ack(1,0);//fail
      key_log_motor_overload_action(7);
    }
  }
}

void key_log_motor_overload_action(uint8_t step)
{
  uint8_t uKeyData[7] = {0};
  
  uKeyData[0] = step;
  keylog_report_currently(KEYLOG_MOTOR_OVERLOAD_ACTION,uKeyData);
}

void motor_speed_time_handle(uint8_t *data,uint8_t len)
{
    //receive:
    if((len != 8) || (NULL == data))
    {
        return;
    }
    m_speed_time_data_rec_t data_rec;
    memcpy(&data_rec,data,8);

    SYNTRON.iSpeedL = data_rec.ls;
    SYNTRON.iSpeedR = data_rec.rs;
    SYNTRON.uAccL = data_rec.accl;
    SYNTRON.uAccR = data_rec.accr;
    if(SYNTRON.iSpeedL > 1200)
    {
      SYNTRON.iSpeedL = 1200;
    }
    else if(SYNTRON.iSpeedL < -1200)
    {
      SYNTRON.iSpeedL = -1200;
    }

    if(SYNTRON.iSpeedR > 1200)
    {
      SYNTRON.iSpeedR = 1200;
    }
    else if(SYNTRON.iSpeedR < -1200)
    {
      SYNTRON.iSpeedR = -1200;
    }
    //send ack
    bsp_rtc_time_t data_time;//get time
    get_offset_cal_time(&data_time);

    m_speed_time_data_send_t data_send;
    memcpy(&data_send,&data_rec,4);
    data_send.time_rec_ms = data_time.ms;
    data_send.time_rec_s = data_time.second;
    data_send.time_send_ms = data_time.ms;
    data_send.time_send_s = data_time.second;

    uint8_t datasend[8] = {0};
    memcpy(datasend,&data_send,8);
    can_pakage(CAN_DEV_RK,CAN_MSG_MOTOR_CONTROL_TIME_ACK,8,datasend);
    //send keylog
    uint8_t data_keylog[8] = {0};
    data_keylog[0] = (uint8_t)(SYNTRON.iSpeedL);
    data_keylog[1] = (uint8_t)(SYNTRON.iSpeedL >> 8);
    data_keylog[2] = (uint8_t)(SYNTRON.iSpeedR);
    data_keylog[3] = (uint8_t)(SYNTRON.iSpeedR >> 8);
    data_keylog[4] = datasend[4];
    data_keylog[5] = datasend[5];
    keylog_report_currently(KEYLOG_MOTOR_SPEED_CHANGE,data_keylog);
}

void motor_force_control_handle(uint8_t *data,uint8_t len)
{
    if(NULL == data)
    {
        return;
    }

    if(1 == data[0])
    {
        air_warn_force_flag = 1;
    }
    else
    {
        air_warn_force_flag = 0;
    }
    force_control_stat_report();
}
