// SPDX-License-Identifier: GPL-2.0-or-later

/* creates initial config directory and file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cairo/cairo-svg.h>
#include <wlroots-0.18/wlr/util/log.h>

void create_config(void) {
	wlr_log_init(WLR_DEBUG, NULL);
	const char *HOME = getenv("HOME");
	if (HOME == NULL) {
		fprintf(stderr, "Unable to determine the user's home directory.\n");
		return;
	}

	const char *dirConfig = "/.config/woodland";
	const char *dirConfigIcons = "/.config/woodland/icons";
	const char *fileConfig = "/woodland.ini";
	const char *fileConfigWindowsSizes = "/windows_sizes.db";

	const char *noicon			= "/noicon.svg";
	const char *dioVolHigh		= "/dio-volume-high.svg";
	const char *dioVolMid		= "/dio-volume-mid.svg";
	const char *dioVolLow		= "/dio-volume-low.svg";
	const char *dioVolOff		= "/dio-volume-off.svg";
	const char *dioBrightness	= "/brightness.svg";
	const char *dioNotes		= "/notes.svg";
	const char *dioNetwork		= "/network.svg";
	const char *dioNetworkOff	= "/network-off.svg";

	char dirConfigBuff[strlen(HOME) + strlen(dirConfig) + 1];
	char dirConfigIconsBuff[strlen(HOME) + strlen(dirConfigIcons) + 1];
	char fileConfigBuff[strlen(HOME) + strlen(dirConfig) + strlen(fileConfig) + 1];
	char fileConfigBuffWindowsSizes[strlen(HOME) + strlen(dirConfig) + strlen(fileConfigWindowsSizes) + 1];
	
	char noiconBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(noicon) + 3];
	char dioVolHighBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioVolHigh) + 3];
	char dioVolMidBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioVolMid) + 3];
	char dioVolLowBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioVolLow) + 3];
	char dioVolOffBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioVolOff) + 3];
	char dioBrightnessBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioBrightness) + 3];
	char dioNotesBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioNotes) + 3];
	char dioNetworkBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioNetwork) + 3];
	char dioNetworkOffBuff[strlen(HOME) + strlen(dirConfigIcons) + strlen(dioNetworkOff) + 3];

	snprintf(dirConfigBuff, sizeof(dirConfigBuff), "%s%s", HOME, dirConfig);
	snprintf(dirConfigIconsBuff, sizeof(dirConfigIconsBuff), "%s%s", HOME, dirConfigIcons);
	snprintf(fileConfigBuff, sizeof(fileConfigBuff), "%s%s%s", HOME, dirConfig, fileConfig);
	snprintf(fileConfigBuffWindowsSizes, sizeof(fileConfigBuffWindowsSizes), "%s%s%s", HOME, dirConfig, fileConfigWindowsSizes);
	
	snprintf(noiconBuff, sizeof(noiconBuff), "%s%s%s", HOME, dirConfigIcons, noicon);
	snprintf(dioVolHighBuff, sizeof(dioVolHighBuff), "%s%s%s", HOME, dirConfigIcons, dioVolHigh);
	snprintf(dioVolMidBuff, sizeof(dioVolMidBuff), "%s%s%s", HOME, dirConfigIcons, dioVolMid);
	snprintf(dioVolLowBuff, sizeof(dioVolLowBuff), "%s%s%s", HOME, dirConfigIcons, dioVolLow);
	snprintf(dioVolOffBuff, sizeof(dioVolOffBuff), "%s%s%s", HOME, dirConfigIcons, dioVolOff);
	snprintf(dioBrightnessBuff, sizeof(dioBrightnessBuff), "%s%s%s", HOME, dirConfigIcons, dioBrightness);
	snprintf(dioNotesBuff, sizeof(dioNotesBuff), "%s%s%s", HOME, dirConfigIcons, dioNotes);
	snprintf(dioNetworkBuff, sizeof(dioNetworkBuff), "%s%s%s", HOME, dirConfigIcons, dioNetwork);
	snprintf(dioNetworkOffBuff, sizeof(dioNetworkOffBuff), "%s%s%s", HOME, dirConfigIcons, dioNetworkOff);

	struct stat dirBuffer;
	struct stat fileBuffer;

	int dirExists = (stat(dirConfigBuff, &dirBuffer) == 0);
	int fileExists = (stat(fileConfigBuff, &fileBuffer) == 0);

	// Check if config directory exists
	if (!dirExists) {
		// Directory does not exist, create it
		if (mkdir(dirConfigBuff, 0755) != 0) {
			perror("mkdir config");
			return;
		}
		if (mkdir(dirConfigIconsBuff, 0755) != 0) {
			perror("mkdir config icons");
			return;
		}
	}

	// Check if config file exists
	if (!fileExists) {
		// File does not exist, create it
		FILE *config = fopen(fileConfigBuff, "w+");
		if (config == NULL) {
			perror("fopen");
			return;
		}

		FILE *config_winsizes = fopen(fileConfigBuffWindowsSizes, "w+");
		if (config_winsizes == NULL) {
			perror("fopen");
			return;
		}

		fprintf(config, "%s\n", "# Configuration file for woodland compositor\n");
		fprintf(config, "%s\n", "[ Welcome screen ]");
		fprintf(config, "%s\n", "# If you have any weclome screen application then it goes here.");
		fprintf(config, "%s\n", "welcome_screen = none\n");
		fprintf(config, "%s\n", "[ Brightness ]");
		fprintf(config, "%s\n", "# d_power_path, the path to the file that controls the brightness level.");
		fprintf(config, "%s\n", "d_power_path = /sys/class/backlight/intel_backlight/brightness");
		fprintf(config, "%s\n", "\n[ Background ]");
		fprintf(config, "%s\n", "# Provide the full path to the image.");
		fprintf(config, "%s\n", "#background = path\n");
		fprintf(config, "%s\n", "[ Touchpad ]");
		fprintf(config, "%s\n", "# Enable or disable tap to click.");
		fprintf(config, "%s\n", "tap_to_click = enable\n");
		fprintf(config, "%s\n", "[ Keyboard layouts ]");
		fprintf(config, "%s\n", "# Alt+Shift to switch layouts");
		fprintf(config, "%s\n", "# e.g: xkb_layouts=us,de");
		fprintf(config, "%s\n", "xkb_layouts=us\n");
		fprintf(config, "%s\n", "[ Multimedia keys ]");
		fprintf(config, "%s\n", "# For default multimedia keys support install: playerctl, alsa-utils");
		fprintf(config, "%s\n", "# or use your own commands.");
		fprintf(config, "%s\n", "play_pause  = playerctl play-pause");
		fprintf(config, "%s\n", "volume_up   = amixer set Master 3%+");
		fprintf(config, "%s\n", "volume_down = amixer set Master 3%-");
		fprintf(config, "%s\n", "volume_mute = amixer set Master toggle\n");
		fprintf(config, "%s\n", "[ Keyboard Shortcuts ]");
		fprintf(config, "%s\n", "# Modifiers names:");
		fprintf(config, "%s\n", "# WLR_MODIFIER_ALT");
		fprintf(config, "%s\n", "# WLR_MODIFIER_CTRL");
		fprintf(config, "%s\n", "# WLR_MODIFIER_SHIFT");
		fprintf(config, "%s\n", "# WLR_MODIFIER_LOGO (Super key)");
		fprintf(config, "%s\n", "#\n# Key names here: /usr/include/xkbcommon/xkbcommon-keysyms.h\n#");
		fprintf(config, "%s\n", "# Default shortcuts:");
		fprintf(config, "%s\n", "# <Super+Esc> to log out");
		fprintf(config, "%s\n", "# <Super+x> to close the current window");
		fprintf(config, "%s\n", "# <Alt+Tab> to switch to the next window");
		fprintf(config, "%s\n", "# <Alt+Ctrl+Tab> to switch to the previous window");
		fprintf(config, "%s\n", "# Example of user defined shortcuts:");
		fprintf(config, "%s\n", "# NOTE: You have to preserve binding_ and command_ prefixes.");
		fprintf(config, "%s\n", "#binding_thunar = WLR_MODIFIER_LOGO XKB_KEY_f");
		fprintf(config, "%s\n", "#command_thunar = thunar\n");
		fprintf(config, "%s\n", "[ Window Placement ]");
		fprintf(config, "%s\n", "# Open specified windows at the given fixed position.");
		fprintf(config, "%s\n", "# to get the title and/or app_id, use wlrctl tool.");
		fprintf(config, "%s\n", "# The placement model is as follows:");
		fprintf(config, "%s\n", "# (declaration) window_place = (keyword) app_id: (app id) app_id (number) x (number) y");
		fprintf(config, "%s\n", "# (declaration) window_place = (keyword) title: (title) title (number) x (number) y");
		fprintf(config, "%s\n", "# Example of how to make 'thunar' start at position x=100 y=100:");
		fprintf(config, "%s\n", "#window_place = app_id: thunar 100 100 (places thunar at x=100 y=100)");
		fprintf(config, "%s\n", "# or\n#window_place = title: \"some title\" 100 100 (places window containing title at x=100 y=100)");
		fprintf(config, "%s\n", "# NOTE: Titles with spaces must be put between double quotes: e.g \"New Document\"\n");
		fprintf(config, "%s\n", "[ Zoom ]");
		fprintf(config, "%s\n", "# Zooming is activated by pressing super key and scrolling.");
		fprintf(config, "%s\n", "# zoom_speed defines how fast zooming area is moving around.");
		fprintf(config, "%s\n", "# zoom_edge_threshold defines the distance from the edges to start panning.");
		fprintf(config, "%s\n", "# zoom_top_edge if 'enabled' then you can scroll on the left top edge to zoom.");
		fprintf(config, "%s\n", "zoom_speed = 0.009\n");
		fprintf(config, "%s\n", "[ Startup ]");
		fprintf(config, "%s\n", "# Specify the startup commands.");
		fprintf(config, "%s\n", "# If no startup command is specified then");
		fprintf(config, "%s\n", "# it will automatically look for the following terminals:");
		fprintf(config, "%s\n", "# foot, xfce4-terminal, kitty, gnome-terminal, alacritty.");
		fprintf(config, "%s\n", "# Example (automatically start thunar and foot):");
		fprintf(config, "%s\n", "# NOTE: the line must start with startup_command");
		fprintf(config, "%s\n", "#startup_command = thunar");
		fprintf(config, "%s\n", "#startup_command = foot\n");
		fprintf(config, "%s\n", "[ Menu ]");
		fprintf(config, "%s\n", "# Here you can specify a few items that will appear");
		fprintf(config, "%s\n", "# when clicking on the left bottom corner of the screen.");
		fprintf(config, "%s\n", "# mn_active_area_x/y sets the size of the clickable area on the bottom left corner.");
		fprintf(config, "%s\n", "mn_font_size = 22");
		fprintf(config, "%s\n", "mn_active_area_x = 30");
		fprintf(config, "%s\n", "mn_active_area_y = 30");
		fprintf(config, "%s\n", "menu_item = Reboot");
		fprintf(config, "%s\n", "menu_item = systemctl reboot");
		fprintf(config, "%s\n", "menu_item = Power Off");
		fprintf(config, "%s\n", "menu_item = systemctl poweroff\n");
		fprintf(config, "%s\n", "[ Windowlist ]");
		fprintf(config, "%s\n", "# Clicking on the top right corner of the screen shows a window list.");
		fprintf(config, "%s\n", "# wl_active_area_x/y sets the size of the clickable area on the top right corner.");
		fprintf(config, "%s\n", "wl_active_area_x = 5");
		fprintf(config, "%s\n", "wl_active_area_y = 5\n");
		fprintf(config, "%s\n", "[ Panel ]");
		fprintf(config, "%s\n", "# Hovering over the bottom right corner of the screen shows a panel.\n");

		// Log that the configuration files were created
		///wlr_log(WLR_INFO, "Configuration files created successfully!");

		fprintf(config_winsizes, "%s\n", "###########################################\n\n");
		fclose(config_winsizes);
		fclose(config);
	
		//////////////////////////////// drawing volume off icon //////////////////////////
		cairo_surface_t *surfaceVolOff = cairo_svg_surface_create(dioVolOffBuff, 100, 100);
		cairo_t *crVolOff = cairo_create(surfaceVolOff);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crVolOff, 0.7, 0.7, 0.7); // Set source color to white
		cairo_arc(crVolOff, 50, 50, 30, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crVolOff); // Fill the circle with the current source color (white)
		/// deviding the circle in half vertically
		cairo_set_operator(crVolOff, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolOff, 5);
		cairo_move_to(crVolOff, 50, 20);
		cairo_line_to(crVolOff, 50, 80);
		cairo_stroke(crVolOff);
		/// draw circle 1 inside the initial circle
		cairo_set_line_width(crVolOff, 5);
		cairo_arc(crVolOff, 50, 50, 12, 0, 2 * M_PI);
		cairo_stroke(crVolOff);
		/// draw circle 2 inside the initial circle
		cairo_set_line_width(crVolOff, 7);
		cairo_arc(crVolOff, 50, 50, 22, 0, 2 * M_PI);
		cairo_stroke(crVolOff);
		/// curve to remove the last circle sound wave
		cairo_set_line_width(crVolOff, 24);
		cairo_curve_to(crVolOff, 50, 19, 102, 50, 50, 81);
		cairo_stroke(crVolOff);
		/// removing last wave for volume off icon
		cairo_set_line_width(crVolOff, 10);
		cairo_move_to(crVolOff, 55, 22);
		cairo_line_to(crVolOff, 55, 79);
		cairo_stroke(crVolOff);
		/// painiting/covering out unneeded transparent lines on the left side of the circle
		cairo_set_operator(crVolOff, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolOff, 5);
		cairo_move_to(crVolOff, 45, 22);
		cairo_line_to(crVolOff, 45, 79);
		cairo_stroke(crVolOff);
		cairo_set_line_width(crVolOff, 13);
		cairo_move_to(crVolOff, 39, 25);
		cairo_line_to(crVolOff, 39, 75);
		cairo_stroke(crVolOff);
		cairo_set_line_width(crVolOff, 12);
		cairo_move_to(crVolOff, 33, 30);
		cairo_line_to(crVolOff, 33, 70);
		cairo_stroke(crVolOff);
		cairo_set_line_width(crVolOff, 10);
		cairo_move_to(crVolOff, 27, 40);
		cairo_line_to(crVolOff, 27, 60);
		cairo_stroke(crVolOff);
		/// cutting space before adding a tail
		cairo_set_operator(crVolOff, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolOff, 17);
		cairo_move_to(crVolOff, 15, 20);
		cairo_line_to(crVolOff, 15, 80);
		cairo_stroke(crVolOff);
		/// adding a tail at the end of volume icon
		cairo_set_operator(crVolOff, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolOff, 10);
		cairo_move_to(crVolOff, 15, 38);
		cairo_line_to(crVolOff, 15, 62);
		cairo_stroke(crVolOff);

		//////////////////////////////// drawing volume low icon //////////////////////////
		cairo_surface_t *surfaceVolLow = cairo_svg_surface_create(dioVolLowBuff, 100, 100);
		cairo_t *crVolLow = cairo_create(surfaceVolLow);

		/// draw initial circle filled inside
		cairo_set_source_rgb(crVolLow, 0.9, 0.9, 0.8); // Set source color to white
		cairo_arc(crVolLow, 50, 50, 30, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crVolLow); // Fill the circle with the current source color (white)
		/// deviding the circle in half vertically
		cairo_set_operator(crVolLow, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolLow, 5);
		cairo_move_to(crVolLow, 50, 20);
		cairo_line_to(crVolLow, 50, 80);
		cairo_stroke(crVolLow);
		/// draw circle 1 inside the initial circle
		cairo_set_line_width(crVolLow, 5);
		cairo_arc(crVolLow, 50, 50, 12, 0, 2 * M_PI);
		cairo_stroke(crVolLow);
		/// draw circle 2 inside the initial circle
		cairo_set_line_width(crVolLow, 7);
		cairo_arc(crVolLow, 50, 50, 22, 0, 2 * M_PI);
		cairo_stroke(crVolLow);
		/// curve to remove the last circle sound wave
		cairo_set_line_width(crVolLow, 24);
		cairo_curve_to(crVolLow, 50, 19, 102, 50, 50, 81);
		cairo_stroke(crVolLow);
		/// painiting/covering out unneeded transparent lines on the left side of the circle
		cairo_set_operator(crVolLow, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolLow, 5);
		cairo_move_to(crVolLow, 45, 22);
		cairo_line_to(crVolLow, 45, 79);
		cairo_stroke(crVolLow);
		cairo_set_line_width(crVolLow, 13);
		cairo_move_to(crVolLow, 39, 25);
		cairo_line_to(crVolLow, 39, 75);
		cairo_stroke(crVolLow);
		cairo_set_line_width(crVolLow, 12);
		cairo_move_to(crVolLow, 33, 30);
		cairo_line_to(crVolLow, 33, 70);
		cairo_stroke(crVolLow);
		cairo_set_line_width(crVolLow, 10);
		cairo_move_to(crVolLow, 27, 40);
		cairo_line_to(crVolLow, 27, 60);
		cairo_stroke(crVolLow);
		/// cutting space before adding a tail
		cairo_set_operator(crVolLow, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolLow, 17);
		cairo_move_to(crVolLow, 15, 20);
		cairo_line_to(crVolLow, 15, 80);
		cairo_stroke(crVolLow);
		/// adding a tail at the end of volume icon
		cairo_set_operator(crVolLow, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolLow, 10);
		cairo_move_to(crVolLow, 15, 38);
		cairo_line_to(crVolLow, 15, 62);
		cairo_stroke(crVolLow);

		//////////////////////////////// drawing volume mid icon //////////////////////////
		cairo_surface_t *surfaceVolMid = cairo_svg_surface_create(dioVolMidBuff, 100, 100);
		cairo_t *crVolMid = cairo_create(surfaceVolMid);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crVolMid, 0.9, 0.9, 0.8); // Set source color to white
		cairo_arc(crVolMid, 50, 50, 30, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crVolMid); // Fill the circle with the current source color (white)
		/// deviding the circle in half vertically
		cairo_set_operator(crVolMid, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolMid, 5);
		cairo_move_to(crVolMid, 50, 20);
		cairo_line_to(crVolMid, 50, 80);
		cairo_stroke(crVolMid);
		/// draw circle 1 inside the initial circle
		cairo_set_line_width(crVolMid, 5);
		cairo_arc(crVolMid, 50, 50, 12, 0, 2 * M_PI);
		cairo_stroke(crVolMid);
		/// draw circle 2 inside the initial circle
		cairo_set_line_width(crVolMid, 7);
		cairo_arc(crVolMid, 50, 50, 22, 0, 2 * M_PI);
		cairo_stroke(crVolMid);
		/// curve to remove the last circle sound wave
		cairo_set_line_width(crVolMid, 10);
		cairo_curve_to(crVolMid, 54, 19, 102, 50, 54, 81);
		cairo_stroke(crVolMid);
		/// painiting/covering out unneeded transparent lines on the left side of the circle
		cairo_set_operator(crVolMid, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolMid, 5);
		cairo_move_to(crVolMid, 45, 25);
		cairo_line_to(crVolMid, 45, 75);
		cairo_stroke(crVolMid);
		cairo_set_line_width(crVolMid, 13);
		cairo_move_to(crVolMid, 39, 25);
		cairo_line_to(crVolMid, 39, 75);
		cairo_stroke(crVolMid);
		cairo_set_line_width(crVolMid, 12);
		cairo_move_to(crVolMid, 33, 30);
		cairo_line_to(crVolMid, 33, 70);
		cairo_stroke(crVolMid);
		cairo_set_line_width(crVolMid, 10);
		cairo_move_to(crVolMid, 27, 40);
		cairo_line_to(crVolMid, 27, 60);
		cairo_stroke(crVolMid);
		/// cutting space before adding a tail
		cairo_set_operator(crVolMid, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolMid, 17);
		cairo_move_to(crVolMid, 15, 20);
		cairo_line_to(crVolMid, 15, 80);
		cairo_stroke(crVolMid);
		/// adding a tail at the end of volume icon
		cairo_set_operator(crVolMid, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolMid, 10);
		cairo_move_to(crVolMid, 15, 38);
		cairo_line_to(crVolMid, 15, 62);
		cairo_stroke(crVolMid);

		//////////////////////////////// drawing volume max icon //////////////////////////
		cairo_surface_t *surfaceVolHigh = cairo_svg_surface_create(dioVolHighBuff, 100, 100);
		cairo_t *crVolHigh = cairo_create(surfaceVolHigh);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crVolHigh, 0.9, 0.9, 0.8); // Set source color to white
		cairo_arc(crVolHigh, 50, 50, 30, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crVolHigh); // Fill the circle with the current source color (white)
		/// deviding the circle in half vertically
		cairo_set_operator(crVolHigh, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolHigh, 5);
		cairo_move_to(crVolHigh, 50, 20);
		cairo_line_to(crVolHigh, 50, 80);
		cairo_stroke(crVolHigh);
		/// draw circle 1 inside the initial circle
		cairo_set_line_width(crVolHigh, 5);
		cairo_arc(crVolHigh, 50, 50, 12, 0, 2 * M_PI);
		cairo_stroke(crVolHigh);
		/// draw circle 2 inside the initial circle
		cairo_set_line_width(crVolHigh, 5);
		cairo_arc(crVolHigh, 50, 50, 22, 0, 2 * M_PI);
		cairo_stroke(crVolHigh);
		/// painiting/covering out unneeded transparent lines on the left side of the circle
		cairo_set_operator(crVolHigh, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolHigh, 5);
		cairo_move_to(crVolHigh, 45, 25);
		cairo_line_to(crVolHigh, 45, 75);
		cairo_stroke(crVolHigh);
		cairo_set_line_width(crVolHigh, 13);
		cairo_move_to(crVolHigh, 39, 25);
		cairo_line_to(crVolHigh, 39, 75);
		cairo_stroke(crVolHigh);
		cairo_set_line_width(crVolHigh, 10);
		cairo_move_to(crVolHigh, 33, 30);
		cairo_line_to(crVolHigh, 33, 70);
		cairo_stroke(crVolHigh);
		cairo_set_line_width(crVolHigh, 10);
		cairo_move_to(crVolHigh, 27, 40);
		cairo_line_to(crVolHigh, 27, 60);
		cairo_stroke(crVolHigh);
		/// cutting space before adding a tail
		cairo_set_operator(crVolHigh, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crVolHigh, 17);
		cairo_move_to(crVolHigh, 15, 20);
		cairo_line_to(crVolHigh, 15, 80);
		cairo_stroke(crVolHigh);
		/// adding a tail at the end of volume icon
		cairo_set_operator(crVolHigh, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crVolHigh, 10);
		cairo_move_to(crVolHigh, 15, 38);
		cairo_line_to(crVolHigh, 15, 62);
		cairo_stroke(crVolHigh);

		/////////////////////////////// drawing bightness icon ////////////////////////////
		cairo_surface_t *surfaceBright = cairo_svg_surface_create(dioBrightnessBuff, 100, 100);
		cairo_t *crBright = cairo_create(surfaceBright);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crBright, 0.9, 0.9, 0.9); // Set source color to white
		cairo_arc(crBright, 50, 50, 30, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crBright); // Fill the circle with the current source color (white)
		/// draw circle 1 inside the initial circle
		cairo_set_operator(crBright, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crBright, 5);
		cairo_arc(crBright, 50, 50, 20, 0, 2 * M_PI);
		cairo_stroke(crBright);

		///////////////////////////////// drawing notes icon //////////////////////////////
		cairo_surface_t *surfaceNotes = cairo_svg_surface_create(dioNotesBuff, 100, 100);
		cairo_t *crNotes = cairo_create(surfaceNotes);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crNotes, 0.9, 0.9, 0.9); // Set source color to white
		/// draw notes
		cairo_set_line_width(crNotes, 7);
		cairo_rectangle (crNotes, 10, 15, 60, 65);
		cairo_stroke(crNotes);
		/// draw text line 1
		cairo_set_line_width(crNotes, 5);
		cairo_move_to(crNotes, 20, 30);
		cairo_line_to(crNotes, 60, 30);
		cairo_stroke(crNotes);
		/// draw text line 1
		cairo_set_line_width(crNotes, 5);
		cairo_move_to(crNotes, 20, 45);
		cairo_line_to(crNotes, 60, 45);
		cairo_stroke(crNotes);
		/// draw text line 3
		cairo_set_line_width(crNotes, 5);
		cairo_move_to(crNotes, 20, 60);
		cairo_line_to(crNotes, 60, 60);
		cairo_stroke(crNotes);
		/// draw pencil
		cairo_set_line_width(crNotes, 15);
		cairo_move_to(crNotes, 75, 35);
		cairo_line_to(crNotes, 55, 85);
		cairo_stroke(crNotes);
		/// draw pencil inside
		cairo_set_operator(crNotes, CAIRO_OPERATOR_CLEAR);
		cairo_set_line_width(crNotes, 6);
		cairo_move_to(crNotes, 73, 39);
		cairo_line_to(crNotes, 56, 83);
		cairo_stroke(crNotes);
		/// draw end of pencil 1
		cairo_set_operator(crNotes, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crNotes, 2);
		cairo_move_to(crNotes, 52, 81);
		cairo_line_to(crNotes, 51, 92);
		cairo_stroke(crNotes);
		/// draw end of pencil 2
		cairo_set_operator(crNotes, CAIRO_OPERATOR_SOURCE);
		cairo_set_line_width(crNotes, 2);
		cairo_move_to(crNotes, 59, 85);
		cairo_line_to(crNotes, 51, 92);
		cairo_stroke(crNotes);

		/////////////////////////////// drawing nwtwork icon //////////////////////////////
		cairo_surface_t *surfaceNetwork = cairo_svg_surface_create(dioNetworkBuff, 100, 100);
		cairo_t *crNetwork = cairo_create(surfaceNetwork);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crNetwork, 0.9, 0.9, 0.9); // Set source color to white
		cairo_arc(crNetwork, 50, 73, 10, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crNetwork); // Fill the circle with the current source color (white)
		/// network wave 1
		cairo_set_line_width(crNetwork, 7);
		cairo_curve_to(crNetwork, 30, 63, 50, 35, 70, 63);
		cairo_stroke(crNetwork);
		/// network wave 2
		cairo_set_line_width(crNetwork, 7);
		cairo_curve_to(crNetwork, 20, 50, 50, 15, 80, 50);
		cairo_stroke(crNetwork);
		/// network wave 3
		cairo_set_line_width(crNetwork, 7);
		cairo_curve_to(crNetwork, 10, 35, 50, -5, 90, 35);
		cairo_stroke(crNetwork);

		//////////////////////////// drawing nwtwork off icon ////////////////////////////
		cairo_surface_t *surfaceNetOff = cairo_svg_surface_create(dioNetworkOffBuff, 100, 100);
		cairo_t *crNetOff = cairo_create(surfaceNetOff);
		/// draw initial circle filled inside
		cairo_set_source_rgb(crNetOff, 0.9, 0.9, 0.9); // Set source color to white
		cairo_arc(crNetOff, 50, 73, 10, 0, 2 * M_PI); // Draw the circle
		cairo_fill(crNetOff); // Fill the circle with the current source color (white)
		/// network wave 1
		cairo_set_line_width(crNetOff, 7);
		cairo_curve_to(crNetOff, 30, 63, 50, 35, 70, 63);
		cairo_stroke(crNetOff);
		/// network wave 2
		cairo_set_line_width(crNetOff, 7);
		cairo_curve_to(crNetOff, 20, 50, 50, 15, 80, 50);
		cairo_stroke(crNetOff);
		/// network wave 3
		cairo_set_line_width(crNetOff, 7);
		cairo_curve_to(crNetOff, 10, 35, 50, -5, 90, 35);
		cairo_stroke(crNetOff);
		/// red x for network off
		cairo_set_source_rgb(crNetOff, 1.0, 0.0, 0.0);
		cairo_set_line_width(crNetOff, 12);
		cairo_move_to(crNetOff, 30, 30);
		cairo_line_to(crNetOff, 70, 70);
		cairo_stroke(crNetOff);
		cairo_move_to(crNetOff, 70, 30);
		cairo_line_to(crNetOff, 30, 70);
		cairo_stroke(crNetOff);

		//////////////////////////// creating default noicon svg ////////////////////////////
		cairo_surface_t *surface = cairo_svg_surface_create(noiconBuff, 100, 100);
		cairo_t *cr = cairo_create(surface);
		// Clear the inside of the circle (transparent fill)
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0); // Transparent fill: RGBA(0, 0, 0, 0)
		cairo_fill(cr);
		// Draw the circle outline
		cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // Red outline: RGB(1, 0, 0)
		cairo_set_line_width(cr, 10.0); // Set line width to 10 (adjust as needed)
		cairo_arc(cr, 50, 50, 40, 0, 2 * M_PI); // Center (50, 50), radius 40
		cairo_stroke(cr); // Draw the outline using stroke instead of fill
		// Draw the diagonal lines for close icon effect
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White line: RGB(1, 1, 1)
		cairo_set_line_width(cr, 5.0); // Set line width to 5 (adjust as needed)
		cairo_move_to(cr, 30, 30); // Move to the starting point of the line
		cairo_line_to(cr, 70, 70); // Draw a line to the ending point
		cairo_move_to(cr, 30, 70); // Move to another starting point
		cairo_line_to(cr, 70, 30); // Draw another line to another ending point
		cairo_stroke(cr); // Draw the lines using stroke

		/// clean up
		cairo_destroy(crVolOff);
		cairo_surface_destroy(surfaceVolOff);
		cairo_destroy(crVolLow);
		cairo_surface_destroy(surfaceVolLow);
		cairo_destroy(crVolMid);
		cairo_surface_destroy(surfaceVolMid);
		cairo_destroy(crVolHigh);
		cairo_surface_destroy(surfaceVolHigh);
		cairo_destroy(crBright);
		cairo_surface_destroy(surfaceBright);
		cairo_destroy(crNotes);
		cairo_surface_destroy(surfaceNotes);
		cairo_destroy(crNetwork);
		cairo_surface_destroy(surfaceNetwork);
		cairo_destroy(crNetOff);
		cairo_surface_destroy(surfaceNetOff);
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
	}
	else {
		// Log that the configuration files already exist
		///wlr_log(WLR_INFO, "Configuration files exist, nothing to do!");
	}
}
