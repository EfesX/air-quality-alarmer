#include "wifi.h"
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI";

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

void save_wifi_creds(const char* ssid, const char* pass)
{
    nvs_handle_t nvs;

    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs));

    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "pass", pass));

    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

void load_wifi_creds(char *ssid, char* pass)
{
    nvs_handle_t nvs;

    ESP_ERROR_CHECK(nvs_open("storage", NVS_READONLY, &nvs));

    size_t required_size;
    
    ESP_ERROR_CHECK(nvs_get_str(nvs, "ssid", NULL, &required_size));
    ESP_ERROR_CHECK(nvs_get_str(nvs, "ssid", ssid, &required_size));

    ESP_ERROR_CHECK(nvs_get_str(nvs, "pass", NULL, &required_size));
    ESP_ERROR_CHECK(nvs_get_str(nvs, "pass", pass, &required_size));

    nvs_close(nvs);
}


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < WIFI_STA_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP (%d/%d)", s_retry_num, WIFI_STA_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }

    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        NULL
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        NULL,
        NULL
    ));

    char saved_ssid[32] = {0};
    char saved_pass[64] = {0};
    load_wifi_creds(saved_ssid, saved_pass);

    if(strlen(saved_ssid) <= 0)
        return ESP_ERR_FLASH_BASE;

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strncpy((char*)wifi_config.sta.ssid, saved_ssid, sizeof(saved_ssid));
    strncpy((char*)wifi_config.sta.password, saved_pass, sizeof(saved_pass));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "trying to connect to saved WIFI: %s : %s", saved_ssid, saved_pass);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_STA_CONNECTION_TIMEOUT_MS)
    );

    if(bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "connected to AP SSID: %s", saved_ssid);
        return ESP_OK;
    } 

    ESP_LOGI(TAG, "failed to connect saved WiFi");
    return ESP_ERR_WIFI_BASE;
}

void wifi_init_ap(void){
    
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASSWORD,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = WIFI_AP_MAX_CONN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if(strlen(WIFI_AP_PASSWORD) == 0)
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WIFI AP started: %s : %s", WIFI_AP_SSID, WIFI_AP_PASSWORD);
}

void wifi_start(void)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_DEF_INPUT;
    io_conf.pin_bit_mask = (1ULL << WIFI_ENA_PIN);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);


    int wifi_ena = gpio_get_level(WIFI_ENA_PIN);

    if(wifi_ena == 0){
        ESP_LOGI(TAG, "wifi support is disabled");
        return;
    }

    ESP_LOGI(TAG, "wifi support is enabled");
        

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if(wifi_init_sta() != ESP_OK)
    {
        wifi_init_ap();
//        start_webserver();
    }
}