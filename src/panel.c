// SPDX-License-Identifier: GPL-2.0-or-later

#include "woodland.h"
#include "dbus-network-management.h"

struct wl_event_source *network_scan_timer;

static bool calendar_was_activated = false;
static bool network_applet_was_activated = false;

/* Panel Icons */
RsvgHandle *svgSound = NULL;
RsvgHandle *svgBrightness = NULL;
RsvgHandle *svgNetwork = NULL;

int update_time(void *data);
static void panel(struct woodland_server *server, int panel_width, int panel_height);

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
		cairo_font_face_destroy(server->font_face);
		server->cr = NULL;
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
	// Set the title for the panel
	const char *my_string = "woodland_panel";
	server->panel_buffer->node.data = (void *)my_string;
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
	destroy_cairo(server);
	create_cairo(server, PPANEL_WIDTH, PPANEL_HEIGHT);
	panel(server, PPANEL_WIDTH, PPANEL_HEIGHT);

	if (server->time_update_timer) {
		wl_event_source_timer_update(server->time_update_timer, 1000); // 1000ms = 1s
	}
	return 0;
}

// Getting current time function
static char *get_current_time() {
	time_t rawtime;
	struct tm *timeinfo;
	char *current_time = malloc(6 * sizeof(char));
	if (current_time == NULL) {
		fprintf(stderr, "Failed to allocate memory for current_time\n");
		return NULL;
	}
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(current_time, 6, "%R", timeinfo);
	return current_time;
}

/**************************** Network applet ****************************/
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

	// Setting the title for the applet
	const char *my_string = "woodland_network_applet";
	server->network_buffer->node.data = (void *)my_string;

	// Drop the reference, the scene node will hold it
	wlr_buffer_drop(wlr_buffer);
}

static int scan_network(void *data) {
	struct woodland_server *server = data;
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
	wl_event_source_remove(network_scan_timer);
	return 0;
}

static void show_network_applet(struct woodland_server *server) {
	if (!server->network_buffer) {
		create_network_applet(server);
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
	char dateBuffer[256];
	double text_width = 0;
	double x_position = 0;
	// Get the width of the widget and font size
	int widget_width = cwidth;
	double font_size = 15.0;
	int text_pos_y = 25;

	if (server->network_is_clicked && !network_applet_was_activated) {
		if (server->ssids[0] != NULL) {
			for (size_t i = 0; server->ssids[i] != NULL; i++) {
				///fprintf(stderr, "In 'show_network_applet' freeing up SSID[%zu]: %s\n", i, server->ssids[i]);
				free(server->ssids[i]); // Don't forget to free
				server->ssids[i] = NULL;
			}
		}
		server->ssids[0] = strdup("Please wait, scanning ...");
		network_applet_was_activated = true;
		network_scan_timer = wl_event_loop_add_timer(server->event_loop, scan_network, server);
		wl_event_source_timer_update(network_scan_timer, 3000);
	}
	///fprintf(stderr, "server->ssids[0]: %s\n", server->ssids[0]);
	for (int i = 0; server->ssids[i] != NULL; i++) {
		///fprintf(stderr, "ssids[%d]: %s\n", i, ssids[i]);
		snprintf(dateBuffer, sizeof(dateBuffer), "%s", server->ssids[i]);
		// Set font size and calculate text extents
		cairo_set_font_size(ccr, font_size);
		cairo_text_extents_t extents;
		cairo_text_extents(ccr, dateBuffer, &extents);

		// Calculate x position to center the text in the widget
		text_width = extents.width;
		x_position = (widget_width - text_width) / 2.0;
		// Highlight as green the currently active network connection
		if (i == 0 &&
			server->ssids[0] != NULL &&
			strcmp(server->ssids[0], "No networks found! Is wifi enabled?") != 0 &&
			strcmp(server->ssids[0], "Please wait, scanning ...") != 0) {
			cairo_set_source_rgb(ccr, 0.4, 0.9, 0.3); // light green
		}
		// Highlight as red all the errors
		else if (server->ssids[0] != NULL &&
				strcmp(server->ssids[0], "No networks found! Is wifi enabled?") == 0) {
			cairo_set_source_rgb(ccr, 0.9, 0.3, 0.3); // light red
		}
		else {
			cairo_set_source_rgb(ccr, 1, 1, 1); // white text
		}
		cairo_move_to(ccr, x_position, text_pos_y);
		cairo_show_text(ccr, server->ssids[i]);
		// Highlight currently hovered
		if (server->ssids[0] != NULL &&
			strcmp(server->ssids[0], "No networks found! Is wifi enabled?") != 0 &&
			strcmp(server->ssids[0], "Please wait, scanning ...") != 0 &&
			server->ssids[server->SsidPosition] != NULL &&
			i == server->SsidPosition) {
			cairo_move_to(ccr, x_position, text_pos_y);
			cairo_set_source_rgb(ccr, 0.4, 0.6, 0.9); // light blue
			cairo_show_text(ccr, server->ssids[server->SsidPosition]);
		}

		text_pos_y = text_pos_y + 26;
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

	cairo_destroy(ccr);
	cairo_surface_flush(ccairo_surface);
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
	/// Get current time
	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	int current_day = timeinfo->tm_mday; // Day of the month

	cairo_set_font_face(ccr, server->font_face);
	cairo_set_font_size(ccr, 15.0);
	cairo_set_source_rgb(ccr, 1.0, 1.0, 1.0); // White text color
	cairo_set_line_width(ccr, 1.0); // Set a default line width for the grid

	// Calculate cell width and height for the calendar grid
	int cell_width = cwidth / 7; // Assuming a week starts on Sunday
	int cell_height = cheight / 7;

	/// Calculate the day of the week for the first day of the month
	struct tm first_day_tm = *timeinfo;
	first_day_tm.tm_mday = 1;
	mktime(&first_day_tm);

	int first_day_of_week = first_day_tm.tm_wday; // 0 = Sunday, 1 = Monday, ...
	if (first_day_of_week == 0) { // Adjust for Monday being the first day
		first_day_of_week = 6; // Sunday becomes 6
	}
	else {
		first_day_of_week--; // Shift other days back by one
	}

	/// Draw calendar grid
	for (int row = 0; row < 6; row++) {
		for (int col = 0; col < 7; col++) {
			int day = (row * 7) + col - first_day_of_week + 1; // Day of the month
			if (day <= 31 && day >= 1) {
				int x = col * cell_width;
				int y = row * cell_height;
				cairo_rectangle(ccr, x, y, cell_width, cell_height);
				cairo_stroke(ccr); // Stroke the rectangle

				/// Highlight current day with stroke
				if (day == current_day) {
					cairo_set_source_rgb(ccr, 1.0, 0.0, 0.0); // Red color for stroke
					cairo_set_line_width(ccr, 5.0); // Adjust the line width as needed
					/// Adjust the rectangle dimensions to fit within the stroke
					cairo_rectangle(ccr, x + 1, y + 1, cell_width - 2, cell_height - 2);
					cairo_stroke(ccr);
				}
				cairo_set_line_width(ccr, 1.0); /// thickness of squares frame
				cairo_set_source_rgb(ccr, 1.0, 1.0, 1.0); // White text color
				cairo_move_to(ccr, x + 5, y + 15); // Adjust text position within cell
				char day_str[4]; // Buffer to hold the day name (3 letters + null terminator)
				strftime(day_str, sizeof(day_str), "%a", &first_day_tm); // Format day name
				cairo_show_text(ccr, day_str); // Display day name

				/// Display date number
				char date_str[3]; // Buffer to hold the date number (2 digits + null terminator)
				snprintf(date_str, sizeof(date_str), "%d", day); // Convert day to string
				cairo_move_to(ccr, x + 5, y + 35); // Adjust text position for date number
				cairo_set_font_size(ccr, 17.0); /// numbers size
				cairo_show_text(ccr, date_str); // Display date number

				first_day_tm.tm_mday++; // Move to the next day for the next iteration
				mktime(&first_day_tm); // Update the time structure
			}
		}
	}

	char dateBuffer[128];
	strftime(dateBuffer, sizeof(dateBuffer), "%A, %d %B %Y", timeinfo);

	// Get the width of the widget and font size
	int widget_width = cwidth;
	double font_size = 20.0;

	// Set font size and calculate text extents
	cairo_set_font_size(ccr, font_size);
	cairo_text_extents_t extents;
	cairo_text_extents(ccr, dateBuffer, &extents);

	// Calculate x position to center the text in the widget
	double text_width = extents.width;
	double x_position = (widget_width - text_width) / 2.0;

	// Move to the calculated x position and show the text
	cairo_move_to(ccr, x_position, 310);  // y-coordinate remains unchanged
	cairo_show_text(ccr, dateBuffer);     // Display centered text

	/// draw frame around
	cairo_set_source_rgba(ccr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width(ccr, 3);
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
	const char *currentTime = get_current_time();
	cairo_set_font_size(server->cr, 24);
	cairo_set_source_rgb(server->cr, 1, 1, 1);
	cairo_move_to(server->cr, panel_width - 170, (panel_height / 2) + 9);

	if (currentTime) {
		cairo_show_text(server->cr, currentTime);
	}
	else {
		cairo_show_text(server->cr, "N/A");
	}
	free((void *)currentTime);
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
static void panel(struct woodland_server *server, int panel_width, int panel_height) {
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
		if (!calendar_was_activated && server->time_is_clicked) {
			///fprintf(stderr, "Open calendar\n");
			show_calendar(server);
			calendar_was_activated = true;
		}
		if (server->calendar_buffer && !server->time_is_clicked && calendar_was_activated) {
			///fprintf(stderr, "Close calendar\n");
			wlr_scene_node_set_enabled(&server->calendar_buffer->node, false);
			calendar_was_activated = false;
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
			network_applet_was_activated = false;
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
		wlr_scene_node_set_position(&server->panel_buffer->node, right_corner_x, right_corner_y);
		wlr_scene_node_raise_to_top(&server->panel_buffer->node);

		// Rendering
		// We need to toggle the node enable/disable for it to update
		// it's content (i couldn't find another solution)
		wlr_scene_node_set_enabled(&server->panel_buffer->node, false);
		wlr_scene_node_set_enabled(&server->panel_buffer->node, true);

	}
}
