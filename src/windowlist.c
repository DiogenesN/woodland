// SPDX-License-Identifier: GPL-2.0-or-later

#include "woodland.h"

static struct wlr_drm_format format = {
	.format = DRM_FORMAT_ARGB8888,
	.len = 1,
	.capacity = 1,
	.modifiers = (uint64_t[]) { DRM_FORMAT_MOD_LINEAR },
};

void list_titles(struct woodland_server *server) {
	int width = 700;
	server->titles_counter = 0;
	int height = 0;
	struct woodland_view *toplevel = NULL;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (toplevel->xdg_toplevel->title && toplevel->xdg_toplevel->app_id) {
			///fprintf(stderr, "toplevel->xdg_toplevel->title: %s\n", toplevel->xdg_toplevel->title);
			///fprintf(stderr, "toplevel->xdg_toplevel->app_id: %s\n", toplevel->xdg_toplevel->app_id);
			///fprintf(stderr, "toplevel->minimized: %d\n", toplevel->minimized);

			server->toplevel_info.titles[server->titles_counter] = toplevel->xdg_toplevel->title;
			server->toplevel_info.app_id[server->titles_counter] = toplevel->xdg_toplevel->app_id;
			server->toplevel_info.minimized[server->titles_counter] = toplevel->minimized;
			server->titles_counter = server->titles_counter + 1;
		}
	}

	// Create buffer
	// Set the verticall cell size to display the app_id and title
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
		///fprintf(stderr, "info.titles[%ld]: %s\n", i, info.titles[i]);
		///fprintf(stderr, "info.app_id[%ld]: %s\n", i, info.app_id[i]);
		///fprintf(stderr, "info.minimized[%ld]: %d\n", i, info.minimized[i]);
		snprintf(line, sizeof(line) + 3, "[ %s ] - %s", server->toplevel_info.app_id[i],
														server->toplevel_info.titles[i]);

		if (i == server->TitlesPosition) {
			cairo_set_source_rgb(cr, 0.4, 0.6, 0.9); // blue text
		}
		else  {
			cairo_set_source_rgb(cr, 1, 1, 1); // white text
		}
		cairo_move_to(cr, 10, (posy - 20));
		cairo_show_text(cr, line);
		line[0] = '\0';
		posy = posy + 40;
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

	// Set the title for the panel
	const char *my_string = "woodland_windowlist";
	server->titles_scene_buffer->node.data = (void *)my_string;

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

