#pragma once

void creds_init(void);
void save_wifi_creds(const char* ssid, const char* pass);
void load_wifi_creds(char *ssid, char* pass);
