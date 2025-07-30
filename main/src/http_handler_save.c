#include "main.h"
#include "creds.h"

#include "ctype.h"
#include "esp_err.h"
#include "esp_http_server.h"

#define MIN(a,b) (a < b ? a : b)

static inline bool parse_multipart_form(const char* buf, char* ssid, char* pass);
static inline bool parse_urlencoded_form(const char* buf, char* ssid, char* pass);
static inline int read_data_from_request(httpd_req_t *req, char *buf);


esp_err_t save_post_handler(httpd_req_t *req) 
{
    char wifi_ssid[32] = {0};
    char wifi_password[64] = {0};
    bool parse_success = false;
    char *buf = NULL;

    // allocate a buffer with reserve (+1) for the null terminator
    buf = malloc(req->content_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // get request data
    int ret = read_data_from_request(req, buf);
    if(ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            httpd_resp_send_408(req);
        free(buf);
        return ESP_FAIL;
    }

    // check and parse possible formats
    if (strstr(buf, "name=\"ssid\"")) // multipart/form-data
        parse_success = parse_multipart_form(buf, wifi_ssid, wifi_password);
    else if (strstr(buf, "ssid="))    // application/x-www-form-urlencoded
        parse_success = parse_urlencoded_form(buf, wifi_ssid, wifi_password);

    free(buf);

    if (parse_success == false)
    {
        httpd_resp_sendstr(req, "Parse data error");
        return ESP_FAIL;
    }

    if(strlen(wifi_password) < 8)
    {
        httpd_resp_sendstr(req, "Error: password must have at least 8 symbols");
        return ESP_FAIL;
    }
    
    if (strlen(wifi_ssid) == 0 || strlen(wifi_password) == 0) 
    {
        httpd_resp_sendstr(req, "Error: empty SSID or password");
        return ESP_FAIL;
    }

    save_wifi_creds(wifi_ssid, wifi_password);
    httpd_resp_sendstr(req, "Settings saved! Rebooting ESP32...");

    xTaskCreate(reboot_task, "rebooting", 1024, NULL, 0, NULL);

    return ESP_OK;
}

static inline char* url_decode(const char* src) 
{
    if (!src) return NULL;
    
    size_t src_len = strlen(src);
    char* decoded = malloc(src_len + 1);
    if (!decoded) return NULL;
    
    char* dst = decoded;
    
    while (*src) 
    {
        if (*src == '+') 
        {
            *dst++ = ' ';
            src++;
        }
        else if (*src == '%' && isxdigit((uint8_t) src[1]) && isxdigit((uint8_t) src[2]))
        {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        }
        else 
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return decoded;
}

// parser for multipart/form-data
static inline bool parse_multipart_form(const char* buf, char* ssid, char* pass) 
{
    char* fields[] = {"name=\"ssid\"", "name=\"password\""};
    char* values[] = {ssid, pass};
    size_t max_lens[] = {32, 64};

    for (int i = 0; i < 2; i++) 
    {
        char* section = strstr(buf, fields[i]);
        if (!section) return false;
        
        char* value_start = strstr(section, "\r\n\r\n");
        if (!value_start) return false;
        value_start += 4;
        
        char* value_end = strstr(value_start, "------");
        if (!value_end) return false;
        
        size_t len = value_end - value_start;
        while (len > 0 && isspace((unsigned char)value_start[len-1])) len--;
        
        // allocate and decode value
        char* encoded_val = malloc(len + 1);
        if (!encoded_val) return false;
        
        strncpy(encoded_val, value_start, len);
        encoded_val[len] = '\0';
        
        char* decoded_val = url_decode(encoded_val);
        free(encoded_val);
        
        if (!decoded_val) return false;
        
        strncpy(values[i], decoded_val, max_lens[i]-1);
        values[i][max_lens[i]-1] = '\0';
        free(decoded_val);
    }
    return true;
}

// parser for application/x-www-form-urlencoded
static inline bool parse_urlencoded_form(const char* buf, char* ssid, char* pass) {
    char* ssid_start = strstr(buf, "ssid=");
    char* pass_start = strstr(buf, "password=");
    
    if (!ssid_start || !pass_start) return false;
    
    // temporary buffers for decoding
    char encoded_ssid[64] = {0};
    char encoded_pass[64] = {0};
    
    if (sscanf(ssid_start, "ssid=%63[^&]", encoded_ssid) != 1) return false;
    if (sscanf(pass_start, "password=%63[^&]", encoded_pass) != 1) return false;
    
    // decoding values
    char* decoded_ssid = url_decode(encoded_ssid);
    char* decoded_pass = url_decode(encoded_pass);
    
    if (!decoded_ssid || !decoded_pass) 
    {
        free(decoded_ssid);
        free(decoded_pass);
        return false;
    }
    
    strncpy(ssid, decoded_ssid, 31);
    strncpy(pass, decoded_pass, 63);
    
    ssid[31] = pass[63] = '\0';
    
    free(decoded_ssid);
    free(decoded_pass);

    return true;
}

static inline int read_data_from_request(httpd_req_t *req, char *buf)
{
    int remaining = req->content_len;
    int received = 0;
    int ret = 0;

    while (remaining > 0) 
    {
        ret = httpd_req_recv(req, buf + received, MIN(remaining, 1024));
        if (ret <= 0)
            break;
        received += ret;
        remaining -= ret;
    }

    buf[received] = '\0';

    return ret;
}