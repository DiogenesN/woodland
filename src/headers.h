// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef HEADERS_H
#define HEADERS_H

#define PPANEL_WIDTH 240
#define PPANEL_HEIGHT 50
#define PNETWORK_WIDTH 330
#define PNETWORK_HEIGHT 675

/* System headers */
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <wordexp.h>
#include <stdbool.h>
#include <libinput.h>
#include <cairo/cairo.h>
#include <stb/stb_image.h>
#include <pixman-1/pixman.h>
#include <libdrm/drm_fourcc.h>
#include <dbus-1.0/dbus/dbus.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server-core.h>
#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wlroots-0.18/wlr/backend.h>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <wlroots-0.18/wlr/util/log.h>
#include <wlroots-0.18/wlr/types/wlr_drm.h>
#include <wlroots-0.18/wlr/types/wlr_seat.h>
#include <wlroots-0.18/wlr/types/wlr_scene.h>
#include <wlroots-0.18/wlr/backend/session.h>
#include <wlroots-0.18/wlr/backend/libinput.h>
#include <wlroots-0.18/wlr/render/allocator.h>
#include <wlroots-0.18/wlr/types/wlr_output.h>
#include <wlroots-0.18/wlr/types/wlr_cursor.h>
#include <wlroots-0.18/wlr/types/wlr_keyboard.h>
#include <wlroots-0.18/wlr/types/wlr_xdg_shell.h>
#include <wlroots-0.18/wlr/types/wlr_compositor.h>
#include <wlroots-0.18/wlr/types/wlr_viewporter.h>
#include <wlroots-0.18/wlr/types/wlr_data_device.h>
#include <wlroots-0.18/wlr/types/wlr_input_device.h>
#include <wlroots-0.18/wlr/types/wlr_drm_lease_v1.h>
#include <wlroots-0.18/wlr/types/wlr_xdg_output_v1.h>
#include <wlroots-0.18/wlr/types/wlr_screencopy_v1.h>
#include <wlroots-0.18/wlr/types/wlr_output_layout.h>
#include <wlroots-0.18/wlr/types/wlr_subcompositor.h>
#include <wlroots-0.18/wlr/types/wlr_xcursor_manager.h>
#include <wlroots-0.18/wlr/types/wlr_data_control_v1.h>
#include <wlroots-0.18/wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlroots-0.18/wlr/types/wlr_export_dmabuf_v1.h>
#include <wlroots-0.18/wlr/types/wlr_presentation_time.h>
#include <wlroots-0.18/wlr/types/wlr_fractional_scale_v1.h>
#include <wlroots-0.18/wlr/types/wlr_relative_pointer_v1.h>
#include <wlroots-0.18/wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlroots-0.18/wlr/types/wlr_output_management_v1.h>
#include <wlroots-0.18/wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlroots-0.18/wlr/types/wlr_pointer_constraints_v1.h>
#include <wlroots-0.18/wlr/types/wlr_foreign_toplevel_management_v1.h>

#endif
