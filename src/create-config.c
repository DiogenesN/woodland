// SPDX-License-Identifier: GPL-2.0-or-later

/* creates initial config directory and file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wlr/util/log.h>

void create_config(void) {
	wlr_log_init(WLR_DEBUG, NULL);
	const char *HOME = getenv("HOME");
	if (HOME == NULL) {
		fprintf(stderr, "Unable to determine the user's home directory.\n");
		return;
	}

	const char *dirConfig = "/.config/woodland";
	const char *fileConfig = "/woodland.ini";
	char dirConfigBuff[strlen(HOME) + strlen(dirConfig) + 1];
	char fileConfigBuff[strlen(HOME) + strlen(dirConfig) + strlen(fileConfig) + 1];

	snprintf(dirConfigBuff, sizeof(dirConfigBuff), "%s%s", HOME, dirConfig);
	snprintf(fileConfigBuff, sizeof(fileConfigBuff), "%s%s%s", HOME, dirConfig, fileConfig);

	struct stat dirBuffer;
	struct stat fileBuffer;

	int dirExists = (stat(dirConfigBuff, &dirBuffer) == 0);
	int fileExists = (stat(fileConfigBuff, &fileBuffer) == 0);

	// Check if config directory exists
	if (!dirExists) {
		// Directory does not exist, create it
		if (mkdir(dirConfigBuff, 0755) != 0) {
			perror("mkdir");
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

		fprintf(config, "%s\n", "# Configuration file for woodland compositor\n");
		fprintf(config, "%s\n", "[ Idle ]");
		fprintf(config, "%s\n", "# The timeout in milliseconds until the system is considered idle.");
		fprintf(config, "%s\n", "# One minute is 60000 milliseconds.");
		fprintf(config, "%s\n", "# idle_timeout = 0 disables the timeout.");
		fprintf(config, "%s\n", "# d_power_path, the path to the file that controls the brightness level.");
		fprintf(config, "%s\n", "idle_timeout = 0");
		fprintf(config, "%s\n", "d_power_path = /sys/class/backlight/intel_backlight/brightness");
		fprintf(config, "%s\n", "\n[ Background ]");
		fprintf(config, "%s\n", "# Provide the full path to the image.");
		fprintf(config, "%s\n", "#background = path\n");
		fprintf(config, "%s\n", "[ Keyboard layouts ]");
		fprintf(config, "%s\n", "# Alt+Shift to switch layouts");
		fprintf(config, "%s\n", "# e.g: xkb_layouts=us,de");
		fprintf(config, "%s\n", "xkb_layouts=us\n");
		fprintf(config, "%s\n", "[ Multimedia keys ]");
		fprintf(config, "%s\n", "# For default multimedia keys support install: playerctl, alsa-utils");
		fprintf(config, "%s\n", "# or use your own commands.");
		fprintf(config, "%s\n", "play_pause  = playerctl play-pause");
		fprintf(config, "%s\n", "volume_up   = amixer set Master 3+");
		fprintf(config, "%s\n", "volume_down = amixer set Master 3-");
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
		fprintf(config, "%s\n", "zoom_speed = 5");
		fprintf(config, "%s\n", "zoom_top_edge = disabled");
		fprintf(config, "%s\n", "zoom_edge_threshold = 30\n");
		fprintf(config, "%s\n", "[ Startup ]");
		fprintf(config, "%s\n", "# Specify the startup commands.");
		fprintf(config, "%s\n", "# If no startup command is specified then");
		fprintf(config, "%s\n", "# it will automatically look for the following terminals:");
		fprintf(config, "%s\n", "# foot, xfce4-terminal, kitty, gnome-terminal, alacritty.");
		fprintf(config, "%s\n", "# Example (automatically start thunar and foot):");
		fprintf(config, "%s\n", "# NOTE: the line must start with startup_command");
		fprintf(config, "%s\n", "#startup_command = thunar");
		fprintf(config, "%s\n", "#startup_command = foot\n");
		fclose(config);

		// Log that the configuration files were created
		///wlr_log(WLR_INFO, "Configuration files created successfully!");
	}
	else {
		// Log that the configuration files already exist
		///wlr_log(WLR_INFO, "Configuration files exist, nothing to do!");
	}
}
