# SPDX-License-Identifier: GPL-2.0-or-later

# Woodland

Woodland is a minimal lightweight wlroots-based window-stacking compositor for Wayland, inspired
by Wayfire and TinyWl. For a minimal desktop environment experience you can use it together with:

Panel:
[diowpanel](https://github.com/DiogenesN/diowpanel)

Menu:
[diowmenu](https://github.com/DiogenesN/diowmenu)

Window list:
[diowwindowlist](https://github.com/DiogenesN/diowwindowlist)

Application launcher:
[diowapplauncher](https://github.com/DiogenesN/diowapplauncher)

Woodland has no reliance on any particular Desktop Environment, Desktop Shell or session.
Also it does not depend on any UI toolkits such as Qt or GTK.

The main goal of Woodland is to provide basic functionality, ease of use and keeoing things simple.
It was tested on Debian 12.

# Special thanks to all the developers, maintainers and contributors of the following projects:

sway

labwc

tinywl

waybox

wayfire

wlroots

vivarium

and also to all open-source enthusiasts in general.

# Features

   1. Screen zooming.
   2. Idle timeout.
   3. Set background image (without relying on third-party unitilies).
   4. Multiple keyboard layouts.
   5. Per application keyboard layout.
   6. Keyboard shortcuts.
   7. User-defined window placement.
   8. Autostart applications.

# TODO:

	Many things like: multimedia keys support, maximizing, window decorations etc.

# Installation

  1. To build the project you need to install the following libs:

		 make
		 pkgconf
		 libstb-dev
		 libdrm-dev
		 libgles-dev
		 libinput-dev
		 libwayland-dev
		 libwlroots-dev
		 libpixman-1-dev
		 libxkbcommon-dev

  2. Open a terminal and run:
 
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

		 sudo chmod +x /etc/profile.d/wayfire.sh

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
Woodland creates the following configuration file:

		~/.config/woodland/woodland.ini

  it is very straightforward and self-explanatory but we will go through each section

  1. Idle
 
	[ Idle ]
	The timeout in milliseconds until the system is considered idle.
	One minute is 60000 milliseconds.
	idle_timeout = 0 disables the timeout.
	idle_timeout = 180000

  2. Background image

	[ Background ]
	Provide the full path to the image.
	background = /home/username/image.png

  3. Keyboard layouts

	[ Keyboard layouts ]
	Alt+Shift to switch layouts
	e.g: xkb_layouts=us,de
	xkb_layouts=us,de

  4. Keyboard shortcuts

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

  5. Window placement

	[ Window Placement ]
	Open specified windows at the given fixed position.
	to get the title and/or app_id, use wlrctl tool.
	The placement model is as follows:
	(declaration) window_place = (keyword) app_id: (app id) app_id (number) x (number) y
	(declaration) window_place = (keyword) title: (title) title (number) x (number) y
	Example of how to make 'thunar' start at position x=100 y=100:
	#window_place = app_id: thunar 100 100 (places thunar at x=100 y=100)
	or
	#window_place = title: "some title" 100 100 (places window containing title at x=100 y=100)
	NOTE: Titles with spaces must be put between double quotes: e.g "New Document"

	Placing thunar
	window_place = app_id: thunar -15 -15

  6. Zoom

	[ Zoom ]
	Zooming is activated by pressing super key and scrolling.
	zoom_speed defines how fast zooming area is moving around.
	zoom_edge_threshold defines the distance from the edges to start panning.
	zoom_top_edge if 'enabled' then you can scroll on the left top edge to zoom.
	zoom_speed = 5
	zoom_top_edge = enabled
	zoom_edge_threshold = 30

  7. Autostart applications

  	[ Startup ]
	Specify the startup commands.
	If no startup command is specified then
	it will automatically look for the following terminals:
	foot, xfce4-terminal, kitty, gnome-terminal, alacritty.
	Example (automatically start thunar and foot):
	NOTE: the line must start with startup_command

	My startup applications:
	startup_command = mako
	startup_command = polari
	startup_command = diowmenu
	startup_command = diowpanel
	startup_command = diowwindowlist

That is it enjoy!

# Support

   My Libera IRC support channel: #linuxfriends

   Matrix: https://matrix.to/#/#linuxfriends2:matrix.org

   Email: nicolas.dio@protonmail.com
