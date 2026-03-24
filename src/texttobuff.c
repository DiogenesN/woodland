// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

static bool is_modifier_key(const char *name) {
	return strcmp(name, "Shift_L") == 0 ||
		   strcmp(name, "Shift_R") == 0 ||
		   strcmp(name, "Left") == 0 ||
		   strcmp(name, "Right") == 0 ||
		   strcmp(name, "Up") == 0 ||
		   strcmp(name, "Down") == 0 ||
		   strcmp(name, "Return") == 0 ||
		   strcmp(name, "Control_L") == 0 ||
		   strcmp(name, "Control_R") == 0 ||
		   strcmp(name, "Alt_L") == 0 ||
		   strcmp(name, "Alt_R") == 0 ||
		   strcmp(name, "Super_L") == 0 ||
		   strcmp(name, "Super_R") == 0 ||
		   strcmp(name, "Meta_L") == 0 ||
		   strcmp(name, "Meta_R") == 0 ||
		   strcmp(name, "Caps_Lock") == 0;
}

void handle_keysym_input(xkb_keysym_t sym,
						char *buffer,
						size_t buffer_size,
						char **out_copy) {
	char name[64];
	if (xkb_keysym_get_name(sym, name, sizeof(name)) <= 0) {
		return;
	}

	if (is_modifier_key(name)) {
		return;
	}

	char utf8[8];
	int len = xkb_keysym_to_utf8(sym, utf8, sizeof(utf8));

	if (sym == XKB_KEY_BackSpace) {
		size_t buflen = strlen(buffer);

		if (buflen > 0) {
			// UTF-8 safe backspace
			while (buflen > 0 && ((buffer[buflen - 1] & 0xC0) == 0x80)) {
				buflen--;
			}
			buffer[--buflen] = '\0';
		}
	}
	else if (len > 0) {
		if (!buffer[0]) {
			strncpy(buffer, utf8, buffer_size - 1);
			buffer[buffer_size - 1] = '\0';
		}
		else {
			strncat(buffer, utf8, buffer_size - strlen(buffer) - 1);
		}
	}
	else {
		return;
	}
	if (*out_copy) {
		free(*out_copy);
		*out_copy = NULL;
	}
	*out_copy = strdup(buffer);
	return;
}
