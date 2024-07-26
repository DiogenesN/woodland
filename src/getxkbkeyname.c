// SPDX-License-Identifier: GPL-2.0-or-later

/* This function returns the XKB key name from the given hexadecimal symbol.
 * the names are defined in: /usr/include/xkbcommon/xkbcommon-keysyms.h
 * Usage:
 	1) First in your code you need to get the sym:
	example:
		uint32_t keycode = event->keycode + 8;
		const xkb_keysym_t *syms;
		// Get a list of keysyms based on the keymap for this keyboard
		int nsyms = xkb_state_key_get_syms(keyboard->device->keyboard->xkb_state, keycode, &syms);
		// The sym would be syms[i] in the following loop:
		for (int i = 0; i < nsyms; i++) {
			// Check if the Super key is pressed or released
			if (syms[i] == XKB_KEY_Super_L || syms[i] == XKB_KEY_Super_R) {
				// do something with syms[i] for instance provide is as argument for a function:
				// handle_keybinding_super(server, syms[i]);
			}
		}
		2) in your handle function (e.g. handle_keybinding_super) get the hexcode:
		static bool handle_keybinding_alt(struct woodland_server *server, xkb_keysym_t sym) {
			char hexCode[256];
			snprintf(hexCode, sizeof(hexCode), "%#06x", sym);
			// Get the key name from the hexcode and print it:
			char *keyname = xkb_keyname(hexCode);
			if (keyname) {
				printf("Key name: %s\n", keyname);
				free(keyname); // Free the allocated memory
			}
			else {
				printf("Key not found or error occurred\n");
			}
		}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *xkb_keyname(const char *hexadecimal) {
    FILE *file = fopen("/usr/include/xkbcommon/xkbcommon-keysyms.h", "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    char buffer[256];
    char *keyname = NULL;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        // Check if the line contains the provided hexadecimal value
        if (strstr(buffer, hexadecimal) != NULL) {
            // Check if the line contains the "#define" keyword
            char *define_ptr = strstr(buffer, "#define");
            if (define_ptr) {
                // Extract the keyname
                char *start = define_ptr + strlen("#define");
                while (*start == ' ' || *start == '\t') start++; // Skip whitespace
                char *end = start;
                while (*end && *end != ' ' && *end != '\t' && *end != '\n') end++; // Find end of keyname
                keyname = (char *)malloc(end - start + 1);
                if (!keyname) {
                    perror("Memory allocation error");
                    fclose(file);
                    return NULL;
                }
                strncpy(keyname, start, end - start);
                keyname[end - start] = '\0';
                break;
            }
        }
    }
    fclose(file);
    return keyname;
}
