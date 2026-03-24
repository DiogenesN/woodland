// SPDX-License-Identifier: GPL-2.0-or-later

#include "woodland.h"
#include "getvaluefromconf.h"

static struct wlr_drm_format format = {
	.format = DRM_FORMAT_ARGB8888,
	.len = 1,
	.capacity = 1,
	.modifiers = (uint64_t[]) { DRM_FORMAT_MOD_LINEAR },
};

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

// Function to process menu items from the configuration file
int show_menu(struct woodland_server *server) {
	char *config = server->config;
	char command[512][1024];
	size_t num_commands = 0;
	int width = 200;
	int height = 0;

	FILE *file = fopen(config, "r");
	if (file == NULL) {
		perror("Error opening config file in menu");
		return 1;
	}
	
	int mn_font_size = get_int_value_from_conf(config, "mn_font_size");

	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		// Ignore comments and empty lines
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}

		// Check for lines starting with 'startup_command'
		if (strstr(line, "menu_item") != NULL) {
			// Split by '='
			char *token = strtok(line, "=");
			token = strtok(NULL, "=");
			if (token != NULL) {
				// Trim leading and trailing spaces from the command
				char *trimmed_command = trim(token);
				if (strlen(trimmed_command) > 0) {
					strcpy(command[num_commands], trimmed_command);
					num_commands++;
					if (num_commands >= 512) {
						break; // Avoid overflow
					}
				}
			}
		}
	}
	fclose(file);

	// Set the verticall cell
	height = (num_commands / 2) * 40;

	// Create buffer
	struct wlr_buffer *wlr_buffer = wlr_allocator_create_buffer(server->allocator,
																width,
																height,
																&format);
	if (!wlr_buffer) {
		fprintf(stderr, "wlr_buffer failed in 'menu'!");
		return -1;
	}

	server->menu_scene_buffer = wlr_scene_buffer_create(&server->scene->tree, wlr_buffer);
	if (!server->menu_scene_buffer) {
		fprintf(stderr, "wlr_scene_buffer failed in 'menu'!");
		return -1;
	}

	server->m_cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(server->m_cairo_surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Cairo status failed in 'menu'!");
		return -1;
	}

	server->m_cr = cairo_create(server->m_cairo_surface);
	if (cairo_status(server->m_cr) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Cairo create failed in 'menu'!\n");
		cairo_surface_destroy(server->m_cairo_surface);
		return -1;
	}
	// Draw buffer background
	cairo_set_source_rgba(server->m_cr, 0.22, 0.22, 0.22, 1.0); // dark grey background
	cairo_paint(server->m_cr);
	cairo_set_font_size(server->m_cr, mn_font_size);

	server->menu_dialog_size = height;
	line[0] = '\0';
	int posy = 48;
	if (num_commands == 0) {
		// If no menu items are specified, return.
		wlr_log(WLR_INFO, "No menu items specified.");
		return -1;
	}
	else {
		// Show menu items
		int y = 0;
		for (size_t i = 0; i < num_commands; i++) {
			///fprintf(stderr, "Menu item: %s\n", command[i]);
			server->items[i] = command[y + i + 1];
			// highlight currect item
			if (i == server->menuPosition) {
				cairo_set_source_rgb(server->m_cr, 0.4, 0.6, 0.9); // blue text
			}
			else  {
				cairo_set_source_rgb(server->m_cr, 1, 1, 1); // white text
			}
			cairo_move_to(server->m_cr, 10, (posy - 20));
			cairo_show_text(server->m_cr, command[y + i]);
			posy = posy + 40;
			y = y + 1;
		}
		cairo_surface_flush(server->m_cairo_surface);

		// Constructing the buffer and showing it on the screen
		unsigned char *cairo_data = cairo_image_surface_get_data(server->m_cairo_surface);
		server->menu_scene_buffer->texture = wlr_texture_from_pixels(server->renderer,
																		DRM_FORMAT_ARGB8888,
																		width * 4,
																		width,
																		height,
																		(const void *)cairo_data);
		if (!server->menu_scene_buffer->texture) {
			fprintf(stderr, "Scene texture failed in 'menu'!\n");
			cairo_destroy(server->m_cr);
			cairo_surface_destroy(server->m_cairo_surface);
			return -1;
		}
		// Position the panel at the left bottom corner
		int output_height = server->transformed_height;
		int right_corner_x = 3;
		int right_corner_y = output_height - height - 10;

		// Set the enum title for the menu
		server->menu_scene_buffer->node.data = (void *)(uintptr_t)NODE_TYPE_MENU;

		wlr_scene_node_set_enabled(&server->menu_scene_buffer->node, true);
		wlr_scene_node_set_position(&server->menu_scene_buffer->node, right_corner_x, right_corner_y);
		wlr_scene_node_raise_to_top(&server->menu_scene_buffer->node);

		wlr_buffer_drop(wlr_buffer);

		if (server->m_cr) {
			cairo_destroy(server->m_cr);
			server->m_cr = NULL;
		}
		if (server->m_cairo_surface) {
			cairo_surface_flush(server->m_cairo_surface);
			cairo_surface_destroy(server->m_cairo_surface);
			server->m_cairo_surface = NULL;
		}
	}
	
	server->menu_width = width;
	server->menu_height = height;
	return 0;
}
