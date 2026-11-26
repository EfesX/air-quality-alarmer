#include "web.h"

#include "esp_err.h"
#include "esp_log.h"

static const char* TAG = "WEB";

extern esp_err_t save_post_handler(httpd_req_t *req);
extern esp_err_t root_get_handler(httpd_req_t *req);

httpd_handle_t start_webserver(void)
{
    ESP_LOGI(TAG, "starting http server...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;

     if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &save);
    }

    return server;
}