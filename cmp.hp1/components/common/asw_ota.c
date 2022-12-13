#include "asw_ota.h"



 static const char * TAG="asw_ota.c";

//--------------------------------//
//---------------------------------//


// #define OTA_URL_SIZE 256
//-----------------------------//
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}
//------------------------------------//
void asw_ota_start(void *pvParameter)
{
    // ESP_LOGI(TAG, "Starting OTA example");

    esp_http_client_config_t config = {
        .url = (char *)pvParameter,
        .cert_pem = NULL, //(char *)server_cert_pem_start,        
        .event_handler = _http_event_handler,
    };

    config.skip_cert_common_name_check = true;

    esp_err_t ret = esp_https_ota(&config);
    printf("update res %d \n", ret);
    if (ret == ESP_OK)
    {
        ESP_LOGW(TAG," will restart by [ota start]");
        esp_restart();
    }
    else
    {
        ESP_LOGE("asw-ota", "Firmware upgrade failed");
    }
}

#if 0
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"

// #include "esp_event_loop.h"  //tgl delete
// #include "esp_log.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
// #include "protocol_examples_common.h"
#include "string.h"

#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "simple_ota_example";
// extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
// extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");




#endif