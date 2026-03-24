// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>

#ifndef _DBUS_HETWORK_MANAGEMENT
#define _DBUS_HETWORK_MANAGEMENT

extern bool auth_success;
char *get_active_ssid(void);
bool check_if_secured_ssid(char *ssid);
void connect_to_open_ssid(const char *ssid);
void connect_to_secured_ssid(const char *ssid, char *password);
size_t list_wifi_devices(char **ssids, size_t max, bool trigger_scan);

#endif
