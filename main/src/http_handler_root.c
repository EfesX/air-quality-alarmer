#include "esp_err.h"
#include "esp_http_server.h"

#include "config_html.h"

esp_err_t root_get_handler(httpd_req_t *req) 
{
    httpd_resp_send(req, config_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}