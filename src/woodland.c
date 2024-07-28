// SPDX-License-Identifier: GPL-2.0-or-later

/* woodland */
/* Minimal but functional Wayland compositor. */

///#define _POSIX_C_SOURCE 200112L
#define STB_IMAGE_IMPLEMENTATION // needed for background image implementation
#define TOUCHPAD_SCROLL_SCALE 0.7 // Scaling factor for touchpad scrolls
#define MOUSE_SCROLL_SCALE 1.0 // Scaling factor for mouse wheel scrolls
#define SCROLL_DEBOUNCE_THRESHOLD 2.0 // Threshold to filter out small scroll values
#define MAX_NR_OF_STARTUP_COMMANDS 265 // maximum number of user defined startup commands

/* Local headers */
#include "runcmd.h"
#include "create-config.c"
#include "getxkbkeyname.h"
#include "getvaluefromconf.h"

/* System headers */
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <libinput.h>
#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <wlr/backend.h>
#include <wlr/util/log.h>
#include <stb/stb_image.h>
#include <pixman-1/pixman.h>
#include <libdrm/drm_fourcc.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_idle.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server-core.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_region.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/render/wlr_texture.h>
#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

/* For brevity's sake, struct members are annotated where they are used. */
enum woodland_cursor_mode {
	WOODLAND_CURSOR_PASSTHROUGH,
	WOODLAND_CURSOR_MOVE,
	WOODLAND_CURSOR_RESIZE,
};

struct woodland_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;
	struct wlr_texture *background_texture;
	// Timer
	struct wl_event_source *timer;
	struct wl_event_source *autostart_timer;
	// XDG Shell
	struct wl_list views;
	struct wl_list minimized_views; // list for minimized views
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	// Idle
	struct wlr_idle *idle;
	struct wlr_idle_timeout *idle_timeout;
	struct wlr_pointer *pointer;
	struct wl_listener new_idle;
	struct wl_listener idle_resume;
	// Additional interfaces
	// Layer shell
	struct wl_list layer_surfaces;
	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_surface;
	// Virtual Keyboard
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
	struct wl_listener new_virtual_keyboard;
	// Foreign toplevel manager
	struct wlr_foreign_toplevel_manager_v1 *wlr_foreign_toplevel_mgr;
	// Output manager
	struct wlr_output_manager_v1 *wlr_output_manager;
	struct wl_listener output_configuration_applied;
	struct wl_listener output_configuration_tested;
	// Drag and drop
	struct wl_listener start_drag;
	struct wl_listener request_start_drag;
	// Data control (clipboard)
	struct wlr_data_control_manager_v1 *data_control_mgr;
	struct wl_listener new_data_control;
	struct wlr_cursor *cursor;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_button;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion_absolute;
	struct wlr_seat *seat;
	struct wl_list keyboards;
	struct wlr_box grab_geobox;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list outputs;
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;
	struct wlr_surface *prev_surface;
	struct woodland_view *grabbed_view;
	struct woodland_view *current_focused_view;
	struct woodland_view *previous_focused_view;
	enum woodland_cursor_mode cursor_mode;
	double grab_x;
	double grab_y;
	float matrix[9];
	float background_matrix[9];
	uint32_t modifier;
	uint32_t resize_edges;
	bool idle_enabled;
	bool should_render;
	bool render_full_stop;
	bool super_key_down;
	bool keybind_handled;
	bool layer_view_found;
	char *config;
	// Zooming
	double zoom_speed;				// Speed of panning
	double zoom_factor;				// How large the zooming area should be on one scroll
	double pan_offset_x;			// Pan offset for x-axis
	double pan_offset_y;			// Pan offset for y-axis
	double zoom_edge_threshold;		// How far from screen edges the zoom pan should start
	char *zoom_top_edge;			// set to enabled in woodland.ini to zoom on top left corcer
};

struct woodland_output {
	struct wl_list link;
	struct wl_listener frame;
	struct wlr_output *wlr_output;
	struct woodland_server *server;
};

struct woodland_view {
	struct woodland_server *server;
	struct wlr_xdg_surface *xdg_surface;
	struct wl_list link;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener set_app_id;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener foreign_destroy;
	struct wl_listener request_minimize;
	struct wl_listener request_fullscreen;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	bool is_fullscreen;
	bool mapped;
	int keyboard_layout;
	int x;
	int y;
};

struct woodland_layer_view {
    struct woodland_server *server;
    struct wl_list link;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wlr_layer_surface_v1 *layer_surface;
    bool mapped;
	double x;
	double y;
};

struct woodland_keyboard {
	struct woodland_server *server;
	struct wl_list link;
	struct wl_listener key;
	struct wl_listener destroy;
	struct wl_listener modifiers;
	struct wlr_input_device *device;
};

/* Yakes an index, looks for it in a string and then returns a new string with
 * index placed in the first position, elements should be separated by comma,
 * i use it to re-arrange the layouts to place the chosen layout at position 0
 * it is needed in order to change the layout per application.
 */
char *updated_layouts(char *index, char *layouts) {
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

	// Copy the rest of the layouts, skipping the 'index' part
	if (*end == ',') {
		end++; // Skip the comma after 'index'
	}
	else if (start != layouts_copy && *(start - 1) == ',') {
		start--; // Include the comma before 'index'
	}
	strncat(new_layouts, ",", new_layouts_size - strlen(new_layouts) - 1);
	strncat(new_layouts, layouts_copy, start - layouts_copy);
	if (*end) {
		strncat(new_layouts, end, new_layouts_size - strlen(new_layouts) - 1);
	}

	// Remove trailing comma if it exists
	if (new_layouts[strlen(new_layouts) - 1] == ',') {
		new_layouts[strlen(new_layouts) - 1] = '\0';
	}

	free(layouts_copy);
	return new_layouts;
}

/* Given a number index, it looks through a string of words devided by comma
 * and returns the word at given index.
 */
char *layout_name_from_index(int index, char *layouts) {
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

// Handle idle event
static void server_new_idle(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_server *server = wl_container_of(listener, server, new_idle);
	// Stop rendering on idle timeout
	server->should_render = false;
	wlr_log(WLR_INFO, "The system is idle now.");
}

// Handle resume event
static void server_idle_resume(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_server *server = wl_container_of(listener, server, idle_resume);
	// Resume rendering when resumed from idle (either a mouse move or keyboard activity)
	server->render_full_stop = false;
	server->should_render = true;
	// Schedule a new frame for each output
	// we need this in order to resume the rendering function 'output_frame'
	struct woodland_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_output_schedule_frame(output->wlr_output);
	}
	wlr_log(WLR_INFO, "The system resumed from idle.");
}

// Function to update pan offset based on mouse position
static void update_pan_offset(struct woodland_server *server,
							  double mouse_x,
							  double mouse_y,
							  double screen_width,
							  double screen_height) {
	// Reset pan offsets if scaling factor is 1.0 (initial zoom level)
	if ((server->zoom_factor - 0.2) <= 1.0) {
		server->pan_offset_x = 0;
		server->pan_offset_y = 0;
		server->background_matrix[2] = 0; // Reset background x offset
		server->background_matrix[5] = 0; // Reset background y offset
		return;
	}
	
	// Check if the mouse is near the left edge of the screen
	if (mouse_x < server->zoom_edge_threshold) {
		// Pan left, ensuring we don't go past the screen's left boundary
		if (server->pan_offset_x > 0) {
			server->pan_offset_x = fmax(server->pan_offset_x - server->zoom_speed, 0);
			server->background_matrix[2] += server->zoom_factor;
		}
	}
	// Check if the mouse is near the right edge of the screen
	else if (mouse_x > screen_width - server->zoom_edge_threshold) {
		// Pan right, ensuring we don't go past the scaled content's right boundary
		double max_pan_x = screen_width * (server->zoom_factor - 1);
		if (server->pan_offset_x < max_pan_x) {
			server->pan_offset_x = fmin(server->pan_offset_x + server->zoom_speed, max_pan_x);
			server->background_matrix[2] -= server->zoom_factor;
		}
	}
	
	// Check if the mouse is near the top edge of the screen
	if (mouse_y < server->zoom_edge_threshold) {
		// Pan up, ensuring we don't go past the screen's top boundary
		if (server->pan_offset_y > 0) {
			server->pan_offset_y = fmax(server->pan_offset_y - server->zoom_speed, 0);
			server->background_matrix[5] += server->zoom_factor;
		}
	}
	// Check if the mouse is near the bottom edge of the screen
	else if (mouse_y > screen_height - server->zoom_edge_threshold) {
		// Pan down, ensuring we don't go past the scaled content's bottom boundary
		double max_pan_y = screen_height * (server->zoom_factor - 1);
		if (server->pan_offset_y < max_pan_y) {
			server->pan_offset_y = fmin(server->pan_offset_y + server->zoom_speed, max_pan_y);
			server->background_matrix[5] -= server->zoom_factor;
		}
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

	wlr_log(WLR_ERROR, "Ignoring request_start_drag, could not validate pointer serial %d", event->serial);
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

static void focus_view(struct woodland_view *view, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (view == NULL || surface == NULL) {
		wlr_log(WLR_ERROR, "focus_view called with NULL view or surface. view: %p, surface: %p",
																			view, surface);
		return;
	}

	wlr_log(WLR_INFO, "Focusing view: %p, surface: %p", view, surface);

	/* Get the seat and server associated with the view */
	struct woodland_server *server = view->server;
	struct wlr_seat *seat = server->seat;
	/* Get the previously focused surface */
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		wlr_log(WLR_INFO, "Surface already focused: %p", surface);
		return;
	}

	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
										seat->keyboard_state.focused_surface);
		if (previous) {
			wlr_log(WLR_INFO, "Deactivating previous surface: %p", previous);
			wlr_xdg_toplevel_set_activated(previous, false);
			wlr_seat_keyboard_notify_clear_focus(seat);
		}
		else {
			wlr_log(WLR_ERROR, "Previous surface is not a valid xdg_surface.");
		}
	}

	/* Check if a keyboard is available for the seat */
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (!keyboard) {
		wlr_log(WLR_ERROR, "No keyboard found for seat. Trying to reassign a keyboard.");
		
		/* Forcefully reassign the first available keyboard */
		struct woodland_keyboard *new_keyboard = NULL;
		struct woodland_keyboard *key;
		wl_list_for_each(key, &server->keyboards, link) {
			new_keyboard = key;
			break;  // Pick the first available keyboard
		}
		
		if (new_keyboard) {
			wlr_seat_set_keyboard(seat, new_keyboard->device);
			keyboard = new_keyboard->device->keyboard;
			wlr_log(WLR_INFO, "Reassigned keyboard to seat: %p", keyboard);
		}
		else {
			wlr_log(WLR_ERROR, "No available keyboard to assign to the seat.");
			return;
		}
	}

	/* Move the view to the front */
	if (view) {
		if (!wl_list_empty(&view->link)) {
			wl_list_remove(&view->link);
			wl_list_insert(&server->views, &view->link);
		}

		// Change keyboard layout per application
		char *layouts = get_char_value_from_conf(server->config, "xkb_layouts");
		if (!layouts) {
			fprintf(stderr, "Error: Failed to get xkb_layouts from config.\n");
			return;
		}

		char *layout_name = layout_name_from_index(view->keyboard_layout, layouts);
		if (!layout_name) {
			fprintf(stderr, "Error: Failed to get layout name for index %d.\n",
															view->keyboard_layout);
			free(layouts);
			return;
		}

		char *new_layout = updated_layouts(layout_name, layouts);
		if (!new_layout) {
			fprintf(stderr, "Error: Failed to update layouts.\n");
			free(layout_name);
			free(layouts);
			return;
		}

		struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		if (!context) {
			fprintf(stderr, "Error: Failed to create xkb_context.\n");
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
			fprintf(stderr, "Error: Failed to create xkb_keymap.\n");
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
		} else {
			printf("No layout found at index %d\n", view->keyboard_layout);
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
	else {
		wlr_log(WLR_ERROR, "'view' is NULL in 'focus_view.");
		return;
	}

	/* Activate the new surface */
	if (view->xdg_surface) {
		wlr_xdg_toplevel_set_activated(view->xdg_surface, true);
	}
	else {
		wlr_log(WLR_ERROR, "'xdg_surface' is NULL in 'focus_view.");
		return;
	}

	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	wlr_seat_keyboard_notify_enter(seat,
								   view->xdg_surface->surface,
								   keyboard->keycodes,
								   keyboard->num_keycodes,
								   &keyboard->modifiers);

	/* Notify the surface that it has entered the output */
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
															server->cursor->x,
															server->cursor->y);
	if (output) {
		wlr_surface_send_enter(view->xdg_surface->surface, output);
	}

	wlr_log(WLR_INFO, "View focused: %p", view);
}

/***************************** Keyboard management *****************************/
static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	(void)data; // Suppress unused parameter warning
	struct woodland_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	wlr_log(WLR_INFO, "Destroying keyboard ...");

	// Remove listeners
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);

	// Unset the keyboard for the seat if this is the active keyboard
	if (wlr_seat_get_keyboard(keyboard->server->seat) == keyboard->device->keyboard) {
		wlr_seat_set_keyboard(keyboard->server->seat, NULL);
	}

	// Remove the keyboard from the server's list of keyboards
	wl_list_remove(&keyboard->link);

	// Free the keyboard struct
	free(keyboard);

	wlr_log(WLR_INFO, "Destroying keyboard done!");
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	struct wlr_keyboard_modifiers *modifiers = data;
	if (!modifiers) {
		wlr_log(WLR_ERROR, "Received NULL modifiers data.");
		return;
	}

	// Log the received modifiers for debugging
	///\wlr_log(WLR_INFO, "Modifiers updated: depressed=0x%x, latched=0x%x, locked=0x%x,
	///group=%d", modifiers->depressed, modifiers->latched, modifiers->locked, modifiers->group);

	///fprintf(stderr, "Modifiers updated: depressed=0x%x, latched=0x%x, locked=0x%x, group=%d",
	///	modifiers->depressed, modifiers->latched, modifiers->locked, modifiers->group);

	// Set the keyboard for the seat
	struct woodland_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
	if (keyboard && keyboard->device) {
		wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);
		// Notify the seat with the updated modifiers
		wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
							&keyboard->device->keyboard->modifiers);
	}
	else {
		wlr_log(WLR_ERROR, "'keyboard' or 'device' is NULL in 'keyboard_handle_modifiers'.");
	}
}

// This function parses the config file (woodland.ini) and storing all
// user defined keyboard shortcuts (keys) in a char array
static void keybindings_group_init(char *config, char *modifierName, char **keynames,
																char **keycommands) {
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
	struct woodland_view *current_view = wl_container_of(server->views.next, current_view, link);
	struct woodland_view *next_view = wl_container_of(current_view->link.next, next_view, link);
	switch (sym) {
	case XKB_KEY_Tab: // Alt+Tab cycle to the next view
		if (wl_list_length(&server->views) < 2) {
			break;
		}
		if (next_view) {
			focus_view(next_view, next_view->xdg_surface->surface);
			/* Move the previous view to the end of the list */
			wl_list_remove(&current_view->link);
			wl_list_insert(server->views.prev, &current_view->link);
		}
		else {
			wlr_log(WLR_ERROR, "'next_view' is NULL in 'handle_keybinding_alt'");
		}
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

static bool handle_keybinding_super(struct woodland_server *server, xkb_keysym_t sym) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * This function assumes Super is held down.
	 */
	// Get the current view and the next view
	struct woodland_view *current_view = wl_container_of(server->views.next, current_view, link);
	struct woodland_view *next_view = wl_container_of(current_view->link.next, next_view, link);
	switch (sym) {
	case XKB_KEY_Escape: // Super+Esc Log out from compositor
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_x: // Super+x close current active window
		if (!wl_list_empty(&server->views)) {
			wlr_xdg_toplevel_send_close(current_view->xdg_surface);
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
	// Get the woodland_keyboard and woodland_server from the listener
	struct woodland_keyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct woodland_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;
	
	// Translate libinput keycode to xkbcommon keycode
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	
	// Get a list of keysyms based on the keymap for this keyboard
	int nsyms = xkb_state_key_get_syms(keyboard->device->keyboard->xkb_state, keycode, &syms);
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	server->modifier = modifiers;

	// Translate the key symbol code into a key name as defined in the header
	char hexCode[256];
	snprintf(hexCode, sizeof(hexCode), "%#06x", syms[0]);
	char *keyname = xkb_keyname(hexCode);
	server->keybind_handled = false;
	
	// Debug logging
	///wlr_log(WLR_INFO, "Key event: keycode=%d, state=%d, modifiers=%d, keyname=%s", 
	///							event->keycode, event->state, modifiers, keyname);

	for (int i = 0; i < nsyms; i++) {
		///fprintf(stderr, "keyname: %s\n", keyname);
		if (strcmp(keyname, "XKB_KEY_ISO_Next_Group") == 0 && event->state == \
										WL_KEYBOARD_KEY_STATE_RELEASED) {
			// Store current or changed keyboard layout
			struct woodland_view *current_view = wl_container_of(server->views.next,
																 current_view,
																 link);
			if (current_view && server->seat->keyboard_state.keyboard->xkb_state) {
				current_view->keyboard_layout = xkb_state_serialize_layout(
									server->seat->keyboard_state.keyboard->xkb_state,
									XKB_STATE_LAYOUT_EFFECTIVE);
			}
		}
		// Check if the Super key is pressed or released
		if (syms[i] == XKB_KEY_Super_L || syms[i] == XKB_KEY_Super_R) {
			if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
				server->super_key_down = true;
				///wlr_log(WLR_INFO, "Super key pressed");
			}
			else if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
				server->super_key_down = false;
				///wlr_log(WLR_INFO, "Super key released");
			}
		}
		// Handle compositor keybindings when the Alt key is held down
		else if ((modifiers & WLR_MODIFIER_ALT) && event->state == \
										WL_KEYBOARD_KEY_STATE_PRESSED) {
			///wlr_log(WLR_INFO, "Handling Alt keybinding for keysym=%d", syms[i]);
			handle_keybinding_alt(server, syms[i]);
		}
		// Handle compositor keybindings when the Ctrl key is held down
		else if ((modifiers & WLR_MODIFIER_CTRL) && event->state == \
										WL_KEYBOARD_KEY_STATE_PRESSED) {
			///wlr_log(WLR_INFO, "Handling Ctrl keybinding for keysym=%d", syms[i]);
			handle_keybinding_ctrl(server, syms[i]);
		}
		// Handle compositor keybindings when the Shift key is held down
		else if ((modifiers & WLR_MODIFIER_SHIFT) && event->state == \
										WL_KEYBOARD_KEY_STATE_PRESSED) {
			///wlr_log(WLR_INFO, "Handling Shift keybinding for keysym=%d", syms[i]);
			handle_keybinding_shift(server, syms[i]);
		}
		// Handle compositor keybindings when the Super key is held down
		else if ((modifiers & WLR_MODIFIER_LOGO) && event->state == \
										WL_KEYBOARD_KEY_STATE_PRESSED) {
			///wlr_log(WLR_INFO, "Handling Super keybinding for keysym=%d", syms[i]);
			handle_keybinding_super(server, syms[i]);
		}
	}
	
	// Pass the key to the client only if it isn't already defined
	// by user as a key shortcut in ~/.config/woodland.ini
	if (!server->keybind_handled) {
		///wlr_log(WLR_INFO, "Key not handled by keybindings, passing to client");
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
	}

	if (keyname != NULL) {
		free(keyname);
		keyname = NULL;
	}
	// Send the keyboard activity event to idle manager
	if (server->idle_enabled && server->idle && server->seat) {
		wlr_idle_notify_activity(server->idle, server->seat);
	}
	else {
		wlr_log(WLR_ERROR, "'idle' is NULL in 'keyboard_handle_key'");
	}
}

static void server_new_keyboard(struct woodland_server *server, struct wlr_input_device *device) {
	struct woodland_keyboard *keyboard = calloc(1, sizeof(struct woodland_keyboard));
	if (!keyboard) {
		wlr_log(WLR_ERROR, "Failed to allocate woodland_keyboard.");
		return;
	}

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

	keyboard->server = server;
	keyboard->device = device;

	wlr_keyboard_set_keymap(device->keyboard, keymap);
	wlr_keyboard_set_repeat_info(device->keyboard, 30, 300);

	// Set up listeners for keyboard events
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->keyboard->events.destroy, &keyboard->destroy);

	// Set the keyboard for the seat
	wlr_seat_set_keyboard(server->seat, device);

	// Add the keyboard to the list of keyboards
	wl_list_insert(&server->keyboards, &keyboard->link);

	// Clean up
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	free(layouts);
}

static void server_new_pointer(struct woodland_server *server, struct wlr_input_device *device) {
	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(server->cursor, device);
}

// Function to enable tap-to-click on a libinput device
void enable_tap_to_click(struct wlr_input_device *device) {
	if (device->type == WLR_INPUT_DEVICE_POINTER) {
		struct libinput_device *libinput_dev = wlr_libinput_get_device_handle(device);
		if (libinput_dev && libinput_device_config_tap_get_finger_count(libinput_dev) > 0) {
			libinput_device_config_tap_set_enabled(libinput_dev, LIBINPUT_CONFIG_TAP_ENABLED);
		}
	}
}

static void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes available. */
	struct wlr_input_device *device = data;
	if ((!device) || (device == NULL)) {
		wlr_log(WLR_ERROR, "'device is nULL in 'server_new_input'");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, new_input);
	switch (device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			server_new_keyboard(server, device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			enable_tap_to_click(device);  // Enable tap-to-click for pointer devices
			server_new_pointer(server, device);
			break;
		default:
			break;
	}

	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communicated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

/* Function to handle the destruction of virtual keyboards */
static void virtual_keyboard_destroy_handler(struct wl_listener *listener, void *data) {
	(void)data;
	struct woodland_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	if (!keyboard) {
		wlr_log(WLR_ERROR, "Keyboard destroy handler called with NULL keyboard.");
		return;
	}

	wlr_log(WLR_INFO, "Destroying virtual keyboard: %p", keyboard);

	// Clean up listeners
	if (!wl_list_empty(&keyboard->modifiers.link)) {
		wl_list_remove(&keyboard->modifiers.link);
	}
	if (!wl_list_empty(&keyboard->key.link)) {
		wl_list_remove(&keyboard->key.link);
	}
	if (!wl_list_empty(&keyboard->destroy.link)) {
		wl_list_remove(&keyboard->destroy.link);
	}

	// Optionally reset the keyboard device if needed
	///keyboard->device = NULL; // Uncomment if required

	// Free the keyboard object
	if (keyboard) {
		free(keyboard);
		keyboard = NULL;
	}
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
	keyboard->device = &virtual_keyboard->input_device;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if ((!context) || (context == NULL)) {
		wlr_log(WLR_ERROR, "'context' is NULL in 'new_virtual_keyboard_handler'.");
		return;
	}
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
																XKB_KEYMAP_COMPILE_NO_FLAGS);
	if ((!keymap) || (keymap == NULL)) {
		wlr_log(WLR_ERROR, "'keymap' is NULL in 'new_virtual_keyboard_handler'.");
		return;
	}

	/* Dereference the keyboard pointer */
	wlr_keyboard_set_keymap(keyboard->device->keyboard, keymap);
	wlr_keyboard_set_repeat_info(keyboard->device->keyboard, 30, 300);

	/* Set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&keyboard->device->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&keyboard->device->keyboard->events.key, &keyboard->key);

	/* Set up listener for the destroy event */
	keyboard->destroy.notify = virtual_keyboard_destroy_handler;
	wl_signal_add(&virtual_keyboard->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->device);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_log(WLR_INFO, "Virtual keyboard initialized: %p", virtual_keyboard);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "'event' is NULL in 'seat_request_cursor'.");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, request_cursor);
	struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor,
							   event->surface,
							   event->hotspot_x,
							   event->hotspot_y);
	}
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in woodland it's always honored
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "'event' is NULL in 'seat_request_set_selection'.");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, request_set_selection);
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

/* XDG and Layer Views */
static bool view_layer_at(struct woodland_layer_view *layer_view, double lx, double ly,
						  struct wlr_surface **surface, double *sx, double *sy) {
	if ((!layer_view) || (layer_view == NULL)) {
		wlr_log(WLR_ERROR, "Empty 'layer_view' in 'view_layer_at'!");
		return false;
	}
	if ((!surface) || (surface == NULL)) {
		wlr_log(WLR_ERROR, "Empty 'surface' in 'view_layer_at'!");
		return false;
	}
	double _sx;
	double _sy;
	double view_sx = lx - layer_view->x;;
	double view_sy = ly - layer_view->y;;

	struct wlr_surface *_surface = wlr_layer_surface_v1_surface_at(layer_view->layer_surface,
																view_sx, view_sy, &_sx, &_sy);
	if (_surface != NULL) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}

	return false;
}

static bool view_at(struct woodland_view *view, double lx, double ly, struct wlr_surface **surface,
																		double *sx, double *sy) {
	/*
	 * XDG toplevels may have nested surfaces, such as popup windows for context
	 * menus or tooltips. This function tests if any of those are underneath the
	 * coordinates lx and ly (in output Layout Coordinates). If so, it sets the
	 * surface pointer to that wlr_surface and the sx and sy coordinates to the
	 * coordinates relative to that surface's top-left corner.
	 */
	double _sx;
	double _sy;
	double view_sx = lx - view->x;
	double view_sy = ly - view->y;
	struct wlr_surface *_surface = NULL;
	_surface = wlr_xdg_surface_surface_at(view->xdg_surface,
										  view_sx,
										  view_sy,
										  &_sx,
										  &_sy);
	if (_surface != NULL) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}
	return false;
}

static struct woodland_view *desktop_view_at(struct woodland_server *server, double lx, double ly,
										struct wlr_surface **surface, double *sx, double *sy) {
	/* This iterates over all of our surfaces and attempts to find one under the
	 * cursor. This relies on server->views being ordered from top-to-bottom.
	 * If we have any layer shell applicaiton then we need to make sure it is
	 * always rendered above other toplevels and not only its buffer but its
	 * wlr_surface too.
	 */
	// First, check if at this coordinates underneath other toplevels there is any layer surface
    struct woodland_layer_view *layer_view;
    wl_list_for_each(layer_view, &server->layer_surfaces, link) {
        if (view_layer_at(layer_view, lx, ly, surface, sx, sy)) {
			/// If any layer surface found underneath any XDG toplevels then do nothing
			wlr_xcursor_manager_set_cursor_image(layer_view->server->cursor_mgr, "left_ptr",
																layer_view->server->cursor);
			server->layer_view_found = true;
        	return NULL;
        }
    }
	struct woodland_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view_at(view, lx, ly, surface, sx, sy)) {
			// Sets explicit cursor theme instead of default xcursor theme
			// because default xcursor theme doesn't scale well
            wlr_xcursor_manager_set_cursor_image(view->server->cursor_mgr, "left_ptr",
            													view->server->cursor);
			return view;
		}
	}
	return NULL;
}

static void process_cursor_move(struct woodland_server *server, uint32_t time) {
	(void)time;
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'process_cursor_move'!");
		return;
	}
	/* Move the grabbed view to the new position. */
	server->grabbed_view->x = ((server->cursor->x + server->pan_offset_x) / \
										server->zoom_factor) - server->grab_x;
	server->grabbed_view->y = ((server->cursor->y + server->pan_offset_y) / \
										server->zoom_factor) - server->grab_y;
}

static void process_cursor_resize(struct woodland_server *server, uint32_t time) {
	(void)time;
	/* Resizing the grabbed view can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the view
	 * on one or two axes, but can also move the view if you resize from the top
	 * or left edges (or top-left corner).
	 *
	 * Note the original author took some shortcuts here. In a more fleshed-out compositor,
	 * you'd wait for the client to prepare a buffer at the new size, then
	 * commit any movement that was prepared.
	 */
	if (!server) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'process_cursor_resize'!");
		return;
	}
	struct woodland_view *view = server->grabbed_view;
	if (!view) {
		wlr_log(WLR_ERROR, "Error: 'view' is NULL in 'process_cursor_resize'!");
		return;
	}
	double border_x = ((server->cursor->x + server->pan_offset_x) / \
									server->zoom_factor) - server->grab_x;
	double border_y = ((server->cursor->y + server->pan_offset_y) / \
									server->zoom_factor) - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	}
	else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	}
	else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}
	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
	view->x = new_left - geo_box.x;
	view->y = new_top - geo_box.y;
	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(view->xdg_surface, new_width, new_height);
}

static void process_cursor_motion(struct woodland_server *server, uint32_t time) {
	// If the cursor mode is set to move, process the cursor move and return.
	if (server->cursor_mode == WOODLAND_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	}
	// If the cursor mode is set to resize, process the cursor resize and return.
	else if (server->cursor_mode == WOODLAND_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}
	// Pointer to the surface under the cursor
	struct wlr_surface *surface = NULL;
	double cursor_x;
	double cursor_y;
	
	// The check 'server->zoom_factor > 1.0' represents the state of zoom
	// if 'server->zoom_factor > 1.0' is 1.0 then no zooming is applied
	// if the value is greater than 1.0 then the user is currently zooming in or out
	if (server->zoom_factor > 1.0) {
		// Adjust cursor coordinates by the scaling factor and pan offset
		cursor_x = (server->cursor->x + server->pan_offset_x) / server->zoom_factor;
		cursor_y = (server->cursor->y + server->pan_offset_y) / server->zoom_factor;
	}
	else {
		cursor_x = server->cursor->x;
		cursor_y = server->cursor->y;
	}
	// Debugging: Log the cursor coordinates
	///fprintf(stderr, "Cursor coordinates: (%.2f, %.2f)\n", server->cursor->x,
	///server->cursor->y);
	///fprintf(stderr, "Adjusted cursor coordinates: (%.2f, %.2f)\n", cursor_x, cursor_y);

	// Variables for storing surface-local coordinates
	double sx;
	double sy;
	// Try to find a regular view under the cursor
	struct woodland_view *view = desktop_view_at(server, cursor_x, cursor_y, &surface, &sx, &sy);
	if (!view && !server->layer_view_found) {
		// No surface found, log an error and reset layer view flag
		server->layer_view_found = false;
	}
	else if (server->layer_view_found) {
		server->layer_view_found = false;
	}
	else {
		view_at(view, cursor_x, cursor_y, &surface, &sx, &sy);
	}

	// Get the seat (input device)
	struct wlr_seat *seat = server->seat;
	// Get the surface under the cursor from the previous motion
	struct wlr_surface *focused_surface = seat->pointer_state.focused_surface;

	// If a surface (either regular or layer) is found under the cursor
	if (surface) {
		// If the surface under the cursor has changed
		if (surface != focused_surface) {
			// Notify the seat of the pointer entering the new surface
			wlr_seat_pointer_notify_enter(seat, surface, sx, sy); // use unscaled sx, sy
			// Set the cursor image to "left_ptr"
			wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);
		}
		else {
			// Notify the seat of the pointer motion within the same surface
			wlr_seat_pointer_notify_motion(seat, time, sx, sy); // use unscaled sx, sy
		}
	}
	else {
		// If no surface is under the cursor, clear the pointer focus if necessary
		if (focused_surface) {
			wlr_seat_pointer_clear_focus(seat);
			// Set the cursor image to "left_ptr"
			wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->cursor);
		}
	}
	// Send the mouse activity event to idle manager
	if (server->idle_enabled && server->idle && server->seat) {
		wlr_idle_notify_activity(server->idle, server->seat);
	}
	else {
		wlr_log(WLR_ERROR, "Error: 'idle' is NULL in 'process_cursor_motion'");
	}
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct wlr_event_pointer_motion *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'event' is NULL in 'server_cursor_motion'!");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, cursor_motion);
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'server_cursor_motion'!");
		return;
	}
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(server->cursor, NULL, event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
	// Update pan offset based on the current cursor position

	// Update pan offset based on the current cursor position
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
															server->cursor->x,
															server->cursor->y);
	if ((!output) || (output == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'output' is NULL in 'seat_request_cursor'.");
		return;
	}
	update_pan_offset(server, server->cursor->x, server->cursor->y, output->width,
																	output->height);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct wlr_event_pointer_motion_absolute *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'event' is NULL in 'server_cursor_motion_absolute'!");
		return;
	}
    struct woodland_server *server = wl_container_of(listener, server, cursor_motion_absolute);
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'server_cursor_motion_absolute'!");
		return;
	}
    wlr_cursor_warp_absolute(server->cursor, NULL, event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
	double sx;
	double sy;
	struct wlr_surface *surface = NULL;
	struct wlr_event_pointer_button *event = data;
	if (!event) {
		wlr_log(WLR_ERROR, "Error: 'event' is NULL in 'server_cursor_button'!");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, cursor_button);
	if (!server) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'server_cursor_button'!");
		return;
	}

	// Adjust cursor coordinates by the scaling factor
	double cursor_x = (server->cursor->x + server->pan_offset_x) / server->zoom_factor;
	double cursor_y = (server->cursor->y + server->pan_offset_y) / server->zoom_factor;

	// Notify the seat of the button event
	wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);

	if (event->state == WLR_BUTTON_RELEASED) {
		// Reset cursor mode and grabbed view on button release
		server->cursor_mode = WOODLAND_CURSOR_PASSTHROUGH;
		server->grabbed_view = NULL;
		return;
	}

	// Check if any layer surface is under the cursor
	struct woodland_layer_view *layer_view;
	wl_list_for_each(layer_view, &server->layer_surfaces, link) {
		if (layer_view->mapped) {
			surface = wlr_layer_surface_v1_surface_at(layer_view->layer_surface,
													  cursor_x,
													  cursor_y,
													  &sx,
													  &sy);
			if (surface) {
				break;
			}
		}
	}

	// If a surface is found under the cursor, focus it and return
	if (surface) {
		return;
	}

	// Check if a view is under the cursor
	struct woodland_view *view = desktop_view_at(server, cursor_x, cursor_y, &surface, &sx, &sy);
	if (view && surface) {
		// Focus the view
		focus_view(view, surface);

		// If the Super key and left mouse button are both pressed, emit a move request
		if (event->button == BTN_LEFT && server->super_key_down) {
			wl_signal_emit(&view->xdg_surface->toplevel->events.request_move, view->xdg_surface);
		}
		else {
			server->super_key_down = false;
		}
	}
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	// Retrieve the axis event data
	static bool zoom_on_top_edge = false;
	struct wlr_event_pointer_axis *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'event' is NULL in 'server_cursor_axis'!");
		return;
	}
	// Retrieve the server instance from the listener
	struct woodland_server *server = wl_container_of(listener, server, cursor_axis);
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'server_cursor_axis'!");
		return;
	}
	struct wlr_output *output = wlr_output_layout_output_at(server->output_layout,
															server->cursor->x,
															server->cursor->y);
	if ((!output) || (output == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'output' is NULL in 'server_cursor_axis'!");
		return;
	}
	double delta = event->delta;
	if (event->source == WLR_AXIS_SOURCE_FINGER) {
		// Scale the delta for touchpad events
		delta *= TOUCHPAD_SCROLL_SCALE;
	}
	else {
		// Scale the delta for mouse wheel events
		delta *= MOUSE_SCROLL_SCALE;
	}
	// Debounce small scroll values to filter out noise
	if (fabs(delta) < SCROLL_DEBOUNCE_THRESHOLD) {
		return;
	}
	switch (event->orientation) {
		case WLR_AXIS_ORIENTATION_VERTICAL:
			if (delta > 0) {
				///fprintf(stderr, "Mouse wheel up\n");
				// Zooming out on super key and scroll or if zoom_top_edge is enabled
				if (strcmp(server->zoom_top_edge, "enabled") == 0 && \
					(int)server->cursor->x < 10 && (int)server->cursor->y < 10) {
					zoom_on_top_edge = true;
				}
				else if (strcmp(server->zoom_top_edge, "enabled") == 0 && \
					(int)server->cursor->x > 10 && (int)server->cursor->y > 10) {
					zoom_on_top_edge = false;
				}
				if ((server->super_key_down) || (zoom_on_top_edge)) {
					// Zooming out
					if ((server->zoom_factor - 0.2) <= 1.0) {
						server->zoom_factor = 1.0;
						server->pan_offset_x = 0;
						server->pan_offset_y = 0;
						server->background_matrix[2] = 0; // Reset background x offset
						server->background_matrix[5] = 0; // Reset background y offset
						update_pan_offset(server,
										  server->cursor->x,
										  server->cursor->y,
										  output->width,
										  output->height);
						// Scaling the matrix for background image	// Begin rendering
						wlr_matrix_project_box(server->background_matrix, &(struct wlr_box){
																		.x = 0,
																		.y = 0,
																		.width = output->width,
																		.height = output->height},
																		WL_OUTPUT_TRANSFORM_NORMAL,
																		0.0,
																		output->transform_matrix);
					}
					else if (server->zoom_factor > 1.0) {
						server->zoom_factor = server->zoom_factor - 0.1;
						update_pan_offset(server,
										  server->cursor->x,
										  server->cursor->y,
										  output->width,
										  output->height);
						// Scaling the matrix for background image	// Begin rendering
						wlr_matrix_project_box(server->background_matrix, &(struct wlr_box){
										.x = server->background_matrix[2] -= server->zoom_factor,
										.y = server->background_matrix[5] -= server->zoom_factor,
										.width = output->width * server->zoom_factor,
										.height = output->height * server->zoom_factor},
										WL_OUTPUT_TRANSFORM_NORMAL,
										0.0,
										output->transform_matrix);
					}
				}
			}
			else if (delta < 0) {
				///fprintf(stderr, "Mouse wheel down\n");
				// Zooming in on super key and scroll or is zoom_top_edge is enabled
				if (strcmp(server->zoom_top_edge, "enabled") == 0 && \
					(int)server->cursor->x < 10 && (int)server->cursor->y < 10) {
					zoom_on_top_edge = true;
				}
				else if (strcmp(server->zoom_top_edge, "enabled") == 0 && \
					(int)server->cursor->x > 10 && (int)server->cursor->y > 10) {
					zoom_on_top_edge = false;
				}
				if ((server->super_key_down) || (zoom_on_top_edge)) {
					// Zooming in
					server->zoom_factor = server->zoom_factor + 0.1;
					update_pan_offset(server,
									  server->cursor->x,
									  server->cursor->y,
									  output->width,
									  output->height);
					// Scaling the matrix for background image
					wlr_matrix_project_box(server->background_matrix, &(struct wlr_box){
												.x = server->background_matrix[2],
												.y = server->background_matrix[5],
												.width = output->width * server->zoom_factor,
												.height = output->height * server->zoom_factor},
												WL_OUTPUT_TRANSFORM_NORMAL,
												0.0,
												output->transform_matrix);
				}
			}
			break;
		case WLR_AXIS_ORIENTATION_HORIZONTAL:
			if (delta > 0) {
				// fprintf(stderr, "Mouse wheel left\n");
			}
			else if (delta < 0) {
				// fprintf(stderr, "Mouse wheel right\n");
			}
			break;
	}

	// If a surface is under the cursor, notify the seat of the axis event
	wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation,
	                        		     delta, event->delta_discrete, event->source);
}


static void server_cursor_frame(struct wl_listener *listener, void *data) {
	(void)data;
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	struct woodland_server *server = wl_container_of(listener, server, cursor_frame);
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'server' is NULL in 'server_cursor_frame'!");
		return;
	}
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

/* Used to move all of the data necessary to render a surface from the top-level
 * frame handler to the per-surface render function. */
struct render_data {
	struct timespec *when;
	struct wlr_output *output;
	struct woodland_view *view;
	struct wlr_renderer *renderer;
	struct woodland_layer_view *lview;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	if ((!surface) || (surface ==  NULL)) {
		wlr_log(WLR_ERROR, "Error: 'surface' is NULL in 'render_surface'!");
		return;
	}
	struct render_data *rdata = data;
	if ((!rdata) || (rdata == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'rdata' is NULL in 'render_surface'!");
		return;
	}
	struct woodland_view *view = rdata->view;
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'view' is NULL in 'render_surface'!");
		return;
	}
	struct wlr_output *output = rdata->output;
	if ((!output) || (output == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'output' is NULL in 'render_surface'!");
		return;
	}

	/* We first obtain a wlr_texture, which is a GPU resource. wlroots
	 * automatically handles negotiating these with the client. The underlying
	 * resource could be an opaque handle passed from the client, or the client
	 * could have sent a pixel buffer which we copied to the GPU, or a few other
	 * means. You don't have to worry about this, wlroots takes care of it. */
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if ((!texture) || (texture == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'texture' is NULL in 'render_surface'!");
		return;
	}

	/* The view has a position in layout coordinates. If you have two displays,
	 * one next to the other, both 1080p, a view on the rightmost display might
	 * have layout coordinates of 2000,100. We need to translate that to
	 * output-local coordinates, or (2000 - 1920).
	 * We also have to apply the scale factor for HiDPI outputs. This is only
	 * part of the puzzle, TinyWL does not fully support HiDPI.
	 */
	struct wlr_box box;
	box.x = view->x + sx;
	box.y = view->y + sy;
	box.width = surface->pending.width;
	box.height = surface->pending.height;
	/*
	 * Those familiar with OpenGL are also familiar with the role of matrices
	 * in graphics programming. We need to prepare a matrix to render the view
	 * with. wlr_matrix_project_box is a helper which takes a box with a desired
	 * x, y coordinates, width and height, and an output geometry, then
	 * prepares an orthographic projection and multiplies the necessary
	 * transforms to produce a model-view-projection matrix.
	 *
	 * Naturally you can do this any way you like, for example to make a 3D
	 * compositor.
	 */
	wlr_matrix_project_box(view->server->matrix,
						   &box,
						   WL_OUTPUT_TRANSFORM_NORMAL,
						   0.0,
						   output->transform_matrix);
	/* This takes our matrix, the texture, and an alpha, and performs the actual
	 * rendering on the GPU. */
	///struct wlr_fbox fbox;
	///wlr_surface_get_buffer_source_box(surface, &fbox);
	///wlr_render_subtexture_with_matrix(rdata->renderer, texture, &fbox, view->server->matrix, 1);
	wlr_render_texture_with_matrix(rdata->renderer, texture, view->server->matrix, 1);

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
	wlr_surface_send_frame_done(surface, rdata->when);
}

static void render_layer_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	(void)sx;
	(void)sy;
	if (!surface || !surface->buffer || !surface->buffer->texture) {
		wlr_log(WLR_ERROR, "Error: Invalid surface in 'render_layer_surface'!");
		return;
	}
	struct render_data *rdata = data;
	if ((!rdata) || (rdata == NULL)) {
		return;
	}
	struct wlr_output *output = rdata->output;
	if ((!output) || (output == NULL)) {
		return;
	}
	struct wlr_renderer *renderer = surface->renderer;
	if ((!renderer) || (renderer == NULL)) {
		return;
	}
	struct wlr_texture *texture = surface->buffer->texture;
	if ((!texture) || (texture == NULL)) {
		return;
	}
	struct woodland_view *view = rdata->view;
	if ((!view) || (view == NULL)) {
		return;
	}

	struct wlr_box box;
	box.x = sx + rdata->lview->x;
	box.y = sy + rdata->lview->y;
	box.width = surface->current.width;
	box.height = surface->current.height;

	wlr_matrix_project_box(view->server->matrix,
						   &box,
						   WL_OUTPUT_TRANSFORM_NORMAL,
						   0.0,
						   output->transform_matrix);
	wlr_render_texture_with_matrix(renderer, texture, view->server->matrix, 1);
	wlr_surface_send_frame_done(surface, rdata->when);
}

static void output_frame(struct wl_listener *listener, void *data) {
	(void)data;
	// Get the current time
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	// Retrieve the woodland_output structure from the listener
	struct woodland_output *output = wl_container_of(listener, output, frame);
	// This stopps the rendering completely after setting the screen black
	if (output->server->render_full_stop) {
		return;
	}
	// Define the renderer
	struct wlr_renderer *renderer = output->server->renderer;
	// Attach the renderer to the output
	if (!wlr_output_attach_render(output->wlr_output, NULL)) {
		wlr_log(WLR_ERROR, "Error: Failed to attach renderer in 'output_frame'!");
		return;
	}
	// Begin rendering
	wlr_renderer_begin(renderer, output->wlr_output->width, output->wlr_output->height);
	// Stop rendering on idle and clear to black
	if (!output->server->should_render) {
		float color[4] = {0.0, 0.0, 0.0, 1.0}; // Set alpha to 1.0 for opaque black
		wlr_renderer_clear(renderer, color);
		wlr_renderer_end(renderer);
		wlr_output_commit(output->wlr_output);
		output->server->render_full_stop = true;
		return;
	}
	// Prepare render_data structure
	struct render_data rdata = {
		.output = output->wlr_output,
		.renderer = renderer,
		.when = &now
	};

	// Render the background image if available
	if (output->server->background_texture) {
		wlr_render_texture_with_matrix(renderer,
									   output->server->background_texture,
									   output->server->background_matrix,
									   1.0f);

		if (output->server->zoom_factor > 1.0) {
			// Update pan offset based on current mouse position
			update_pan_offset(output->server,
							  output->server->cursor->x / output->server->zoom_factor,
							  output->server->cursor->y / output->server->zoom_factor,
							  output->wlr_output->width * output->server->zoom_factor,
							  output->wlr_output->height * output->server->zoom_factor);

			// Set the new viewport with scaling and panning
			glViewport(-output->server->pan_offset_x,
					   -output->server->pan_offset_y,
					   (int)(output->wlr_output->width * output->server->zoom_factor),
					   (int)(output->wlr_output->height * output->server->zoom_factor));
		}
	}
	else {
		// Clear with default color if background texture is not available
		float color[4] = {0.1, 0.1, 0.1, 1.0};
		wlr_renderer_clear(renderer, color);

		if (output->server->zoom_factor > 1.0) {
			// Update pan offset based on current mouse position
			update_pan_offset(output->server,
							  output->server->cursor->x / output->server->zoom_factor,
							  output->server->cursor->y / output->server->zoom_factor,
							  output->wlr_output->width * output->server->zoom_factor,
							  output->wlr_output->height * output->server->zoom_factor);

			// Set the new viewport with scaling and panning
			glViewport(-output->server->pan_offset_x,
					   -output->server->pan_offset_y,
					   (int)(output->wlr_output->width * output->server->zoom_factor),
					   (int)(output->wlr_output->height * output->server->zoom_factor));
		}
	}

	// Render each view in reverse order
	struct woodland_view *view;
	wl_list_for_each_reverse(view, &output->server->views, link) {
		if (view->mapped) {
			rdata.view = view;
			wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, &rdata);
		}
	}

	// Render each layer view in reverse order
	struct woodland_layer_view *layer_view;
	wl_list_for_each_reverse(layer_view, &output->server->layer_surfaces, link) {
		if (layer_view->mapped) {
			rdata.lview = layer_view;
			wlr_layer_surface_v1_for_each_surface(layer_view->layer_surface,
												  render_layer_surface,
												  &rdata);
		}
	}

	// Render software cursors
	wlr_output_render_software_cursors(output->wlr_output, NULL);
	// End rendering
	wlr_renderer_end(renderer);
	// Commit the rendered output
	if (!wlr_output_commit(output->wlr_output)) {
		wlr_log(WLR_ERROR, "Failed to commit output");
	}
}

static void handle_output_configuration_applied(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	if (!config) {
		wlr_log(WLR_ERROR, "Error: 'config' is NULL in 'handle_output_configuration_applied'.");
		return;
	}

	struct woodland_server *server = wl_container_of(listener,
													 server,
													 output_configuration_applied);

	bool success = true; // Assume success initially

	// Iterate over each output configuration and apply it
	struct wlr_output_configuration_head_v1 *config_head;
	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *output = config_head->state.output;
		if (!output) {
			wlr_log(WLR_ERROR, "Error: Output is NULL in configuration.");
			success = false;
			break;
		}

		// Apply the mode, if set
		if (config_head->state.mode) {
			wlr_output_set_mode(output, config_head->state.mode);
		} else {
			// Apply custom mode
			wlr_output_set_custom_mode(output,
									   config_head->state.custom_mode.width,
									   config_head->state.custom_mode.height,
									   config_head->state.custom_mode.refresh);
		}

		// Apply the transformation
		wlr_output_set_transform(output, config_head->state.transform);

		// Apply the position
		wlr_output_layout_add(server->output_layout, output,
							  config_head->state.x, config_head->state.y);

		// Commit the output
		if (!wlr_output_commit(output)) {
			wlr_log(WLR_ERROR, "Failed to commit output.");
			success = false;
			break;
		}
	}

	if (success) {
		wlr_output_configuration_v1_send_succeeded(config);
	}
	else {
		wlr_output_configuration_v1_send_failed(config);
	}

	// Destroy the configuration after handling it
	wlr_output_configuration_v1_destroy(config);
}

static void handle_output_configuration_tested(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	if (!config) {
		wlr_log(WLR_ERROR, "Error: 'config' is NULL in 'handle_output_configuration_tested'.");
		return;
	}

	struct woodland_server *server = wl_container_of(listener, server, output_configuration_tested);

	bool success = true; // Assume success initially

	// Iterate over each output configuration and test it
	struct wlr_output_configuration_head_v1 *config_head;
	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *output = config_head->state.output;
		if (!output) {
			wlr_log(WLR_ERROR, "Error: Output is NULL in configuration.");
			success = false;
			break;
		}
		// Perform necessary tests for each output configuration
		// If any test fails, set success to false
		// For example, check resolution, refresh rate, etc.
		// if (some_test_failed) {
		//     success = false;
		//     break;
		// }
	}

	if (success) {
		wlr_output_configuration_v1_send_succeeded(config);
	}
	else {
		wlr_output_configuration_v1_send_failed(config);
	}

	// Destroy the configuration after handling it
	wlr_output_configuration_v1_destroy(config);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
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
	/* Some backends don't have modes. DRM+KMS does, and we need to set a mode
	 * before we can use the output. The mode is a tuple of (width, height,
	 * refresh rate), and each monitor supports only a specific set of modes. We
	 * just pick the monitor's preferred mode, a more sophisticated compositor
	 * would let the user configure it. */
	
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}
	/* Allocates and configures our state for this output */
	struct woodland_output *output = calloc(1, sizeof(struct woodland_output));
	if (output == NULL) {
		wlr_log(WLR_ERROR, "Error: Failed to allocate memory for woodland_output!");
		return;
    }
	output->wlr_output = wlr_output;
	output->server = server;
	output->server->should_render = true;
	// Matrix for background image
	wlr_matrix_project_box(output->server->background_matrix, &(struct wlr_box){
							.x = -output->server->pan_offset_x,
							.y = -output->server->pan_offset_y,
							.width = output->wlr_output->width * output->server->zoom_factor,
							.height = output->wlr_output->height * output->server->zoom_factor},
							WL_OUTPUT_TRANSFORM_NORMAL,
							0.0,
							output->wlr_output->transform_matrix);
	/* Sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
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
	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}

/************************ XDG Shell and foreign toplevel implementation ***********************/
static void _xdg_surface_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	wlr_log(WLR_INFO, "XDG surface destroying...");

	// Retrieve the view associated with this listener
	struct woodland_view *view = wl_container_of(listener, view, destroy);
	if (!view) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in '_xdg_surface_destroy'!");
		return;
	}

	// Ensure the server and seat are valid
	struct woodland_server *server = view->server;
	if (!server) {
		wlr_log(WLR_ERROR, "Error: Empty 'server' in '_xdg_surface_destroy'!");
		return;
	}

	struct wlr_seat *seat = server->seat;
	if (!seat) {
		wlr_log(WLR_ERROR, "Error: Empty 'seat' in '_xdg_surface_destroy'!");
		return;
	}

	// Check for the next view to focus
	bool focus_surface = false;
	struct woodland_view *prev_view = NULL;
	if (!wl_list_empty(&server->views)) {
		struct woodland_view *iter;
		wl_list_for_each_reverse(iter, &server->views, link) {
			if (iter && iter != view) {
				prev_view = iter;
				///break;
			}
		}
	}

	if (prev_view && prev_view != view) {
		struct wlr_surface *prev_surface = prev_view->xdg_surface->surface;
		if (prev_surface && prev_surface != view->xdg_surface->surface) {
			focus_surface = true;
		}
	}

	// Clean up the foreign toplevel handle if it exists
	if (view->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(view->foreign_toplevel);
	}

	// Remove the view from all lists it is part of
	if (!wl_list_empty(&view->map.link)) {
		wl_list_remove(&view->map.link);
	}
	if (!wl_list_empty(&view->unmap.link)) {
		wl_list_remove(&view->unmap.link);
	}
	if (!wl_list_empty(&view->destroy.link)) {
		wl_list_remove(&view->destroy.link);
	}
	if (!wl_list_empty(&view->set_title.link)) {
		wl_list_remove(&view->set_title.link);
	}
	if (!wl_list_empty(&view->set_app_id.link)) {
		wl_list_remove(&view->set_app_id.link);
	}
	if (!wl_list_empty(&view->request_move.link)) {
		wl_list_remove(&view->request_move.link);
	}
	if (!wl_list_empty(&view->request_resize.link)) {
		wl_list_remove(&view->request_resize.link);
	}
	if (!wl_list_empty(&view->request_minimize.link)) {
		wl_list_remove(&view->request_minimize.link);
	}
	if (!wl_list_empty(&view->request_fullscreen.link)) {
		wl_list_remove(&view->request_fullscreen.link);
	}
	if (view->link.prev != &view->link && view->link.next != &view->link) {
		wl_list_remove(&view->link);
	}
	if ((!view) || (view != NULL)) {
		free(view);
		view = NULL;
	}

	// If we found a surface to focus, do so
	if (focus_surface && prev_view) {
		struct wlr_xdg_surface *previous = prev_view->xdg_surface;
		if (previous) {
			wlr_log(WLR_INFO, "Activating previous surface: %p", previous);
			wlr_xdg_toplevel_set_activated(previous, true);
			if (seat->keyboard_state.keyboard) {
				wlr_log(WLR_INFO, "Notifying keyboard enter...");
				wlr_seat_keyboard_notify_enter(seat, previous->surface,
											   seat->keyboard_state.keyboard->keycodes,
											   seat->keyboard_state.keyboard->num_keycodes,
											   &seat->keyboard_state.keyboard->modifiers);
			}
			else {
				wlr_log(WLR_ERROR, "No keyboard found to notify enter.");
			}
		}
		else {
			wlr_log(WLR_ERROR, "Previous surface is not a valid xdg_surface to focus.");
		}
	}
	else {
		wlr_log(WLR_INFO, "No previous surface to focus.");
	}

	wlr_log(WLR_INFO, "XDG surface destroyed!");
}


static void handle_foreign_toplevel_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	wlr_log(WLR_INFO, "Foreign handle destroying...");
	struct woodland_view *view = wl_container_of(listener, view, foreign_destroy);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'handle_foreign_toplevel_destroy'!");
		return;
	}
	if (!wl_list_empty(&view->foreign_destroy.link)) {
		wl_list_remove(&view->foreign_destroy.link);
	}
	if (!wl_list_empty(&view->foreign_close_request.link)) {
		wl_list_remove(&view->foreign_close_request.link);
	}
	if (!wl_list_empty(&view->foreign_activate_request.link)) {
		wl_list_remove(&view->foreign_activate_request.link);
	}
	if (!wl_list_empty(&view->foreign_fullscreen_request.link)) {
		wl_list_remove(&view->foreign_fullscreen_request.link);
	}
	wlr_log(WLR_INFO, "Foreign handle destroyed!");
}

static void handle_foreign_close_request(struct wl_listener *listener, void *data) {
	(void)data;
	wlr_log(WLR_INFO, "Foreign handle closing...");
	struct woodland_view *view = wl_container_of(listener, view, foreign_close_request);
	if (view && view->foreign_toplevel) {
		wlr_xdg_toplevel_send_close(view->xdg_surface);
	}
	else {
		wlr_log(WLR_ERROR, "Return from 'handle_foreign_close_request'!");
		return;
	}
	wlr_log(WLR_INFO, "Foreign handle closed!");
}

static void handle_foreign_activate_request(struct wl_listener *listener, void *data) {
	(void)data;
	wlr_log(WLR_INFO, "Foreign handle activating...");
	struct woodland_view *view = wl_container_of(listener, view, foreign_activate_request);
	struct woodland_server *server = view->server;
	if (view && view->foreign_toplevel) {
		// Remove the view from the list of minimized views
		if (!wl_list_empty(&view->link)) {
			wl_list_remove(&view->link);
			// Add it back to the list of active views
			wl_list_insert(&server->views, &view->link);
		}
		// Map the surface to show it
		if (!view->mapped) {
			wl_signal_emit(&view->xdg_surface->events.map, view->xdg_surface);
		}
		if (view->xdg_surface->surface) {
			focus_view(view, view->xdg_surface->surface);
		}
	}
	else {
		wlr_log(WLR_ERROR, "Return from 'handle_foreign_activate_request'!");
		return;
	}
	wlr_log(WLR_INFO, "Foreign handle activated!");
}

static void handle_foreign_fullscreen_request(struct wl_listener *listener, void *data) {
	(void)data;
	wlr_log(WLR_INFO, "Foreign handle fullscreen requesting...");
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'event' is NULL in 'handle_foreign_fullscreen_request'.");
		return;
	}
	struct woodland_view *view = wl_container_of(listener, view, foreign_fullscreen_request);
	if (view && view->foreign_toplevel) {
		wlr_xdg_toplevel_set_fullscreen(view->xdg_surface, event->fullscreen);
	}
	else {
		wlr_log(WLR_ERROR, "Return from 'handle_foreign_fullscreen_request'!");
		return;
	}
	wlr_log(WLR_INFO, "Foreign handle fullscreen set!");
}

/* Get user defined window placement coordinates from wooldand.ini config */
static void get_window_placement(char *file, char *ids[], char *identifiers[], int x[], int y[]) {
	FILE *fp = fopen(file, "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file %s\n", file);
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

static void xdg_surface_set_title(struct wl_listener *listener, void *data) {
	// Get the wlr_xdg_surface and ensure it's not null
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface == NULL) {
		wlr_log(WLR_ERROR, "Error: Empty 'xdg_surface' in 'xdg_surface_set_title'!");
		return;
	}

	// Get the toplevel structure and ensure it's not null
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	if (toplevel == NULL) {
		wlr_log(WLR_ERROR, "Error: Empty 'toplevel' in 'xdg_surface_set_title'!");
		return;
	}

	// Get the view structure and ensure it's not null
	struct woodland_view *view = wl_container_of(listener, view, set_title);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'xdg_surface_set_title'!");
		return;
	}
	// 'toplevel->title' is the new updated title when for instance you click on a new tab
	// on mousepad then 'toplevel->title' is the new title of the mousepad
	// it is not the same titme that the toplevel was originally mapped with
	// noe we need to ckeck is it's NULL or empty then assign the string "nil"
	if ((!toplevel->title) || (toplevel->title == NULL)) {
		toplevel->title = "nil";
	}
	// If foreign_toplevel is set and not minimized, set the title
	if (view->foreign_toplevel && !view->xdg_surface->toplevel->requested.minimized) {
		wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel, toplevel->title);
	}
	wlr_log(WLR_INFO, "XDG toplevel title set");
}

static void xdg_surface_set_appid(struct wl_listener *listener, void *data) {
	(void)listener;
	(void)data;

	// Get the wlr_xdg_surface and ensure it's not null
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface == NULL) {
		wlr_log(WLR_ERROR, "Error: Empty 'xdg_surface' in 'xdg_surface_set_appid'!");
		return;
	}

	// Get the toplevel structure and ensure it's not null
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	if (toplevel == NULL) {
		wlr_log(WLR_ERROR, "Error: Empty 'toplevel' in 'xdg_surface_set_appid'!");
		return;
	}

	// Get the view structure and ensure it's not null
	struct woodland_view *view = wl_container_of(listener, view, set_app_id);
	if (view == NULL) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'xdg_surface_set_appid'!");
		return;
	}

	if ((!toplevel->app_id) || (toplevel->app_id == NULL)) {
		toplevel->app_id = "nil";
	}
	// If foreign_toplevel is set and not minimized, set the title
	if (view->foreign_toplevel && !view->xdg_surface->toplevel->requested.minimized) {
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, toplevel->app_id);
	}
	wlr_log(WLR_INFO, "XDG toplevel app_id set");
}

static void xdg_surface_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	(void)data;
	wlr_log(WLR_INFO, "XDG surface mapping...");
	struct woodland_view *view = wl_container_of(listener, view, map);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'xdg_surface_map'!");
		return;
	}
	struct wlr_output *output = wlr_output_layout_output_at(view->server->output_layout,
																view->server->cursor->x,
																view->server->cursor->y);
	if ((!output) || (output == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'output' in 'xdg_surface_map'!");
		return;
	}

	struct wlr_foreign_toplevel_manager_v1 *foreign_topmgr = \
						view->server->wlr_foreign_toplevel_mgr;
	if ((!foreign_topmgr) || (foreign_topmgr == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'foreign_topmgr' in 'xdg_surface_map'!");
		return;
	}
	// Set the new window position
	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

	// Center the window
	view->x = (output->width - geo_box.width) / 2;
	view->y = (output->height - geo_box.height) / 2;

	// If the window width or height exceeds the screen geometry then resize to fit the screen
	if (geo_box.width > output->width) {
		wlr_xdg_toplevel_set_size(view->xdg_surface, output->width, geo_box.height);
	}
	if (geo_box.height > output->height) {
		wlr_xdg_toplevel_set_size(view->xdg_surface, geo_box.width, output->height);
	}

	// Executing window placement
	const char *title = NULL;
	const char *app_id = NULL;
	title = view->xdg_surface->toplevel->title;
	if ((!title) || (title == NULL)) {
		title = "nil";
	}
	app_id = view->xdg_surface->toplevel->app_id;
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
	get_window_placement(view->server->config, ids, identifiers, x_arr, y_arr);

	// If 'surface->current.committed' == WLR_SURFACE_STATE_BUFFER it lets us know that
	// the client required a toplevel move or resize and we can use this information
	// to filter which windows should be let to use the client required coordinates
	// and which windows should be always placed in center.
	bool clientRequiredPlacement = false;
	if (view->xdg_surface->surface->current.committed == WLR_SURFACE_STATE_BUFFER) {
		clientRequiredPlacement = true;
	}

	if (title != NULL && app_id == NULL) {
		for (int i = 0; i < 1024 && ids[i] != NULL; i++) {
			if (strcmp(ids[i], "title:") == 0) {
				// If this title is found in woodland.ini for automatic placement
				if (strcmp(identifiers[i], title) == 0) {
					view->x = x_arr[i];
					view->y = y_arr[i];
					break;
				}
				if (strcmp(identifiers[i], title) != 0 && clientRequiredPlacement && \
													geo_box.x != 0 && geo_box.y != 0) {
					view->x = geo_box.x;
					view->y = geo_box.y;
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
					view->x = x_arr[i];
					view->y = y_arr[i];
					break;
				}
				if (strcmp(identifiers[i], app_id) != 0 && clientRequiredPlacement && \
													geo_box.x != 0 && geo_box.y != 0) {
					view->x = geo_box.x;
					view->y = geo_box.y;
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
					view->x = x_arr[i];
					view->y = y_arr[i];
					break;
				}
				if (strcmp(identifiers[i], app_id) != 0 && clientRequiredPlacement && \
													geo_box.x != 0 && geo_box.y != 0) {
					view->x = geo_box.x;
					view->y = geo_box.y;
					break;
				}
			}
			else if (strcmp(ids[i], "title:") == 0) {
				if (strcmp(identifiers[i], title) == 0) {
					view->x = x_arr[i];
					view->y = y_arr[i];
					break;
				}
				if (strcmp(identifiers[i], title) != 0 && clientRequiredPlacement && \
													geo_box.x != 0 && geo_box.y != 0) {
					view->x = geo_box.x;
					view->y = geo_box.y;
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

	// Create a foreign toplevel handle
	// First we need to check if this toplevel has been minimized
	// if it's been minimized then we must not create a new one
	// instead we just activate it, otherwise there will be
	// many duplicates of the same items in window lust and crashes
	if (!view->xdg_surface->toplevel->requested.minimized) {
		view->foreign_toplevel = wlr_foreign_toplevel_handle_v1_create(foreign_topmgr);

		// Set title and app_id
		wlr_foreign_toplevel_handle_v1_set_title(view->foreign_toplevel, title);
		wlr_foreign_toplevel_handle_v1_set_app_id(view->foreign_toplevel, app_id);

		wl_list_init(&view->foreign_destroy.link);
		wl_list_init(&view->foreign_close_request.link);
		wl_list_init(&view->foreign_activate_request.link);
		wl_list_init(&view->foreign_fullscreen_request.link);

		// Add listeners for foreign toplevel events
		view->foreign_destroy.notify = handle_foreign_toplevel_destroy;
		wl_signal_add(&view->foreign_toplevel->events.destroy, &view->foreign_destroy);

		view->foreign_close_request.notify = handle_foreign_close_request;
		wl_signal_add(&view->foreign_toplevel->events.request_close,
										&view->foreign_close_request);

		view->foreign_activate_request.notify = handle_foreign_activate_request;
		wl_signal_add(&view->foreign_toplevel->events.request_activate,
										&view->foreign_activate_request);

		view->foreign_fullscreen_request.notify = handle_foreign_fullscreen_request;
		wl_signal_add(&view->foreign_toplevel->events.request_fullscreen,
										&view->foreign_fullscreen_request);
	}
	// Set mapped flag, this flag is read by rendering function
	// and it renders only a view with mapped flag true
	view->mapped = true;
	
	// Focus the view
	focus_view(view, view->xdg_surface->surface);

	wlr_log(WLR_INFO, "XDG surface mapped!");
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data) {
	(void)data;
	wlr_log(WLR_INFO, "XDG surface unmapping...");
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct woodland_view *view = wl_container_of(listener, view, unmap);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'xdg_surface_unmap'!");
		return;
	}
	view->mapped = false;
	wlr_log(WLR_INFO, "XDG surface unmapped!");
}

static void begin_interactive(struct woodland_view *view,
							  enum woodland_cursor_mode mode,
							  uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propagating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	if (!view) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'begin_interactive'!");
		return;
	}
	if (!mode) {
		wlr_log(WLR_ERROR, "Error: Empty 'mode' in 'begin_interactive'!");
		return;
	}
	struct woodland_server *server = view->server;
	if (!server) {
		wlr_log(WLR_ERROR, "Error: Empty 'server' in 'begin_interactive'!");
		return;
	}
	struct wlr_surface *focused_surface = server->seat->pointer_state.focused_surface;
	if (view->xdg_surface->surface != focused_surface) {
		/* Deny move/resize requests from unfocused clients. */
		wlr_log(WLR_ERROR, "Error: view->xdg_surface->surface != focused_surface!");
		return;
	}
	server->grabbed_view = view;
	server->cursor_mode = mode;

	if (mode == WOODLAND_CURSOR_MOVE) {
		server->grab_x = ((server->cursor->x + server->pan_offset_x) / \
											server->zoom_factor) - view->x;
		server->grab_y = ((server->cursor->y + server->pan_offset_y) / \
											server->zoom_factor) - view->y;
	} else if (mode == WOODLAND_CURSOR_RESIZE) {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
		double border_x = (view->x + geo_box.x) + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (view->y + geo_box.y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = ((server->cursor->x + server->pan_offset_x) / \
											server->zoom_factor) - border_x;
		server->grab_y = ((server->cursor->y + server->pan_offset_y) / \
											server->zoom_factor) - border_y;
		server->grab_geobox = geo_box;
		server->grab_geobox.x += view->x;
		server->grab_geobox.y += view->y;
		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	struct woodland_view *view = wl_container_of(listener, view, request_fullscreen);
	struct wlr_xdg_toplevel_set_fullscreen_event *event = data;

	if (event->fullscreen) {
		// Set the view to fullscreen
		wlr_log(WLR_INFO, "Setting view to fullscreen");
		view->is_fullscreen = true;

		// Get the output to fullscreen on
		struct wlr_output *output = wlr_output_layout_output_at(view->server->output_layout,
																view->server->cursor->x,
																view->server->cursor->y);
		if (!output) {
			wlr_log(WLR_ERROR, "No output found for fullscreen");
			return;
		}

		// Configure the surface to cover the whole output
		///struct wlr_box *output_box = wlr_output_layout_get_box(view->server->output_layout,
		///																				output);
		int width;
		int height;
		wlr_output_transformed_resolution(output, &width, &height);
		wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);

		// Set the output mode to fullscreen
		wlr_xdg_toplevel_set_fullscreen(view->xdg_surface, true);
		wlr_output_commit(output);
	}
	else {
		// Unset fullscreen
		wlr_log(WLR_INFO, "Unsetting view from fullscreen");
		view->is_fullscreen = false;
		wlr_xdg_toplevel_set_fullscreen(view->xdg_surface, false);
	}

	// Send a configure event to the client to apply the changes
	wlr_xdg_surface_schedule_configure(view->xdg_surface);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
	(void)data;
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct woodland_view *view = wl_container_of(listener, view, request_move);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'xdg_toplevel_request_move'!");
		return;
	}
	begin_interactive(view, WOODLAND_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	if ((!event) || (event == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'event' is NULL in 'xdg_toplevel_request_resize'!");
		return;
	}
	struct woodland_view *view = wl_container_of(listener, view, request_resize);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'view' in 'xdg_toplevel_request_resize'!");
		return;
	}
	begin_interactive(view, WOODLAND_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
	(void)listener;
	(void)data;
	struct woodland_view *view = wl_container_of(listener, view, request_minimize);
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: 'view' is NULL in 'xdg_toplevel_request_minimize'.");
		return;
	}
	struct woodland_server *server = view->server;
	struct wlr_output *output;
	struct wlr_surface_output *surface_output;
	wl_list_for_each(surface_output, &view->xdg_surface->surface->current_outputs, link) {
		output = surface_output->output;
		if (output) {
			break;
		}
	}
	// Remove the view from the list of active views
	if (!wl_list_empty(&view->link)) {
		wl_list_remove(&view->link);
		// Add it to the list of minimized views
		wl_list_insert(&server->minimized_views, &view->link);
	}
	// Unmap the surface to hide it
	if (view->xdg_surface->mapped) {
		wlr_surface_send_leave(view->xdg_surface->surface, output);
		wl_signal_emit(&view->xdg_surface->events.unmap, &view->unmap);
		wlr_xdg_surface_schedule_configure(view->xdg_surface);
	}
	// Optionally, you might want to call a render function to update the display
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	wlr_log(WLR_INFO, "XDG new surface creating...");
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	struct wlr_xdg_surface *xdg_surface = data;
	if ((!xdg_surface) || (xdg_surface == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'xdg_surface' in 'server_new_xdg_surface'!");
		return;
	}
	struct woodland_server *server = wl_container_of(listener, server, new_xdg_surface);
	if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'server' in 'server_new_xdg_surface'!");
		return;
	}

	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		wlr_log(WLR_ERROR, "Current surface is not XDG toplevel, skipping.");
		return;
	}
	/* Allocate a woodland_view for this surface */
	struct woodland_view *view = calloc(1, sizeof(struct woodland_view));
	if ((!view) || (view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Failed to allocate memory in 'server_new_xdg_surface'!");
		return;
	}
	view->server = server;
	view->xdg_surface = xdg_surface;

	wl_list_init(&view->map.link);
	wl_list_init(&view->unmap.link);
	wl_list_init(&view->destroy.link);
	wl_list_init(&view->set_title.link);
	wl_list_init(&view->set_app_id.link);
	wl_list_init(&view->request_move.link);
	wl_list_init(&view->request_resize.link);
	wl_list_init(&view->request_minimize.link);
	wl_list_init(&view->request_fullscreen.link);

	/* Listen to the various events it can emit */
	view->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = _xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
	/* cotd */
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	if ((!toplevel) || (toplevel == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'toplevel' in 'server_new_xdg_surface'!");
		return;
	}
	view->set_title.notify = xdg_surface_set_title;
	wl_signal_add(&toplevel->events.set_title, &view->set_title);
	view->set_app_id.notify = xdg_surface_set_appid;
	wl_signal_add(&toplevel->events.set_app_id, &view->set_app_id);
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_minimize.notify = xdg_toplevel_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);
	view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);

	/* Add it to the list of views. */
	wl_list_insert(&server->views, &view->link);
	wlr_log(WLR_INFO, "XDG new surface created!");
}

/* Additional interfaces */
/******************************* Layer Shell Protocol *******************************/
static void arrange_layers(struct woodland_layer_view *layer_view,
						   struct wlr_layer_surface_v1 *layer_surface,
						   struct wlr_output *output,
						   struct wlr_layer_surface_v1_state *state) {
    if ((!layer_view) || (layer_view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_view' in 'arrange_layers'!");
		return;
	}
	if ((!layer_surface) || (layer_surface == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_surface' in 'arrange_layers'!");
		return;
	}
	if ((!output) || (output == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'output' in 'arrange_layers'!");
		return;
	}
    if ((!state) || (state == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'state' in 'arrange_layers'!");
		return;
	}
	// Get the dimensions of the output
	int x = 0;
	int y = 0;
	int output_width;
	int output_height;
	///int output_width = output->width;
	///int output_height = output->height;
    wlr_output_transformed_resolution(output, &output_width, &output_height);
	// Calculate x position based on horizontal anchors
	if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		x = output_width - state->desired_width - state->margin.right;
		///wlr_log(WLR_INFO, "Anchor right: x=%d", x);
	}
    else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		x = state->margin.left;
		///wlr_log(WLR_INFO, "Anchor left: x=%d", x);
	}
	else {
		x = (output_width - state->desired_width) / 2;
		///wlr_log(WLR_INFO, "Anchor center (horizontal): x=%d", x);
	}
	// Calculate y position based on vertical anchors
	if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		y = output_height - (state->desired_height) - state->margin.bottom;
		///wlr_log(WLR_INFO, "Anchor bottom: y=%d", y);
	}
    else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
		y = state->margin.top;
		///wlr_log(WLR_INFO, "Anchor top: y=%d", y);
	}
	else {
		y = (output_height - state->desired_height) / 2;
		///wlr_log(WLR_INFO, "Anchor center (vertical): y=%d", y);
	}
	// Assgning x and y coordinates for the surface
	layer_view->x = x;
	layer_view->y = y;
	// This fixes slurp
	if ((state->desired_width == 0) || (state->desired_height == 0)) {
		state->desired_width = output_width;
		state->desired_height = output_height;
	}
	wlr_layer_surface_v1_configure(layer_surface, state->desired_width, state->desired_height);
}

static void layer_surface_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	(void)listener;
    struct woodland_layer_view *layer_view = wl_container_of(listener, layer_view, destroy);
    if ((!layer_view) || (layer_view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_view' in 'layer_surface_destroy'!");
		return;
	}
    if (!wl_list_empty(&layer_view->map.link)) {
        wl_list_remove(&layer_view->map.link);
    }
    if (!wl_list_empty(&layer_view->unmap.link)) {
        wl_list_remove(&layer_view->unmap.link);
    }
    if (!wl_list_empty(&layer_view->commit.link)) {
        wl_list_remove(&layer_view->commit.link);
    }
    if (!wl_list_empty(&layer_view->destroy.link)) {
        wl_list_remove(&layer_view->destroy.link);
    }
    if (!wl_list_empty(&layer_view->link)) {
    	wl_list_remove(&layer_view->link);
    }
    if (layer_view) {
    	free(layer_view);
    	layer_view = NULL;
    }
	wlr_log(WLR_INFO, "Layer surface Destroyed!");
}

static void wlr_surface_commit(struct wl_listener *listener, void *data) {
	(void)data;
	(void)listener;
    struct woodland_layer_view *layer_view = wl_container_of(listener, layer_view, commit);
    if ((!layer_view) || (layer_view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_view' in 'wlr_surface_commit'!");
		return;
	}
    if (layer_view->layer_surface->current.committed) {
    	arrange_layers(layer_view, layer_view->layer_surface, layer_view->layer_surface->output,
															&layer_view->layer_surface->current);
	    wlr_log(WLR_INFO, "Layer surface committed: %p", layer_view->layer_surface);
    }
}

static void layer_surface_map(struct wl_listener *listener, void *data) {
	(void)data;
    struct woodland_layer_view *layer_view = wl_container_of(listener, layer_view, map);
    if ((!layer_view) || (layer_view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_view' in 'layer_surface_map'!");
		return;
	}
    layer_view->mapped = true;
	///wlr_surface_send_enter(layer_view->layer_surface->surface,
	///							layer_view->layer_surface->output);
	wlr_xcursor_manager_set_cursor_image(layer_view->server->cursor_mgr, "left_ptr",
																	layer_view->server->cursor);
    wlr_log(WLR_INFO, "Layer surface mapped: %p", layer_view->layer_surface);
}

static void layer_surface_unmap(struct wl_listener *listener, void *data) {
	(void)data;
	///struct wlr_layer_surface_v1 *layer_surface = data;
    struct woodland_layer_view *layer_view = wl_container_of(listener, layer_view, unmap);
    if ((!layer_view) || (layer_view == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_view' in 'layer_surface_unmap'!");
		return;
	}
    layer_view->mapped = false;
	///wlr_surface_send_leave(layer_surface->surface, layer_surface->output);
	wlr_xcursor_manager_set_cursor_image(layer_view->server->cursor_mgr, "left_ptr",
																	layer_view->server->cursor);
    wlr_log(WLR_INFO, "Layer surface unmapped: %p", data);
}

static void server_new_layer_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
    if ((!layer_surface) || (layer_surface == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'layer_surface' in 'server_new_layer_surface'!");
		return;
	}
	wlr_log(WLR_INFO, "New layer surface created: %p", layer_surface);
	struct woodland_server *server = wl_container_of(listener, server, new_layer_surface);
    if ((!server) || (server == NULL)) {
		wlr_log(WLR_ERROR, "Error: Empty 'server' in 'server_new_layer_surface'!");
		return;
	}
	struct woodland_layer_view *layer_view = calloc(1, sizeof(struct woodland_layer_view));
	if (!layer_view) {
		wlr_log(WLR_ERROR, "Failed to allocate woodland_layer_view");
		return;
	}
	// Initialize listener links to prevent double removal issues
	layer_view->x = 0;
	layer_view->y = 0;
	layer_view->mapped = false;
	layer_view->server = server;
	layer_view->layer_surface = layer_surface;
    if (!layer_surface->output) {
		struct wlr_output *output = wlr_output_layout_output_at(layer_view->server->output_layout,
																layer_view->server->cursor->x,
																layer_view->server->cursor->y);
		if (output) {
			wlr_log(WLR_INFO, "Added output to layer surface: %p", layer_surface);
			layer_surface->output = output;
		}
		else {
			wlr_log(WLR_ERROR, "Failed to add output to layer surface: %p", layer_surface);
			free(layer_view);
			return;
		}
	}
	// Set up listeners for the layer surface signals	wl_list_init(&layer_view->map.link);
	wl_list_init(&layer_view->map.link);
	wl_list_init(&layer_view->unmap.link);
	wl_list_init(&layer_view->commit.link);
	wl_list_init(&layer_view->destroy.link);
	layer_view->commit.notify = wlr_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit, &layer_view->commit);
	layer_view->map.notify = layer_surface_map;
	wl_signal_add(&layer_surface->events.map, &layer_view->map);
	layer_view->unmap.notify = layer_surface_unmap;
	wl_signal_add(&layer_surface->events.unmap, &layer_view->unmap);
	layer_view->destroy.notify = layer_surface_destroy;
	wl_signal_add(&layer_surface->events.destroy, &layer_view->destroy);

	wl_list_insert(&server->layer_surfaces, &layer_view->link);

	arrange_layers(layer_view, layer_surface, layer_surface->output, &layer_surface->current);
	wlr_log(WLR_INFO, "Layer surface configured: %p", layer_surface);
}

/* Set background image function */
static int set_background_image_func(void *data) {
	struct woodland_server *server = data;
	if (!server) {
		wlr_log(WLR_ERROR, "Failed to get woodland_server!");
		return 1;
	}
	int width;
	int height;
	int channels;
	char *background_img = get_char_value_from_conf(server->config, "background");
	unsigned char *pixels = stbi_load(background_img, &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels) {
		wlr_log(WLR_ERROR, "No background image provided or Failed to load: %s", background_img);
		return 1;
	}
	server->background_texture = wlr_texture_from_pixels(server->renderer,
														DRM_FORMAT_ABGR8888,
														width * 4,
														width,
														height,
														pixels);
	stbi_image_free(pixels);
	free(background_img);
	background_img = NULL;
	if (!server->background_texture) {
		wlr_log(WLR_ERROR, "Failed to create texture from image: %s", background_img);
		return 1;
	}
	wl_event_source_remove(server->timer);
	return 0;
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
			wlr_log(WLR_INFO, "Launching command: %s\n", command[i]);
			run_cmd(command[i]);
		}
	}
	wl_event_source_remove(server->autostart_timer);
	return 0;
}

/* Main function */
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

	/* Getting zoom variables */
	server.pan_offset_x = 0;
	server.pan_offset_y = 0;
	server.zoom_factor = 1.0;
	server.zoom_speed = get_double_value_from_conf(server.config, "zoom_speed");
	server.zoom_top_edge = get_char_value_from_conf(server.config, "zoom_top_edge");
	server.zoom_edge_threshold = get_double_value_from_conf(server.config, "zoom_edge_threshold");

	/* Idle variable */
	server.idle_enabled = false;

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	/* Create the Wayland display */
	server.wl_display = wl_display_create();
	if (!server.wl_display) {
		wlr_log(WLR_ERROR, "Failed to create Wayland display!");
		return 1;
	}

	/* Create the seat for input devices */
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	if (!server.seat) {
		wlr_log(WLR_ERROR, "Failed to create seat!");
		wl_display_destroy(server.wl_display);
		return 1;
	}

	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	server.backend = wlr_backend_autocreate(server.wl_display);
	if (!server.backend) {
		wlr_log(WLR_ERROR, "Failed to create backend!");
		return 1;
	}

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_log(WLR_ERROR, "Failed to add Unix socket to Wayland display!");
		return 1;
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run the
	 * startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);

	/*** Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	server.renderer = wlr_renderer_autocreate(server.backend);
	if (!server.renderer) {
		wlr_log(WLR_ERROR, "Failed to create renderer!");
		return 1;
	}
	if (!wlr_renderer_init_wl_display(server.renderer, server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to initialize renderer with Wayland display!");
		return 1;
	}

	/*** Timer to set background image
	 * I didn't have much time to fiddle around with serial error:
	 * xdg_wm_base@8: error 4: wrong configure serial
	 * so I just added this timer as a workaround, it sets the background after
	 * a few seconds and thus it avoids the wrong serial error.
	 */
	struct wl_event_loop *event_loop = wl_display_get_event_loop(server.wl_display);
	if (!event_loop) {
		wlr_log(WLR_ERROR, "Failed to get event loop from Wayland display!");
		return 1;
	}
	server.timer = wl_event_loop_add_timer(event_loop, set_background_image_func, &server);
	if (!server.timer) {
		wlr_log(WLR_ERROR, "Failed to create timer!");
		return 1;
	}
	wl_event_source_timer_update(server.timer, 3000);

	/*** Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (!server.allocator) {
		wlr_log(WLR_ERROR, "Failed to create allocator!");
		return 1;
	}

	/*** This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the handling of the request_set_selection event below. */
	server.compositor = wlr_compositor_create(server.wl_display, server.renderer);
	if (!server.compositor) {
		wlr_log(WLR_ERROR, "Failed to create compositor!");
		return 1;
	}

	/*** Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	server.output_layout = wlr_output_layout_create();
	if (!server.output_layout) {
		wlr_log(WLR_ERROR, "Failed to create output layout!");
		return 1;
	}
	/*** Configure a listener to be notified when new outputs are available on the backend. */
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	if (!server.backend) {
		wlr_log(WLR_ERROR, "Backend is not initialized!");
		return 1;
	}
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	/*** Output manager */
	server.wlr_output_manager = wlr_output_manager_v1_create(server.wl_display);
	if (!server.wlr_output_manager) {
		wlr_log(WLR_ERROR, "Failed to create output manager!");
		return 1;
	}
	server.output_configuration_applied.notify = handle_output_configuration_applied;
	wl_signal_add(&server.wlr_output_manager->events.apply, &server.output_configuration_applied);
	server.output_configuration_tested.notify = handle_output_configuration_tested;
	wl_signal_add(&server.wlr_output_manager->events.test, &server.output_configuration_tested);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	/*** Initialize list for keyboards. */
	wl_list_init(&server.keyboards);

	/*** Configure a listener to be notified when new input devices are available
	 & on the backend.
	*/
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);

	/*** Configure a listener for seat cursor requests. */
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);

	/*** Configure a listener for seat selection requests. */
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

	/*** Drag and drop */
	server.start_drag.notify = seat_start_drag;
	wl_signal_add(&server.seat->events.start_drag, &server.start_drag);
	server.request_start_drag.notify = seat_request_start_drag;
	wl_signal_add(&server.seat->events.request_start_drag, &server.request_start_drag);

	/*** Idle timer */
	// Get timeout from confing file
	int idle_timeout = get_int_value_from_conf(server.config, "idle_timeout");
	// idle_timeout = 0 disabled the idle manager
	if (idle_timeout != 0) {
		/*** Initialize idle management features. */
		server.idle_enabled = true;

		/*** Create an idle manager for handling idle state. */
		server.idle = wlr_idle_create(server.wl_display);
		if (!server.idle) {
			wlr_log(WLR_ERROR, "Failed to create idle manager!");
			return 1;
		}

		/*** Create an idle timeout for the seat. */
		server.idle_timeout = wlr_idle_timeout_create(server.idle, server.seat, idle_timeout);
		if (!server.idle_timeout) {
			wlr_log(WLR_ERROR, "Failed to create idle timeout!");
			return 1;
		}

		/*** Configure event listeners for idle and resume events. */
		server.new_idle.notify = server_new_idle;
		wl_signal_add(&server.idle_timeout->events.idle, &server.new_idle);

		server.idle_resume.notify = server_idle_resume;
		wl_signal_add(&server.idle_timeout->events.resume, &server.idle_resume);
	}
	else {
		server.idle_enabled = false;
	}

	/* Set up our list of views and the xdg-shell. The xdg-shell is a Wayland
	 * protocol which is used for application windows. For more detail on
	 * shells, refer to the original authot article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	/*** Initialize lists for views and minimized views. */
	wl_list_init(&server.views);
	wl_list_init(&server.minimized_views);

	/*** Create an XDG shell and set up the new surface event listener. */
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display);
	if (!server.xdg_shell) {
		wlr_log(WLR_ERROR, "Failed to create XDG shell!");
		return 1;
	}
	server.new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

	/*** Create a cursor and attach it to the output layout. */
	server.cursor = wlr_cursor_create();
	if (!server.cursor) {
		wlr_log(WLR_ERROR, "Failed to create cursor!");
		return 1;
	}
	if (!server.output_layout) {
		wlr_log(WLR_ERROR, "Output layout is not initialized!");
		return 1;
	}
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);


    // Get the XCURSOR_SIZE environment variable
    const char *env_cursor_size = getenv("XCURSOR_SIZE");
    int cursor_size = 48; // Default cursor size
    // If the environment variable is set, use its value
    if (env_cursor_size != NULL) {
        int env_size = atoi(env_cursor_size);
        if (env_size > 0) { // Ensure the value is valid
            cursor_size = env_size;
        }
    }
	// Creates an xcursor manager and loads the theme
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, cursor_size);
	if (!server.cursor_mgr) {
		wlr_log(WLR_ERROR, "Failed to create XCursor manager.");
		return 1;
	}
	wlr_xcursor_manager_load(server.cursor_mgr, 1);
	if (!server.cursor_mgr) {
		wlr_log(WLR_ERROR, "Failed to load XCursor manager.");
		return 1;
	}
	// Set the initial cursor image
	wlr_xcursor_manager_set_cursor_image(server.cursor_mgr, "left_ptr", server.cursor);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in the
	 * original authot input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	/*** Configure event listeners for cursor events. */

	/*** Cursor motion event listener. */
	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);

	/*** Cursor absolute motion event listener. */
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);

	/*** Cursor button event listener. */
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);

	/*** Cursor axis event listener. */
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);

	/*** Cursor frame event listener. */
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);


	/*** Initialize list for layer surfaces. */
	wl_list_init(&server.layer_surfaces);

	/*** Create layer shell and configure a listener for new layer surfaces. */
	server.layer_shell = wlr_layer_shell_v1_create(server.wl_display);
	if (!server.layer_shell) {
		wlr_log(WLR_ERROR, "Failed to create layer shell!");
		return 1;
	}
	server.new_layer_surface.notify = server_new_layer_surface;
	wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_surface);

	/*** Create virtual keyboard manager and configure a listener for new virtual keyboards. */
	server.virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(server.wl_display);
	if (!server.virtual_keyboard_mgr) {
		wlr_log(WLR_ERROR, "Failed to create virtual keyboard manager!");
		return 1;
	}
	server.new_virtual_keyboard.notify = new_virtual_keyboard_handler;
	wl_signal_add(&server.virtual_keyboard_mgr->events.new_virtual_keyboard,
											  &server.new_virtual_keyboard);

	/*** Initialize data-related interfaces. */
	if (!wlr_viewporter_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to create viewporter!");
		return 1;
	}
	if (!wlr_data_device_manager_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to create data device manager!");
		return 1;
	}
	if (!wlr_screencopy_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to create screencopy manager!");
		return 1;
	}
	if (!wlr_data_control_manager_v1_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Failed to create data control manager!");
		return 1;
	}
	if (!wlr_xdg_output_manager_v1_create(server.wl_display, server.output_layout)) {
		wlr_log(WLR_ERROR, "Failed to create XDG output manager!");
		return 1;
	}
	server.wlr_foreign_toplevel_mgr = wlr_foreign_toplevel_manager_v1_create(server.wl_display);
	if (!server.wlr_foreign_toplevel_mgr) {
		wlr_log(WLR_ERROR, "Failed to create foreign toplevel manager!");
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
	if (startup_cmd) {
		if (fork() == 0) {
			run_cmd(startup_cmd);
		}
	}
	else {
		/*** Startup commands after delay */
		server.autostart_timer = wl_event_loop_add_timer(event_loop,
														 process_startup_commands,
														 &server);
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

	// Clean up signals (assuming signal cleanup functions are available)
	// Free allocated memory
	if (server.zoom_top_edge) {
		free(server.zoom_top_edge);
		server.zoom_top_edge = NULL;
	}
	if (server.config) {
		free(server.config);
		server.config = NULL;
	}

	// Destroy wlroots objects in reverse order of their creation
	if (server.idle_timeout) {
		wlr_idle_timeout_destroy(server.idle_timeout);
		server.idle_timeout = NULL;
	}
	if (server.cursor_mgr) {
		wlr_xcursor_manager_destroy(server.cursor_mgr);
		server.cursor_mgr = NULL;
	}
	if (server.allocator) {
		wlr_allocator_destroy(server.allocator);
		server.allocator = NULL;
	}
	if (server.cursor) {
		wlr_cursor_destroy(server.cursor);
		server.cursor = NULL;
	}
	if (server.renderer) {
		wlr_renderer_destroy(server.renderer);
		server.renderer = NULL;
	}
	if (server.backend) {
		wlr_backend_destroy(server.backend);
		server.backend = NULL;
	}
	if (server.output_layout) {
		wlr_output_layout_destroy(server.output_layout);
		server.output_layout = NULL;
	}
	if (server.seat) {
		wlr_seat_destroy(server.seat);
		server.seat = NULL;
	}
	if (server.wl_display) {
		wl_display_destroy_clients(server.wl_display);
		wl_display_flush_clients(server.wl_display);
		wl_display_destroy(server.wl_display);
		server.wl_display = NULL;
	}
	wlr_log(WLR_INFO, "See you next time in Woodland :)");

	return 0;
}
