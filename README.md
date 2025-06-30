# SPDX-License-Identifier: GPL-2.0-or-later

# Woodland

Woodland is a minimal lightweight wlroots-based window-stacking compositor for Wayland, inspired
by Wayfire and TinyWl. This version is ported to wlroots 0.18.  Woodland was born out of the idea
that there was no window-stacking Wayland compositors that would also have the zooming capability
which is crucial for me. There was only GNOME and Wayfire, the  first one is not my taste at all.

Zooming in GNOME is not ideal. Wayfire is great and  zooming  works  well  but I wanted to implement
some  functionality  and  it  was  C++,  I  can  only  speak  C.  Another  concern  is longevity and
maintainability, if tomorrow Wayfire goes away I will  remain with no options, that is why I decided
to make my own compositor and implement all the functionality  I need  and I will  be glad if someone
finds it useful too. Another thing, I build it on Debian  13 stable  (not testing) so a new version is
expected once every two years (following Debian stable release cycle). Woodland has no reliance on any
particular Desktop Environment, Desktop Shell or session. Also it does not depend on any UI toolkits such as Qt or GTK.

Recommended quick app launcher:

Application launcher:
[diowapplauncher](https://github.com/DiogenesN/diowapplauncher)\
Welcome Screen:
[welcomescreen](https://github.com/DiogenesN/welcomescreen).


# Special thanks to all the developers, maintainers and contributors of the following projects:

dwl\
sway\
labwc\
tinywl\
waybox\
wayfire\
wlroots\
vivarium\

# and also to all open-source enthusiasts in general.

# Compositor features

   1. Screen zooming (Super+mouse wheel or scrolling on the top left corner of the screen).
   2. A little panel (hovering bottom right corner of the screen).
   3. Set background image (without relying on third-party utilities).
   4. Multiple keyboard layouts.
   5. Per application keyboard layout.
   6. Keyboard shortcuts.
   7. Multimedia keys support.
   8. User-defined window placement.
   9. Automatic window size save.
   19. Autostart applications.
   11. Menu showing a list of opened windows (clicking the top right corner).
   12. Menu showing a list of user defined items (clicking the bottom left corner).

# Panel features

   1. Volume adjusting.
   2. Brightness adjusting.
   3. Right click on panel brightness icon switches off the display, press super key to switch back on.
   4. Time widget, clicking on it opens up a calendar for the current month.
   5. Network widget, clicking on it opens up a network applet.

# Bugs

   Whenever you click on the network icon on the panel, the compositor would freeze for exactly 10 seconds, this is done in order to scan the available wifi networks.

# Installation

  1. To build the project you need to install the following libs:

	        gcc
	        bash
	        make
	        pkgconf
	        libstb-dev
	        libdrm-dev
	        librsvg2-dev
	        libinput-dev
	        libcairo2-dev
	        libdbus-1-dev
	        linux-libc-dev
	        libwayland-dev
	        libpixman-1-dev
	        libxkbcommon-dev
	        libwlroots-0.18-dev

  2. Open a terminal in the extracted folder and run:
 
		 chmod +x ./configure
		 ./configure

  3. if all went well then run:

		 make
		 sudo make install
		 
		 (if you just want to test it then run: make run)

# Usage

You have many options how to launch woodland (or pretty much any application).
The simplest one is to just run woodland from a TTY or login manager.
If you want to autostart woodland without any login manager then these are the steps:

  1. Create this file:

		 sudo nano /etc/profile.d/woodland.sh

  2. The content of woodland.sh:

		 if [ -z $WAYLAMD_DISPLAY ] && [ "$(tty)" = "/dev/tty1" ]; then
			exec woodland > /dev/null 2>&1
		 fi

  3. Make it executable:

		 sudo chmod +x /etc/profile.d/woodland.sh

  4. Modify 'getty@tty1.service' for autologin. (Disclaimer: Be cautious!!! This might be a security risk so do at your own risk.)

		 sudo nano /etc/systemd/system/getty.target.wants/getty@tty1.service

  5. Find the line that starts with 'ExecStart', comment it out and add this one instead:

		 ExecStart=-/sbin/agetty --skip-login --nonewline --noissue --autologin YOURUSERNAME --noclear - $TERM

# First intallation start

You can launch woodland with arguments: woodland -s xfce4-terminal
or you can launch it without any arguments.
If you launch it without arguments for the first time then it will automatically look for the following terminals on the system:

		 foot
		 kitty
		 alacrity
		 xfce4-terminal
		 gnome-terminal

If it finds any of those installed, it will automatically launch the first one found.
To disable this behavior you will need to set up at least one startup command in woodland.ini.

# Configuration
Woodland creates the following configuration files:

	~/.config/woodland/woodland.ini
	~/.config/woodland/windows_sizes.db

  'windows_sizes.db' is automatically written on any window closing and storing the sizes before closing.\
  'woodland.ini' is very straightforward and self-explanatory but we will go through each section

  1. Welcome screen

    [ Welcome screen ]
    If you have any weclome screen application then it goes here,
    for instance you can use my welcome screen application like this:
    welcome_screen = welcomescreen --resolution 1920x1080

  2. Brightness
 
    [ Brightness ]
    In order for backlight to worl you have to do the following:

    sudo usermod -aG video $USER
    sudo touch /etc/udev/rules.d/90-backlight.rules
    sudo nano /etc/udev/rules.d/90-backlight.rules
    add the following to '90-backlight.rules':
    ACTION=="add", SUBSYSTEM=="backlight", KERNEL=="intel_backlight", RUN+="/bin/chgrp video /sys/class/backlight/intel_backlight/brightness", RUN+="/bin/chmod 664 /sys/class/backlight/intel_backlight/brightness"

     d_power_path, the path to the file that controls the brightness level.\
     d_power_path = /sys/class/backlight/intel_backlight/brightness

  3. Background image

	[ Background ]
	Provide the full path to the image.
	background = /home/username/image.png

  4. Touchpan tap-to-click

    [ Touchpad ]
    Enable or disable tap to click (default enable).
    tap_to_click = enable

  5. Keyboard layouts

	[ Keyboard layouts ]
	Alt+Shift to switch layouts
	e.g: xkb_layouts=us,de
	xkb_layouts=us,de

  6. Multimedian keys

	[ Multimedia keys ]
	For default multimedia keys support install: playerctl, alsa-utils
	or use your own commands.
	play_pause  = playerctl play-pause
	volume_up   = amixer set Master 3+
	volume_down = amixer set Master 3-
	volume_mute = amixer set Master toggle

  7. Keyboard shortcuts

	[ Keyboard Shortcuts ]
	Modifiers names:
	WLR_MODIFIER_ALT
	WLR_MODIFIER_CTRL
	WLR_MODIFIER_SHIFT
	WLR_MODIFIER_LOGO (Super key)
		
	Key names here: /usr/include/xkbcommon/xkbcommon-keysyms.h
	
	Default shortcuts:
	<Super+Esc> to log out
	<Super+x> to close the current window
	<Alt+Tab> to switch to the next window
	Example of user defined shortcuts:
	NOTE: You have to preserve binding_ and command_ prefixes.
	binding_thunar = WLR_MODIFIER_LOGO XKB_KEY_f
	command_thunar = thunar

  8. Window placement

	[ Window Placement ]
	Open specified windows at the given fixed position.
	to get the title and/or app_id, use wlrctl tool.
	The placement model is as follows:
	(declaration) window_place = (keyword) app_id: (app id) app_id (number) x (number) y
	(declaration) window_place = (keyword) title: (title) title (number) x (number) y
	Example of how to make 'thunar' start at position x=100 y=100:
	window_place = app_id: thunar 100 100
	or
	window_place = title: "some title" 100 100
	NOTE: Titles with spaces must be put between double quotes: e.g "New Document"

	Placing thunar
	window_place = app_id: thunar -15 -15

  9. Zoom

	[ Zoom ]
	Zooming is activated by pressing super key and scrolling.
	Another way of zooming is by scrolling ont he top left corner of the screen,
	zoom_speed defines how fast zooming area is moving around.
    zoom_speed = 0.009


  10. Autostart applications

  	[ Startup ]
	Specify the startup commands.
	If no startup command is specified then
	it will automatically look for the following terminals:
	foot, xfce4-terminal, kitty, gnome-terminal, alacritty.
	Example (automatically start thunar and foot):
	NOTE: the line must start with startup_command

	My startup applications:
	startup_command = polari

  11. Menu

    [ Menu ]
    Here you can specify a few items that will appear
    when clicking on the left bottom corner of the screen.
    First 'menu_item' should be the name of the application (e.g. Thunar File Manager).
    Second 'menu_item' is the command for the application (thunar).
    Examples:
    menu_item = Reboot
    menu_item = systemctl reboot
    menu_item = Power Off
    menu_item = systemctl poweroff

  12. Window list

    [ Windowlist ]
    Clicking on the top right corner of the screen shows a window list.

  13. Panel

    [ Panel ]
    Hovering over the bottom right corner of the screen shows a panel.
    Right click on the brightness icon will switch off the screen,
    pressing Super key will turn it back on.

That is it enjoy!

# Support

   My Libera IRC support channel: #linuxfriends

   Matrix: https://matrix.to/#/#linuxfriends2:matrix.org

   Email: nicolas.dio@protonmail.com
