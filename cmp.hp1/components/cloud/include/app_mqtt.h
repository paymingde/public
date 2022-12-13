#ifndef _APP_MQTT_H
#define _APP_MQTT_H
#include "Asw_global.h"
#include "data_process.h"
#include "asw_mqtt.h"
#include "asw_nvs.h"
#include "asw_fat.h"

#include "esp32_time.h"
#include "meter.h"
#include "http_client.h"
#include "asw_modbus.h"
#include "wifi_sta_server.h"

typedef enum tagDCCommEvt
{
    dcIdle,                 //!< dcIdle
    dcalilyun_authenticate, //!< dcalilyun_authenticate
    dcalilyun_read,         //!< dcalilyun_read
    dcalilyun_publish,      //!< dcalilyun_publish
    dcalilyun_heartbeat,    //!< dcalilyun_heartbeat
    processing_data
} TDCEvent;

typedef enum SYNC_STATE
{
    CLOUD_SYNC_INIT,
    CLOUD_SYNC_MONITOR,
    CLOUD_SYNC_INVERTER,
    CLOUD_SYNC_FINISHED
} SYNC_STATE;

int clear_cloud_invinfo(void);
//------------------------
int get_connect_stage(void);
//--------------------------------//
int cloud_setup(void);
int get_net_state(void);
int mqtt_connect(void);

/* Eng.Stg.Mch-lanstick */
// int mqtt_publish(int flag, Inv_data invdata, int * lost_flag);
int mqtt_publish(char *json_msg);

int trans_resrrpc_pub(cloud_inv_msg *resp, unsigned char *ws, int len);

// Eng.Stg.Mch-Lanstick +
void clear_all_sync_info(void);
int is_all_info_sync_ok(void);
int get_time_manual(void);
int GetMinute1(int ihour, int iminute, int imsec);

void Check_upload_unit32(const char *field, uint32_t value, char *body, int *datalen); //[tgl mark] uint8_t->uint32_t
void Check_upload_sint16(const char *field, uint16_t value, char *body, int *datalen);

#endif