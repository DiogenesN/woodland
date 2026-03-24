// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APPLAUNCHER_H
#define APPLAUNCHER_H

#include "woodland.h"

extern size_t items_count;
void show_applist(struct woodland_server *server);
void redraw_applist(struct woodland_server *server);
void show_applauncher(struct woodland_server *server);
void free_desktop_items(struct woodland_server *server);
void redraw_applauncher(struct woodland_server *server);
void setup_desktop_items(struct woodland_server *server);
void process_directory(struct woodland_server *server, const char *directoryPath, bool is_bin_only);
void filtered_list(struct AppItem *source, size_t source_count, struct AppItem **fList, const char *key);

#endif
