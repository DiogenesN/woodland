// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _DBUS_HETWORK_MANAGEMENT
#define _DBUS_HETWORK_MANAGEMENT

bool check_if_secured_ssid(char *ssid);
void connect_to_open_ssid(const char *ssid);
size_t list_wifi_devices(char **ssids, size_t max);
void connect_to_secured_ssid(const char *ssid, char *password);

#endif
