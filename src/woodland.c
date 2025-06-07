// SPDX-License-Identifier: GPL-2.0-or-later

/* woodland */
/* Minimal but functional Wayland compositor. */

#define STB_IMAGE_IMPLEMENTATION // needed for background image implementation
#define TOUCHPAD_SCROLL_SCALE 0.7 // Scaling factor for touchpad scrolls
#define MOUSE_SCROLL_SCALE 1.0 // Scaling factor for mouse wheel scrolls
#define SCROLL_DEBOUNCE_THRESHOLD 3.0 // Threshold to filter out small scroll values
#define MAX_NR_OF_STARTUP_COMMANDS 265 // maximum number of user defined startup commands
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define WL_LIST_SAFE_REMOVE(link)			\
	do {									\
		if ((link)->prev && (link)->next) { \
			wl_list_remove(link);			\
			(link)->prev = NULL;			\
			(link)->next = NULL;			\
		}									\
	} while (0)

/* Local headers */
#include "menu.h"
#include "panel.h"
#include "runcmd.h"
#include "woodland.h"
#include "windowlist.h"
#include "create-config.c"
#include "getxkbkeyname.h"
#include "getvaluefromconf.h"

/**
 *************** Helper functions *************** 
 */

static struct wlr_linux_dmabuf_feedback_v1_tranche default_tranche = {
	.target_device = 0,  // Auto-detect GPU
	.flags = 0,		  // Default flags
	///.formats = (struct wlr_drm_format_set) {0},
};

static struct wlr_linux_dmabuf_feedback_v1 default_feedback = {
	.main_device = 0,		   
	.tranches.data = &default_tranche,  
	.tranches.size = 1,		 
};

// Refresh Wi-Fi network list
static void refresh_networks(struct woodland_server *server) {
	// Refreshing the list of available wifi networks
	if (server->ssids[0] != NULL) {
		for (size_t i = 0; server->ssids[i] != NULL; i++) {
			///printf("In 'scan_network' freeing up SSID[%zu]: %s\n", i, server->ssids[i]);
			free(server->ssids[i]); // Don't forget to free
			server->ssids[i] = NULL;
		}
	}
	fprintf(stderr, "Scanning for networks...\n");
	server->number_of_ssids = list_wifi_devices(server->ssids, 256);
	if (server->number_of_ssids <= 0) {
		server->ssids[0] = strdup("No networks found! Is wifi enabled?");
	}
	fprintf(stderr, "Scanning done!\n");
}

/* Takes an index, looks for it in a string and then returns a new string with
 * the layout at the given index placed in the first position. The remaining
 * layouts will be arranged in their original order, with elements separated
 * by commas. I use it to re-arrange the layouts to place the chosen layout
 * at position 0. It is needed in order to change the layout per application.
 */
static char *updated_layouts(char *index, char *layouts) {
	if (!index || !layouts) {
		return NULL;
	}

	// Copy layouts to a temporary buffer to avoid modifying the original string
	char *layouts_copy = strdup(layouts);
	if (!layouts_copy) {
		return NULL;
	}

	// Find the starting position of 'index' in 'layouts_copy'
	char *start = strstr(layouts_copy, index);
	if (!start) {
		free(layouts_copy);
		return strdup(layouts); // If 'index' is not found, return the original string
	}

	// Find the end position of 'index' in 'layouts_copy'
	char *end = start + strlen(index);

	// Allocate memory for the new layouts string
	size_t new_layouts_size = strlen(layouts) + 1;
	char *new_layouts = malloc(new_layouts_size);
	if (!new_layouts) {
		free(layouts_copy);
		return NULL;
	}

	// Copy 'index' to the beginning of the new layouts string
	strcpy(new_layouts, index);

	// Copy the remaining layouts in their original order, starting from the end of 'index'
	if (*end == ',') {
		end++; // Skip the comma after 'index'
	}
	char *remaining = end;
	while (*remaining != '\0') {
		strncat(new_layouts, ",", new_layouts_size - strlen(new_layouts) - 1);
		char *comma = strchr(remaining, ',');
		if (!comma) {
			strncat(new_layouts, remaining, new_layouts_size - strlen(new_layouts) - 1);
			break;
		}
		strncat(new_layouts, remaining, comma - remaining);
		remaining = comma + 1;
	}

	// Now copy the layouts that were before 'index' in the original string
	char *current = layouts_copy;
	while (current < start) {
		strncat(new_layouts, ",", new_layouts_size - strlen(new_layouts) - 1);
		char *comma = strchr(current, ',');
		if (!comma || comma >= start) {
			strncat(new_layouts, current, start - current);
			break;
		}
		strncat(new_layouts, current, comma - current);
		current = comma + 1;
	}

	// Remove trailing comma if it exists
	if (new_layouts[strlen(new_layouts) - 1] == ',') {
		new_layouts[strlen(new_layouts) - 1] = '\0';
	}

	free(layouts_copy);
	layouts_copy = NULL;
	return new_layouts;
}

/* Given a number index, it looks through a string of words devided by comma
 * and returns the word at given index.
 */
static char *layout_name_from_index(int index, char *layouts) {
	if (!layouts || index < 0) {
		return NULL;
	}

	// Copy layouts to a temporary buffer to avoid modifying the original string
	char *layouts_copy = strdup(layouts);
	if (!layouts_copy) {
		return NULL;
	}

	char *layout_name = NULL;
	char *token = strtok(layouts_copy, ",");
	int current_index = 0;

	while (token) {
		if (current_index == index) {
			layout_name = strdup(token);
			break;
		}
		token = strtok(NULL, ",");
		current_index++;
	}

	free(layouts_copy);
	return layout_name;
}

static void change_keyboard_layout(struct woodland_server *server,
								   struct wlr_keyboard *keyboard,
								   struct woodland_view *view) {
	// Change keyboard layout per application
	char *layouts = get_char_value_from_conf(server->config, "xkb_layouts");
	if (!layouts) {
		wlr_log(WLR_ERROR, "Error: Failed to get xkb_layouts from config.");
		return;
	}

	char *layout_name = layout_name_from_index(view->keyboard_layout, layouts);
	if (!layout_name) {
		wlr_log(WLR_ERROR, "Error: Failed to get layout name for index %d.", view->keyboard_layout);
		free(layouts);
		return;
	}

	char *new_layout = updated_layouts(layout_name, layouts);
	if (!new_layout) {
		wlr_log(WLR_ERROR, "Error: Failed to update layouts.");
		free(layout_name);
		free(layouts);
		return;
	}

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Error: Failed to create xkb_context.");
		free(new_layout);
		free(layout_name);
		free(layouts);
		return;
	}

	struct xkb_rule_names rules = {
		.layout = new_layout,
		.options = "grp:alt_shift_toggle"
	};

	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context,
														  &rules,
														  XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		wlr_log(WLR_ERROR, "Error: Failed to create xkb_keymap.");
		xkb_context_unref(context);
		free(new_layout);
		free(layout_name);
		free(layouts);
		return;
	}

	// Update the keyboard's keymap and state
	wlr_keyboard_set_keymap(keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	if (layout_name) {
		//printf("Layout at index %d: %s\n", view->keyboard_layout, layout_name);
		free(layout_name);
	}
	else {
		wlr_log(WLR_ERROR, "No layout found at index %d", view->keyboard_layout);
	}

	if (new_layout) {
		free(new_layout);
		new_layout = NULL;
	}

	if (layouts) {
		free(layouts);
		layouts = NULL;
	}
}

static void focus_toplevel(struct woodland_view *toplevel) {
	if (!toplevel) {
		return;
	}

	struct woodland_server *server = toplevel->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
	//we use the keyboard to obtain the surface to which it was previosly attached;
	struct wlr_surface *prev = seat->keyboard_state.focused_surface;
	if (prev == surface) {
		return;
	}
	if (prev) {
		struct wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev);
		if (prev_toplevel) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
	}
	struct wlr_keyboard *kbd = wlr_seat_get_keyboard(seat);
	wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");

	// Only reorder if we're not in cycling mode.
	if (!server->cycling_mode) {
		WL_LIST_SAFE_REMOVE(&toplevel->link);
		wl_list_insert(&server->toplevels, &toplevel->link);
		// You might do the reordering only once.
	}

	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	// Do not forcibly reinsert if cycling_mode is active.
	if (!server->cycling_mode) {
		WL_LIST_SAFE_REMOVE(&toplevel->link);
		wl_list_insert(&server->toplevels, &toplevel->link);
	}
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	if (kbd) {
		wlr_seat_keyboard_notify_enter(seat, surface, kbd->keycodes, kbd->num_keycodes, &kbd->modifiers);
		// Change keyboard layout per application
		change_keyboard_layout(server, kbd, toplevel);
	}
}

static bool cycle_windows(struct woodland_server *server) {
	int len = wl_list_length(&server->toplevels);
	if (len < 2) {
		return false;
	}

	server->cycling_mode = true;

	struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;
	struct woodland_view *current = NULL;
	struct woodland_view *toplevel;

	// Find currently focused view
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (toplevel->xdg_toplevel->base->surface == focused) {
			current = toplevel;
			break;
		}
	}

	// Start cycling from the one after current (or from the start if no current)
	struct woodland_view *iter = current ? wl_container_of(current->link.next, iter, link) : NULL;
	if (!iter || &iter->link == &server->toplevels) {
		iter = wl_container_of(server->toplevels.next, iter, link);
	}

	// Cycle through the list, skipping minimized views
	struct woodland_view *start = iter;
	do {
		if (!iter->minimized) {
			focus_toplevel(iter);
			server->cycling_mode = false;
			return true;
		}
		iter = wl_container_of(iter->link.next, iter, link);
		if (&iter->link == &server->toplevels) {
			iter = wl_container_of(server->toplevels.next, iter, link);
		}
	} while (iter != start);

	// All windows might be minimized
	server->cycling_mode = false;
	return false;
}

static bool cycle_windows_reverse(struct woodland_server *server) {
	int len = wl_list_length(&server->toplevels);
	if (len < 2) {
		return false;
	}

	server->cycling_mode = true;

	struct wlr_surface *focused = server->seat->keyboard_state.focused_surface;
	struct woodland_view *current = NULL;
	struct woodland_view *toplevel;

	// Find currently focused view
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (toplevel->xdg_toplevel->base->surface == focused) {
			current = toplevel;
			break;
		}
	}

	// Start from previous of current, or last if current is NULL
	struct woodland_view *iter;
	if (current) {
		iter = wl_container_of(current->link.prev, iter, link);
		if (&iter->link == &server->toplevels) {
			iter = wl_container_of(server->toplevels.prev, iter, link);
		}
	}
	else {
		iter = wl_container_of(server->toplevels.prev, iter, link);
	}

	struct woodland_view *start = iter;
	do {
		if (!iter->minimized) {
			focus_toplevel(iter);
			server->cycling_mode = false;
			return true;
		}
		iter = wl_container_of(iter->link.prev, iter, link);
		if (&iter->link == &server->toplevels) {
			iter = wl_container_of(server->toplevels.prev, iter, link);
		}
	} while (iter != start);

	// All views might be minimized
	server->cycling_mode = false;
	return false;
}

/* brightness control */
static int get_current_brightness(const char *path) {
	int brightness = 1;
	FILE *brightness_file = fopen(path, "r");
	if (brightness_file != NULL) {
		fscanf(brightness_file, "%d", &brightness);
		fclose(brightness_file);
	}
    else {
		wlr_log(WLR_ERROR, "Error in 'get_current_brightness' opening the file: %s", path);
	}
	return brightness;
}

static void set_brightness(int level, const char *path) {
	FILE *brightness_file = fopen(path, "w");
	if (brightness_file != NULL) {
		fprintf(brightness_file, "%d", level);
		fclose(brightness_file);
	}
    else {
		wlr_log(WLR_ERROR, "Error in 'set_brightness' opening the file: %s", path);
		return;
	}
}

/* Drag and drop */
/// Handle a request to start a drag event
static void seat_request_start_drag(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_start_drag_event *event = data;
	if (event == NULL) {
		wlr_log(WLR_ERROR, "Received NULL event in seat_request_start_drag");
		return;
	}

	struct woodland_server *server = wl_container_of(listener, server, request_start_drag);
	if (server == NULL || server->seat == NULL) {
		wlr_log(WLR_ERROR, "Received NULL server or seat in seat_request_start_drag");
		return;
	}

	wlr_log(WLR_INFO, "Request to start dragging with event %p", event);

	if (wlr_seat_validate_pointer_grab_serial(server->seat, event->origin, event->serial)) {
		wlr_log(WLR_INFO, "Accepting drag start request");
		wlr_seat_start_pointer_drag(server->seat, event->drag, event->serial);
		return;
	}

	wlr_log(WLR_ERROR, "Ignoring request_start_drag, could not validate pointer serial %d",
																			event->serial);
	if (event->drag != NULL && event->drag->source != NULL) {
		wlr_data_source_destroy(event->drag->source);
	}
}

/// Handle a start_drag event
static void seat_start_drag(struct wl_listener *listener, void *data) {
	(void)data;

	struct wlr_drag *drag = data;
	if (drag == NULL) {
		wlr_log(WLR_ERROR, "Received NULL drag in seat_start_drag");
		return;
	}

	struct woodland_server *server = wl_container_of(listener, server, start_drag);
	if (server == NULL || server->seat == NULL) {
		wlr_log(WLR_ERROR, "Received NULL server or seat in seat_start_drag");
		return;
	}

	wlr_log(WLR_INFO, "Starting drag");

	// Don't actually do anything: the drag event becomes active in the wlr_seat and
	// automatically does the right thing w.r.t passing this information through to
	// surfaces
	// wl_signal_add(&drag->events.destroy, &server->seat->drag.events.destroy);
}

/* Pointer constraints */
static void handle_pointer_constraint_destroy(struct wl_listener *listener, void *data) {
	struct woodland_server *server = wl_container_of(listener, server, constraint_destroy);
	struct wlr_pointer_constraint_v1 *constraint = data;

	// Deactivate the constraint
	wlr_pointer_constraint_v1_send_deactivated(constraint);

	// Clean up
	server->active_pointer_constraint = NULL;
}

static void handle_new_pointer_constraint(struct wl_listener *listener, void *data) {
	struct woodland_server *server = wl_container_of(listener, server, new_pointer_constraint);
	struct wlr_pointer_constraint_v1 *constraint = data;

	// You might want to store the constraint in your server struct or elsewhere
	server->active_pointer_constraint = constraint;

	// Activate the constraint
	wlr_pointer_constraint_v1_send_activated(constraint);

	// Set up listener for constraint destruction
	wl_signal_add(&constraint->events.destroy, &server->constraint_destroy);
	server->constraint_destroy.notify = handle_pointer_constraint_destroy;
}

/* Input devices */
static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	// This event is raised by the keyboard base wlr_input_device to signal
	// the destruction of the wlr_keyboard. It will no longer receive events
	// and should be destroyed.
	(void)data;
	struct woodland_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	WL_LIST_SAFE_REMOVE(&keyboard->modifiers.link);
	WL_LIST_SAFE_REMOVE(&keyboard->key.link);
	WL_LIST_SAFE_REMOVE(&keyboard->destroy.link);
	WL_LIST_SAFE_REMOVE(&keyboard->link);
	free(keyboard);
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	// This event is raised when a modifier key, such as shift or alt, is
	// pressed. We simply communicate this to the client. */
	(void)data;
	struct woodland_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
	// A seat can only have one keyboard, but this is a limitation of the
	// Wayland protocol - not wlroots. We assign all connected keyboards to the
	// same seat. You can swap out the underlying wlr_keyboard like this and
	// wlr_seat handles this transparently.
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);

	// Send modifiers to the client.
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->wlr_keyboard->modifiers);
}

/* This function parses the config file (woodland.ini) and storing all
 * user defined keyboard shortcuts (keys) in a char array
 */
static void keybindings_group_init(char *config, char *modifierName, char **keynames, char **keycommands) {
	FILE *file = fopen(config, "r");
	if (!file) {
		perror("Error opening file");
		return;
	}
	char buffer[1024];
	int keyIndex = 0;
	while (fgets(buffer, sizeof(buffer), file)) {
		// Remove trailing newline if present
		buffer[strcspn(buffer, "\n")] = 0;

		// Ignore comments and empty lines
		if (buffer[0] == '#' || buffer[0] == '\0') {
			continue;
		}

		// Check if the line starts with "binding_"
		if (strncmp(buffer, "binding_", 8) == 0) {
			char *equalPos = strchr(buffer, '=');
			if (equalPos) {
				equalPos++; // Move past the '='
				while (*equalPos == ' ' || *equalPos == '\t') equalPos++; // Skip spaces/tabs

				// Check if the modifier name matches
				if (strstr(equalPos, modifierName) == equalPos) {
					equalPos += strlen(modifierName);
					while (*equalPos == ' ' || *equalPos == '\t') equalPos++; //Skip spaces/tabs

					// Extract the keyname
					char *keyname = equalPos;
					keynames[keyIndex] = strdup(keyname);

					// Get the next line for the command
					if (fgets(buffer, sizeof(buffer), file)) {
						buffer[strcspn(buffer, "\n")] = 0;
						// Ignore comments and empty lines
						if (buffer[0] == '#' || buffer[0] == '\0') {
							continue;
						}
						if (strncmp(buffer, "command_", 8) == 0) {
							equalPos = strchr(buffer, '=');
							if (equalPos) {
								equalPos++; // Move past the '='
								// Skip spaces/tabs
								while (*equalPos == ' ' || *equalPos == '\t') equalPos++;

								// Extract the command
								keycommands[keyIndex] = strdup(equalPos);
								keyIndex++; // Increment after setting command
							}
						}
					}
				}
			}
		}
	}
	fclose(file);
}

static void process_keybindings(struct woodland_server *server, char *config, char *modName,
																		xkb_keysym_t sym) {
	char *keynames[1024] = { 0 };
	char *keycommands[1024] = { 0 };

	// Parsing config file, extracting all user defined XKB keys bound to specified mod
	// Storing all the key names in 'keynames' and corresponding commands in 'keycommands' 
	keybindings_group_init(config, modName, keynames, keycommands);

	// Converting currently pressed key (sym parameter) into a key name
	// 'keyname' contains the char representation of the currently pressed key
	char hexCode[256];
	snprintf(hexCode, sizeof(hexCode), "%#06x", sym);
	char *keyname = xkb_keyname(hexCode);
	// Checking if the currently pressed key 'keyname' is found in 'keynames' char array
	for (int i = 0; keynames[i] != NULL; i++) {
		// If currently pressed key 'keyname' is found in 'keynames'
		// then execute the corresponding command from 'keycommands'
		if (strcmp(keyname, keynames[i]) == 0) {
			///fprintf(stderr, "Found shortcut (%s+%s)\n", modName, keynames[i]);
			///fprintf(stderr, "Executing command: %s\n", keycommands[i]);
			server->keybind_handled = true;
			run_cmd(keycommands[i]);  // Assuming you have a function to execute the command
			break;
		}
	}
	// Free resources
	for (int i = 0; keynames[i] != NULL; i++) {
		free(keynames[i]);
		keynames[i] = NULL;
		free(keycommands[i]);
		keycommands[i] = NULL;
	}
	if (keyname != NULL) {
		free(keyname);
		keyname = NULL;
	}
}

static bool handle_keybinding_alt(struct woodland_server *server, xkb_keysym_t sym) {
	// This function assumes Alt is held down.
	// Get the current view and the next view
	struct woodland_view *current_view = wl_container_of(server->toplevels.next, current_view, link);
	struct woodland_view *next_view = wl_container_of(current_view->link.next, next_view, link);
	switch (sym) {
	case XKB_KEY_Tab: // Alt+Tab cycle to the next view
		server->keybind_handled = true;
		cycle_windows(server);
		break;
	default:
		// Executing user defined shortcuts from config file
		process_keybindings(server, server->config, "WLR_MODIFIER_ALT", sym);
		break;
	}
	return true;
}

static bool handle_keybinding_ctrl(struct woodland_server *server, xkb_keysym_t sym) {
	(void)server;
	// This function assumes Ctrl is held down.
	// Executing user defined shortcuts from config file
	process_keybindings(server, server->config, "WLR_MODIFIER_CTRL", sym);
	return true;
}

static bool handle_keybinding_shift(struct woodland_server *server, xkb_keysym_t sym) {
	(void)server;
	// This function assumes Shift is held down.
	// Executing user defined shortcuts from config file
	process_keybindings(server, server->config, "WLR_MODIFIER_SHIFT", sym);
	return true;
}

 /**
 * Here we handle compositor keybindings. This is when the compositor is
 * processing keys, rather than passing them on to the client for its own processing.
 *
 * This function assumes Super is held down.
 */
static bool handle_keybinding_super(struct woodland_server *server, xkb_keysym_t sym) {
	// Get the current view and the next view
	struct woodland_view *current_view = wl_container_of(server->toplevels.next, current_view, link);
	struct woodland_view *next_view = wl_container_of(current_view->link.next, next_view, link);
	switch (sym) {
	case XKB_KEY_Escape: // Super+Esc Log out from compositor
		server->keybind_handled = true;
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_x: // Super+x close current active window
		if (!wl_list_empty(&server->toplevels)) {
			server->keybind_handled = true;
			wlr_xdg_toplevel_send_close(current_view->xdg_toplevel);
		}
		break;
	default:
		// Executing user defined shortcuts from config file
		process_keybindings(server, server->config, "WLR_MODIFIER_LOGO", sym);
		break;
	}
	return true;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct woodland_keyboard *keyboard =wl_container_of(listener, keyboard, key);
	struct woodland_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	///struct wlr_seat *seat = server->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	char hexCode[256];
	snprintf(hexCode, sizeof(hexCode), "%#06x", syms[0]);
	char *keyname = xkb_keyname(hexCode);
	if (!keyname) {
		wlr_log(WLR_ERROR, "Failed to get keyname in 'keyboard_handle_key'");
	}

	keyboard->server->keybind_handled = false;

	for (int i = 0; i < nsyms; i++) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			// Typing the password
			if (server->network_password_prompt) {
				char name[64];
				if (xkb_keysym_get_name(syms[i], name, sizeof(name)) > 0) {
					// Skip known modifiers
					if (strcmp(name, "Shift_L") == 0 || strcmp(name, "Shift_R") == 0 ||
						strcmp(name, "Control_L") == 0 || strcmp(name, "Control_R") == 0 ||
						strcmp(name, "Alt_L") == 0 || strcmp(name, "Alt_R") == 0 ||
						strcmp(name, "Super_L") == 0 || strcmp(name, "Super_R") == 0 ||
						strcmp(name, "Meta_L") == 0 || strcmp(name, "Meta_R") == 0 ||
						strcmp(name, "Caps_Lock") == 0) {
						continue;
					}
					char str_buff[8];
					int len = xkb_keysym_to_utf8(syms[i], str_buff, sizeof(str_buff));

					if (syms[i] == XKB_KEY_BackSpace) {
						size_t buflen = strlen(server->buff);
						if (buflen > 0) {
							while (buflen > 0 && ((server->buff[buflen - 1] & 0xC0) == 0x80)) {
								buflen = buflen - 1;
							}
							server->buff[--buflen] = '\0';
						}
						if (server->ssids[3]) {
							free(server->ssids[3]);
							server->ssids[3] = NULL;
						}
						server->ssids[3] = strdup(server->buff);
					}
					else if (len > 0) {
						if (!server->buff[0]) {
							strncpy(server->buff, str_buff, sizeof(server->buff) - 1);
						}
						else {
							strncat(server->buff, str_buff, sizeof(server->buff) - strlen(server->buff) - 1);
						}
						if (server->ssids[3]) {
							free(server->ssids[3]);
							server->ssids[3] = NULL;
						}
						server->ssids[3] = strdup(server->buff);
					}
				}
			}
			// Change keyboard layout
			if (syms[i] == XKB_KEY_ISO_Next_Group) {
				struct woodland_view *current_view = wl_container_of(keyboard->server->toplevels.next,
																	 current_view,
																	 link);
				if (current_view && keyboard->server->seat->keyboard_state.keyboard->xkb_state) {
					if (current_view->keyboard_layout >= (keyboard->server->LayoutIndexes - 1)) {
						current_view->keyboard_layout = 0;
					}
					else {
						current_view->keyboard_layout = current_view->keyboard_layout + 1;
					}
				}
			}
			// Multimedia keys support
			else if (syms[i] == XKB_KEY_XF86AudioPlay || syms[i] == XKB_KEY_XF86AudioPause ||
				syms[i] == XKB_KEY_XF86AudioMute) {
				run_cmd(keyboard->server->play_pause);
				return;
			}
			else if (syms[i] == XKB_KEY_XF86AudioRaiseVolume) {
				run_cmd(keyboard->server->volume_up);
				return;
			}
			else if (syms[i] == XKB_KEY_XF86AudioLowerVolume) {
				run_cmd(keyboard->server->volume_down);
				return;
			}
			else if (syms[i] == XKB_KEY_XF86MonBrightnessUp) {
				// Get current brightness
				if (keyboard->server->brightness_path) {
					keyboard->server->saved_brightness = get_current_brightness(
												keyboard->server->brightness_path);
					set_brightness(keyboard->server->saved_brightness + 1,
										keyboard->server->brightness_path);
				}
				else {
					wlr_log(WLR_ERROR, "'brightness_path' is NULL in 'keyboard_handle_key'");
				}
				return;
			}
			else if (syms[i] == XKB_KEY_XF86MonBrightnessDown) {
				// Get current brightness
				if (keyboard->server->brightness_path) {
					keyboard->server->saved_brightness = get_current_brightness(
												keyboard->server->brightness_path);
					set_brightness(keyboard->server->saved_brightness - 1,
										keyboard->server->brightness_path);
				}
				else {
					wlr_log(WLR_ERROR, "'brightness_path' is NULL in 'keyboard_handle_key'");
				}
				return;
			}
			else if (syms[i] == XKB_KEY_XF86Switch_VT_1) {
				wlr_session_change_vt(server->session, 1);
				return;
			}
			else if (syms[i] == XKB_KEY_XF86Switch_VT_2) {
				wlr_session_change_vt(server->session, 2);
				return;
			}
		}
		// Check if the Super key is pressed or released
		if (syms[i] == XKB_KEY_Super_L || syms[i] == XKB_KEY_Super_R) {
			keyboard->server->super_key_down = (event->state == WL_KEYBOARD_KEY_STATE_PRESSED);
			if (server->display_is_off) {
				///fprintf(stderr, "Super key pressed!\n");
				server->display_is_off = false;
				struct woodland_output *output;
				wl_list_for_each(output, &server->outputs, link) {
					struct wlr_output_state state;
					wlr_output_state_init(&state);
					wlr_output_state_set_enabled(&state, true);
					if (!wlr_output_commit_state(output->wlr_output, &state)) {
						fprintf(stderr, "Display off failed to commit output state\n");
					}
					wlr_output_state_finish(&state);
					wlr_output_schedule_frame(output->wlr_output);
				}
			}
		}
		// Handle compositor keybindings
		else if (keyname) {
			if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				handle_keybinding_alt(keyboard->server, syms[i]);
			}
			else if ((modifiers & WLR_MODIFIER_CTRL) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				handle_keybinding_ctrl(keyboard->server, syms[i]);
			}
			else if ((modifiers & WLR_MODIFIER_SHIFT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				handle_keybinding_shift(keyboard->server, syms[i]);
			}
			else if ((modifiers & WLR_MODIFIER_LOGO) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				handle_keybinding_super(keyboard->server, syms[i]);
			}
		}
	}
	// Clean up keyname memory
	if (keyname) {
		free(keyname);
		keyname = NULL;
	}
	// Pass the key to the client if not handled by keybindings
	if (!keyboard->server->keybind_handled) {
		wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(keyboard->server->seat,
									 event->time_msec,
									 event->keycode,
									 event->state);
	}
}

static void server_new_keyboard(struct woodland_server *server, struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
	struct woodland_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	char *layouts = get_char_value_from_conf(server->config, "xkb_layouts");
	if (!layouts) {
		wlr_log(WLR_ERROR, "Error: Keyboard layouts could not be loaded from config\n");
		free(keyboard);
		return;
	}
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Failed to create XKB context.");
		free(keyboard);
		free(layouts);
		return;
	}
	struct xkb_rule_names rules = {
		.layout = layouts, // Specify multiple layouts separated by commas
		.options = "grp:alt_shift_toggle" // Option to switch layout with Alt+Shift
	};
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context,
														  &rules,
														  XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		wlr_log(WLR_ERROR, "Failed to create XKB keymap.");
		xkb_context_unref(context);
		free(keyboard);
		free(layouts);
		return;
	}

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
	wlr_seat_set_capabilities(server->seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);

	// And add the keyboard to our list of keyboards
	wl_list_insert(&server->keyboards, &keyboard->link);

	// Clean up
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	free(layouts);
	
	keyboard->server->LayoutIndexes = xkb_keymap_num_layouts(
								keyboard->server->seat->keyboard_state.keyboard->keymap);
}

static void new_virtual_keyboard_handler(struct wl_listener *listener, void *data) {
	/* This event is raised when a new virtual keyboard is created. */
	struct wlr_virtual_keyboard_v1 *virtual_keyboard = data;
	if ((!virtual_keyboard) || (virtual_keyboard == NULL)) {
		wlr_log(WLR_ERROR, "'virtual_keyboard' is NULL in 'new_virtual_keyboard_handler'.");
		return;
	}

	struct woodland_server *server = wl_container_of(listener, server, new_virtual_keyboard);

	/* Create a new woodland_keyboard structure to represent the virtual keyboard. */
	struct woodland_keyboard *keyboard = calloc(1, sizeof(struct woodland_keyboard));
	if ((!virtual_keyboard) || (virtual_keyboard == NULL)) {
		wlr_log(WLR_ERROR, "'keyboard' memory alloc failed in 'new_virtual_keyboard_handler'.");
		return;
	}

	keyboard->server = server;
	keyboard->wlr_keyboard = &virtual_keyboard->keyboard;
	keyboard->device = &virtual_keyboard->keyboard.base;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if ((!context) || (context == NULL)) {
		wlr_log(WLR_ERROR, "'context' is NULL in 'new_virtual_keyboard_handler'.");
		return;
	}
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if ((!keymap) || (keymap == NULL)) {
		wlr_log(WLR_ERROR, "'keymap' is NULL in 'new_virtual_keyboard_handler'.");
		return;
	}

	/* Dereference the keyboard pointer */
	wlr_keyboard_set_keymap(&virtual_keyboard->keyboard, keymap);
	wlr_keyboard_set_repeat_info(&virtual_keyboard->keyboard, 30, 300);

	/* Set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&virtual_keyboard->keyboard.events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&virtual_keyboard->keyboard.events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&virtual_keyboard->keyboard.base.events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, &virtual_keyboard->keyboard);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_log(WLR_INFO, "Virtual keyboard initialized: %p", virtual_keyboard);
}

// Function to enable tap-to-click on a libinput device
static void enable_tap_to_click(struct wlr_input_device *device) {
	if (device->type == WLR_INPUT_DEVICE_POINTER) {
		struct libinput_device *libinput_dev = wlr_libinput_get_device_handle(device);
		if (libinput_dev && libinput_device_config_tap_get_finger_count(libinput_dev) > 0) {
			libinput_device_config_tap_set_enabled(libinput_dev, LIBINPUT_CONFIG_TAP_ENABLED);
		}
	}
}

/**
 * We don't do anything special with pointers. All of our pointer handling
 * is proxied through wlr_cursor. On another compositor, you might take this
 * opportunity to do libinput configuration on the device to set
 * acceleration, etc.
 */
static void server_new_pointer(struct woodland_server *server, struct wlr_input_device *device) {
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct woodland_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		if (strcmp(server->tap_enable, "enable") == 0) {
			enable_tap_to_click(device);  // Enable tap-to-click for pointer devices
		}
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct woodland_server *server = wl_container_of(listener, server, request_cursor);
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

 /**
 * This event is raised by the seat when a client wants to set the selection,
 * usually when the user copies something. wlroots allows compositors to
 * ignore such requests if they so choose, but in tinywl we always honor
 */
static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	struct woodland_server *server = wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}
/* End of input devices setup */

static struct woodland_view *get_toplevel_for_surface(struct woodland_server *server,
														struct wlr_surface *surface) {
	struct woodland_view *toplevel;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (toplevel->xdg_toplevel->base->surface == surface) {
			return toplevel;
		}
	}
	return NULL;
}

static struct woodland_view *desktop_toplevel_at(struct woodland_server *server,
																		double lx,
																		double ly,
																		struct wlr_surface **surface,
																		double *sx,
																		double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * We only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a woodland_view. */
	struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the woodland_view at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree->node.data;
}

static void process_cursor_move(struct woodland_server *server) {
	struct woodland_view *toplevel = server->grabbed_toplevel;
	double base_dx = server->cursor->x - server->grab_x;
	double base_dy = server->cursor->y - server->grab_y;
	// Move a single window
	wlr_scene_node_set_position(&toplevel->scene_tree->node, base_dx, base_dy);
}

static void process_cursor_resize(struct woodland_server *server) {
	struct woodland_view *toplevel = server->grabbed_toplevel;
	if (!toplevel) {
		return;
	}
	if (server->cursor_mode == WOODLAND_CURSOR_RESIZE) {
		wlr_xdg_toplevel_set_resizing(toplevel->xdg_toplevel, true);
	}
	else {
		wlr_xdg_toplevel_set_resizing(toplevel->xdg_toplevel, false);
	}
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	
	// Calculate group-aware resize parameters
	struct wlr_box group_geo;
	group_geo = server->grab_geobox;

	// Calculate resize deltas based on group geometry
	int new_left = group_geo.x;
	int new_right = group_geo.x + group_geo.width;
	int new_top = group_geo.y;
	int new_bottom = group_geo.y + group_geo.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) new_top = new_bottom - 1;
	}
	if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) new_bottom = new_top + 1;
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) new_left = new_right - 1;
	}
	if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) new_right = new_left + 1;
	}

	// Existing single window resize logic
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
								 new_left - toplevel->xdg_toplevel->base->current.geometry.x,
								 new_top - toplevel->xdg_toplevel->base->current.geometry.y);

	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_right - new_left, new_bottom - new_top);
}

static void process_cursor_motion(struct woodland_server *server, uint32_t time) {
	/* Handle interactive modes first */
	if (server->cursor_mode == WOODLAND_CURSOR_MOVE) {
		process_cursor_move(server);
		return;
	}
	else if (server->cursor_mode == WOODLAND_CURSOR_RESIZE) {
		process_cursor_resize(server);
		return;
	}

	double sx;
	double sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;

	struct woodland_view *toplevel = desktop_toplevel_at(server,
														server->cursor->x,
														server->cursor->y,
														&surface,
														&sx,
														&sy);

	if (!toplevel) {
		/* Clear foreign toplevel focus when not over any window */
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
		if (seat->keyboard_state.focused_surface) {
			struct woodland_view *focused_toplevel = get_toplevel_for_surface(server,
												seat->keyboard_state.focused_surface);
			if (focused_toplevel && focused_toplevel->foreign_handle) {
				wlr_foreign_toplevel_handle_v1_set_activated(focused_toplevel->foreign_handle, false);
			}
		}
	}
	if (surface) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	}
	else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

 /**
 * This event is forwarded by the cursor when a pointer emits a _relative_
 * pointer motion event (i.e. a delta) 
 *
 * The cursor doesn't move unless we tell it to. The cursor automatically
 * handles constraining the motion to the output layout, as well as any
 * special configuration applied for the specific input device which
 * generated the event. You can pass NULL for the device if you want to move
 * the cursor around without any input.
 */
static void server_cursor_motion(struct wl_listener *listener, void *data) {
	// Safety check for event listener and data
	if (!listener || !data) {
		fprintf(stderr, "server_cursor_motion: NULL listener or data\n");
		return;
	}

	// Get a pointer to the main server structure from the listener
	struct woodland_server *server = wl_container_of(listener, server, cursor_motion);
	if (!server || !server->cursor || !server->output_layout || !server->scene) {
		fprintf(stderr, "server_cursor_motion: server or critical members uninitialized\n");
		return;
	}

	// Cast the void pointer to a pointer motion event
	struct wlr_pointer_motion_event *event = data;
	if (!event || !event->pointer) {
		fprintf(stderr, "server_cursor_motion: NULL event or event->pointer\n");
		return;
	}

	// Find the output under the current cursor position
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
											server->cursor->x, server->cursor->y);
	if (!output) {
		fprintf(stderr, "server_cursor_motion: No output under cursor\n");
		return;
	}

	// Get the root scene tree node for panning/zooming
	struct wlr_scene_tree *pan_zoom_root = wlr_scene_tree_from_node(&server->scene->tree.node);
	if (!pan_zoom_root) {
		fprintf(stderr, "server_cursor_motion: Failed to get scene tree from root node\n");
		return;
	}
	int output_width = server->transformed_width;
	int output_height = server->transformed_height;

	// Define margins for when panning should start
	double left_threshold = 10;
	double top_threshold = 10;
	double right_threshold = (output_width / server->zoom_factor) - 10;
	double bottom_threshold = (output_height / server->zoom_factor) - 10;

	// Calculate the full content size with zoom applied
	int32_t zoomed_width = (int32_t)(output_width * server->zoom_factor);
	int32_t zoomed_height = (int32_t)(output_height * server->zoom_factor);

	// Keep track of panning offsets (shared static for simplicity)
	static double pan_x = 0;
	static double pan_y = 0;

	if (server->zoom_factor > 1.0) {
		// Cursor is near the edges — initiate panning
		if (server->cursor->x < left_threshold ||
			server->cursor->x > right_threshold ||
			server->cursor->y < top_threshold ||
			server->cursor->y > bottom_threshold) {

			// Compute the desired pan target based on cursor position
			double pan_x_target = (server->cursor->x / (double)output_width) *
												(zoomed_width - output_width);
			double pan_y_target = (server->cursor->y / (double)output_height) *
												(zoomed_height - output_height);

			// Smoothly move pan position toward the target using zoom_speed
			pan_x += (pan_x_target - pan_x) * server->zoom_speed_m;
			pan_y += (pan_y_target - pan_y) * server->zoom_speed_m;

			// Clamp pan to avoid showing outside the zoomed area
			if (pan_x < 0) {
				pan_x = 0;
			}
			if (pan_y < 0) {
				pan_y = 0;
			}
			if (pan_x > zoomed_width - output_width) {
				pan_x = zoomed_width - output_width;
			}
			if (pan_y > zoomed_height - output_height) {
				pan_y = zoomed_height - output_height;
			}
			// Apply the pan offset by shifting the scene node
			wlr_scene_node_set_position(&pan_zoom_root->node, -pan_x, -pan_y);
		}
	}
	else {
		// Reset pan offsets when zoom factor is 1.0
		pan_x = 0;
		pan_y = 0;
		wlr_scene_node_set_position(&pan_zoom_root->node, 0, 0);
	}

	// Show/hide panel
	double nx = 0;
	double ny = 0;

	// Get the buffer local coordinates
	struct wlr_scene_node *node = wlr_scene_node_at(&server->scene->tree.node,
													server->cursor->x,
													server->cursor->y,
													&nx,
													&ny);

	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, output, &output_box);

	// Static flags to track previous hover state
	static bool cursor_in_panel_region = false;
	static bool cursor_in_volume_region = false;
	static bool cursor_in_brightness_region = false;
	static bool cursor_in_time_region = false;
	static bool cursor_in_network_region = false;
	static bool cursor_in_menu_list_region = false;
	static bool cursor_in_window_list_region = false;
	static bool cursor_in_network_applet_region = false;

	bool in_panel_region = server->cursor->x > output_box.width - PPANEL_WIDTH &&
												server->cursor->y >= (output_box.height - 7);

	if (in_panel_region && !cursor_in_panel_region) {
		///fprintf(stderr, "Show panel\n");
		if (!server->time_update_timer) {
			server->time_update_timer = wl_event_loop_add_timer(server->event_loop, update_time, server);
			wl_event_source_timer_update(server->time_update_timer, 1000);
		}
		else {
			wl_event_source_timer_update(server->time_update_timer, 1000);
		}
		server->panel_is_hidden = false;
	}
	else if ((server->cursor->x < (output_box.width - PPANEL_WIDTH) &&
		!server->time_is_clicked && !server->network_is_clicked) ||
		(server->cursor->y < (output_box.height - 45) &&
		!server->time_is_clicked &&
		!server->network_is_clicked)) {

		if (!server->panel_is_hidden) {
			///fprintf(stderr, "Hide panel\n");
			server->panel_is_hidden = true;
			server->volume_change = false;
			server->brightness_change = false;
			server->time_hovered = false;
			server->time_is_clicked = false;
			server->network_hovered = false;
			server->network_ly_hovered = false;
			server->network_is_clicked = false;
			if (server->time_update_timer && server->panel_buffer) {
				wlr_scene_node_set_enabled(&server->panel_buffer->node, false);
				wl_event_source_remove(server->time_update_timer);
				server->time_update_timer = NULL;
			}
		}
	}
	cursor_in_panel_region = in_panel_region;

	// Activate panel widgets on mouse hover
	if (node->data) {
		const char *retrieved = (const char *)node->data;

		// Check if the mouse cursor is hovering over the panel title
		if (strcmp(retrieved, "woodland_panel") == 0) {
			///fprintf(stderr, "server->scene->tree.node.data: %s\n", retrieved);
			///fprintf(stderr, "nx: %d\n", (int)nx);
			///fprintf(stderr, "ny: %d\n", (int)ny);

			// Volume change
			bool in_volume_region = !server->panel_is_hidden && (int)nx > 185 && (int)ny > 3;
			if (in_volume_region) {
				///fprintf(stderr, "Volume change hover\n");
				server->volume_change = true;
				cursor_in_volume_region = false;
			}
			else if (!in_volume_region && !cursor_in_volume_region) {
				///fprintf(stderr, "Volume change leave\n");
				server->volume_change = false;
				cursor_in_volume_region = true;
			}
			cursor_in_volume_region = in_volume_region;

			// Brightness change
			bool in_brightness_region = !server->panel_is_hidden && nx > 145 && nx < 185;
			if (in_brightness_region && !cursor_in_brightness_region) {
				///fprintf(stderr, "Brightness change hover\n");
				server->brightness_change = true;
			}
			else if (!in_brightness_region && cursor_in_brightness_region) {
				///fprintf(stderr, "Brightness change leave\n");
				server->brightness_change = false;
			}
			cursor_in_brightness_region = in_brightness_region;

			// Time hovered
			bool in_time_region = !server->panel_is_hidden && nx > 60 && nx < 138;
			if (in_time_region && !cursor_in_time_region) {
				///fprintf(stderr, "Time hovered\n");
				server->time_hovered = true;
			}
			else if (!in_time_region && cursor_in_time_region) {
				///fprintf(stderr, "Time leave\n");
				server->time_hovered = false;
			}
			cursor_in_time_region = in_time_region;

			// Network hovered
			bool in_network_region = !server->panel_is_hidden && nx >= 7 && nx < 50;
			if (in_network_region && !cursor_in_network_region) {
				///fprintf(stderr, "Network hovered\n");
				server->network_hovered = true;
			}
			else if (!in_network_region && cursor_in_network_region) {
				///fprintf(stderr, "Network leave\n");
				server->network_hovered = false;
			}
			cursor_in_network_region = in_network_region;
		}
		bool in_network_applet_region = !server->panel_is_hidden &&
										strcmp(retrieved, "woodland_network_applet") == 0 &&
										nx >= 3 &&
										ny < (PNETWORK_HEIGHT - 3);
		if (in_network_applet_region) {
			// Inside network applet
			////fprintf(stderr, "Entered the network applet dialog region\n");
			server->network_ly_hovered = true;

			// SsidPosition converts the current mouse cursor to the network ssid name in the list
			server->SsidPosition = (int)ny / 26;
			///fprintf(stderr, "the position of item: %d\n", server->SsidPosition);
		}
		else if (!in_network_applet_region && cursor_in_network_applet_region) {

			///fprintf(stderr, "Left the network applet dialog region\n");
			server->network_ly_hovered = false;
		}
		cursor_in_network_applet_region = in_network_applet_region;

		// Hovering over the titles in the windowlist dialog.
		bool in_windowlist_region = strcmp(retrieved, "woodland_windowlist") == 0 &&
											server->titles_clicked &&
											nx >= 3 &&
											ny < (server->titles_dialog_size - 3);;
		if (in_windowlist_region) {
			///fprintf(stderr, "in_windowlist_region\n");
			int relative_y = ny - 3; // Adjust for top margin
			int row = relative_y / 40;
			server->TitlesPosition = row;

			if (row >= 0 && row < server->titles_counter) {
				server->TitlesPosition = row;
			}
			else {
				server->TitlesPosition = -1;
			}

			if (server->titles_scene_buffer) {
				wlr_scene_node_destroy(&server->titles_scene_buffer->node);
				server->titles_scene_buffer = NULL;
			}
			list_titles(server);
			
			server->titles_ly_hovered = true;
			cursor_in_window_list_region = true;
			///fprintf(stderr, "server->TitlesPosition: %d\n", (int)server->TitlesPosition);
		}
		else if (!in_windowlist_region && cursor_in_window_list_region) {
			///fprintf(stderr, "Left the window list region\n");
			server->titles_ly_hovered = false;
			cursor_in_window_list_region = false;
		}
		cursor_in_window_list_region = in_windowlist_region;

		// Hovering over the titles in the menu dialog.
		bool in_menu_region = strcmp(retrieved, "woodland_menu") == 0 &&
											server->menu_clicked &&
											nx >= 3 &&
											nx <= 197 &&
											ny > 3 &&
											ny < (server->menu_dialog_size - 3);
		if (in_menu_region) {
			///fprintf(stderr, "in_menu_region\n");
			int relative_y = ny - 3; // Adjust for top margin
			int row = relative_y / 40;
			server->menuPosition = row;

			if (server->menu_scene_buffer) {
				wlr_scene_node_destroy(&server->menu_scene_buffer->node);
				server->menu_scene_buffer = NULL;
			}
			show_menu(server);
			
			server->menu_ly_hovered = true;
			cursor_in_menu_list_region = true;
		}
		else if (!in_menu_region && cursor_in_menu_list_region) {
			///fprintf(stderr, "Left the menu list region\n");
			server->menu_ly_hovered = false;
			cursor_in_menu_list_region = false;
		}
		cursor_in_menu_list_region = in_menu_region;
	}

	wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
	// Sends relative motion used mostly in games for 360-degree mouse view
	wlr_relative_pointer_manager_v1_send_relative_motion(server->wlr_relative_pointer_manager,
														server->seat,
														(uint64_t)event->time_msec * 1000,
														event->delta_x,
														event->delta_y,
														event->unaccel_dx,
														event->unaccel_dy);
	// Handle focus changes and client-side pointer motion notification
	process_cursor_motion(server, event->time_msec);
}

/* This function is a workaround for GTK apps to stop them from auto-resizing when scaling/zooming */
static void keep_scaling_factor(struct woodland_server *server) {
	struct woodland_view *iter;
	wl_list_for_each_reverse(iter, &server->toplevels, link) {
		wlr_xdg_toplevel_set_resizing(iter->xdg_toplevel, true);
	}
	return;
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	// Safety checks
	if (!listener || !data) {
		fprintf(stderr, "server_cursor_axis: NULL listener or data\n");
		return;
	}

	struct woodland_server *server = wl_container_of(listener, server, cursor_axis);
	if (!server || !server->cursor || !server->output_layout || !server->scene || !server->seat) {
		fprintf(stderr, "server_cursor_axis: server or critical fields uninitialized\n");
		return;
	}

	struct wlr_pointer_axis_event *event = data;
	if (!event) {
		fprintf(stderr, "server_cursor_axis: NULL axis event\n");
		return;
	}

	// Determine the output under the cursor
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
															server->cursor->x,
															server->cursor->y);
	if (!output) {
		fprintf(stderr, "server_cursor_axis: No output under cursor\n");
		return;
	}

	struct wlr_scene_tree *pan_zoom_root = wlr_scene_tree_from_node(&server->scene->tree.node);
	if (!pan_zoom_root) {
		fprintf(stderr, "server_cursor_axis: Failed to get scene tree from root node\n");
		return;
	}

	struct wlr_output_state state;
	wlr_output_state_init(&state);

	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */

	double delta = event->delta;
	double ZoomFactor = 0;
	static double MouseZoomFactor = 0.2;
	static double TouchpadZoomFactor = 0.01;

	// Adjust delta based on input source (mouse wheel vs touchpad)
	if (event->source == WL_POINTER_AXIS_SOURCE_FINGER) {
		// Scale the delta for touchpad events
		delta *= TOUCHPAD_SCROLL_SCALE;
		ZoomFactor = TouchpadZoomFactor;
		server->zoom_speed_m = server->zoom_speed + 0.02;
	}
	else {
		// Scale the delta for mouse wheel events
		delta *= MOUSE_SCROLL_SCALE;
		ZoomFactor = MouseZoomFactor;
		server->zoom_speed_m = server->zoom_speed;
	}

	//------- Filtering out small scroll values -----//
	// Define a variable to hold the threshold value
	static double scroll_debounce_threshold = SCROLL_DEBOUNCE_THRESHOLD;

	// Calculate the average scroll value over a certain period
	static double sum_delta = 0;
	static int num_samples = 0;
	const int max_samples = 10;

	sum_delta += delta;
	num_samples++;

	if (num_samples >= max_samples) {
		double avg_delta = sum_delta / num_samples;
		scroll_debounce_threshold = avg_delta * 0.3; // Adjust the threshold based on average scroll value
		sum_delta = 0;
		num_samples = 0;
	}

	// Use the variable in your code
	if (fabs(delta) < scroll_debounce_threshold) {
		return;
	}

	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, output, &output_box);

	// Panel actions
	if (!server->panel_is_hidden) {
		switch (event->orientation) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			if (delta > 0) {
				///fprintf(stderr, "Mouse wheel down\n");
				if (server->volume_change) {
					run_cmd(server->volume_down);
					return;
					break;
				}
				if (server->brightness_change) {
					if (server->brightness_path) {
						server->saved_brightness = get_current_brightness(server->brightness_path);
						set_brightness(server->saved_brightness - 3, server->brightness_path);
					}
					return;
					break;
				}
			}
			else {
				///fprintf(stderr, "Mouse wheel up\n");
				if (server->volume_change) {
					run_cmd(server->volume_up);
					return;
					break;
				}
				if (server->brightness_change) {
					if (server->brightness_path) {
						server->saved_brightness = get_current_brightness(server->brightness_path);
						set_brightness(server->saved_brightness + 3, server->brightness_path);
					}
					return;
					break;
				}
			}
		}
	}
	// Cycle windows on scrolling the top left corner
	if (server->cursor->x > (output_box.width - 10) && server->cursor->y <
								(output_box.height - output_box.height + 10)) {
		switch (event->orientation) {
		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			if (delta > 0) {
				///fprintf(stderr, "Mouse wheel down\n");
				cycle_windows(server);
				return;
				break;
			}
			else {
				///fprintf(stderr, "Mouse wheel up\n");
				cycle_windows_reverse(server);
				return;
				break;
			}
		}
	}
	//------- Zooming logic -------//
	// Zooming on scrolling on the left-top corner of the screen or Super key + mouse scroll
	if ((server->cursor->x < 5 && server->cursor->y < 5) || server->super_key_down) {
		switch (event->orientation) {
			case WL_POINTER_AXIS_VERTICAL_SCROLL:
			if (delta > 0) {
				///fprintf(stderr, "Mouse wheel down\n");
				// Reset scaling factor
				if ((server->zoom_factor) == 1.0) {
					keep_scaling_factor(server);
					return;
				}
				else if ((server->zoom_factor - 0.7) < 1.0) {
					// Set default zoom level, reset
					server->zoom_factor = 1.0;
					wlr_output_state_set_scale(&state, 1.0);
					if (!wlr_output_commit_state(output, &state)) {
						fprintf(stderr, "Zoom out: failed to commit output state\n");
					}
					keep_scaling_factor(server);
					wlr_output_state_finish(&state);
					// Reset position if zoom was off
					wlr_scene_node_set_position(&pan_zoom_root->node, 0, 0);
					return;
				}

				if (server->zoom_factor > 1.0) {
					// Zoom out by reducing the factor slightly
					server->zoom_factor = server->zoom_factor - ZoomFactor;

					wlr_output_state_set_scale(&state, server->zoom_factor);
					if (!wlr_output_commit_state(output, &state)) {
						fprintf(stderr, "Zoom out: failed to commit output state\n");
					}
					keep_scaling_factor(server);
					wlr_output_state_finish(&state);
					wlr_scene_node_set_position(&pan_zoom_root->node, 0, 0);
					return;
				}
			}
			else {
				///fprintf(stderr, "Mouse wheel up\n");
				// Zoom in by increasing zoom factor slightly
				server->zoom_factor = server->zoom_factor + ZoomFactor;

				if (server->zoom_factor > 10.0) {// Arbitrary upper limit
					server->zoom_factor = 10.0;
				}

				wlr_output_state_set_scale(&state, server->zoom_factor);
				if (!wlr_output_commit_state(output, &state)) {
					fprintf(stderr, "Zoom in: failed to commit output state\n");
				}
				keep_scaling_factor(server);
				wlr_output_state_finish(&state);
				wlr_scene_node_set_position(&pan_zoom_root->node, 0, 0);
				return;
			}
			break;
		default:
			keep_scaling_factor(server);
			break;
		}
	}
	// Notify the client with pointer focus of the axis event.
	wlr_seat_pointer_notify_axis(server->seat,
								event->time_msec,
								event->orientation,
								event->delta,
								event->delta_discrete,
								event->source,
								event->relative_direction);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct woodland_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(server, event->time_msec);
}

/* Reset the cursor mode to passthrough. */
static void reset_cursor_mode(struct woodland_server *server) {
	server->cursor_mode = WOODLAND_CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = NULL;
}

/**
 * Handling cursor button pressing
 */
static void server_cursor_button(struct wl_listener *listener, void *data) {
	// Check for NULL listener and data
	if (!listener || !data) {
		fprintf(stderr, "Error: NULL listener or data in server_cursor_button\n");
		return;
	}

	struct woodland_server *server = wl_container_of(listener, server, cursor_button);
	if (!server) {
		fprintf(stderr, "Error: Failed to get server from listener\n");
		return;
	}

	struct wlr_pointer_button_event *event = data;
	if (!event) {
		fprintf(stderr, "Error: NULL event in server_cursor_button\n");
		return;
	}

	// Notify seat about pointer button event
	wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

	// Check if button was released
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		reset_cursor_mode(server);
		return;
	}
	// Resolve surface and toplevel beneath cursor
	double sx;
	double sy;
	struct wlr_surface *surface = NULL;
	struct woodland_view *toplevel = desktop_toplevel_at(server,
														server->cursor->x,
														server->cursor->y,
														&surface, &sx, &sy);

	// Check if surface was found
	if (surface) {
		// Enter surface to ensure proper pointer focus
		wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
	}
	else {
		// If no surface is found, notify seat about pointer leave
		wlr_seat_pointer_notify_clear_focus(server->seat);
	}

	// Check if toplevel was found
	if (toplevel) {
		// Focus and activate toplevel
		focus_toplevel(toplevel);
		// If the Super key and left mouse button are both pressed, emit a move request
		if (event->button == BTN_LEFT && server->super_key_down) {
			wl_signal_emit(&toplevel->xdg_toplevel->events.request_move, toplevel->xdg_surface);
		}
		else {
			server->super_key_down = false;
		}
	}
	else {
		// Handle case when no toplevel is found
		fprintf(stderr, "Warning: No toplevel found at cursor position\n");
	}

	// Open the calendar
	if (event->button == BTN_LEFT && server->time_hovered && server->time_is_clicked) {
		///fprintf(stderr, "Closing calendar\n");
		server->time_is_clicked = false;
	}
	else if (event->button == BTN_LEFT && server->time_hovered && !server->time_is_clicked) {
		///fprintf(stderr, "Opening calendar\n");
		server->time_is_clicked = true;
		server->calendar_texture = true;
	}
	// Open the network applet
	if (event->button == BTN_LEFT &&
		server->network_hovered &&
		server->network_is_clicked &&
		!server->network_ly_hovered) {
		///fprintf(stderr, "Closing network applet\n");
		server->network_is_clicked = false;
		server->network_ly_hovered = false;
		server->network_password_prompt = false;
	}
	else if (event->button == BTN_LEFT && server->network_hovered && !server->network_is_clicked) {
		///fprintf(stderr, "Opening network applet\n");
		server->network_is_clicked = true;
		server->network_texture = true;
	}
	// Clicking on a wifi network SSID name
	if (event->button == BTN_LEFT &&
		server->number_of_ssids > 0 &&
		server->ssids[server->SsidPosition] != NULL) {

		///fprintf(stderr, "Selected SSID: %s\n", server->ssids[server->SsidPosition]);
		if (strcmp(server->ssids[1], "____________________________________________") != 0 &&
					check_if_secured_ssid(server->ssids[server->SsidPosition])) {

			server->ssids[0] = strdup(server->ssids[server->SsidPosition]);
			server->ssids[1] = strdup("____________________________________________");
			server->ssids[2] = strdup("Please enter password");
			server->ssids[3] = strdup("|");
			server->ssids[4] = strdup("Connect");
			server->ssids[5] = strdup("Cancel");
			server->network_password_prompt = true;
		}
		else if (strcmp(server->ssids[1], "____________________________________________") != 0 &&
						!check_if_secured_ssid(server->ssids[server->SsidPosition])) {
			///fprintf(stderr, "SSID: %s is free\n", server->ssids[server->SsidPosition]);

			// Connecting to a free open wifi network
			connect_to_open_ssid(server->ssids[server->SsidPosition]);
			refresh_networks(server);
		}
		else if (strcmp(server->ssids[1], "____________________________________________") == 0) {
			///fprintf(stderr, "Enter password and connect or cancel\n");
			///fprintf(stderr, "Button clicked: %s\n", server->ssids[server->SsidPosition]);
			if (strcmp(server->ssids[server->SsidPosition], "Connect") == 0) {
				///fprintf(stderr, "Connecting to: %s with password %s\n", server->ssids[0],
				///														server->ssids[3]);
				
				// Connecting to SSID
				server->buff[0] = '\0';
				connect_to_secured_ssid(server->ssids[0], server->ssids[3]);

				// Refreshing the list of available wifi networks
				refresh_networks(server);
			}
			else if (strcmp(server->ssids[server->SsidPosition], "Cancel") == 0) {
				///fprintf(stderr, "Cancelled\n");
				server->buff[0] = '\0';
				refresh_networks(server);
			}
		}
	}
	// Clicking on windowlist
	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, NULL, &output_box);

	if (event->button == BTN_LEFT &&
		!server->titles_clicked &&
		(int)server->cursor->x > (output_box.width - 3) &&
		(int)server->cursor->y < 3) {

		// Opening window list dialog
		///fprintf(stderr, "Windowlist clicked.\n");
		// Window list
		server->titles_clicked = true;
		list_titles(server);
	}
	else if (event->button == BTN_LEFT && server->titles_clicked && server->titles_ly_hovered) {
		// Closing window list dialog
		///fprintf(stderr, "toplevel_info->titles[%d] = %s\n",
		///		server->TitlesPosition,
		///		server->toplevel_info.titles[server->TitlesPosition]);
		server->titles_clicked = false;
		///server->titles_ly_hovered = false;
		struct woodland_view *found_toplevel = NULL;
		if (!wl_list_empty(&server->toplevels)) {
			struct woodland_view *iter;
			wl_list_for_each(iter, &server->toplevels, link) {
				if (iter && strcmp(iter->xdg_toplevel->app_id,
					server->toplevel_info.app_id[server->TitlesPosition]) == 0) {
					found_toplevel = iter;
					///fprintf(stderr, "iter->xdg_toplevel->app_id: %s\n", iter->xdg_toplevel->app_id);
					///fprintf(stderr, "server->toplevel_info.app_id[server->TitlesPosition]: %s\n",
					///		server->toplevel_info.app_id[server->TitlesPosition]);
				}
			}
		}
		// Reset minimized flag to allow it to minimize again after unminimizing
		found_toplevel->minimized = false;
		wlr_scene_node_set_enabled(&found_toplevel->scene_tree->node, true);
		wlr_scene_node_raise_to_top(&found_toplevel->scene_tree->node);
		focus_toplevel(found_toplevel);

		if (server->titles_scene_buffer) {
			wlr_scene_node_destroy(&server->titles_scene_buffer->node);
			server->titles_scene_buffer = NULL;
		}
	}
	else if (event->button == BTN_LEFT && server->titles_clicked && !server->titles_ly_hovered) {
		if (server->titles_scene_buffer) {
			wlr_scene_node_destroy(&server->titles_scene_buffer->node);
			server->titles_scene_buffer = NULL;
		}
		server->titles_clicked = false;
		///fprintf(stderr, "Windowlist closed\n");
	}

	// Menu clicked
	if (event->button == BTN_LEFT &&
		!server->menu_clicked &&
		(int)server->cursor->x < 30 &&
		(int)server->cursor->y > (output_box.height - 30)) {

		// Opening window list dialog
		///fprintf(stderr, "Menu opened.\n");
		// Menu list
		server->menu_clicked = true;
		show_menu(server);
	}
	else if (event->button == BTN_LEFT && server->menu_clicked && server->menu_ly_hovered) {
		// Closing menu list dialog
		///fprintf(stderr, "Closing menu.\n");
		server->menu_clicked = false;
		if (server->menu_scene_buffer) {
			wlr_scene_node_destroy(&server->menu_scene_buffer->node);
			server->menu_scene_buffer = NULL;
		}
		if (!server->menu_scene_buffer) {
			if (server->m_cr) {
				cairo_destroy(server->m_cr);
				server->m_cr = NULL;
			}
			if (server->m_cairo_surface) {
				cairo_surface_flush(server->m_cairo_surface);
				cairo_surface_destroy(server->m_cairo_surface);
				server->m_cairo_surface = NULL;
			}
			if (!server->m_cairo_surface && !server->m_cr) {
				///printf("Running command: %s\n", server->items[server->menuPosition]);
				usleep(1000);
				run_cmd(server->items[server->menuPosition]);
			}
		}
	}
	else if (event->button == BTN_LEFT && server->menu_clicked && !server->menu_ly_hovered) {
		if (server->menu_scene_buffer) {
			wlr_scene_node_destroy(&server->menu_scene_buffer->node);
			server->menu_scene_buffer = NULL;
		}
		server->menu_clicked = false;
		///fprintf(stderr, "Menu closed\n");
	}
	// Switching off display
	else if (event->button == BTN_RIGHT && server->brightness_change && !server->display_is_off) {
		///fprintf(stderr, "Switching off display.\n");
		struct woodland_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			struct wlr_output_state state;
			wlr_output_state_init(&state);
			wlr_output_state_set_enabled(&state, false);
			if (!wlr_output_commit_state(output->wlr_output, &state)) {
				fprintf(stderr, "Display off failed to commit output state\n");
			}
			wlr_output_state_finish(&state);
			wlr_output_schedule_frame(output->wlr_output);
		}
		server->display_is_off = true;
	}
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	 (void)data;
	struct woodland_server *server = wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

/**
 * This function is called every time an output is ready to display a frame,
 * generally at the output's refresh rate (e.g. 60Hz).
 * Retrieve the woodland_output structure from the listener
 */
static void output_frame(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;
	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);

	// Render the scene if needed and commit the output.
	wlr_scene_output_commit(scene_output, NULL);;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_output *output = wl_container_of(listener, output, destroy);
	WL_LIST_SAFE_REMOVE(&output->frame.link);
	WL_LIST_SAFE_REMOVE(&output->request_state.link);
	WL_LIST_SAFE_REMOVE(&output->destroy.link);
	WL_LIST_SAFE_REMOVE(&output->link);
	free(output);
}

static void output_request_state(struct wl_listener *listener, void *data) {
	/* This function is called when the backend requests a new state for
	 * the output. For example, Wayland and xx11 backends request a new mode
	 * when the output window is resized. */
	(void)data;
	struct woodland_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	(void)data;
	struct wlr_output *wlr_output = data;
	if ((!wlr_output) || (wlr_output == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'wlr_output' in 'server_new_output'!");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, new_output);
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'server' in 'server_new_output'!");
		return;
	}
	/* Configures the output created by the backend to use our allocator
	 * and our renderer */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);
		/* The output may be disabled, switch it on. */
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	/* Some backends don't have modes. DRM+KMS does, and we need to set a mode
	 * before we can use the output. The mode is a tuple of (width, height,
	 * refresh rate), and each monitor supports only a specific set of modes. We
	 * just pick the monitor's preferred mode, a more sophisticated compositor
	 * would let the user configure it. */
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	// Implementing gray background
	struct wlr_scene *scene = server->scene;
	float bg_color[4] = { 0.2, 0.2, 0.2, 1.0 }; // dark gray background
	wlr_scene_rect_create(&scene->tree, wlr_output->width, wlr_output->height, bg_color);

	/* Atomically applies the new output state. */
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
	/* Allocates and configures our state for this output */
	struct woodland_output *output = calloc(1, sizeof(struct woodland_output));
	if (output == NULL) {
		wlr_log(WLR_ERROR, "Error: Failed to allocate memory for woodland_output!");
		return;
	}
	output->wlr_output = wlr_output;
	output->server = server;

	/* Sets up a listener for the frame event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* Sets up a listener for the state request event. */
	output->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	/* Sets up a listener for the destroy event. */
	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);

	output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, l_output, output->scene_output);

	// updated the output layout connection:
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(server->seat, caps);
}

static void begin_interactive(struct woodland_view *toplevel,
							enum woodland_cursor_mode mode,
							uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct woodland_server *server = toplevel->server;

	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;

	if (mode == WOODLAND_CURSOR_MOVE) {
		server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
		server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
	}
	else {
		struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->current.geometry;

		double border_x = (toplevel->scene_tree->node.x + geo_box->x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
		double border_y = (toplevel->scene_tree->node.y + geo_box->y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = *geo_box;
		server->grab_geobox.x += toplevel->scene_tree->node.x;
		server->grab_geobox.y += toplevel->scene_tree->node.y;

		server->resize_edges = edges;
	}
}

// newly added // Add this helper function
static void normalize_resize_edges(struct woodland_server *server) {
	if ((server->resize_edges & (WLR_EDGE_LEFT|WLR_EDGE_RIGHT)) == 
								(WLR_EDGE_LEFT|WLR_EDGE_RIGHT)) {
		server->resize_edges &= ~(WLR_EDGE_LEFT|WLR_EDGE_RIGHT);
	}
	if ((server->resize_edges & (WLR_EDGE_TOP|WLR_EDGE_BOTTOM)) == 
								(WLR_EDGE_TOP|WLR_EDGE_BOTTOM)) {
		server->resize_edges &= ~(WLR_EDGE_TOP|WLR_EDGE_BOTTOM);
	}
}

/**
 ******************* XDG Toplevel, Foreign toplevel and Popups management *******************
 */
/* Get user defined window placement coordinates from wooldand.ini config */
static void get_window_placement(char *file, char *ids[], char *identifiers[], int x[], int y[]) {
	FILE *fp = fopen(file, "r");
	if (fp == NULL) {
		wlr_log(WLR_ERROR, "Could not open file %s", file);
		return;
	}
	char line[1024];
	int count = 0;
	while (fgets(line, sizeof(line), fp) && count < 1024) {
		// Ignore comments
		if (line[0] == '#') {
			continue;
		}
		// Look for lines that start with 'window_place'
		if (strncmp(line, "window_place", 12) == 0) {
			// Find the '=' sign
			char *equal_sign = strchr(line, '=');
			if (equal_sign == NULL) {
				continue;
			}
			// Skip past '=' and any spaces
			char *data = equal_sign + 1;
			while (isspace(*data)) {
				data++;
			}
			// Parse the id ('app_id:' or 'title:')
			char *id_start = data;
			while (*data && !isspace(*data)) {
				data++;
			}
			*data = '\0';
			ids[count] = strdup(id_start);
			data++;
			// Skip spaces
			while (isspace(*data)) {
				data++;
			}
			// Parse the identifier (enclosed in double quotes if present)
			char *identifier_start;
			char *identifier_end;
			if (*data == '"') {
				identifier_start = data + 1;
				identifier_end = strchr(identifier_start, '"');
				if (identifier_end == NULL) {
					free(ids[count]);
					continue;
				}
			}
			else {
				identifier_start = data;
				identifier_end = data;
				while (*identifier_end && !isspace(*identifier_end)) {
					identifier_end++;
				}
			}
			*identifier_end = '\0';
			identifiers[count] = strdup(identifier_start);
			data = identifier_end + 1;
			// Skip spaces
			while (isspace(*data)) {
				data++;
			}
			// Parse the x and y coordinates
			char *x_str = data;
			while (*data && !isspace(*data)) {
				data++;
			}
			*data = '\0';
			char *y_str = data + 1;
			while (*data && !isspace(*data)) {
				data++;
			}
			*data = '\0';
			if (x_str == NULL || y_str == NULL) {
				free(ids[count]);
				free(identifiers[count]);
				continue;
			}
			x[count] = atoi(x_str);
			y[count] = atoi(y_str);
			count++;
		}
	}
	fclose(fp);
}

/**
 * Handle activation of a foreign toplevel.
 *
 * @param listener The listener that triggered this function.
 * @param data The event data.
 */
static void handle_activate(struct wl_listener *listener, void *data) {
	// Get the event and toplevel from the listener and data
	struct wlr_foreign_toplevel_handle_v1_activated_event *event = data;
	if (!event) {
		wlr_log(WLR_ERROR, "Activation failed: Missing event data");
		return;
	}

	struct woodland_view *toplevel = wl_container_of(listener, toplevel, request_activate);
	if (!toplevel) {
		wlr_log(WLR_ERROR, "Activation failed: Missing toplevel");
		return;
	}

	// Check if the event's toplevel is valid
	if (!event->toplevel) {
		wlr_log(WLR_ERROR, "Activation failed: Invalid foreign handle");
		return;
	}

	// Check if the toplevel's foreign handle matches the event's toplevel
	if (toplevel->foreign_handle != event->toplevel) {
		wlr_log(WLR_ERROR, "Handle mismatch: %p (expected) vs %p (actual)", toplevel->foreign_handle,
																			event->toplevel);
		// Find the correct toplevel based on the event's toplevel
		struct woodland_view *correct_toplevel = NULL;
		struct woodland_view *tmp_toplevel;
		wl_list_for_each(tmp_toplevel, &toplevel->server->toplevels, link) {
			if (tmp_toplevel->foreign_handle == event->toplevel) {
				correct_toplevel = tmp_toplevel;
				break;
			}
		}
		// If no matching toplevel is found, log an error and return
		if (!correct_toplevel) {
			wlr_log(WLR_ERROR, "Failed to find toplevel for handle %p", event->toplevel);
			return;
		}
		// Update the toplevel to the correct one
		toplevel = correct_toplevel;
	}

	// Focus the window and bring it to front
	focus_toplevel(toplevel);

	// Update foreign handle state
	wlr_foreign_toplevel_handle_v1_set_activated(event->toplevel, true);

	if (toplevel->xdg_toplevel) {
		wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	}
	else {
		wlr_log(WLR_ERROR, "Missing XDG toplevel for handle %p", event->toplevel);
	}
	wlr_log(WLR_ERROR, "Successfully activated XDG toplevel");
}

static void handle_close(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, request_close);
	if (!toplevel) {
		wlr_log(WLR_ERROR, "Close request failed: No toplevel found");
		return;
	}
	wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
	struct woodland_server *server = toplevel->server;

	if (!toplevel->xdg_toplevel->base->initialized) {
		return;
	}

	// Toggle fullscreen state
	bool fullscreen = !toplevel->fullscreened;
	toplevel->fullscreened = fullscreen;

	if (fullscreen) {
		// Save current geometry
		toplevel->saved_geometry.x = toplevel->scene_tree->node.x;
		toplevel->saved_geometry.y = toplevel->scene_tree->node.y;
		toplevel->saved_geometry.width = toplevel->xdg_toplevel->current.width;
		toplevel->saved_geometry.height = toplevel->xdg_toplevel->current.height;

		// Set fullscreen state and new size
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
								server->transformed_width,
								server->transformed_height);
		wlr_scene_node_set_position(&toplevel->scene_tree->node, 0, 0);
	}
	else {
		// Restore original state
		wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, false);
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
								toplevel->saved_geometry.width,
								toplevel->saved_geometry.height);
		wlr_scene_node_set_position(&toplevel->scene_tree->node,
									toplevel->saved_geometry.x,
									toplevel->saved_geometry.y);
	}

	// Send configure event immediately
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);

	// Force immediate redraw
	struct woodland_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_scene_output_commit(output->scene_output, NULL);
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		wlr_scene_output_send_frame_done(output->scene_output, &now);
	}
}

static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_minimize_event *event = data;
	(void)event;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, request_minimize);
	if (toplevel && !toplevel->minimized) {
		///fprintf(stderr, "Minimizing toplevel\n");
		// Hide from scene graph
		wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);

		// Mark as minimized
		toplevel->minimized = true;
	}
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, request_resize);

	if (toplevel) {
		// newly added
		toplevel->resized = true;
		toplevel->server->resize_edges = event->edges;
		normalize_resize_edges(toplevel->server);
		begin_interactive(toplevel, WOODLAND_CURSOR_RESIZE, toplevel->server->resize_edges);
	}
}

/* This event is raised when a client would like to begin an interactive
 * move, typically because the user clicked on their client-side
 * decorations. Note that a more sophisticated compositor should check the
 * provided serial against a list of button press serials sent to this
 * client, to prevent the client from requesting this whenever they want.
 */
static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, WOODLAND_CURSOR_MOVE, 0);
}

static void handle_toplevel_set_title(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, set_title);
	///fprintf(stderr, "title: %s\n", view->xdg_toplevel->title);
	// Set window title
	if ((!toplevel->xdg_toplevel->title) || (toplevel->xdg_toplevel->title == NULL)) {
		toplevel->xdg_toplevel->title = "nil";
	}
	if (toplevel->foreign_handle && toplevel->xdg_toplevel->title) {
		wlr_foreign_toplevel_handle_v1_set_title(toplevel->foreign_handle, toplevel->xdg_toplevel->title);
	}
}

static void handle_toplevel_set_app_id(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, set_app_id);
	///fprintf(stderr, "app_id: %s\n", view->xdg_toplevel->app_id);
	if ((!toplevel->xdg_toplevel->app_id) || (toplevel->xdg_toplevel->app_id == NULL)) {
		toplevel->xdg_toplevel->app_id = "nil";
	}
	if (toplevel->foreign_handle && toplevel->xdg_toplevel->app_id) {
		wlr_foreign_toplevel_handle_v1_set_app_id(toplevel->foreign_handle, toplevel->xdg_toplevel->app_id);
	}
	if (toplevel->xdg_toplevel->app_id) {
		toplevel->app_id = strdup(toplevel->xdg_toplevel->app_id);
	}
}

/**
 * Handles the XDG toplevel map event.
 * 
 * This function is called when an XDG toplevel surface is mapped.
 * It sets up the necessary state for the toplevel, including its position,
 * foreign toplevel handle, and event listeners.
 */
static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	(void)data;
	// Get the toplevel and server from the listener
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, map);
	struct woodland_server *server = toplevel->server;

	// Sanity check: Ensure this is an actual XDG toplevel before continuing
	if (!toplevel->xdg_toplevel) {
		wlr_log(WLR_ERROR, "xdg_toplevel_map: Skipping non-xdg_toplevel surface");
		return;
	}

	// Log the mapping of the XDG toplevel
	wlr_log(WLR_DEBUG, "Mapping XDG Toplevel: %p (title: %s)", toplevel, toplevel->xdg_toplevel->title);

	// Insert the toplevel into the list of managed windows
	wl_list_insert(&server->toplevels, &toplevel->link);

	// Create Foreign Toplevel Handle (Only if Foreign Toplevel Management is active)
	if (server->toplevel_manager) {
		// Create a new foreign toplevel handle
		toplevel->foreign_handle = wlr_foreign_toplevel_handle_v1_create(server->toplevel_manager);
		if (!toplevel->foreign_handle) {
			wlr_log(WLR_ERROR, "Failed to create foreign toplevel handle");
			// Remove the toplevel from the list of managed windows
			WL_LIST_SAFE_REMOVE(&toplevel->link);
			return;
		}

		// Set window properties
		if (toplevel->xdg_toplevel->title) {
			wlr_foreign_toplevel_handle_v1_set_title(toplevel->foreign_handle,
													 toplevel->xdg_toplevel->title);
		}
		if (toplevel->xdg_toplevel->app_id) {
			wlr_foreign_toplevel_handle_v1_set_app_id(toplevel->foreign_handle,
													toplevel->xdg_toplevel->app_id);
		}

	}
	else {
		wlr_log(WLR_DEBUG, "Skipping foreign toplevel handle creation (manager not initialized)");
	}

	// Assign window to output
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
															toplevel->scene_tree->node.x,
															toplevel->scene_tree->node.y
	);
	if (output) {
		// Enter the output for the foreign toplevel handle
		if (toplevel->foreign_handle) {
			wlr_foreign_toplevel_handle_v1_output_enter(toplevel->foreign_handle, output);
		}

		// Default placement
		// Get the output's layout
		struct wlr_box output_box;
		wlr_output_layout_get_box(server->output_layout, NULL, &output_box);

		// Get the transformed resolution of the output
		///int width = output_box.width;
		///int height = output_box.height;

		// Get only transformed resolution because when scaling is applied
		// all newly opened applications are being resized
		int width = server->transformed_width;
		int height = server->transformed_height;

		// Calculate the position to center the window
		struct wlr_box *geo = &toplevel->xdg_toplevel->base->current.geometry;
		int window_width = geo->width;
		int window_height = geo->height;

		double x = 0;
		double y = 0;
		if (window_width == 0 || window_height == 0) {
			// Use a default size for windows with width = 0 and height = 0 geometry
			window_width = 700; // Magic number to place the window right in the center
			window_height = 300;
			x = output_box.x + (width / 2.0) - (window_width / 2.0);
			y = output_box.y + (height / 2.0) - (window_height / 2.0);
		}
		else {
			// Default center placement for other normal windows
			x = output_box.x + (width / 2.0) - (window_width / 2.0);
			y = output_box.y + (height / 2.0) - (window_height / 2.0);
		}

		// If the window width or height exceeds the screen geometry then resize to fit the screen	
		struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->current.geometry;;
		if (geo_box->width > output->width) {
			wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, output->width, geo_box->height);
		}
		if (geo_box->height > output->height) {
			wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, geo_box->width, output->height);
		}

		// Set the new window position
		wlr_scene_node_set_position(&toplevel->scene_tree->node, (int)round(x), (int)round(y));

		// ******************* automatic window placement ******************* //
		// Get the scene node for the view
		struct wlr_scene_node *node = &toplevel->scene_tree->node;

		// Executing window placement
		const char *title = NULL;
		const char *app_id = NULL;
		title = toplevel->xdg_toplevel->title;
		if ((!title) || (title == NULL)) {
			title = "nil";
		}
		app_id = toplevel->xdg_toplevel->app_id;
		if ((!app_id) || (app_id == NULL)) {
			app_id = "nil";
		}

		// Executing window placement
		char *ids[1024] = {0};
		char *identifiers[1024] = {0};
		int x_arr[1024] = {0};
		int y_arr[1024] = {0};

		// Gets all the titles or app_id of windows in woodland.ini marked for user defined placement
		// ids - is a char array containing the prefixes keywords (either keyword 'title:' or 'app_id:'
		// identifiers - is a char array containing the actual window title or app_id
		// x_arr and y_arr - char arrays containing x and y coordinates of windows to be placed
		get_window_placement(toplevel->server->config, ids, identifiers, x_arr, y_arr);

		// If 'surface->current.committed' == WLR_SURFACE_STATE_BUFFER it lets us know that
		// the client required a toplevel move or resize and we can use this information
		// to filter which windows should be let to use the client required coordinates
		// and which windows should be always placed in center.
		bool clientRequiredPlacement = false;
		if (toplevel->xdg_toplevel->base->surface->current.committed == WLR_SURFACE_STATE_BUFFER) {
			clientRequiredPlacement = true;
		}

		if (title != NULL && app_id == NULL) {
			for (int i = 0; i < 1024 && ids[i] != NULL; i++) {
				if (strcmp(ids[i], "title:") == 0) {
				    // If this title is found in woodland.ini for automatic placement
				    if (strcmp(identifiers[i], title) == 0) {
				        wlr_scene_node_set_position(node, x_arr[i], y_arr[i]);
				        break;
				    }
				    if (strcmp(identifiers[i], title) != 0 && clientRequiredPlacement && \
				                                        geo_box->x != 0 && geo_box->y != 0) {
				        wlr_scene_node_set_position(node, geo_box->x, geo_box->y);
				        break;
				    }
				}
				// Free resources
				if (ids[i] != NULL) {
				    free(ids[i]);
				    ids[i] = NULL;
				}
				if (identifiers[i] != NULL) {
				    free(identifiers[i]);
				    identifiers[i] = NULL;
				}
			}
		}
		else if (title == NULL && app_id != NULL) {
			for (int i = 0; i < 1024 && ids[i] != NULL; i++) {
				if (strcmp(ids[i], "app_id:") == 0) {
				    if (strcmp(identifiers[i], app_id) == 0) {
				        wlr_scene_node_set_position(node, x_arr[i], y_arr[i]);
				        break;
				    }
				    if (strcmp(identifiers[i], app_id) != 0 && clientRequiredPlacement && \
				                                        geo_box->x != 0 && geo_box->y != 0) {
				        wlr_scene_node_set_position(node, geo_box->x, geo_box->y);
				        break;
				    }
				}
				// Free resources
				if (ids[i] != NULL) {
				    free(ids[i]);
				    ids[i] = NULL;
				}
				if (identifiers[i] != NULL) {
				    free(identifiers[i]);
				    identifiers[i] = NULL;
				}
			}
		}
		else if (title != NULL && app_id != NULL) {
			for (int i = 0; i < 1024 && ids[i] != NULL; i++) {
				// Set position for windows titles
				if (strcmp(ids[i], "app_id:") == 0) {
				    if (strcmp(identifiers[i], app_id) == 0) {
				        wlr_scene_node_set_position(node, x_arr[i], y_arr[i]);
				        break;
				    }
				    if (strcmp(identifiers[i], app_id) != 0 && clientRequiredPlacement && \
				                                        geo_box->x != 0 && geo_box->y != 0) {
				        wlr_scene_node_set_position(node, geo_box->x, geo_box->y);
				        break;
				    }
				}
				else if (strcmp(ids[i], "title:") == 0) {
				    if (strcmp(identifiers[i], title) == 0) {
				        wlr_scene_node_set_position(node, x_arr[i], y_arr[i]);
				        break;
				    }
				    if (strcmp(identifiers[i], title) != 0 && clientRequiredPlacement && \
				                                        geo_box->x != 0 && geo_box->y != 0) {
				        wlr_scene_node_set_position(node, geo_box->x, geo_box->y);
				        break;
				    }
				}
				// Free resources
				if (ids[i] != NULL) {
				    free(ids[i]);
				    ids[i] = NULL;
				}
				if (identifiers[i] != NULL) {
				    free(identifiers[i]);
				    identifiers[i] = NULL;
				}
			}
		}
		// ******************* Finished automatic window placement ******************* //
		// Finalize window creation
		wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);

		// Initialize listeners
		toplevel->request_activate.notify = handle_activate;
		wl_signal_add(&toplevel->foreign_handle->events.request_activate, &toplevel->request_activate);

		toplevel->request_close.notify = handle_close;
		wl_signal_add(&toplevel->foreign_handle->events.request_close, &toplevel->request_close);

		// Set the toplevel as resizing as a workaround for scale modifying the size of some toplevels
		wlr_xdg_toplevel_set_resizing(toplevel->xdg_toplevel, true);

		// Focus the toplevel
		focus_toplevel(toplevel);
	}
	else {
		wlr_log(WLR_ERROR, "xdg_toplevel_map: Failed to assign output for %s", toplevel->xdg_toplevel->title);
		// Remove the toplevel from the list of managed windows
		WL_LIST_SAFE_REMOVE(&toplevel->link);
		if (toplevel->foreign_handle) {
			wlr_foreign_toplevel_handle_v1_destroy(toplevel->foreign_handle);
			toplevel->foreign_handle = NULL;
		}
	}
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, unmap);
	struct woodland_server *server = toplevel->server;
	toplevel->minimized = false;

	// Remove from list first to prevent re-tiling logic from seeing this window
	WL_LIST_SAFE_REMOVE(&toplevel->link);

	if (toplevel->foreign_handle) {
		// Remove listeners first to prevent dangling pointers
		WL_LIST_SAFE_REMOVE(&toplevel->request_activate.link);
		WL_LIST_SAFE_REMOVE(&toplevel->request_close.link);
		
		// Destroy the foreign toplevel handle
		wlr_foreign_toplevel_handle_v1_destroy(toplevel->foreign_handle);
		toplevel->foreign_handle = NULL;
		toplevel->foreign_handle = NULL; // Critical NULL assignment
	}

	struct wlr_box layout_box;
	wlr_output_layout_get_box(server->output_layout, NULL, &layout_box);

	int window_count = 0;
	struct woodland_view *win;
	wl_list_for_each(win, &server->toplevels, link) window_count++;

	struct woodland_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_scene_output_commit(output->scene_output, NULL);
	}

	//error-chance, z-added
	if (toplevel == server->grabbed_toplevel) {
		reset_cursor_mode(server);
	}
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	(void)data;
	struct woodland_view *toplevel = wl_container_of(listener, toplevel, commit);

	if (toplevel->xdg_toplevel->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface. Woodland
		 * configures the first time opened xdg_toplevel with 0,0 size to let
		 * the client pick the dimensions itself or if the toplevel has been
		 * previously opened then it applies the last time saved width and height. */
		///wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
		char app_id_width[128];
		char app_id_height[128];
		snprintf(app_id_width, sizeof(app_id_width), "%s_width", toplevel->app_id);
		snprintf(app_id_height, sizeof(app_id_height), "%s_height", toplevel->app_id);
		///fprintf(stderr, "app_id_width: %s\n", app_id_width);
		///fprintf(stderr, "app_id_height: %s\n", app_id_height);
		int LastToplevelWidth = get_int_value_from_conf(toplevel->server->config_sizes, app_id_width);
		int LastToplevelHeight = get_int_value_from_conf(toplevel->server->config_sizes, app_id_height);

		/* If the toplevel is firt time opened then it has no records in 'windows_sizes.db
		 * and 'get_int_value_from_conf' will not find its app_id and will return 1
		 * wrongly applying width = 1 and height = 1, that's why we need to set all
		 * the initial first time opened toplevels width and height to 0. Setting it
		 * to 0 let's the toplevels apply their own size. */
		if (LastToplevelWidth == 1) {
			LastToplevelWidth = 0;
		}
		if (LastToplevelHeight == 1) {
			LastToplevelHeight = 0;
		}

		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, LastToplevelWidth, LastToplevelHeight);

		// Set the toplevel as resizing as a workaround for scale modifying the size of some toplevels
		wlr_xdg_toplevel_set_resizing(toplevel->xdg_toplevel, true);
	}
}

/**
 * Called when an xdg_toplevel is destroyed.
 *
 * This function handles the cleanup and focuses the previous toplevel in the stack.
 */
static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	// Ignore the data parameter
	(void)data;
	// Get the woodland_view associated with the listener
	if (!listener) {
		wlr_log(WLR_ERROR, "Invalid listener in xdg_toplevel_destroy");
		return;
	}

	struct woodland_view *toplevel = wl_container_of(listener, toplevel, destroy);
	if (!toplevel) {
		wlr_log(WLR_ERROR, "Failed to get woodland_view from listener");
		return;
	}

	// Check if the toplevel has a valid server
	if (!toplevel->server) {
		wlr_log(WLR_ERROR, "Toplevel has no valid server");
		return;
	}

	// Save toplevel size before closing
	struct wlr_box toplevel_box;
	wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &toplevel_box);
	///fprintf(stderr, "%s_width = %d\n", toplevel->app_id, toplevel_box.width);
	///fprintf(stderr, "%s_height = %d\n", toplevel->app_id, toplevel_box.height);

	// First check is there is any change in this toplevel size since last time
	bool WriteNewSizes = false;
	char app_id_width[128];
	char app_id_height[128];
	snprintf(app_id_width, sizeof(app_id_width), "%s_width", toplevel->app_id);
	snprintf(app_id_height, sizeof(app_id_height), "%s_height", toplevel->app_id);
	///fprintf(stderr, "app_id_width: %s\n", app_id_width);
	///fprintf(stderr, "app_id_height: %s\n", app_id_height);
	int LastToplevelWidth = get_int_value_from_conf(toplevel->server->config_sizes, app_id_width);
	int LastToplevelHeight = get_int_value_from_conf(toplevel->server->config_sizes, app_id_height);
	///fprintf(stderr, "LastToplevelWidth: %d\n", LastToplevelWidth);
	///fprintf(stderr, "LastToplevelHeight: %d\n", LastToplevelHeight);

	// If there are any size changes then remove those old lines
	if ((LastToplevelWidth != toplevel_box.width) || (LastToplevelHeight != toplevel_box.height)) {
		fprintf(stderr, "New toplevel size doesn't match the last saved size, removing old lines.\n");
		if (toplevel->resized) {
			WriteNewSizes = true;
			remove_given_text_line_from_conf(toplevel->server->config_sizes, app_id_width);
			remove_given_text_line_from_conf(toplevel->server->config_sizes, app_id_height);
		}
	}
	else {
		fprintf(stderr, "New toplevel size matches the last saved size, skipping.\n");
		WriteNewSizes = false;
	}

	// And now get new size values
	if (toplevel->resized && WriteNewSizes) {
		fprintf(stderr, "Writing new sizes for: %s to config.\n", toplevel->app_id);
		// write toplevel width to server.config_sizes
		FILE *config_winsizes = fopen(toplevel->server->config_sizes, "a+");
		if (config_winsizes == NULL) {
			perror("fopen");
			return;
		}
		// write toplevel width and height to server.config_sizes
		fprintf(config_winsizes, "%s_width = %d\n", toplevel->app_id, toplevel_box.width);
		fprintf(config_winsizes, "%s_height = %d\n", toplevel->app_id, toplevel_box.height);
		fclose(config_winsizes);
	}
	// Find the previous view to focus
	bool focus_surface = false;
	struct woodland_view *prev_view = NULL;
	if (!wl_list_empty(&toplevel->server->toplevels)) {
		struct woodland_view *iter;
		wl_list_for_each_reverse(iter, &toplevel->server->toplevels, link) {
			// Skip the current toplevel and check for NULL
			if (iter && iter != toplevel) {
				prev_view = iter;
			}
		}
	}

	if (prev_view && prev_view != toplevel) {
		struct wlr_surface *prev_surface = prev_view->xdg_toplevel->base->surface;
		if (prev_surface && prev_surface != toplevel->xdg_toplevel->base->surface) {
			focus_surface = true;
		}
	}

	// Check if we found a valid previous view
	if (focus_surface && prev_view && \
		prev_view != toplevel && \
		prev_view->xdg_toplevel && \
		prev_view->xdg_toplevel->base) {
		// Check if the previous view has a valid surface
		struct wlr_surface *prev_surface = prev_view->xdg_toplevel->base->surface;
		if (prev_surface && prev_surface != toplevel->xdg_toplevel->base->surface) {
			// Focus the previous surface
			wlr_log(WLR_INFO, "Activating previous surface: %p", prev_view->xdg_toplevel->base);
			focus_toplevel(prev_view);
		}
		else {
			wlr_log(WLR_ERROR, "Previous surface is not valid");
		}
	}
	else {
		wlr_log(WLR_INFO, "No previous surface to focus");
	}
	if (toplevel->app_id) {
		free(toplevel->app_id);
		toplevel->app_id = NULL;
	}
	// Remove the toplevel from all the lists
	WL_LIST_SAFE_REMOVE(&toplevel->map.link);
	WL_LIST_SAFE_REMOVE(&toplevel->unmap.link);
	WL_LIST_SAFE_REMOVE(&toplevel->commit.link);
	WL_LIST_SAFE_REMOVE(&toplevel->destroy.link);
	WL_LIST_SAFE_REMOVE(&toplevel->set_title.link);
	WL_LIST_SAFE_REMOVE(&toplevel->set_app_id.link);
	WL_LIST_SAFE_REMOVE(&toplevel->request_move.link);
	WL_LIST_SAFE_REMOVE(&toplevel->request_resize.link);
	WL_LIST_SAFE_REMOVE(&toplevel->request_minimize.link);
	WL_LIST_SAFE_REMOVE(&toplevel->request_fullscreen.link);
	// Free the toplevel
	if (toplevel) {
		free(toplevel);
		toplevel = NULL;
	}
	wlr_log(WLR_INFO, "XDG Toplevel destroyed successfully!");
}

/**
 * Called when an xdg_toplevel is created.
 *
 * Whenever user launches a new application, this function is called.
 */
static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	/* This event is raised when a client creates a new toplevel (application window). */
	(void)data;
	struct woodland_server *server = wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	/* Allocate a woodland_view for this surface */
	struct woodland_view *toplevel = calloc(1, sizeof(*toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;
	toplevel->scene_tree = wlr_scene_xdg_surface_create(&toplevel->server->scene->tree, xdg_toplevel->base);
	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;

	/* Listen to the various events it can emit */
	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);

	toplevel->request_minimize.notify = xdg_toplevel_request_minimize;
	wl_signal_add(&xdg_toplevel->events.request_minimize, &toplevel->request_minimize);

	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);

	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);

	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

	// Add listeners for set_title and set_app_id events
	toplevel->set_title.notify = handle_toplevel_set_title;
	wl_signal_add(&xdg_toplevel->events.set_title, &toplevel->set_title);

	toplevel->set_app_id.notify = handle_toplevel_set_app_id;
	wl_signal_add(&xdg_toplevel->events.set_app_id, &toplevel->set_app_id);
}

 /**
 * XDG Popup management
 */
static void xdg_popup_commit(struct wl_listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	(void)data;
	struct woodland_popup *popup = wl_container_of(listener, popup, commit);

	if (popup->xdg_popup->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface.
		 * tinywl sends an empty configure. A more sophisticated compositor
		 * might change an xdg_popup's geometry to ensure it's not positioned
		 * off-screen, for example. */
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

static void xdg_popup_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_popup is destroyed. */
	(void)data;
	struct woodland_popup *popup = wl_container_of(listener, popup, destroy);
	WL_LIST_SAFE_REMOVE(&popup->commit.link);
	WL_LIST_SAFE_REMOVE(&popup->destroy.link);
	free(popup);
}

/* We must add xdg popups to the scene graph so they get rendered. The
 * wlroots scene graph provides a helper for this, but to use it we must
 * provide the proper parent scene node of the xdg popup. To enable this,
 * we always set the user data field of xdg_surfaces to the corresponding
 * scene node.
 */
static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
	// Check for NULL listener and data
	if (!listener || !data) {
		return;
	}

	// This event is raised when a client creates a new popup.
	struct wlr_xdg_popup *xdg_popup = data;
	struct woodland_server *server = wl_container_of(listener, server, new_xdg_popup);

	// Check if server or xdg_popup is NULL
	if (!server || !xdg_popup) {
		return;
	}

	// Allocate memory for the new popup
	struct woodland_popup *popup = calloc(1, sizeof(*popup));
	if (!popup) {
		// Handle memory allocation failure
		return;
	}

	popup->xdg_popup = xdg_popup;

	// Get the parent surface and its scene tree
	struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
	if (!parent) {
		free(popup);
		return;
	}

	struct wlr_scene_tree *parent_tree = parent->data;
	if (!parent_tree) {
		free(popup);
		return;
	}

	// Create a new scene surface for the popup
	xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);
	if (!xdg_popup->base->data) {
		free(popup);
		return;
	}

	// Calculate screen coordinates and place popups within screen resolution
	// and prevent popups to go beyond screen boundaries
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
															server->cursor->x,
															server->cursor->y);

	if (output) {
		///struct wlr_box output_box;
		///wlr_output_layout_get_box(server->output_layout, output, &output_box);
		int width = server->transformed_width;
		int height = server->transformed_height;

		struct wlr_box box;
		box.x = 0;
		box.y = 0;
		box.width = width;
		box.height = height;
		///box.width = output_box.width;
		///box.height = output_box.height;
		wlr_xdg_popup_unconstrain_from_box(xdg_popup, &box);
	}

	// Add commit signal handler
	popup->commit.notify = xdg_popup_commit;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

	// Add destroy signal handler
	popup->destroy.notify = xdg_popup_destroy;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

/* Run a terminal at startup of no startup command specified */
// Function to find and open the first available terminal emulator
static void startup_terminal(void) {
	char *terminals[] = {"foot", "xfce4-terminal", "kitty", "gnome-terminal", "alacritty"};
	char *bin_paths[] = {"/usr/bin/", "/usr/local/bin/"};
	int num_terminals = sizeof(terminals) / sizeof(terminals[0]);
	int num_paths = sizeof(bin_paths) / sizeof(bin_paths[0]);

	for (int i = 0; i < num_terminals; ++i) {
		for (int j = 0; j < num_paths; ++j) {
			char terminal_path[256];
			snprintf(terminal_path, sizeof(terminal_path), "%s%s", bin_paths[j], terminals[i]);
			// Check if the terminal executable exists
			if (access(terminal_path, X_OK) != -1) {
				// Open the terminal using run_cmd
				run_cmd(terminals[i]);
				return; // Exit the function once the terminal is opened
			}
		}
	}
	wlr_log(WLR_ERROR, "No supported terminal emulators found.");
	wlr_log(WLR_ERROR, "You need to start woodland with an explicit command.");
	wlr_log(WLR_ERROR, "Example: woodland -s appname");
}

/* Processing startup commands */
// Function to trim spaces and other whitespace characters from the start and end of a string
static char *trim(char *str) {
	char *start = str;
	char *end = str + strlen(str) - 1;
	// Trim leading whitespace characters
	while (isspace((unsigned char)*start)) {
		start++;
	}
	// Trim trailing whitespace characters
	while (end > start && isspace((unsigned char)*end)) {
		*end-- = '\0';
	}
	return start;
}

// Function to process startup commands from the configuration file
static int process_startup_commands(void *data) {
	struct woodland_server *server = data;
	char *config = server->config;
	char command[MAX_NR_OF_STARTUP_COMMANDS][1024];
	int num_commands = 0;

	FILE *file = fopen(config, "r");
	if (file == NULL) {
		perror("Error opening file");
		return 1;
	}

	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		// Ignore comments and empty lines
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}

		// Check for lines starting with 'startup_command'
		if (strstr(line, "startup_command") != NULL) {
			// Split by '='
			char *token = strtok(line, "=");
			token = strtok(NULL, "=");
			if (token != NULL) {
				// Trim leading and trailing spaces from the command
				char *trimmed_command = trim(token);
				if (strlen(trimmed_command) > 0) {
					strcpy(command[num_commands], trimmed_command);
					num_commands++;
					if (num_commands >= MAX_NR_OF_STARTUP_COMMANDS) {
						break; // Avoid overflow
					}
				}
			}
		}
	}
	fclose(file);
	if (num_commands == 0) {
		// If no commands are specified, launch the default terminal
		wlr_log(WLR_INFO, "No startup commands specified. Launching default terminal.");
		startup_terminal();
	}
	else {
		// Execute each command from the array
		for (int i = 0; i < num_commands; ++i) {
			wlr_log(WLR_INFO, "Launching command: %s", command[i]);
			run_cmd(command[i]);
		}
	}
	wl_event_source_remove(server->autostart_timer);

	// If compositor is shut down sooner than 7 seconds then remove this timer
	server->autostart_cmd_ran = true;
	return 0;
}

/* Background picture setup */
static void background_setup(struct woodland_server *server,
							struct wlr_output *output,
							char **background_img) {
	// Display the background picture
	int channels = 0;
	int output_width = server->transformed_width;
	int output_height = server->transformed_height;

	if (!output) {
		wlr_log(WLR_ERROR, "Failed to get output at cursor position");
		return;
	}

	struct wlr_drm_format format = {
		.format = DRM_FORMAT_ARGB8888,
		.len = 1,
		.capacity = 1,
		.modifiers = (uint64_t[]) { DRM_FORMAT_MOD_LINEAR },
	};

	struct wlr_buffer *wlr_buffer = wlr_allocator_create_buffer(server->allocator,
			                                                   output_width,
			                                                   output_height,
			                                                   &format);
	if (!wlr_buffer) {
		wlr_log(WLR_ERROR, "Failed to create buffer");
		free(*background_img);
		*background_img = NULL;
		return;
	}

	server->background_scene_buffer = wlr_scene_buffer_create(&server->scene->tree, wlr_buffer);
	if (!server->background_scene_buffer) {
		wlr_log(WLR_ERROR, "Failed to create scene buffer");
		wlr_buffer_drop(wlr_buffer);
		free(*background_img);
		*background_img = NULL;
		return;
	}

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(server->scene, output);
	if (!scene_output) {
		wlr_log(WLR_ERROR, "Failed to get scene output");
		wlr_buffer_drop(wlr_buffer);
		free(*background_img);
		*background_img = NULL;
		return;
	}

	unsigned char *pixels = stbi_load(*background_img,
									&output_width,
									&output_height,
									&channels,
									STBI_rgb_alpha);
	if (!pixels) {
		wlr_log(WLR_ERROR, "Failed to load background image: %s", background_img);
		wlr_buffer_drop(wlr_buffer);
		free(*background_img);
		*background_img = NULL;
		return;
	}

	if (channels != 4) {
		wlr_log(WLR_ERROR, "Background image doesn't have alpha channel");
	}

	server->background_scene_buffer->texture = wlr_texture_from_pixels(server->renderer,
																		DRM_FORMAT_ABGR8888,
																		output_width * 4,
																		output_width,
																		output_height,
																		pixels);

	if (!server->background_scene_buffer->texture) {
		wlr_log(WLR_ERROR, "Failed to create texture from pixels");
		wlr_buffer_drop(wlr_buffer);
		stbi_image_free(pixels);
		free(*background_img);
		*background_img = NULL;
		return;
	}

	stbi_image_free(pixels);
	wlr_buffer_drop(wlr_buffer);
	free(*background_img);
	*background_img = NULL;
}

/**
 ******************** Main function ********************
 */
int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);
	// Create initial configuration files
	create_config();

	// Declaring variables
	char *startup_cmd = NULL;
	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			fprintf(stderr, "Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		fprintf(stderr, "Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	struct woodland_server server = { 0 };
	// Storing full path to config
	const char *HOME = getenv("HOME");
	if (HOME == NULL) {
		wlr_log(WLR_ERROR, "Unable to determine the user's home directory.\n");
		return 1;
	}
	const char *configPath = "/.config/woodland/woodland.ini";
	server.config = malloc(sizeof(char) * strlen(HOME) + strlen(configPath) + 3);
	if (server.config == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate memory for config path.\n");
		return 1;
	}
	snprintf(server.config, strlen(HOME) + strlen(configPath) + 3, "%s%s", HOME, configPath);

	const char *configSizesPath = "/.config/woodland/windows_sizes.db";
	server.config_sizes = malloc(sizeof(char) * strlen(HOME) + strlen(configSizesPath) + 3);
	if (server.config_sizes == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate memory for config sizes path.\n");
		return 1;
	}
	snprintf(server.config_sizes, strlen(HOME) + strlen(configSizesPath) + 3, "%s%s", HOME, configSizesPath);

	const char *IconsPath = "/.config/woodland/icons";

	/// getting icons
	server.volumeHigh = malloc((sizeof(char) * strlen(HOME)) +
					(sizeof(char) * strlen(IconsPath)) +
					(sizeof(char) * strlen("dio-volume-high.svg") + 3));
	snprintf(server.volumeHigh, (sizeof(char) * strlen(HOME)) +
					(sizeof(char) * strlen(IconsPath)) +
					(sizeof(char) * strlen("dio-volume-high.svg") + 3),
					"%s%s/%s",
					HOME,
					IconsPath,
					"dio-volume-high.svg");
	if (!server.volumeHigh) {
		perror("dio-volume-high.svg");
	}

	server.brightnessIcon = malloc((sizeof(char) * strlen(HOME)) +
					(sizeof(char) * strlen(IconsPath)) +
					(sizeof(char) * strlen("dio-volume-high.svg") + 3));
	snprintf(server.brightnessIcon, (sizeof(char) * strlen(HOME)) +
					(sizeof(char) * strlen(IconsPath)) +
					(sizeof(char) * strlen("brightness.svg") + 3),
					"%s%s/%s",
					HOME,
					IconsPath,
					"brightness.svg");
	if (!server.brightnessIcon) {
		perror("brightness.svg");
	}

	server.networkIcon = malloc((sizeof(char) * strlen(HOME)) +
					(sizeof(char) * strlen(IconsPath)) +
					(sizeof(char) * strlen("dio-volume-high.svg") + 3));
	snprintf(server.networkIcon, (sizeof(char) * strlen(HOME)) +
					(sizeof(char) * strlen(IconsPath)) +
					(sizeof(char) * strlen("network.svg") + 3),
					"%s%s/%s",
					HOME,
					IconsPath,
					"network.svg");
	if (!server.networkIcon) {
		perror("network.svg");
	}

	server.tap_enable = get_char_value_from_conf(server.config, "tap_to_click");
	server.play_pause = get_char_value_from_conf(server.config, "play_pause");
	server.volume_up = get_char_value_from_conf(server.config, "volume_up");
	server.volume_down = get_char_value_from_conf(server.config, "volume_down");
	server.volume_mute = get_char_value_from_conf(server.config, "volume_mute");
	server.brightness_path = get_char_value_from_conf(server.config, "d_power_path");
	server.saved_brightness = get_current_brightness(server.brightness_path);

	/* Getting zoom variables */
	server.zoom_factor = 1.0;
	server.zoom_speed = get_double_value_from_conf(server.config, "zoom_speed");
	server.zoom_speed_m = server.zoom_speed;

	/* Getting welcome screen command */
	char *welcome_screen_CMD = get_char_value_from_conf(server.config, "welcome_screen");

	// The Wayland display is managed by libwayland. It handles accepting
	// clients from the Unix socket, manging Wayland globals, and so on.
	server.wl_display = wl_display_create();
	if (!server.wl_display) {
		wlr_log(WLR_ERROR, "Failed to create Wayland display!");
		return 1;
	}

	// The backend is a wlroots feature which abstracts the underlying input and
	// output hardware. The autocreate option will choose the most suitable
	// backend based on the current environment, such as opening an xx11 window
	// if an xx11 server is running.
	server.event_loop = wl_display_get_event_loop(server.wl_display);
	if (!server.event_loop) {
		wlr_log(WLR_ERROR, "Failed to create event_loop!");
		return 1;
	}
	server.backend = wlr_backend_autocreate(server.event_loop, &server.session);
	if (!server.backend) {
		wlr_log(WLR_ERROR, "Failed to create backend!");
		return 1;
	}

	// Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	// can also specify a renderer using the WLR_RENDERER env var.
	// The renderer is responsible for defining the various pixel formats it
	// supports for shared memory, this configures that for clients.
	server.renderer = wlr_renderer_autocreate(server.backend);
	if (!server.renderer) {
		wlr_log(WLR_ERROR, "Failed to create renderer!");
		return 1;
	}
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	// Autocreates an allocator for us.
	// The allocator is the bridge between the renderer and the backend. It
	// handles the buffer creation, allowing wlroots to render onto the screen 
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (!server.allocator) {
		wlr_log(WLR_ERROR, "Failed to create allocator!");
		return 1;
	}

	// This creates some hands-off wlroots interfaces. The compositor is
	// necessary for clients to allocate surfaces, the subcompositor allows to
	// assign the role of subsurfaces to surfaces and the data device manager
	// handles the clipboard. Each of these wlroots interfaces has room for you
	// to dig your fingers in and play with their behavior if you want. Note that
	// the clients cannot set the selection directly without compositor approval,
	// see the handling of the request_set_selection event below.
	server.compositor = wlr_compositor_create(server.wl_display, 5, server.renderer);
	if (!server.compositor) {
		wlr_log(WLR_ERROR, "Failed to create compositor!");
		return 1;
	}
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

	// Creates an output layout, which a wlroots utility for working with an
	// arrangement of screens in a physical layout. */
	server.output_layout = wlr_output_layout_create(server.wl_display);
	if (!server.output_layout) {
		wlr_log(WLR_ERROR, "Failed to create wlr_output_layout");
	}
	else {
		wlr_log(WLR_DEBUG, "wlr_output_layout created: %p", server.output_layout);
	}

	server.xdg_output_manager = wlr_xdg_output_manager_v1_create(server.wl_display, server.output_layout);
	if (!server.xdg_output_manager) {
		wlr_log(WLR_ERROR, "Failed to create xdg_output_manager_v1");
	}
	else {
		wlr_log(WLR_DEBUG, "xdg_output_manager_v1 created: %p", server.xdg_output_manager);
	}

	// Configure a listener to be notified when new outputs are available on the backend.
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);
	
	// Create a scene graph. This is a wlroots abstraction that handles all
	// rendering and damage tracking. All the compositor author needs to do
	// is add things that should be rendered to the scene graph at the proper
	// positions and then call wlr_scene_output_commit() to render a frame if necessary.
	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);
	
	// Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	// used for application windows. For more detail on shells, refer to
	// https://drewdevault.com/2018/07/29/Wayland-shells.html.
	wl_list_init(&server.toplevels);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);

	// Handle toplevels
	server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	
	// Handle popups
	server.new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

	server.toplevel_manager = wlr_foreign_toplevel_manager_v1_create(server.wl_display);
	if (!server.toplevel_manager) {
		fprintf(stderr, "Failed to create foreign toplevel manager\n");
		exit(EXIT_FAILURE);
	}

	server.wlr_relative_pointer_manager = wlr_relative_pointer_manager_v1_create(server.wl_display);
	if (!server.wlr_relative_pointer_manager) {
		wlr_log(WLR_ERROR, "Failed to create relative pointer manager!");
		return 1;
	}
	server.wlr_pointer_constraints = wlr_pointer_constraints_v1_create(server.wl_display);
	if (!server.wlr_pointer_constraints) {
		wlr_log(WLR_ERROR, "Failed to create pointer constraints!");
		return 1;
	}
	server.new_pointer_constraint.notify = handle_new_pointer_constraint;
	wl_signal_add(&server.wlr_pointer_constraints->events.new_constraint,
										&server.new_pointer_constraint);
	// Creates a cursor, which is a wlroots utility for tracking the cursor
	// image shown on screen.
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	// Creates an xcursor manager, another wlroots utility which loads up
	// Xcursor themes to source cursor images from and makes sure that cursor
	// images are available at all scale factors on the screen (necessary for HiDPI support).
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

	// wlr_cursor *only* displays an image on screen. It does not move around
	// when the pointer moves. However, we can attach input devices to it, and
	// it will generate aggregate events for all of them. In these events, we
	// can choose how we want to process them, forwarding them to clients and
	// moving the cursor around. More detail on this process is described in
	// https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html.
	// And more comments are sprinkled throughout the notify functions above.
	server.cursor_mode = WOODLAND_CURSOR_PASSTHROUGH;
	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	// Configures a seat, which is a single "seat" at which a user sits and
	// operates the computer. This conceptually includes up to one keyboard,
	// pointer, touch, and drawing tablet device. We also rig up a listener to
	// let us know when new input devices are available on the backend.
	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	
	// Create the seat for input devices 
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	if (!server.seat) {
		wlr_log(WLR_ERROR, "Failed to create seat!");
		wl_display_destroy(server.wl_display);
		return 1;
	}

	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

	/*** Drag and drop */
	server.start_drag.notify = seat_start_drag;
	wl_signal_add(&server.seat->events.start_drag, &server.start_drag);
	server.request_start_drag.notify = seat_request_start_drag;
	wl_signal_add(&server.seat->events.request_start_drag, &server.request_start_drag);

	if (!wlr_screencopy_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to create screencopy manager!");
		return -1;
	}

	/*** Create virtual keyboard manager and configure a listener for new virtual keyboards. */
	server.virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(server.wl_display);
	if (!server.virtual_keyboard_mgr) {
		wlr_log(WLR_ERROR, "Failed to create virtual keyboard manager!");
		return 1;
	}
	server.new_virtual_keyboard.notify = new_virtual_keyboard_handler;
	wl_signal_add(&server.virtual_keyboard_mgr->events.new_virtual_keyboard, &server.new_virtual_keyboard);

	if (!wlr_viewporter_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to create viewporter!");
		return 1;
	}
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_log(WLR_ERROR, "Failed to add Unix socket to Wayland display!");
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(server.backend)) {
		wlr_log(WLR_ERROR, "Failed to start backend.");
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	{
		struct wlr_drm_format_set formats = {0}; // Initialize DRM format set

		// Add a valid DRM format and modifier
		if (!wlr_drm_format_set_add(&formats, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_INVALID)) {
			default_tranche.formats = formats;
			wlr_log(WLR_ERROR, "Failed to add DRM_FORMAT_ARGB8888 to DMABUF feedback");
			return 1;
		}
		else {
			wlr_log(WLR_DEBUG, "Successfully added DRM_FORMAT_ARGB8888 with modifier DRM_FORMAT_MOD_INVALID");
		}

		// Assign formats to default tranche
		default_tranche.formats = formats;

		// Validate tranche setup
		if (default_tranche.formats.len == 0) {
			wlr_log(WLR_ERROR, "Default tranche has no valid formats!");
			return 1;
		}

		// Log feedback details before creating DMABUF object
		wlr_log(WLR_DEBUG, "DMABUF feedback setup:");
		wlr_log(WLR_DEBUG, "Main device: %lx", (unsigned long)default_feedback.main_device);
		wlr_log(WLR_DEBUG, "Tranches size: %zu", default_feedback.tranches.size);

		server.linux_dmabuf = wlr_linux_dmabuf_v1_create_with_renderer(server.wl_display, 3, server.renderer);
		if (!server.linux_dmabuf) {
			wlr_log(WLR_ERROR, "Failed to create Linux DMABUF object");
			return 1;
		}
	}

	wlr_log(WLR_DEBUG, "Initialized DMA-BUF support");

	// Set the WAYLAND_DISPLAY environment variable to our socket and run the
	// startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);

	struct wlr_output *output = wlr_output_layout_output_at(server.output_layout,
															server.cursor->x,
															server.cursor->y);
	wlr_output_transformed_resolution(output, &server.transformed_width, &server.transformed_height);
	// Background picture setup
	char *background_img = get_char_value_from_conf(server.config, "background");
	if (!background_img) {
		wlr_log(WLR_ERROR, "No background image provided in config");
	}
	else {
		background_setup(&server, output, &background_img);
	}

	// Panel setup
	server.cr = NULL;
	server.font_face = NULL;
	server.panel_buffer = NULL;
	server.cairo_surface = NULL;
	server.network_buffer = NULL;
	server.calendar_buffer = NULL;
	server.wlr_panel_buffer = NULL;
	server.panel_scene_output = NULL;

	// Showing time clock
	panel_setup(&server, output, PPANEL_WIDTH, PPANEL_HEIGHT);

	// Window list
	///server.toplevel_info info = {0};
	server.toplevel_info.titles[0] = '\0';
	server.toplevel_info.app_id[0] = '\0';
	server.toplevel_info.minimized[0] = '\0';

	// Running startup commands
	if (startup_cmd) {
		if (fork() == 0) {
			run_cmd(startup_cmd);
		}
	}
	else {
		/* Run welcome screen */
		fprintf(stderr, "welcome_screen_CMD: %s\n", welcome_screen_CMD);
		run_cmd(welcome_screen_CMD);
		/*** Startup commands after delay */
		server.autostart_timer = wl_event_loop_add_timer(server.event_loop,
														 process_startup_commands,
														 &server);
		if (!server.autostart_timer) {
			wlr_log(WLR_ERROR, "Failed to create autostart_timer!");
			return 1;
		}
		wl_event_source_timer_update(server.autostart_timer, 7000);
	}

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Woodland compositor on WAYLAND_DISPLAY=%s", socket);
	wl_display_run(server.wl_display);

	/* Once wl_display_run returns, we shut down the server. */
	wlr_log(WLR_INFO, "Shutting down Woodland compositor...");

	// Free allocated memory
	wlr_log(WLR_DEBUG, "Shutting down server.ssids");
	if (server.ssids[0] != NULL) {
		for (size_t i = 0; server.ssids[i] != NULL; i++) {
			fprintf(stderr, "server.ssids[%ld]: %s\n", i, server.ssids[i]);
			free(server.ssids[i]);
			server.ssids[i] = NULL;
		}
	}
	wlr_log(WLR_DEBUG, "Shutting down welcome_screen_CMD");
	if (welcome_screen_CMD) {
		free(welcome_screen_CMD);
		welcome_screen_CMD = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down background_img");
	if (background_img) {
		free(background_img);
		background_img = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.backend");
	if (server.backend) {
		wlr_backend_destroy(server.backend);
		server.backend = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.seat");
	if (server.seat) {
		wlr_seat_destroy(server.seat);
		server.seat = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.scene");
	if (server.scene) {
		wlr_scene_node_destroy(&server.scene->tree.node);
		server.scene = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.config_sizes");
	if (server.config_sizes) {
		free(server.config_sizes);
		server.config_sizes = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.tap_enable");
	if (server.tap_enable) {
		free(server.tap_enable);
		server.tap_enable = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.brightness_path");
	if (server.brightness_path) {
		free(server.brightness_path);
		server.brightness_path = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.play_pause");
	if (server.play_pause) {
		free(server.play_pause);
		server.play_pause = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.volume_up");
	if (server.volume_up) {
		free(server.volume_up);
		server.volume_up = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.volume_down");
	if (server.volume_down) {
		free(server.volume_down);
		server.volume_down = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.volume_mute");
	if (server.volume_mute) {
		free(server.volume_mute);
		server.volume_mute = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.config");
	if (server.config) {
		free(server.config);
		server.config = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.volumeHigh");
	if (server.volumeHigh) {
		free(server.volumeHigh);
		server.volumeHigh = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.brightnessIcon");
	if (server.brightnessIcon) {
		free(server.brightnessIcon);
		server.brightnessIcon = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.networkIcon");
	if (server.networkIcon) {
		free(server.networkIcon);
		server.networkIcon = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.wlr_panel_buffer");
	if (server.wlr_panel_buffer) {
		wlr_buffer_drop(server.wlr_panel_buffer);
		server.wlr_panel_buffer = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.background_scene_buffer->texture");
	if (server.background_scene_buffer) {
		wlr_texture_destroy(server.background_scene_buffer->texture);
		server.background_scene_buffer->texture = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.panel_buffer->texture");
	if (server.panel_buffer->texture) {
		wlr_texture_destroy(server.panel_buffer->texture);
		server.panel_buffer->texture = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.calendar_buffer->texture");
	if (server.calendar_texture && server.calendar_buffer->texture) {
		cairo_surface_flush(server.cairo_surface);
		wlr_texture_destroy(server.calendar_buffer->texture);
		server.calendar_buffer->texture = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.network_buffer->texture");
	if (server.network_texture && server.network_buffer->texture) {
		wlr_texture_destroy(server.network_buffer->texture);
		server.network_buffer->texture = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.font_face");
	if (server.font_face) {
		cairo_font_face_destroy(server.font_face);
		server.font_face = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.cr");
	if (server.cr) {
		cairo_destroy(server.cr);
		server.cr = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.cairo_surface");
	if (server.cairo_surface) {
		cairo_surface_destroy(server.cairo_surface);
		server.cairo_surface = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.time_update_timer");
	if (server.time_update_timer) {
		wl_event_source_remove(server.time_update_timer);
		server.time_update_timer = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.autostart_timer");
	if (!server.autostart_cmd_ran && server.autostart_timer) {
		wl_event_source_remove(server.autostart_timer);
		server.autostart_timer = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.output_layout");
	if (server.output_layout) {
		wlr_output_layout_destroy(server.output_layout);
		server.output_layout = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.renderer");
	if (server.renderer) {
		wlr_renderer_destroy(server.renderer);
		server.renderer = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.cursor_mgr");
	if (server.cursor_mgr) {
		wlr_xcursor_manager_destroy(server.cursor_mgr);
		server.cursor_mgr = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.cursor");
	if (server.cursor) {
		wlr_cursor_destroy(server.cursor);
		server.cursor = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.allocator");
	if (server.allocator) {
		wlr_allocator_destroy(server.allocator);
		server.allocator = NULL;
	}
	wlr_log(WLR_DEBUG, "Shutting down server.wl_display");
	if (server.wl_display) {
		wl_display_destroy_clients(server.wl_display);
		wl_display_flush_clients(server.wl_display);
		wl_display_destroy(server.wl_display);
		server.wl_display = NULL;
	}
	wlr_log(WLR_INFO, "See you next time in Woodland :)");
	return 0;
}
