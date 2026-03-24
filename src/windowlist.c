// SPDX-License-Identifier: GPL-2.0-or-later

#include "woodland.h"

static struct wlr_drm_format format = {
	.format = DRM_FORMAT_ARGB8888,
	.len = 1,
	.capacity = 1,
	.modifiers = (uint64_t[]) { DRM_FORMAT_MOD_LINEAR },
};

/* Return the string that contains the given subtring in a text file
 * if the substring is not found then a predefined noicon.svg is returned
 * uasge:
    const char *fullPathToConf = "/home/diogenes/.config/woodland/icons.cache";
    const char *substring = "org.inkscape.Inkscape";
    const char *fallback = "/home/diogenes/.config/woodland/icons/noicon.svg";
    char *result = find_substring_in_file(fullPathToConf, substring, fallback);
    printf("Substring found: %s\n", result);
    free(result); // Remember to free the allocated memory
 */

static char *find_substring_in_file(const char *fullPathToConf, const char *substring, const char *fallback) {
	char buffer[2048];

	if (!fullPathToConf || !substring || !fallback) {
		return NULL;
	}

	FILE *fp = fopen(fullPathToConf, "r");
	if (!fp) {
		return strdup(fallback);
	}

	while (fgets(buffer, sizeof(buffer), fp)) {
		size_t len = strlen(buffer);

		if (len > 0 && buffer[len - 1] == '\n') {
			buffer[len - 1] = '\0';
		}

		if (buffer[0] == '#') {
			continue;
		}

		char *comment = strchr(buffer, '#');
		if (comment) {
			*comment = '\0';
		}

		if (strstr(buffer, substring)) {
			fclose(fp);
			return strdup(buffer);
		}
	}

	fclose(fp);
	return strdup(fallback);
}

void list_titles(struct woodland_server *server) {
	if (server->titles_clicked) {
		int width = 700;
		int height = 0;
		server->titles_counter = 0;
		struct woodland_view *toplevel = NULL;
		wl_list_for_each(toplevel, &server->toplevels, link) {
			if (toplevel->xdg_toplevel->title && toplevel->xdg_toplevel->app_id) {
				///fprintf(stderr, "toplevel->xdg_toplevel->title: %s\n", toplevel->xdg_toplevel->title);
				///fprintf(stderr, "toplevel->xdg_toplevel->app_id: %s\n", toplevel->xdg_toplevel->app_id);
				///fprintf(stderr, "toplevel->minimized: %d\n", toplevel->minimized);

				server->toplevel_info.titles[server->titles_counter] = toplevel->xdg_toplevel->title;
				server->toplevel_info.app_id[server->titles_counter] = toplevel->xdg_toplevel->app_id;
				server->toplevel_info.minimized[server->titles_counter] = toplevel->minimized;
				server->toplevel_info.views[server->titles_counter] = toplevel;

				// Make sure to free the old path if it exists to avoid memory leaks
				free(server->toplevel_info.icon_paths[server->titles_counter]);
				server->toplevel_info.icon_paths[server->titles_counter] = 
				find_substring_in_file(server->icon_cache_path,
										toplevel->xdg_toplevel->app_id,
										server->noicon_path);

				server->titles_counter = server->titles_counter + 1;
			}
		}

		// Create buffer
		// Set the vertical cell size to display the app_id and title
		height = server->titles_counter * 40;
		server->titles_dialog_size = height;

		struct wlr_buffer *wlr_buffer = wlr_allocator_create_buffer(server->allocator,
																	width,
																	height,
																	&format);
		if (!wlr_buffer) {
			fprintf(stderr, "wlr_buffer failed in 'list_titles'!");
			return;
		}

		server->titles_scene_buffer = wlr_scene_buffer_create(&server->scene->tree, wlr_buffer);
		if (!server->titles_scene_buffer) {
			fprintf(stderr, "wlr_scene_buffer failed in 'list_titles'!");
			return;
		}

		cairo_surface_t *cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		if (cairo_surface_status(cairo_surface) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Cairo status failed in 'list_titles'!");
			return;
		}

		cairo_t *cr = cairo_create(cairo_surface);
		if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Cairo create failed in 'list_titles'!\n");
			cairo_surface_destroy(cairo_surface);
			return;
		}
		// Draw buffer background
		cairo_set_source_rgba(cr, 0.22, 0.22, 0.22, 1.0); // dark grey background
		cairo_paint(cr);
		cairo_set_font_size(cr, 22);

		int posy = 48;
		char line[2048];

		for (size_t i = 0; server->toplevel_info.app_id[i] != NULL; i++) {
			// Prepare the string
			snprintf(line, sizeof(line), "[ %s ] - %s", 
				     server->toplevel_info.app_id[i],
				     server->toplevel_info.titles[i]);

			// Determine state and set style
			if (i == server->TitlesPosition) {
				// Hovered/Selected: Blue and Normal
				cairo_set_source_rgb(cr, 0.4, 0.6, 0.9);
				cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			} 
			else if (server->toplevel_info.minimized[i]) {
				// Minimized: Light Grey and Italic
				cairo_set_source_rgb(cr, 0.7, 0.7, 0.7); // Light grey
				cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
			} 
			else {
				// Default: White and Normal
				cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
				cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			}

			// DRAW THE ICON FIRST
			int icon_size = 24;
			int margin = 10;
			
			if (server->toplevel_info.icon_paths[i]) {
				GError *error = NULL;
				RsvgHandle *handle = rsvg_handle_new_from_file(server->toplevel_info.icon_paths[i], &error);
				if (handle) {
				    // Position icon at x=10
				    RsvgRectangle viewport = { .x = margin,
				    							.y = posy - 39,
				    							.width = icon_size,
				    							.height = icon_size };
				    rsvg_handle_render_document(handle, cr, &viewport, &error);
				    g_object_unref(handle);
				}
				else if (error) {
				    fprintf(stderr, "Icon error: %s\n", error->message);
				    g_error_free(error);
				}
			}

			// DRAW THE TEXT SECOND (Offset to the right of the icon)
			// If icon exists, start text at 10 (margin) + 24 (icon) + 10 (spacing) = 44
			cairo_move_to(cr, margin + icon_size + 10, (posy - 20)); 
			cairo_show_text(cr, line);

			posy += 39;
		}
		cairo_surface_flush(cairo_surface);

		// Constructing the buffer and showing it on the screen
		unsigned char *cairo_data = cairo_image_surface_get_data(cairo_surface);
		server->titles_scene_buffer->texture = wlr_texture_from_pixels(server->renderer,
																		DRM_FORMAT_ARGB8888,
																		width * 4,
																		width,
																		height,
																		(const void *)cairo_data);
		if (!server->titles_scene_buffer->texture) {
			fprintf(stderr, "Scene texture failed in 'list_titles'!\n");
			cairo_destroy(cr);
			cairo_surface_destroy(cairo_surface);
			return;
		}
		// Position the panel at the right top corner
		int output_width = server->transformed_width;
		int right_corner_x = output_width - width - 3;
		int right_corner_y = 3;

		// Set the enum title for the windowlist
		server->titles_scene_buffer->node.data = (void *)(uintptr_t)NODE_TYPE_WINDOWLIST;

		wlr_scene_node_set_enabled(&server->titles_scene_buffer->node, true);
		wlr_scene_node_set_position(&server->titles_scene_buffer->node, right_corner_x, right_corner_y);
		wlr_scene_node_raise_to_top(&server->titles_scene_buffer->node);

		wlr_buffer_drop(wlr_buffer);

		if (cr) {
			cairo_destroy(cr);
			cr = NULL;
		}
		if (cairo_surface) {
			cairo_surface_flush(cairo_surface);
			cairo_surface_destroy(cairo_surface);
			cairo_surface = NULL;
		}
	}
}

