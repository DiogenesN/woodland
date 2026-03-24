// SPDX-License-Identifier: GPL-2.0-or-later

#include "woodland.h"

size_t items_count = 0;
size_t items_capacity = 0;

static struct wlr_drm_format format = {
	.format = DRM_FORMAT_ARGB8888,
	.len = 1,
	.capacity = 1,
	.modifiers = (size_t[]) { DRM_FORMAT_MOD_LINEAR },
};

//--------------------------------- HELPERS --------------------------------- //
/* return the total number of files in a directory
 * this helper is needed in order to denermine the
 * buffer size of the char server->all_items->name{number of files]
 */
static int nr_of_files_in_dir(char *dirname) {
	DIR *dir;
	struct dirent *entry;
	int count = 0;

	dir = opendir(dirname);
	if (dir == NULL) {
		return -1; // error opening directory
	}

	while ((entry = readdir(dir)) != NULL) {
		// Skip "." and ".."
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		// Build full path
		char path[2048];
		snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

		struct stat st;
		if (stat(path, &st) == 0) {
			if (S_ISREG(st.st_mode)) {
				count++;
			}
		}
	}

	closedir(dir);
	return count;
}

void free_desktop_items(struct woodland_server *server) {
	if (!server->all_items) return;

	for (size_t i = 0; i < items_count; i++) {
		free(server->all_items[i].name);
		free(server->all_items[i].exec);
		free(server->all_items[i].icon_path);

		// --- ADDED THIS ---
		if (server->all_items[i].icon_surface) {
			cairo_surface_destroy(server->all_items[i].icon_surface);
			server->all_items[i].icon_surface = NULL;
		}

		server->all_items[i].name = NULL;
		server->all_items[i].exec = NULL;
		server->all_items[i].icon_path = NULL;
	}

	free(server->all_items);
	free(server->filteredList);

	server->all_items = NULL;
	server->filteredList = NULL;
	server->total_buffer_size = 0;
	items_count = 0;
}

/* setup the initial variables */
void setup_desktop_items(struct woodland_server *server) {
	size_t lshare = nr_of_files_in_dir(server->local_share_path);
	size_t ushare = nr_of_files_in_dir("/usr/share/applications");
	size_t ulshare = nr_of_files_in_dir("/usr/local/share/applications");
	size_t ubin = nr_of_files_in_dir("/usr/local/bin");
	size_t ulbin = nr_of_files_in_dir("/usr/bin");

	server->total_buffer_size = (lshare + ushare + ulshare + ubin + ulbin + 10);

	// Allocate ACTUAL storage (Array of Structs)
	server->all_items = malloc(sizeof(struct AppItem) * server->total_buffer_size);

	// Allocate the FILTERED list (Array of POINTERS)
	// Notice the 'struct AppItem *' and the pointer-sized allocation
	server->filteredList = malloc(sizeof(struct AppItem *) * server->total_buffer_size);

	if (!server->all_items || !server->filteredList) {
		fprintf(stderr, "Could not allocate memory in 'setup_desktop_items'\n");
		return;
	}

	// Initialize everything to zero
	memset(server->all_items, 0, sizeof(struct AppItem) * server->total_buffer_size);
	memset(server->filteredList, 0, sizeof(struct AppItem *) * server->total_buffer_size);
	///fprintf(stderr, "Successfully setup desktop items for %zu slots\n", server->total_buffer_size);
}

// Rename it to avoid conflicts with system headers
char *woodland_strcasestr(const char *haystack, const char *needle) {
	if (!haystack || !needle) return NULL; 
	size_t len = strlen(needle);
	if (len == 0) return (char *)haystack;

	while (*haystack) {
		if (strncasecmp(haystack, needle, len) == 0) {
			return (char *)haystack;
		}
		haystack++;
	}
	return NULL;
}

/* * Usage:
 * struct AppItem warehouse[4096]; // Filled during startup
 * struct AppItem *filtered[4096]; // Empty array of pointers
 * * // Search for "term"
 * filtered_list(warehouse, 4096, filtered, "term");
 * * // Print results
 * for (int i = 0; filtered[i] != NULL; i++) {
 * printf("Match found: %s (Path: %s)\n", filtered[i]->name, filtered[i]->exec);
 * }
 */
void filtered_list(struct AppItem *source, size_t source_count, struct AppItem **fList, const char *key) {
	size_t j = 0;
	size_t key_len = strlen(key);

	for (size_t i = 0; i < source_count; i++) {
		// Skip empty slots in storage
		if (!source[i].name) continue;

		// If the search key is empty, show everything (optional)
		if (key_len == 0 || woodland_strcasestr(source[i].name, key)) {
			// We store the ADDRESS of the item in our list of pointers
			fList[j++] = &source[i];
		}
	}
	// Null-terminate the list of pointers so we know where it ends
	fList[j] = NULL;
}

/* Search a text file for a specific substring and return the first matching line.
 * If no match is found, or the file cannot be opened, it returns a 
 * strdup'd copy of the default 'noicon_path'.
 *
 * Usage:
 * const char *cachePath = "/home/user/.cache/icon_paths.txt";
 * const char *target = "org.inkscape.Inkscape";
 * * // Pass the server struct as the first argument
 * char *result = find_substring_in_file(server, cachePath, target);
 *
 * if (result) {
 * printf("Icon path found: %s\n", result);
 * free(result); // Must free because strdup was used
 * }
 */
static char *find_substring_in_file(struct woodland_server *server,
									const char *fullPathToConf,
									const char *substring) {
	if (!fullPathToConf || !substring) return strdup(server->noicon_path);

	FILE *pathToConfig = fopen(fullPathToConf, "r");
	if (!pathToConfig) {
		perror("Failed to open config");
		return strdup(server->noicon_path);
	}

	char buffer[4096];
	char *result = NULL;

	while (fgets(buffer, sizeof(buffer), pathToConfig) != NULL) {
		// Remove newline safely
		size_t len = strlen(buffer);
		if (len > 0 && buffer[len - 1] == '\n') {
			buffer[len - 1] = '\0';
		}

		// Search logic
		if (strstr(buffer, substring) != NULL && strstr(buffer, "#") == NULL) {
			result = strdup(buffer);
			break; // Stop loop once found
		}
	}

	fclose(pathToConfig);

	// If result is still NULL, return the default path
	return result ? result : strdup(server->noicon_path);
}

static char *ltrim(char *s) {
	while (*s && isspace((unsigned char)*s)) {
		s++;
	}
	return s;
}

static void add_to_server_lists(struct woodland_server *server,
								const char *name,
								const char *exec,
								const char *icon_name) {
	// Check if we have room in our arrays (assuming MAX_ITEMS is your limit)
	if (items_count >= server->total_buffer_size) {
		fprintf(stderr, "Warning: Maximum item limit reached (%ld)\n", server->total_buffer_size);
		return;
	}

	char *n = strdup(name);
	char *e = strdup(exec);
	char *i = find_substring_in_file(server, server->icon_cache_path, icon_name);

	// If any allocation failed, clean up the others and abort
	if (!n || !e || !i) {
		free(n);
		free(e);
		free(i);
		n = NULL;
		e = NULL;
		i = NULL;
		return;
	}

	server->all_items[items_count].name = n;
	server->all_items[items_count].exec = e;
	server->all_items[items_count].icon_path = i;// Inside add_to_server_lists...

	server->all_items[items_count].icon_surface = NULL;
	RsvgHandle *handle = rsvg_handle_new_from_file(server->all_items[items_count].icon_path, NULL);
	if (handle) {
		// Create a small surface just for the icon (e.g., 20x20)
		server->all_items[items_count].icon_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 20, 20);
		cairo_t *icon_cr = cairo_create(server->all_items[items_count].icon_surface);

		if (cairo_status(icon_cr) == CAIRO_STATUS_SUCCESS) {
			RsvgRectangle rect = { .x = 0, .y = 0, .width = 20, .height = 20 };
			rsvg_handle_render_document(handle, icon_cr, &rect, NULL);
		}

		cairo_destroy(icon_cr); // This destroys the context, but NOT the surface.
		g_object_unref(handle);
	}
	items_count++;
}

/* * Scans a directory for .desktop files (or binaries) and populates the 
 * server's central application database (all_items).
 * * Logic Flow:
 * 1. process_directory() loops through files.
 * 2. process_desktop_file() parses the Key=Value pairs.
 * 3. add_to_server_lists() allocates a struct and stores the data.
 *
 * Usage:
 * // Pre-requisite: server->all_items must be allocated via setup_desktop_items()
 * process_directory(server, "/usr/share/applications", false);
 * change the 'false' flag to 'true' if you parse a directory with binaries
 * * // Data is stored as:
 * // server->all_items[0].name = "GIMP"
 * // server->all_items[0].exec = "/usr/bin/gimp"
 * // server->all_items[0].icon_path = "/usr/share/icons/.../gimp.svg"
 *
 * Clean up:
 * Call free_desktop_items(server) to loop through and free the strdup'd 
 * strings and the pre-rendered Cairo surfaces.
 */
static void process_desktop_file(struct woodland_server *server, const char *filename) {
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror("fopen in process_desktop_file");
		return;
	}

	char buffer[2048];
	char name[2048] = {0};
	char exec[2048] = {0};
	char icon[2048] = {0};

	int found_name = 0;
	int found_exec = 0;
	int found_icon = 0;


	while (fgets(buffer, sizeof(buffer), f)) {
		// skip comments
		if (buffer[0] == '#')
			continue;

		char *eq = strchr(buffer, '=');
		if (!eq) {
			continue;
		}

		*eq = '\0';
		char *key = buffer;
		char *value = ltrim(eq + 1);

		// remove newline
		size_t len = strlen(value);
		if (len > 0 && value[len - 1] == '\n') {
			value[len - 1] = '\0';
		}

		// ---- Name ----
		if (!found_name && strcmp(key, "Name") == 0) {
			snprintf(name, sizeof(name), "%s", value);
			found_name = 1;
		}

		// ---- Exec ----
		else if (!found_exec && strcmp(key, "Exec") == 0) {
			// remove % arguments
			char *percent = strchr(value, '%');
			if (percent) {
				*percent = '\0';
			}

			snprintf(exec, sizeof(exec), "%s", value);
			found_exec = 1;
		}

		// ---- Icon ----
		else if (!found_icon && strcmp(key, "Icon") == 0) {
			snprintf(icon, sizeof(icon), "%s", value);
			found_icon = 1;
		}

		if (found_name && found_exec && found_icon) {
			break;
		}
	}
	fclose(f);

	// validate
	if (!found_name || !found_exec || !found_icon) {
		return;
	}

	add_to_server_lists(server, name, exec, icon);
}

/* Getting one .desktop file at a time from the given directory path */
void process_directory(struct woodland_server *server, const char *directoryPath, bool is_bin_only) {
	DIR *dirPath = opendir(directoryPath);
	if (!dirPath) {
		perror("Error opening directory");
		return;
	}

	struct dirent *entry;
	const char *suffix = ".desktop";
	size_t suffix_len = strlen(suffix);

	while ((entry = readdir(dirPath)) != NULL) {
		// Skip hidden files and navigation dots
		if (entry->d_name[0] == '.') continue;
		char filepath[4096];
		snprintf(filepath, sizeof(filepath), "%s/%s", directoryPath, entry->d_name);

		if (is_bin_only) {
			// Check if it's a regular file and executable
			if ((entry->d_type == DT_REG || \
				entry->d_type == DT_LNK || \
				entry->d_type == DT_UNKNOWN) && \
				access(filepath, X_OK) == 0) {
				// For binaries: Name = Exec = Icon = Filename
				add_to_server_lists(server, entry->d_name, filepath, entry->d_name);
			}
		}
		else {
			// Original .desktop logic
			size_t name_len = strlen(entry->d_name);
			if (name_len > suffix_len && strcmp(entry->d_name + (name_len - suffix_len), suffix) == 0) {
				process_desktop_file(server, filepath);
			}
		}
	}
	closedir(dirPath);
}

/* show/hide applauncher */
void show_applauncher(struct woodland_server *server) {
	int width = (server->transformed_width / 3);
	int height = 60;

	// Create buffer
	server->applauncher_wlr_buffer = wlr_allocator_create_buffer(server->allocator,
																width,
																height,
																&format);

	if (!server->applauncher_wlr_buffer) {
		return;
	}

	server->applauncher_scene_buffer = wlr_scene_buffer_create(&server->scene->tree,
																server->applauncher_wlr_buffer);

	if (!server->applauncher_scene_buffer) {
		wlr_buffer_drop(server->applauncher_wlr_buffer);
		server->applauncher_wlr_buffer = NULL;
		return;
	}

	cairo_surface_t *ap_cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(ap_cairo_surface)!= CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "show_applauncher: cairo surface failed\n");
		if (server->applauncher_wlr_buffer) {
			wlr_buffer_drop(server->applauncher_wlr_buffer);
			wlr_scene_node_destroy(&server->applauncher_scene_buffer->node);
			server->applauncher_wlr_buffer = NULL;
			server->applauncher_scene_buffer = NULL;
		}
		if (server->ssids[0] != NULL) {
			free(server->ssids[0]);
			server->ssids[0] = NULL;
			server->buff[0] = '\0';
		}
		return;
	}

	cairo_t *ap_cr = cairo_create(ap_cairo_surface);
	if (cairo_status(ap_cr)!= CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "show_applauncher: cairo ap_cr failed\n");
		if (ap_cairo_surface) {
			cairo_surface_destroy(ap_cairo_surface);
			ap_cairo_surface = NULL;
		}
		if (server->applauncher_wlr_buffer) {
			wlr_buffer_drop(server->applauncher_wlr_buffer);
			wlr_scene_node_destroy(&server->applauncher_scene_buffer->node);
			server->applauncher_wlr_buffer = NULL;
			server->applauncher_scene_buffer = NULL;
		}
		if (server->ssids[0] != NULL) {
			free(server->ssids[0]);
			server->ssids[0] = NULL;
			server->buff[0] = '\0';
		}
		return;
	}

	// Background
	cairo_set_source_rgba(ap_cr, 0.22, 0.22, 0.22, 1.0);
	cairo_paint(ap_cr);

	// Panel fill
	cairo_set_source_rgba(ap_cr, 0.2, 0.2, 0.2, 1.0);
	cairo_rectangle(ap_cr, 0, 0, width, height);
	cairo_fill(ap_cr);

	// Outer frame 
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width(ap_cr, 1.0);
	cairo_rectangle(ap_cr, 0, 0, width, height);
	cairo_stroke(ap_cr);

	// Prompt box stroke
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(ap_cr, 5, 5, width - 10, 49);
	cairo_stroke(ap_cr);
	
	// Typing text
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_font_size(ap_cr, 30);
	cairo_move_to(ap_cr, 15, 42);

	if (server->ssids[0] == NULL) {
		server->ssids[0] = strdup("_");
	}
	cairo_show_text(ap_cr, server->ssids[0]);
	if (server->ssids[0] != NULL) {
		free(server->ssids[0]);
		server->ssids[0] = NULL;
	}

	// Flush cairo before reading pixels
	cairo_surface_flush(ap_cairo_surface);

	if (server->applauncher_scene_buffer->texture) {
		wlr_texture_destroy(server->applauncher_scene_buffer->texture);
		server->applauncher_scene_buffer->texture = NULL;
	}

	// Constructing the buffer and showing it on the screen
	unsigned char *cairo_data = cairo_image_surface_get_data(ap_cairo_surface);
	server->applauncher_scene_buffer->texture = wlr_texture_from_pixels(server->renderer,
																		DRM_FORMAT_ARGB8888,
																		width * 4,
																		width,
																		height,
																		(const void *)cairo_data);

	// centering the dialog
	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, NULL, &output_box);

	double x = output_box.x + (server->transformed_width / 2.0) - (width / 2.0);
	double y = output_box.y + (server->transformed_height / 3.0) - (height / 2.0);
	wlr_scene_node_set_enabled(&server->applauncher_scene_buffer->node, true);
	wlr_scene_node_set_position(&server->applauncher_scene_buffer->node, x, y);
	wlr_scene_node_raise_to_top(&server->applauncher_scene_buffer->node);

	if (ap_cr) {
		cairo_destroy(ap_cr);
		ap_cr = NULL;
	}
	if (ap_cairo_surface) {
		cairo_surface_flush(ap_cairo_surface);
		cairo_surface_destroy(ap_cairo_surface);
		ap_cairo_surface = NULL;
	}
	return;
}

void redraw_applauncher(struct woodland_server *server) {
	if (!server->applauncher_wlr_buffer || !server->applauncher_scene_buffer) {
		return;
	}

	int width = (server->transformed_width / 3);
	int height = 60;

	// 'wlr_buffer_lock' is the most important call
	// it allows 'consumers' (the calling funciton)
	// to use the wlr_buffer created elsewhere
	wlr_buffer_lock(server->applauncher_wlr_buffer);
	wlr_scene_buffer_set_buffer(server->applauncher_scene_buffer, server->applauncher_wlr_buffer);


	cairo_surface_t *ap_cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(ap_cairo_surface)!= CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "redraw_applauncher: cairo surface failed\n");
		if (server->applauncher_wlr_buffer) {
			wlr_buffer_unlock(server->applauncher_wlr_buffer);
			wlr_buffer_drop(server->applauncher_wlr_buffer);
			wlr_scene_node_destroy(&server->applauncher_scene_buffer->node);
			server->applauncher_wlr_buffer = NULL;
			server->applauncher_scene_buffer = NULL;
		}
		if (server->ssids[0] != NULL) {
			free(server->ssids[0]);
			server->ssids[0] = NULL;
			server->buff[0] = '\0';
		}
		return;
	}

	cairo_t *ap_cr = cairo_create(ap_cairo_surface);
	if (cairo_status(ap_cr)!= CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "show_applauncher: cairo ap_cr failed\n");
		if (ap_cairo_surface) {
			cairo_surface_destroy(ap_cairo_surface);
			ap_cairo_surface = NULL;
		}
		if (server->applauncher_wlr_buffer) {
			wlr_buffer_unlock(server->applauncher_wlr_buffer);
			wlr_buffer_drop(server->applauncher_wlr_buffer);
			wlr_scene_node_destroy(&server->applauncher_scene_buffer->node);
			server->applauncher_wlr_buffer = NULL;
			server->applauncher_scene_buffer = NULL;
		}
		if (server->ssids[0] != NULL) {
			free(server->ssids[0]);
			server->ssids[0] = NULL;
			server->buff[0] = '\0';
		}
		return;
	}

	// Background
	cairo_set_source_rgba(ap_cr, 0.22, 0.22, 0.22, 1.0);
	cairo_paint(ap_cr);

	// Panel fill
	cairo_set_source_rgba(ap_cr, 0.2, 0.2, 0.2, 1.0);
	cairo_rectangle(ap_cr, 0, 0, width, height);
	cairo_fill(ap_cr);

	// Outer frame 
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width(ap_cr, 1.0);
	cairo_rectangle(ap_cr, 0, 0, width, height);
	cairo_stroke(ap_cr);

	// Prompt box stroke
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(ap_cr, 5, 5, width - 10, 49);
	cairo_stroke(ap_cr);
	
	// Typing text
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_font_size(ap_cr, 30);
	cairo_move_to(ap_cr, 15, 42);

	if (server->ssids[0] == NULL) {
		server->ssids[0] = strdup("_");
	}

	// show the text typed in the prompr
	cairo_show_text(ap_cr, server->ssids[0]);

	if (server->ssids[0] != NULL) {
		free(server->ssids[0]);
		server->ssids[0] = NULL;
	}

 	// Flush changes to the surface
	cairo_surface_flush(ap_cairo_surface);

	if (server->applauncher_scene_buffer->texture) {
		wlr_texture_destroy(server->applauncher_scene_buffer->texture);
		server->applauncher_scene_buffer->texture = NULL;
	}

	// Constructing the buffer and showing it on the screen
	unsigned char *cairo_data = cairo_image_surface_get_data(ap_cairo_surface);
	server->applauncher_scene_buffer->texture = wlr_texture_from_pixels(server->renderer,
																		DRM_FORMAT_ARGB8888,
																		width * 4,
																		width,
																		height,
																		(const void *)cairo_data);

	// centering the dialog
	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, NULL, &output_box);

	double x = output_box.x + (server->transformed_width / 2.0) - (width / 2.0);
	double y = output_box.y + (server->transformed_height / 3.0) - (height / 2.0);
	wlr_scene_node_set_enabled(&server->applauncher_scene_buffer->node, true);
	wlr_scene_node_set_position(&server->applauncher_scene_buffer->node, x, y);
	wlr_scene_node_raise_to_top(&server->applauncher_scene_buffer->node);

	if (ap_cr) {
		cairo_destroy(ap_cr);
		ap_cr = NULL;
	}
	if (ap_cairo_surface) {
		cairo_surface_flush(ap_cairo_surface);
		cairo_surface_destroy(ap_cairo_surface);
		ap_cairo_surface = NULL;
	}
	wlr_buffer_unlock(server->applauncher_wlr_buffer);
	return;
}

/* showing the list of app server->all_items->name */
void show_applist(struct woodland_server *server) {
	if (server->all_items[0].name == NULL) {
		return;
	}

	size_t width = (server->transformed_width / 3);
	size_t height = ((server->transformed_height / 2) + 7);

	// Create buffer
	server->applist_wlr_buffer = wlr_allocator_create_buffer(server->allocator,
																width,
																height,
																&format);

	if (!server->applist_wlr_buffer) {
		return;
	}

	server->applist_scene_buffer = wlr_scene_buffer_create(&server->scene->tree,
															server->applist_wlr_buffer);

	if (!server->applist_scene_buffer) {
		wlr_buffer_drop(server->applist_wlr_buffer);
		server->applist_wlr_buffer = NULL;
		return;
	}
	return;
}

void redraw_applist(struct woodland_server *server) {
	if (server->all_items[0].name == NULL || \
		!server->applist_wlr_buffer || \
		!server->applist_scene_buffer) {
		return;
	}

	size_t width = (server->transformed_width / 3);
	size_t height = 0;

	if (server->filteredList != NULL) {
		// adjusting the height of the list
		for (size_t i = 0; server->filteredList[i] != NULL; i++) {
			height = height + 30;
		}
	}
	if (height > (size_t)((server->transformed_height / 2) + 7)) {
		height = (size_t)((server->transformed_height / 2) + 7);
	}
	else {
		height = height + 7;
	}

	// 'wlr_buffer_lock' is the most important call
	// it allows 'consumers' (the calling funciton)
	// to use the wlr_buffer created elsewhere
	wlr_buffer_lock(server->applist_wlr_buffer);
	wlr_scene_buffer_set_dest_size(server->applist_scene_buffer, width, height);

	cairo_surface_t *ap_cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(ap_cairo_surface)!= CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "show_applauncher: cairo surface failed\n");
		if (server->applist_wlr_buffer) {
			wlr_buffer_drop(server->applist_wlr_buffer);
			wlr_scene_node_destroy(&server->applist_scene_buffer->node);
			server->applist_wlr_buffer = NULL;
			server->applist_scene_buffer = NULL;
		}
		if (server->ssids[0] != NULL) {
			free(server->ssids[0]);
			server->ssids[0] = NULL;
			server->buff[0] = '\0';
		}
		return;
	}

	cairo_t *ap_cr = cairo_create(ap_cairo_surface);
	if (cairo_status(ap_cr)!= CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "show_applauncher: cairo ap_cr failed\n");
		if (ap_cairo_surface) {
			cairo_surface_destroy(ap_cairo_surface);
			ap_cairo_surface = NULL;
		}
		if (server->applist_wlr_buffer) {
			wlr_buffer_drop(server->applist_wlr_buffer);
			wlr_scene_node_destroy(&server->applist_scene_buffer->node);
			server->applist_wlr_buffer = NULL;
			server->applist_scene_buffer = NULL;
		}
		if (server->ssids[0] != NULL) {
			free(server->ssids[0]);
			server->ssids[0] = NULL;
			server->buff[0] = '\0';
		}
		return;
	}

	// Background
	cairo_set_source_rgba(ap_cr, 0.22, 0.22, 0.22, 1.0);
	cairo_paint(ap_cr);

	// Panel fill
	cairo_set_source_rgba(ap_cr, 0.2, 0.2, 0.2, 1.0);
	cairo_rectangle(ap_cr, 0, 0, width, height);
	cairo_fill(ap_cr);

	// Outer frame 
	cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
	cairo_set_line_width(ap_cr, 1.0);
	cairo_rectangle(ap_cr, 0, 0, width, height);
	cairo_stroke(ap_cr);
	cairo_set_font_size(ap_cr, 23);

	// Creating the list of all apps
	size_t pos_y = 25;

	// 1. Only loop through the filtered list
	// 2. Add a limit! Don't draw 4000 items if the screen only fits 20.
    // Start the loop from the scroll_offset instead of 0
    for (size_t i = server->scroll_offset; 
    	server->filteredList[i] != NULL && 
    	i < server->scroll_offset + 20; i++) {
		// No searching needed! We use the struct or the linked index.
		struct AppItem *item = server->filteredList[i];
		char *displayName = server->filteredList[i]->name;

		cairo_move_to(ap_cr, 40, pos_y);
        // Always highlight the "top" visible item
		if (i == server->scroll_offset) {
			cairo_set_source_rgb(ap_cr, 0.4, 0.6, 0.9);
		}
		else {
			cairo_set_source_rgba(ap_cr, 1.0, 1.0, 1.0, 1.0);
		}
		cairo_show_text(ap_cr, displayName);

		// Fast Icon Drawing
		if (item->icon_surface) {
			cairo_set_source_surface(ap_cr, item->icon_surface, 10, pos_y - 18);
			cairo_paint(ap_cr);
		}
		pos_y += 30;
	}

	// Flush cairo before reading pixels
	cairo_surface_flush(ap_cairo_surface);

	if (server->applist_scene_buffer->texture) {
		wlr_texture_destroy(server->applist_scene_buffer->texture);
		server->applist_scene_buffer->texture = NULL;
	}

	// Constructing the buffer and showing it on the screen
	unsigned char *cairo_data = cairo_image_surface_get_data(ap_cairo_surface);
	server->applist_scene_buffer->texture = wlr_texture_from_pixels(server->renderer,
																		DRM_FORMAT_ARGB8888,
																		width * 4,
																		width,
																		height,
																		(const void *)cairo_data);

	// centering the dialog
	struct wlr_box output_box;
	wlr_output_layout_get_box(server->output_layout, NULL, &output_box);

	double x = output_box.x + (server->transformed_width / 2.0) - (width / 2.0);
	double y = output_box.y + (server->transformed_height / 3.0) + 35;

	// hide list when everything is removed
	int len = strlen(server->buff);
	if (len <= 0 && server->scroll_offset < 1) {
		wlr_scene_node_set_enabled(&server->applist_scene_buffer->node, false);
	}
	else {
		wlr_scene_node_set_enabled(&server->applist_scene_buffer->node, true);
		wlr_scene_node_set_position(&server->applist_scene_buffer->node, x, y);
		wlr_scene_node_raise_to_top(&server->applist_scene_buffer->node);
	}

	if (ap_cr) {
		cairo_destroy(ap_cr);
		ap_cr = NULL;
	}
	if (ap_cairo_surface) {
		cairo_surface_flush(ap_cairo_surface);
		cairo_surface_destroy(ap_cairo_surface);
		ap_cairo_surface = NULL;
	}
	wlr_buffer_unlock(server->applist_wlr_buffer);
	return;
}
