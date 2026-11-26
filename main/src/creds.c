#include "creds.h"

#include "esp_err.h"
#include "nvs_flash.h"

void creds_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

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