#include "http_client.h"

//===========================================//
static const char *TAG="http_client.c";


static struct tm _http_get_tm = {0};
#define TIME_HEAD "{\"time\":\"" //don't change it

static FILE *f_download;
//===========================================//

esp_err_t time_req_http_event_handler(esp_http_client_event_t *evt)
{
    int res = 0;
    // int err=0;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "time HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "time HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (evt->data_len > 50)
        {
            return -1;
        }

        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // Write out data
            // printf("%.*s", evt->data_len, (char*)evt->data);
            char year[8] = {0};
            char mon[8] = {0};
            char day[8] = {0};
            char hour[8] = {0};
            char min[8] = {0};
            char sec[8] = {0};
            char *start = strstr((char *)evt->data, TIME_HEAD);
            if (start == NULL)
                return -1;
            char *addr = start + strlen(TIME_HEAD);
            strncpy(year, addr, 4);
            addr = addr + 4;

            strncpy(mon, addr, 2);
            addr = addr + 2;

            strncpy(day, addr, 2);
            addr = addr + 2;

            strncpy(hour, addr, 2);
            addr = addr + 2;

            strncpy(min, addr, 2);
            addr = addr + 2;

            strncpy(sec, addr, 2);

            _http_get_tm.tm_year = atoi(year);
            _http_get_tm.tm_mon = atoi(mon);
            _http_get_tm.tm_mday = atoi(day);
            _http_get_tm.tm_hour = atoi(hour);
            _http_get_tm.tm_min = atoi(min);
            _http_get_tm.tm_sec = atoi(sec);
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "time HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        res = -1;
        break;
    }
    return res;
}

//===============================================//

esp_err_t file_req_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "time HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "time HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // Write out data
            // printf("%.*s", evt->data_len, (char*)evt->data);
            fwrite(evt->data, 1, evt->data_len, f_download);
            ESP_LOGI(TAG, "write file ...");
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "time HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "time HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    }
    return ESP_OK;
}

///----------------------------------------------//

//---------------------------------------------//

int http_get_time(char *time_url, struct tm *p_time)
{
    int res = -1;
    int content_len = 0;

    esp_http_client_config_t config = {
        .url = time_url,
        .event_handler = time_req_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        res = esp_http_client_get_status_code(client);
        content_len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                 res,
                 content_len);
        if (res == 200 && content_len < 50)
        {
            memcpy(p_time, &_http_get_tm, sizeof(struct tm));
            goto OK;
        }
        else
            goto ERR;
    }
    else
    {
        goto ERR;
        // ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

ERR:
    esp_http_client_cleanup(client);
    sleep(3);
    return -1;
OK:
    esp_http_client_cleanup(client);
    return 0;
}

int http_get_file(char *url, char *save_name, int * file_len)
{
    // you can change file name
    // char save_path_and_name[50] = {0};
    ESP_LOGI(TAG,"http get file %s \n", save_name);
    f_download = fopen(save_name, "wb+");
    int res = -1;
    int content_len = 0;

    esp_http_client_config_t config = {
        .url = url,
        //.timeout_ms=60*1000,
        .event_handler = file_req_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        res = esp_http_client_get_status_code(client);
        content_len = esp_http_client_get_content_length(client);
        *file_len = content_len;
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                 res,
                 content_len);

        fclose(f_download);
        if (res == 200)
        {
            goto OK;
        }
        else
            goto ERR;
    }
    else
    {
        fclose(f_download);
        goto ERR;
        // ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

ERR:
    esp_http_client_cleanup(client);
    sleep(3);
    return -1;
OK:
    esp_http_client_cleanup(client);
    return 0;
}

