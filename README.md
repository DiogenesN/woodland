# SPDX-License-Identifier: GPL-2.0-or-later

# Woodland

Woodland is a minimal lightweight wlroots-based window-stacking compositor for Wayland, inspired
by Wayfire and TinyWl. This version is ported to wlroots 0.18. Woodland was born out of the idea
that there was no window-stacking Wayland compositors that would also have the zooming capability
which is crucial for me. There was only GNOME and Wayfire, the first one is not my taste at all.

Zooming in GNOME is not ideal. Wayfire is great and zooming works well but I wanted to implement
some functionality and it was C++, I can only speak C and a bit of RUST. Another concern is longevity and
maintainability, if tomorrow Wayfire goes away, I will have with no other options, that is why I decided
to make my own compositor and implement all the functionality I need and I will be glad if someone
finds it useful too. Another thing, I build it on Debian stable (not testing) so a new version is
expected once every two years (following Debian stable release cycle).

Woodland has no reliance on any particular Desktop Environment, Desktop Shell or session. Also it does not depend on any UI toolkits such as Qt or GTK. Woodland is probably one of the last traditional old-fashion desktop experience that uses a simple stacking model with no tiling capabilities.

For Arch users see (thanks to TrialnError): https://aur.archlinux.org/packages/woodland

Recommended welcome screen:

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
   11. Menu (windowlist) showing a list of opened windows (clicking the top right corner).
   12. Menu showing a list of user defined items (clicking the bottom left corner).
   13. Applauncher to quickly launch your favorite applicaitons.ottom left corner).
   14. Built-in minimal network manager.

# Panel features

   1. Volume adjusting.
   2. Brightness adjusting.
   3. Right click on panel brightness icon switches off the display, press super key to switch back on.
   4. Time widget, clicking on it opens up a calendar for the current month.
   5. Network widget, clicking on it opens up a network applet.

# Note
Makefile.in contains the gcc flag  -march=native. This will compile woodland for your CPU on this machine and will make your binary incompatible if you try to run it on other machines with a different CPU. If you want compatibility then remove this flag. 

# Bugs
   1. If it fails to start from another Wayland compositor then make the following changes in: ~/.config/woodland/woodland.ini\
      Change:\
      tap_to_click = enable

      To:\
      tap_to_click = disable

      NOTE: Launching from another compositor might not show the menus/panels but <Super><Space> should work to open the applauncher.

   2. Whenever you type in the WIFI password in the network manager, the compositor would freeze for exactly 10 seconds, this is needed for password.
   3. At this stage the 'zwlr_layer_shell_v1' protocol is not implemented so some stuff like waybar, slurp won't work. You can still take screenshots with grim only without slurp. If you want to take a screenshot of a particular portion of the screen, then simply zoom in and use grim.

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
If you want to autostart woodland without any login manager then below is a good approach:

  1. Install the following packages:

		 sudo apt install greetd tuigreet seatd

  2. Create the following file:

		 sudo nano /usr/local/bin/start-woodland

  3. the content of '/usr/local/bin/start-woodland':

         #!/bin/sh
         exec dbus-run-session -- /usr/local/bin/woodland

  4. Make it executable:

		 sudo chmod +x /usr/local/bin/start-woodland

  5. Edit the following configuration file:

         sudo nano /etc/greetd/config.toml

  6. Modify 'config.toml' as follows:

         [terminal]
         # The VT to run the greeter on. Can be "next", "current" or a number
         # designating the VT.
         vt = 1

         # The default session, also known as the greeter.
         [default_session]

         # `agreety` is the bundled agetty/login-lookalike. You can replace `/bin/sh`
         # with whatever you want started, such as `sway`.
         command = "/usr/local/bin/start-woodland"
         # if using wlgreet
         #command = "sway --config /etc/greetd/sway-config"

         # The user to run the command as. The privileges this user must have depends
         # on the greeter. A graphical greeter may for example require the user to be
         # in the `video` group.
         user = "YOURUSERNAME"

  7. Add your username to the following groups:

         sudo usermod -aG input YOURUSERNAME
         sudo usermod -aG video YOURUSERNAME

  8. Disable 'getty' service and enable 'greetd':

         sudo systemctl disable getty@tty1
         sudo systemctl enable greetd

# First intallation start

You can launch woodland with arguments: woodland -s xfce4-terminal
or you can launch it without any arguments.
If you launch it without arguments for the first time then it will automatically look for the following terminals on the system:

		 foot
		 kitty
		 alacrity
		 xfce4-terminal
		 gnome-terminal

If it finds any of those installed, it will automatically launch the first one found.\
If you have none of these installed then you can launch any installed apps by activating the applauncher by pressing <Super><Space>.
To disable looking for pre-installed terminal emulators, you have to set up at least one startup command in woodland.ini.

# Screenshots
 Showing the menu on the bottom left and the panel on the bottom right.\
![Alt text](https://github.com/DiogenesN/woodland/blob/main/screenshots/1.menu_panel.png)

 Showing the calendar.\
![Alt text](https://github.com/DiogenesN/woodland/blob/main/screenshots/2.calendar.png)

 Showing the windowlist on the top right.\
![Alt text](https://github.com/DiogenesN/woodland/blob/main/screenshots/3.windowlist.png)

 Showing the network manager.\
![Alt text](https://github.com/DiogenesN/woodland/blob/main/screenshots/4.network_manager.png)

 Showing the applauncher.\
![Alt text](https://github.com/DiogenesN/woodland/blob/main/screenshots/5.applauncher.png)

# Configuration
Woodland creates the following configuration files:

	~/.config/woodland/icons.cache
	~/.config/woodland/woodland.ini
	~/.config/woodland/windows_sizes.db

  'icons.cache' stores the paths to all the SVG icons from the specified (or default) icon theme.\
  'windows_sizes.db' is automatically written on any window closing and storing the sizes before closing.\
  'woodland.ini' is very straightforward, self-explanatory and includes the comments and examples:

    # Configuration file for woodland compositor

    [ Welcome screen ]
    # If you have any weclome screen application then it goes here.
    welcome_screen = none

    [ Icon Theme ]
    # NOTE: To apply the theme you have to restart the compositor
    # The icon theme is used to provide icons for the windowlist menu
    # Provide the full path to your icon theme, example:
    # icons_theme = /usr/share/icons/Lyra-blue-dark
    # To disable icons, set it to: icons_theme = none
    icons_theme = /usr/share/icons/hicolor

    [ Brightness ]
    # d_power_path, the path to the file that controls the brightness level.
    d_power_path = /sys/class/backlight/intel_backlight/brightness

    [ Background ]
    # Provide the full path to the image.
    # The default wallpaper will be automatically set on the first launch.
    background = /home/YOURUSERNAME/.config/woodland/icons/woodland.png

    [ Touchpad ]
    # enable or disable tap to click.
    tap_to_click = enable

    [ Keyboard layouts ]
    # Alt+Shift to switch layouts
    # e.g: xkb_layouts=us,de
    xkb_layouts=us

    [ Multimedia keys ]
    # For default multimedia keys support install: playerctl, alsa-utils
    # or use your own commands.
    play_pause  = playerctl play-pause
    volume_up   = amixer set Master 3%+
    volume_down = amixer set Master 3%-
    volume_mute = amixer set Master toggle

    [ Keyboard Shortcuts ]
    # Modifiers names:
    # WLR_MODIFIER_ALT
    # WLR_MODIFIER_CTRL
    # WLR_MODIFIER_SHIFT
    # WLR_MODIFIER_LOGO (Super key)
    #
    # Key names here: /usr/include/xkbcommon/xkbcommon-keysyms.h
    #
    # Default shortcuts:
    # <Super+Esc> to log out
    # <Super+x> to close the current window
    # <Alt+Tab> to switch to the next window
    # <Alt+Ctrl+Tab> to switch to the previous window
    # <Super+Space> to open the applauncher
    # Example of user defined shortcuts:
    # NOTE: You have to preserve binding_ and command_ prefixes.
    #binding_thunar = WLR_MODIFIER_LOGO XKB_KEY_f
    #command_thunar = thunar

    # Below is an example of how to use my DMelody player with multimetia keys support
    # it sets the keys for the previous track, toggle pause and the next track.
    # dmelody controls
    binding_dmelody_prev = WLR_MODIFIER_LOGO XKB_KEY_KP_Left
    command_dmelody_prev = dmelody --previous
    binding_dmelody_toggle_pause = WLR_MODIFIER_LOGO XKB_KEY_KP_Begin
    command_dmelody_toggle_pause = dmelody --toggle-pause
    binding_dmelody_next = WLR_MODIFIER_LOGO XKB_KEY_KP_Right
    command_dmelody_next = dmelody --next

    # And for screenshot shortcut
    # Screenshot interractive
    # takes a screenshot when pressing <Super> and <number 2> (XKB_KEY_KP_Down) on numpad.
    binding_screenshot = WLR_MODIFIER_LOGO XKB_KEY_KP_Down
    command_screenshot = grim

    [ Window Placement ]
    # Open specified windows at the given fixed position.
    # to get the title and/or app_id, use wlrctl tool.
    # The placement model is as follows:
    # (declaration) window_place = (keyword) app_id: (app id) app_id (number) x (number) y
    # (declaration) window_place = (keyword) title: (title) title (number) x (number) y
    # Example of how to make 'thunar' start at position x=100 y=100:
    #window_place = app_id: thunar 100 100 (places thunar at x=100 y=100)
    # or
    #window_place = title: "some title" 100 100 		(places window containing title at x=100 y=100)
    # NOTE: Titles with spaces must be put between double quotes: e.g "New Document"

    # Few examples from my config:
    # Placing thunar
    window_place = app_id: thunar 875 1

    # Placing mousepad
    window_place = app_id: mousepad 870 1

    [ Zoom ]
    # Zooming is activated by pressing super key and scrolling.
    # zoom_speed defines how fast zooming area is moving around.
    # zoom_edge_threshold defines the distance from the edges to start panning.
    # zoom_top_edge if 'enabled' then you can scroll on the left top edge to zoom.
    zoom_speed = 0.009

    [ Startup ]
    # Specify the startup commands.
    # If no startup command is specified then
    # it will automatically look for the following terminals:
    # foot, xfce4-terminal, kitty, gnome-terminal, alacritty.
    # Example (automatically start thunar and foot):
    # NOTE: the line must start with startup_command
    #startup_command = thunar
    #startup_command = foot

    [ Menu ]
    # Here you can specify a few items that will appear
    # when clicking on the left bottom corner of the screen.
    # mn_active_area_x/y sets the size of the clickable area on the bottom left corner.
    mn_font_size = 22
    mn_active_area_x = 30
    mn_active_area_y = 30

    menu_item = Thunar
    menu_item = thunar
    menu_item = Reboot
    menu_item = systemctl reboot
    menu_item = Power Off
    menu_item = systemctl poweroff

    [ Windowlist ]
    # Clicking on the top right corner of the screen shows a window list.
    # wl_active_area_x/y sets the size of the clickable area on the top right corner.
    wl_active_area_x = 5
    wl_active_area_y = 5

    [ Panel ]
    # Hovering over the bottom right corner of the screen shows a panel.

    [ Applauncher ]
    # Press <Super+Space> to open applauncer to run your favorite app.
    # If your app doesn't show up in the search box, you need to refresh it.
    # To refresh, type in the search box /refresh (hit Enter).

Tha's it!

# Support

   My Libera IRC support channel: #linuxfriends

   Matrix: https://matrix.to/#/#linuxfriends2:matrix.org

   Email: nicolas.dio@protonmail.com
