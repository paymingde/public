#include "asw_mqueue.h"

static const char *TAG = "mqueue.c";

int8_t create_queue_msg(void)
{
    /** 5 inverters max support*/
    mq0 = xQueueCreate(5, sizeof(Inv_data)); // inv--->cloud
    if (mq0 == NULL)
    {
        ESP_LOGE(TAG, "create mq0 error");
        return ASW_FAIL;
    }

    mq1 = xQueueCreate(1, sizeof(cloud_inv_msg)); // cloud--->inv
    if (mq1 == NULL)
    {
        ESP_LOGE(TAG, "create mq1 error");
        return ASW_FAIL;
    }

    mq2 = xQueueCreate(1, sizeof(meter_data_t)); // meter-->cloud
    // mq2 = xQueueCreate(1, sizeof(meter_cloud_data)); // meter-->cloud meter_data_t
    if (mq2 == NULL)
    {
        ESP_LOGE(TAG, "create mq2 error");
        return ASW_FAIL;
    }

    /** cgi和inv之间，通过2个消息队列实现双向通信*/
    to_inv_fdbg_mq = xQueueCreate(1, sizeof(cloud_inv_msg));
    if (to_inv_fdbg_mq == NULL)
    {
        ESP_LOGE(TAG, "create error: to_inv_fdbg_mq");
        return ASW_FAIL;
    }

    to_cgi_fdbg_mq = xQueueCreate(1, sizeof(fdbg_msg_t));
    if (to_cgi_fdbg_mq == NULL)
    {
        ESP_LOGE(TAG, "create error: to_cgi_fdbg_mq");
        return ASW_FAIL;
    }
    return ASW_OK;
}
