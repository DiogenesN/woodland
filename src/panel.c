// SPDX-License-Identifier: GPL-2.0-or-later

#include "panel.h"
#include "woodland.h"
#include "dbus-network-management.h"

/* Panel Icons */
RsvgHandle *svgSound = NULL;
RsvgHandle *svgBrightness = NULL;
RsvgHandle *svgNetwork = NULL;

static struct wlr_drm_format format = {
	.format = DRM_FORMAT_ARGB8888,
	.len = 1,
	.capacity = 1,
	.modifiers = (uint64_t[]) { DRM_FORMAT_MOD_LINEAR },
};

/**
 * Showing a panel at the botton right corner of the screen
 */
static void create_cairo(struct woodland_server *server, int panel_width, int panel_height) {
	if (!server->cairo_surface) {
		server->cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, panel_width, panel_height);
		if (cairo_surface_status(server->cairo_surface) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Cairo status failed in 'panel_setup'!");
			return;
		}
	}

	if (!server->cr) {
		server->cr = cairo_create(server->cairo_surface);
		if (cairo_status(server->cr) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Cairo create failed in 'panel_setup'!\n");
			cairo_surface_destroy(server->cairo_surface);
			return;
		}
	}
}

static void destroy_cairo(struct woodland_server *server) {
	if (server->cr) {
		cairo_destroy(server->cr);
		server->cr = NULL;
	}

	if (server->font_face) {
		cairo_font_face_destroy(server->font_face);
		server->font_face = NULL;
	}

	if (server->cairo_surface) {
		cairo_surface_flush(server->cairo_surface);
		cairo_surface_destroy(server->cairo_surface);
		server->cairo_surface = NULL;
	}
}

static void create_panel_buffer(struct woodland_server *server, int panel_width, int panel_height) {
	// Creating the buffer
	server->wlr_panel_buffer = wlr_allocator_create_buffer(server->allocator,
														panel_width,
														panel_height,
														&format);
	if (!server->wlr_panel_buffer) {
		fprintf(stderr, "wlr_panel_buffer failed in 'panel'!");
		return;
	}

	server->panel_buffer = wlr_scene_buffer_create(&server->scene->tree, server->wlr_panel_buffer);
	if (!server->panel_buffer) {
		fprintf(stderr, "wlr_scene_buffer failed in 'panel'!");
		return;
	}
	// Set the enum title for the panel
	if (server->panel_buffer) {
		server->panel_buffer->node.data = (void *)(uintptr_t)NODE_TYPE_PANEL;
	}
}

void panel_setup(struct woodland_server *server,
				struct wlr_output *output,
				int panel_width,
				int panel_height) {
	if (!server->panel_buffer) {
		create_panel_buffer(server, panel_width, panel_height);
	}
	server->panel_scene_output = wlr_scene_get_scene_output(server->scene, output);
	if (!server->panel_scene_output) {
		fprintf(stderr, "Failed to create 'server->panel_scene_output' in 'panel_setup'!\n");
		return;
	}
	destroy_cairo(server);
	create_cairo(server, panel_width, panel_height);
}

// Updates the time widget every second
int update_time(void *data) {
	if (!data) {
		fprintf(stderr, "No 'data' in 'update_time'!\n");
		return 1;
	}
	struct woodland_server *server = data;
	if (!server) {
		fprintf(stderr, "No 'server' in 'update_time'!\n");
		return 1;
	}
	if (!server->time_update_timer) {
    	fprintf(stderr, "No 'time_update_timer' in 'update_time'!\n");
		return 1;
	}
	if (server->panel_is_hidden) {
		// Don't continue or schedule further updates
		fprintf(stderr, "update_time exiting because panel is hidden.\n");
		return 1; // non-zero return value removes the event source
	}
	
	// Calling panel every second
	///destroy_cairo(server);
	///create_cairo(server, PPANEL_WIDTH, PPANEL_HEIGHT);
	panel(server, PPANEL_WIDTH, PPANEL_HEIGHT);

	if (server->time_update_timer) {
		wl_event_source_timer_update(server->time_update_timer, 1000); // 1000ms = 1s
	}
	return 0;
}

// Getting current time function
static void get_current_time(char buf[6]) {
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buf, 6, "%R", timeinfo);
}

/**************************** Network applet ****************************/
void clean_ssids(struct woodland_server *server) {
	for (size_t i = 0; i < 256; i++) {
		free(server->ssids[i]); // safe even if NULL
		server->ssids[i] = NULL;
	}
	///fprintf(stderr, "Cleaning ssids done!\n");
}

static void create_network_applet(struct woodland_server *server) {
	int cwidth = PNETWORK_WIDTH;
	int cheight = PNETWORK_HEIGHT;

	// Creating the buffer
	struct wlr_buffer *wlr_buffer = wlr_allocator_create_buffer(server->allocator, cwidth, cheight, &format);
	if (wlr_buffer == NULL) {
		fprintf(stderr, "Failes to create buffer in 'create_network_applet'!\n");
		return;
	}
	server->network_buffer = wlr_scene_buffer_create(&server->scene->tree, wlr_buffer);

	// Initialize position and enable state
	wlr_scene_node_set_enabled(&server->network_buffer->node, true);
	wlr_scene_node_raise_to_top(&server->network_buffer->node);

	// Setting the enum title for the netwoek applet
	server->network_buffer->node.data = (void *)(uintptr_t)NODE_TYPE_NETWORK_APPLET;

	// Drop the reference, the scene node will hold it
	wlr_buffer_drop(wlr_buffer);
}

static int wifi_scan_complete(void *data) {
	struct woodland_server *server = data;

	clean_ssids(server);

	///fprintf(stderr, "Scan finished, collecting networks...\n");
	server->number_of_ssids = list_wifi_devices(server->ssids, 256, false);

	if (server->number_of_ssids <= 0) {
		server->ssids[0] = strdup("No networks found! Is wifi enabled?");
	}

	if (server->wifi_scan_timer) {
		wl_event_source_remove(server->wifi_scan_timer);
		server->wifi_scan_timer = NULL;
	}
	///fprintf(stderr, "Scanning done!\n");
	return 0; // one-shot timer
}

/* Refresh Wi-Fi network list */
int refresh_networks(void *data) {
	struct woodland_server *server = data;
	// Trigger scan (passing 256 as max to be safe)

	clean_ssids(server);
	server->ssids[0] = strdup("Please wait, scanning ...");

	server->number_of_ssids = list_wifi_devices(server->ssids, 256, true);

	if (auth_success && !server->network_password_prompt) {
		free(server->ssids[0]);
		server->ssids[0] = NULL;
		server->ssids[0] = strdup("Please wait, scanning ...");
		auth_success = false;
	}
	else if (auth_success && server->network_password_prompt) {
		free(server->ssids[0]);
		server->ssids[0] = NULL;
		server->ssids[0] = strdup("Successfully Connected!");
		auth_success = false;
		server->network_password_prompt = false;
	}
	else if (!auth_success && server->network_password_prompt) {
		free(server->ssids[0]);
		server->ssids[0] = NULL;
		server->ssids[0] = strdup("wrong password, try again!");
		auth_success = false;
		server->network_password_prompt = false;
	}

	// Timer logic.
	if (!server->wifi_scan_timer) {
		server->wifi_scan_timer = wl_event_loop_add_timer(server->event_loop, wifi_scan_complete, server);
	}
	wl_event_source_timer_update(server->wifi_scan_timer, 10000);
	server->network_applet_was_activated = true;
	return 0;
}

static void show_network_applet(struct woodland_server *server) {
	if (!server->network_buffer) {
		create_network_applet(server);
	}

	if (server->network_is_clicked && !server->network_applet_was_activated) {
		///refresh_networks(server);
		clean_ssids(server);
		server->ssids[0] = strdup("Please wait, scanning ...");
		server->number_of_ssids = list_wifi_devices(server->ssids, 256, true);

		// Timer logic.
		if (!server->wifi_scan_timer) {
			server->wifi_scan_timer = wl_event_loop_add_timer(server->event_loop, wifi_scan_complete, server);
		}
		wl_event_source_timer_update(server->wifi_scan_timer, 10000);
		server->network_applet_was_activated = true;
	}

	int cwidth = PNETWORK_WIDTH;
	int cheight = PNETWORK_HEIGHT;
	int output_width = server->transformed_width;
	int output_height = server->transformed_height;
	int right_corner_x = output_width - (cwidth + 3);
	int right_corner_y = output_height - (PPANEL_HEIGHT + cheight + 3);

	cairo_surface_t *ccairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cwidth, cheight);
	if (cairo_surface_status(ccairo_surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Cairo status failed in 'panel show_network_applet'!\n");
		return;
	}

	cairo_t *ccr = cairo_create(ccairo_surface);
	if (cairo_status(ccr) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Cairo create failed in 'show_network_applet'!\n");
		cairo_surface_destroy(ccairo_surface);
		return;
	}

	// Draw buffer background
	cairo_set_source_rgba(ccr, 0.22, 0.22, 0.22, 1.0); // dark grey background
	cairo_paint(ccr);

	/******************************************************************************************/
	// Calculating the size to place the text in the center
	// Get the width of the widget and font size
	int widget_width = cwidth;
	double font_size = 15.0;
	int text_pos_y = 25;

	// 1. Pre-calculate the "state" once before the loop
	bool is_no_networks = (server->ssids[0] && strcmp(server->ssids[0],
							"No networks found! Is wifi enabled?") == 0);
	bool is_wrong_pass = (server->ssids[0] && strcmp(server->ssids[0], "wrong password, try again!") == 0);
	bool is_scanning   = (server->ssids[0] && strcmp(server->ssids[0], "Please wait, scanning ...") == 0);
	bool is_error      = (is_no_networks || is_wrong_pass);
	bool is_valid_list = (!is_error && !is_scanning);

	cairo_set_font_size(ccr, font_size);

	for (int i = 0; server->ssids[i] != NULL; i++) {
		cairo_text_extents_t extents;
		cairo_text_extents(ccr, server->ssids[i], &extents);

		double x_position = (widget_width - extents.width) / 2.0;

		// --- Logic: Determine Text Color ---
		if (is_error && i == 0) {
			cairo_set_source_rgb(ccr, 0.9, 0.3, 0.3); // Error Red
		}
		else if (i == server->SsidPosition && is_valid_list) {
			cairo_set_source_rgb(ccr, 0.4, 0.6, 0.9); // Hover Blue
		}
		else if (i == 0 && is_valid_list) {
			cairo_set_source_rgb(ccr, 0.4, 0.9, 0.3); // Connected Green
		}
		else {
			cairo_set_source_rgb(ccr, 1.0, 1.0, 1.0); // Default White
		}

		// --- Draw once ---
		cairo_move_to(ccr, x_position, text_pos_y);
		cairo_show_text(ccr, server->ssids[i]);
		cairo_surface_flush(ccairo_surface);
		text_pos_y += 26;
	}
	/******************************************************************************************/
	// Constructing the buffer and showing it on the screen
	const void *cairo_data = cairo_image_surface_get_data(ccairo_surface);

	// Update texture
	if (server->network_buffer->texture) {
		wlr_texture_destroy(server->network_buffer->texture);
		server->network_buffer->texture = NULL;
	}
	server->network_buffer->texture = wlr_texture_from_pixels(server->renderer,
													DRM_FORMAT_ARGB8888,
													cwidth * 4,
													cwidth,
													cheight,
													cairo_data);

	if (!server->network_buffer->texture) {
		cairo_destroy(ccr);
		cairo_surface_flush(ccairo_surface);
		cairo_surface_destroy(ccairo_surface);
		ccr = NULL;
		ccairo_surface = NULL;
		return;
	}

	wlr_scene_node_set_position(&server->network_buffer->node, right_corner_x, right_corner_y);
	wlr_scene_node_raise_to_top(&server->network_buffer->node);
	wlr_scene_node_set_enabled(&server->network_buffer->node, true);

	cairo_surface_flush(ccairo_surface);
	cairo_destroy(ccr);
	cairo_surface_destroy(ccairo_surface);
	ccr = NULL;
	ccairo_surface = NULL;
	cairo_data = NULL;
}

/**************************** Calendar ****************************/
static void create_calendar(struct woodland_server *server) {
	int cwidth = 330;
	int cheight = 330;

	// Creating the buffer
	struct wlr_buffer *wlr_buffer = wlr_allocator_create_buffer(server->allocator, cwidth, cheight, &format);
	if (!wlr_buffer) {
		fprintf(stderr, "Failed to create wlr_buffer in 'create_calendar'!\n");
		return;
	}
	server->calendar_buffer = wlr_scene_buffer_create(&server->scene->tree, wlr_buffer);
	wlr_buffer_drop(wlr_buffer); // Drop the reference, the scene node will hold it

	// Initialize position and enable state
	wlr_scene_node_set_enabled(&server->calendar_buffer->node, false);
}

static void show_calendar(struct woodland_server *server) {
	if (!server->calendar_buffer) {
		create_calendar(server);
	}

	int cwidth = 330;
	int cheight = 330;
	int output_width = server->transformed_width;
	int output_height = server->transformed_height;
	int right_corner_x = output_width - (cwidth + 3);
	int right_corner_y = output_height - (PPANEL_HEIGHT + cheight + 3);

	cairo_surface_t *ccairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cwidth, cheight);
	if (cairo_surface_status(ccairo_surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Cairo status failed in 'panel calendar'!");
		return;
	}
	cairo_t *ccr = cairo_create(ccairo_surface);
	if (cairo_status(ccr) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Cairo create failed in 'panel calendar'!");
		cairo_surface_destroy(ccairo_surface);
		return;
	}

	// Draw buffer background
	cairo_set_source_rgba(ccr, 0.22, 0.22, 0.22, 1.0); // dark grey background
	cairo_paint(ccr);

	/******************************************************************************************/
	// Get current time (thread-safe)
	time_t rawtime;
	time(&rawtime);

	struct tm now;
	localtime_r(&rawtime, &now);
	int current_day   = now.tm_mday;

	// Compute first day of month
	struct tm first = now;
	first.tm_mday = 1;
	mktime(&first);

	// Convert Sunday=0 to Monday=0 layout
	int first_day_of_week = first.tm_wday;
	first_day_of_week = (first_day_of_week == 0) ? 6 : first_day_of_week - 1;

	// Compute number of days in month (leap-year correct)
	struct tm next_month = first;
	next_month.tm_mon += 1;
	next_month.tm_mday = 1;
	mktime(&next_month);

	next_month.tm_mday = 0;   // last day of current month
	mktime(&next_month);

	int days_in_month = next_month.tm_mday;

	// Precompute weekday abbreviations (Monday-first order)
	const char *weekday_names[7] = {
		"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
	};

	// Layout
	int rows = 6;
	int cols = 7;

	int cell_width  = (cwidth  / cols);
	int cell_height = (cheight / (rows + 1));

	// Cairo setup
	cairo_set_font_face(ccr, server->font_face);
	cairo_set_source_rgb(ccr, 1.0, 1.0, 1.0);
	cairo_set_line_width(ccr, 1.0);

	// Render grid
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {

			int index = row * cols + col;
			int day = index - first_day_of_week + 1;

			if (day < 1 || day > days_in_month)
				continue;

			int x = col * cell_width;
			int y = row * cell_height;

			// Draw cell border
			cairo_rectangle(ccr, x, y, cell_width, cell_height);
			cairo_stroke(ccr);

			// Highlight current day
			if (day == current_day) {
				cairo_save(ccr);

				cairo_set_source_rgb(ccr, 1.0, 0.0, 0.0);
				cairo_set_line_width(ccr, 4.0);

				cairo_rectangle(
					ccr,
					x + 2,
					y + 2,
					cell_width  - 4,
					cell_height - 4
				);
				cairo_stroke(ccr);

				cairo_restore(ccr);
			}

			/// Compute weekday index (Monday-first)
			int weekday = (first_day_of_week + (day - 1)) % 7;
			const char *day_name = weekday_names[weekday];

			// ---- Draw weekday abbreviation (centered upper half) ----
			cairo_set_font_size(ccr, 15.0);

			cairo_text_extents_t ext;
			cairo_text_extents(ccr, day_name, &ext);

			double tx = x + (cell_width  - ext.width)  / 2 - ext.x_bearing;
			double ty = y + (cell_height * 0.35) + ext.height / 2;

			cairo_move_to(ccr, tx, ty);
			cairo_show_text(ccr, day_name);

			// ---- Draw date number (centered lower half) ----
			char date_str[12];
			snprintf(date_str, sizeof(date_str), "%d", day);

			cairo_set_font_size(ccr, 18.0);
			cairo_text_extents(ccr, date_str, &ext);

			tx = x + (cell_width  - ext.width)  / 2 - ext.x_bearing;
			ty = y + (cell_height * 0.75) + ext.height / 2;

			cairo_move_to(ccr, tx, ty);
			cairo_show_text(ccr, date_str);
		}
	}

	// Bottom full date string
	char date_buffer[128];
	strftime(date_buffer, sizeof(date_buffer), "%A, %d %B %Y", &now);

	cairo_set_font_size(ccr, 20.0);

	cairo_text_extents_t extents;
	cairo_text_extents(ccr, date_buffer, &extents);

	double text_x = (cwidth - extents.width) / 2 - extents.x_bearing;
	double text_y = (cheight - 10);

	cairo_move_to(ccr, text_x, text_y);
	cairo_show_text(ccr, date_buffer);

	/// Outer frame
	cairo_set_line_width(ccr, 3.0);
	cairo_rectangle(ccr, 0, 0, cwidth, cheight);
	cairo_stroke(ccr);
	/******************************************************************************************/
	// Constructing the buffer and showing it on the screen
	const void *cairo_data = cairo_image_surface_get_data(ccairo_surface);

	// Update texture
	if (server->calendar_buffer->texture) {
		wlr_texture_destroy(server->calendar_buffer->texture);
		server->calendar_buffer->texture = NULL;
	}
	server->calendar_buffer->texture = wlr_texture_from_pixels(server->renderer,
													DRM_FORMAT_ARGB8888,
													cwidth * 4,
													cwidth,
													cheight,
													cairo_data);

	if (!server->calendar_buffer->texture) {
		// Handle the case where texture creation fails
		cairo_destroy(ccr);
		cairo_surface_flush(ccairo_surface);
		cairo_surface_destroy(ccairo_surface);
		return;
	}

	wlr_scene_node_set_position(&server->calendar_buffer->node, right_corner_x, right_corner_y);
	wlr_scene_node_raise_to_top(&server->calendar_buffer->node);
	wlr_scene_node_set_enabled(&server->calendar_buffer->node, true);

	cairo_destroy(ccr);
	cairo_surface_flush(ccairo_surface);
	cairo_surface_destroy(ccairo_surface);
	ccr = NULL;
	ccairo_surface = NULL;
}

/**************************** Time ****************************/
// This approach excludes any memory leaks concerning Cairo
static void show_time(struct woodland_server *server, int panel_width, int panel_height) {
	char currentTime[6];
	get_current_time(currentTime);
	cairo_set_font_size(server->cr, 24);
	cairo_set_source_rgb(server->cr, 1, 1, 1);
	cairo_move_to(server->cr, panel_width - 170, (panel_height / 2) + 9);
	cairo_show_text(server->cr, currentTime);
	cairo_surface_flush(server->cairo_surface);
}

/**************************** Volume ****************************/
static void show_volume(struct woodland_server *server, int panel_width, int panel_height) {
	const RsvgRectangle rectSound = {
		.x =  panel_width - 42,
		.y = (panel_height / 2) - 19,
		.width = 40,
		.height = 40,
	};

	svgSound = rsvg_handle_new_from_file(server->volumeHigh, NULL);
	if (!svgSound) {
		fprintf(stderr, "failed to get svgSound\n");
		return;
	}
	rsvg_handle_render_document(svgSound, server->cr, &rectSound, NULL);	
	cairo_surface_flush(server->cairo_surface);
	g_object_unref(svgSound);
	svgSound = NULL;
}

/**************************** Brightness ****************************/
static void show_brightness(struct woodland_server *server, int panel_width, int panel_height) {
	const RsvgRectangle rectBrightness = {
		.x =  panel_width - 93,
		.y = (panel_height / 2) - 19,
		.width = 40,
		.height = 40,
	};

	svgBrightness = rsvg_handle_new_from_file(server->brightnessIcon, NULL);
	if (!svgBrightness) {
		fprintf(stderr, "failed to get svgBrightness\n");
		return;
	}
	rsvg_handle_render_document(svgBrightness, server->cr, &rectBrightness, NULL);	
	cairo_surface_flush(server->cairo_surface);
	g_object_unref(svgBrightness);
	svgBrightness = NULL;
}

/**************************** Network ****************************/
static void show_network(struct woodland_server *server, int panel_width, int panel_height) {
	const RsvgRectangle rectNetwork = {
		.x =  panel_width - 224,
		.y = (panel_height / 2) - 17,
		.width = 34,
		.height = 34,
	};

	svgNetwork = rsvg_handle_new_from_file(server->networkIcon, NULL);
	if (!svgNetwork) {
		fprintf(stderr, "failed to get svgNetwork\n");
		return;
	}
	rsvg_handle_render_document(svgNetwork, server->cr, &rectNetwork, NULL);	
	cairo_surface_flush(server->cairo_surface);
	g_object_unref(svgNetwork);
	svgNetwork = NULL;
}

/**
 **************************** Panel ****************************
 */
// This function runs in a loop every second
void panel(struct woodland_server *server, int panel_width, int panel_height) {
	if (!server) {
		fprintf(stderr, "No 'time_update_timer' or 'server' in 'panel'!\n");
		return;
	}
	if (!server->cairo_surface && server->panel_is_hidden) {
		///fprintf(stderr, "Panel is hidden, do nothing.\n");
		if (server->panel_buffer) {
			wlr_scene_node_set_enabled(&server->panel_buffer->node, false);
		}
		if (server->network_buffer) {
			wlr_scene_node_set_enabled(&server->network_buffer->node, false);
		}
		if (server->calendar_buffer) {
			wlr_scene_node_set_enabled(&server->calendar_buffer->node, false);
		}
		destroy_cairo(server);
		return;
	}
	else if (server->cairo_surface && !server->panel_is_hidden) {
		// Draw buffer background
		cairo_set_source_rgba(server->cr, 0.22, 0.22, 0.22, 1.0); // dark grey background
		cairo_paint(server->cr);
		cairo_surface_flush(server->cairo_surface);

		// Widgets
		// Time widget
		show_time(server, panel_width, panel_height);

		// Volume widget
		show_volume(server, panel_width, panel_height);

		// Brightness widget
		show_brightness(server, panel_width, panel_height);

		// Network widget
		show_network(server, panel_width, panel_height);

		// Calendar widget
		if (!server->calendar_was_activated && server->time_is_clicked) {
			///fprintf(stderr, "Open calendar\n");
			show_calendar(server);
			server->calendar_was_activated = true;
		}
		if (server->calendar_buffer && !server->time_is_clicked && server->calendar_was_activated) {
			///fprintf(stderr, "Close calendar\n");
			wlr_scene_node_set_enabled(&server->calendar_buffer->node, false);
			server->calendar_was_activated = false;
		}

		// Network applet widget
		if (server->network_is_clicked) {
			///fprintf(stderr, "Open network applet\n");
			show_network_applet(server);
			server->network_was_activated = true;
		}
		if (server->network_buffer && !server->network_is_clicked && server->network_was_activated) {
			///fprintf(stderr, "Close network applet\n");
			wlr_scene_node_set_enabled(&server->network_buffer->node, false);
			server->network_was_activated = false;
			server->network_applet_was_activated = false;
		}

		// Constructing the buffer and showing it on the screen
		server->cairo_data = cairo_image_surface_get_data(server->cairo_surface);

		// Destroy previous texture (if any)
		if (server->panel_buffer && server->panel_buffer->texture) {
			wlr_texture_destroy(server->panel_buffer->texture);
			server->panel_buffer->texture = NULL;
		}

		server->panel_buffer->texture = wlr_texture_from_pixels(server->renderer,
														DRM_FORMAT_ARGB8888,
														panel_width * 4,
														panel_width,
														panel_height,
														server->cairo_data);

		if (!server->panel_buffer->texture) {
			fprintf(stderr, "Panel texture failed in 'panel'!\n");
			cairo_destroy(server->cr);
			cairo_surface_destroy(server->cairo_surface);
			return;
		}
		// Position the panel at the right bottom corner
		int output_width = server->transformed_width;
		int output_height = server->transformed_height;
		int right_corner_x = output_width - PPANEL_WIDTH;
		int right_corner_y = output_height - PPANEL_HEIGHT;

		// Upload text to buffer texture
		if (server->panel_buffer) {
			wlr_scene_node_set_position(&server->panel_buffer->node, right_corner_x, right_corner_y);
			wlr_scene_node_raise_to_top(&server->panel_buffer->node);
			// Rendering
			// We need to toggle the node enable/disable for it to update
			// it's content (i couldn't find another solution)
			wlr_scene_node_set_enabled(&server->panel_buffer->node, false);
			wlr_scene_node_set_enabled(&server->panel_buffer->node, true);
		}
	}
}
