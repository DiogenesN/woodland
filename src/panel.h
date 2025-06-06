// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PANEL_H
#define PANEL_H

#include "woodland.h"
#include "dbus-network-management.h"

int update_time(void *data);
void panel_setup(struct woodland_server *server,
				struct wlr_output *output,
				int panel_width,
				int panel_height);

#endif
