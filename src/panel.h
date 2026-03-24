// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PANEL_H
#define PANEL_H

#include "woodland.h"
#include "dbus-network-management.h"

int update_time(void *data);
int refresh_networks(void *data);
void clean_ssids(struct woodland_server *server);
void panel(struct woodland_server *server, int panel_width, int panel_height);
void panel_setup(struct woodland_server *server, struct wlr_output *output, int panel_width, int panel_height);

#endif
