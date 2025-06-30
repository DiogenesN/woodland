// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WOODLAND_H
#define WOODLAND_H

#include "headers.h"

/* For brevity's sake, struct members are annotated where they are used. */
enum woodland_cursor_mode {
	WOODLAND_CURSOR_PASSTHROUGH,
	WOODLAND_CURSOR_MOVE,
	WOODLAND_CURSOR_RESIZE,
};

typedef struct {
	char *titles[256];
	char *app_id[256];
	bool minimized[256];
} toplevel_info;

struct woodland_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;
	struct wlr_xdg_output_manager_v1 *xdg_output_manager;
	// Background buffer
	struct wlr_scene_buffer *background_scene_buffer;
	// Panel
	struct wlr_buffer *wlr_panel_buffer;
	struct wlr_scene_buffer *panel_buffer;
	struct wlr_scene_buffer *calendar_buffer;
	struct wlr_scene_buffer *network_buffer;
	struct wlr_scene_output *panel_scene_output;
	// DRM Lease
	struct wlr_drm_lease_v1_manager *drm_lease_manager;
	struct wl_listener drm_lease_request;
	// Windowlist
	struct wlr_scene_buffer *titles_scene_buffer;
	toplevel_info toplevel_info;
	// Menu
	struct wlr_scene_buffer *menu_scene_buffer;
	// DMABuf
	struct wlr_linux_dmabuf_v1 *linux_dmabuf;
	// Scene
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	// Session
	struct wlr_session *session;
	// Timer
	struct wl_event_loop *event_loop;
	struct wl_event_source *autostart_timer;
	struct wl_event_source *time_update_timer;
	// XDG Shell
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_popup;
	struct wl_listener new_xdg_toplevel;
	struct wl_list toplevels;
	struct wl_list popups;
	// Additional interfaces
	// Virtual Keyboard
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
	struct wl_listener new_virtual_keyboard;
	// Foreign toplevel manager
	struct wlr_foreign_toplevel_manager_v1 *toplevel_manager;
	// Drag and drop
	struct wl_listener start_drag;
	struct wl_listener request_start_drag;
	// Pointer constraints
	struct wlr_pointer_constraints_v1 *wlr_pointer_constraints;
	struct wlr_pointer_constraint_v1 *active_pointer_constraint;
	struct wl_listener new_pointer_constraint;
	struct wl_listener constraint_destroy;
	// Relative pointer manager
	struct wlr_relative_pointer_manager_v1 *wlr_relative_pointer_manager;
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
	struct woodland_view *grabbed_toplevel;
	enum woodland_cursor_mode cursor_mode;
	
	xkb_keysym_t keybinding_handler;
	xkb_layout_index_t LayoutIndexes;

	// Globals
	int32_t transformed_width;
	int32_t transformed_height;

	// Keep track of autostart loop to know if to free it at the end
	bool autostart_cmd_ran;

	// Panel variables
	cairo_surface_t *cairo_surface;
	cairo_t *cr;
	cairo_font_face_t *font_face;
	int SsidPosition;
	size_t number_of_ssids; // holds the number of available SSIDs, zero means Wi-Fi is disabled
	uint8_t *cairo_data;
	bool network_password_prompt;
	bool network_was_activated; // prevents multiple calls to open the widget
	bool panel_is_hidden;
	bool calendar_texture; // keeps track if calendar was activated hence texture needs cleared
	bool time_is_clicked;
	bool network_is_clicked;
	bool network_texture; // keeps track if network was activated hence texture needs cleared
	bool volume_change;
	bool brightness_change;
	bool time_hovered;
	bool network_hovered;
	bool network_ly_hovered;
	bool display_is_off; // Right click on brightness icon switches off display, super key to reactivate
	char *volumeHigh;
	char *brightnessIcon;
	char *networkIcon;
	char *ssids[156];
	char buff[128];

	// Windowlist dialog
	int titles_counter;
	int titles_dialog_size;
	size_t TitlesPosition;
	bool titles_clicked;
	bool titles_ly_hovered;

	// Menu dialog
	cairo_surface_t *m_cairo_surface;
	cairo_t *m_cr;
	int menu_width;
	int menu_height;
	int menu_dialog_size;
	size_t menuPosition;
	bool menu_clicked;
	bool menu_ly_hovered;
	char *items[256];

	// Other variables;
	double grab_x;
	double grab_y;
	uint32_t modifier;
	uint32_t resize_edges;
	uint32_t saved_brightness;
	bool super_key_down;
	bool keybind_handled;
	bool cycling_mode;
	char *config;
	char *config_sizes;
	char *tap_enable;
	char *brightness_path;
	char *play_pause;
	char *volume_up;
	char *volume_down;
	char *volume_mute;
	// Zooming
	double zoom_speed;				// Static speed of panning as defined in config
	double zoom_speed_m;			// Modified speed
	double zoom_factor;				// How large the zooming area should be on one scroll
};

struct woodland_output {
	struct wl_list link;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wlr_output *wlr_output;
	struct woodland_server *server;
	struct wl_listener request_state;
	struct wlr_scene_output *scene_output;
};

struct woodland_view {
	struct woodland_server *server;
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_list link;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener set_title;
    struct wlr_box saved_geometry;
	struct wl_listener set_app_id;
	struct wl_listener request_move;
	struct wl_listener request_close;
	struct wl_listener request_resize;
	struct wl_listener foreign_destroy;
	struct wl_listener request_minimize;
	struct wl_listener request_activate;
	struct wl_listener request_fullscreen;
	struct wlr_foreign_toplevel_handle_v1 *foreign_handle;
	xkb_layout_index_t keyboard_layout;
	bool resized;
	bool minimized;
	bool fullscreened;
	char *app_id;
};

struct woodland_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_list link;
};

struct woodland_keyboard {
	struct woodland_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_list link;
	struct wl_listener key;
	struct wl_listener destroy;
	struct wl_listener modifiers;
	struct wlr_input_device *device;
	bool destroyed;
};

#endif
