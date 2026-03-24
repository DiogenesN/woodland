// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TEXTTOBUFF_H
#define TEXTTOBUFF_H

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

void handle_keysym_input(xkb_keysym_t sym,
						char *buffer,
						size_t buffer_size,
						char **out_copy);

#endif
