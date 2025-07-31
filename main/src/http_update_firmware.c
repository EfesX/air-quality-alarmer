#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_log.h"

#include "main.h"

#define OTA_BUF_SIZE 4096
#define MAX_TIMEOUTS 5

static char ota_buf[OTA_BUF_SIZE + 1];

static const char* TAG = "OTA";

esp_err_t update_firmware_handler(httpd_req_t *req) 
{
    const int content_length = req->content_len;
    int remaining = content_length;
    int timeout_counter = 0;
    int received = 0;
    esp_ota_handle_t ota_handle;
    
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_length);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition not found");
        return ESP_FAIL;
    }

    if (content_length > update_partition->size) {
        ESP_LOGE(TAG, "Firmware too big: %d > %d", content_length, (int)update_partition->size);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware too large");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x, size %d", 
        update_partition->subtype, (unsigned int)update_partition->address, content_length);

    esp_err_t ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving: %d/%d bytes", content_length - remaining, content_length);

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, ota_buf, MIN(remaining, OTA_BUF_SIZE));

        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                if (++timeout_counter > MAX_TIMEOUTS) {
                    ESP_LOGE(TAG, "Timeout limit reached");
                    esp_ota_abort(ota_handle);
                    httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Timeout limit reached");
                    return ESP_FAIL;
                }
                continue;
            }
            ESP_LOGE(TAG, "Receive error: %d", recv_len);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            return ESP_FAIL;
        }

        if (esp_ota_write(ota_handle, ota_buf, recv_len) != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(ret));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_write error");
            break;
        }

        received  += recv_len;
        remaining -= recv_len;

        ESP_LOGI(TAG, "Receiving: %d/%d bytes", content_length - remaining, content_length);
    }

    if (received != content_length) {
        ESP_LOGE(TAG, "Incomplete transfer: %d/%d bytes", received, content_length);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete transfer");
        esp_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(ret));
        if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed - corrupt firmware?");
        }
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
        return ESP_FAIL;
    }

    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Firmware update successful!");
    httpd_resp_sendstr(req, "Firmware update successful! Rebooting...");

    xTaskCreatePinnedToCore(reboot_task, "reboot", 2048, NULL, 0, NULL, tskNO_AFFINITY);

    return ESP_OK;
}
