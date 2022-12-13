/**
 * @file asw_mqtt.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-04-26
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ASW_MQTT_H
#define _ASW_MQTT_H

#include "Asw_global.h"
#include "aiot_mqtt_sign.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "aiot_mqtt_sign.h"

#include "app_mqtt.h"
#include "cJSON.h"
#include "update.h"

#define MQTT_ROOT_CA_FILENAME "-----BEGIN CERTIFICATE-----\n"                                      \
                              "MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\n" \
                              "A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\n" \
                              "b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\n" \
                              "MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\n" \
                              "YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\n" \
                              "aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\n" \
                              "jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\n" \
                              "xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\n" \
                              "1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\n" \
                              "snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\n" \
                              "U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\n" \
                              "9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\n" \
                              "BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\n" \
                              "AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\n" \
                              "yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\n" \
                              "38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\n" \
                              "AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\n" \
                              "DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\n" \
                              "HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n"                             \
                              "-----END CERTIFICATE-----" ///< Root CA file name

// char my_uri[100];
extern char rrpc_res_topic[256];
extern int netework_state;
extern char pub_topic[150];
extern char sub_topic_get[150];
extern char sub_topic[150];
extern char DEMO_DEVICE_NAME[IOTX_DEVICE_NAME_LEN + 1];

//--------------------------//
int asw_mqtt_setup(void);

void asw_mqtt_client_stop();
void asw_mqtt_client_start();

void set_mqtt_server(char *pdk, char *server, int port);
// void set_mqtt_server(char *server,int port);

void set_product_key(char *product_key);
void set_device_name(char *device_name);
void set_device_secret(char *device_secret);
int asw_publish(void *cpayload);
int sent_newmsg(void);
int mqtt_app_start(void);
void mqtt_client_destroy_free(void);
int parse_mqtt_msg(char *payload);
int asw_mqtt_publish(const char *topic, const char *data, int data_len, int qos);

void send_msg(int type, char *buff, int lenthg, char *ws);

int get_mqtt_pub_ack(void);

void asw_mqtt_client_stop(void);

void setting_new_server(cJSON *body);

int get_rrpc_restopic(char *rpc_topic, int rpc_len, char *response_topic);
#endif