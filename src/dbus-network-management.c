// SPDX-License-Identifier: GPL-2.0-or-later

#include "headers.h"

#define NM_PATH "/org/freedesktop/NetworkManager"
#define NM_IFACE "org.freedesktop.NetworkManager"
#define NM_SERVICE "org.freedesktop.NetworkManager"
#define NM_INTERFACE "org.freedesktop.NetworkManager"
#define NM_AP_IFACE "org.freedesktop.NetworkManager.AccessPoint"
#define NM_DEVICE_INTERFACE "org.freedesktop.NetworkManager.Device"
#define NM_AP_INTERFACE "org.freedesktop.NetworkManager.AccessPoint"
#define NM_SETTINGS_INTERFACE "org.freedesktop.NetworkManager.Settings"
#define NM_WIRELESS_DEVICE_INTERFACE "org.freedesktop.NetworkManager.Device.Wireless"

static bool check_dbus_error(DBusError *error) {
	if (dbus_error_is_set(error)) {
		fprintf(stderr, "D-Bus error: %s\n", error->message);
		dbus_error_free(error);
		return true; // Return true to indicate an error
	}
	return false; // Return false if no error
}

/**
 ************************************* Scan for available networks *************************************
 */

static char *get_active_ap(DBusConnection *conn, const char *device_path) {
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	dbus_error_init(&error);

	msg = dbus_message_new_method_call(NM_SERVICE,
										device_path,
										"org.freedesktop.DBus.Properties",
										"Get"
	);
	const char *iface = NM_WIRELESS_DEVICE_INTERFACE;
	const char *prop = "ActiveAccessPoint";
	dbus_message_append_args(msg,
							DBUS_TYPE_STRING, &iface,
							DBUS_TYPE_STRING, &prop,
							DBUS_TYPE_INVALID
	);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (check_dbus_error(&error)) {
		fprintf(stderr, "Failure occurred in 'get_active_ap'!\n");
		return NULL;
	}

	DBusMessageIter iter;
	DBusMessageIter variant;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &variant);

	const char *ap_path = NULL;
	dbus_message_iter_get_basic(&variant, &ap_path);

	char *result = NULL;
	if (ap_path && strlen(ap_path) > 0) {
		result = strdup(ap_path);
	}

	dbus_message_unref(reply);
	return result;
}

static void request_scan(DBusConnection *conn, const char *device_path) {
	DBusMessage *msg, *reply;
	DBusError error;
	dbus_error_init(&error);

	msg = dbus_message_new_method_call(
		NM_SERVICE,
		device_path,
		NM_WIRELESS_DEVICE_INTERFACE,
		"RequestScan"
	);

	// Pass an empty dict (a{sv}) as scan options
	DBusMessageIter args;
	DBusMessageIter dict;
	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
	dbus_message_iter_close_container(&args, &dict);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (check_dbus_error(&error)) {
		fprintf(stderr, "Failure occurred in 'request_scan' make sure Wi-Fi is enabled!\n");
		return;
	}

	dbus_message_unref(reply);

	// Optional: wait a bit to let the scan complete
	sleep(10); // You can tweak or use async logic if desired
}

static char *get_ssid(DBusConnection *conn, const char *ap_path) {
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	dbus_error_init(&error);

	msg = dbus_message_new_method_call(NM_SERVICE,
										ap_path,
										"org.freedesktop.DBus.Properties",
										"Get"
	);
	const char *iface = NM_AP_IFACE;
	const char *prop = "Ssid";
	dbus_message_append_args(msg,
							DBUS_TYPE_STRING, &iface,
							DBUS_TYPE_STRING, &prop,
							DBUS_TYPE_INVALID
	);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (check_dbus_error(&error)) {
		fprintf(stderr, "Failure occurred in 'get_ssid'!\n");
        dbus_message_unref(reply); // Unref reply in case of error
		return NULL;
	}

	DBusMessageIter iter;
	DBusMessageIter variant;
	DBusMessageIter array;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &variant);
	dbus_message_iter_recurse(&variant, &array);

	unsigned char ssid[256];
	size_t i = 0;
	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_BYTE && i < sizeof(ssid) - 1) {
		dbus_message_iter_get_basic(&array, &ssid[i++]);
		dbus_message_iter_next(&array);
	}
	ssid[i] = '\0';

	char *ssid_str = strdup((char *)ssid);
	if (!ssid_str) {
		fprintf(stderr, "Memory allocation failed in 'get_ssid'!\n");
		dbus_message_unref(reply);
		return NULL;
	}
	else {
		dbus_message_unref(reply);
	}
	return ssid_str;
}

static size_t list_access_points(DBusConnection *conn, const char *device_path, char **ssids, size_t max) {
	char *active_ap = get_active_ap(conn, device_path);

	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	dbus_error_init(&error);

	msg = dbus_message_new_method_call(NM_SERVICE,
									device_path,
									NM_WIRELESS_DEVICE_INTERFACE,
									"GetAllAccessPoints"
	);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (check_dbus_error(&error)) {
		fprintf(stderr, "Failure occurred in 'get_active_ap'!\n");
        dbus_message_unref(reply); // Unref reply in case of error
		return -1;
	}

	DBusMessageIter iter;
	DBusMessageIter array;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &array);

	// Temporarily store all SSIDs and separate active
	char *tmp_ssids[256];
	size_t tmp_count = 0;
	char *active_ssid = NULL;
	char *ssid = NULL;

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_OBJECT_PATH) {
		const char *ap_path = NULL;
		dbus_message_iter_get_basic(&array, &ap_path);
		ssid = get_ssid(conn, ap_path);
		if (!ssid) {
			dbus_message_iter_next(&array);
			continue;
		}

		if (active_ap && strcmp(ap_path, active_ap) == 0) {
			active_ssid = ssid;
		}
		else if (tmp_count < sizeof(tmp_ssids) / sizeof(tmp_ssids[0])) {
			tmp_ssids[tmp_count++] = ssid;
		}
		else {
			free(ssid);
			ssid = NULL;
		}

		dbus_message_iter_next(&array);
	}

	size_t count = 0;
	if (active_ssid && count < max) {
		ssids[count++] = active_ssid;
	} else if (active_ssid) {
		// Couldn’t store it, so free it
		free(active_ssid);
	}

	for (size_t i = 0; i < tmp_count && count < max; ++i) {
		ssids[count++] = tmp_ssids[i];
	}
	for (size_t i = count; i < tmp_count; ++i) {
		// Clean up unused SSIDs
		free(tmp_ssids[i]);
	}
	if (active_ap) {
		free(active_ap); // clean and safe now
	}
	dbus_message_unref(reply);
	return count;
}

size_t list_wifi_devices(char **ssids, size_t max) {
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	dbus_error_init(&error);

	DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

	if (check_dbus_error(&error)) {
		fprintf(stderr, "Failure occurred in 'list_wifi_devices'!\n");
		return -1;
	}
	
	msg = dbus_message_new_method_call(NM_SERVICE,
										NM_PATH,
										NM_IFACE,
										"GetDevices"
	);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (check_dbus_error(&error)) {
		fprintf(stderr, "Failure occurred in 'list_wifi_devices 2'!\n");
		return -1;
	}

	size_t count = 0;
	DBusMessageIter iter;
	DBusMessageIter array;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_OBJECT_PATH) {
		const char *device_path;
		dbus_message_iter_get_basic(&array, &device_path);

		// Check device type
		DBusMessage *type_msg = dbus_message_new_method_call(NM_SERVICE,
															device_path,
															"org.freedesktop.DBus.Properties",
															"Get"
		);
		const char *iface = NM_DEVICE_INTERFACE;
		const char *prop = "DeviceType";
		dbus_message_append_args(type_msg,
								DBUS_TYPE_STRING, &iface,
								DBUS_TYPE_STRING, &prop,
								DBUS_TYPE_INVALID
		);

		DBusMessage *type_reply = dbus_connection_send_with_reply_and_block(conn, type_msg, -1, &error);
		dbus_message_unref(type_msg);

		if (check_dbus_error(&error)) {
			fprintf(stderr, "Failure occurred in 'list_wifi_devices 3'!\n");
			return -1;
		}

		DBusMessageIter type_iter;
		DBusMessageIter variant;
		dbus_message_iter_init(type_reply, &type_iter);
		dbus_message_iter_recurse(&type_iter, &variant);
		uint32_t dev_type;
		dbus_message_iter_get_basic(&variant, &dev_type);
		dbus_message_unref(type_reply);

		if (dev_type == 2) { // NM_DEVICE_TYPE_WIFI
			request_scan(conn, device_path);
			count += list_access_points(conn, device_path, ssids + count, max - count);
		}

		dbus_message_iter_next(&array);
	}

	dbus_message_unref(reply);
	dbus_connection_unref(conn);
	return count;
}

/**
 ************************************* Check if given SSID is secured *************************************
 */

bool check_if_secured_ssid(char *ssid) {
	DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		fprintf(stderr, "Failed to connect to the D-Bus system bus\n");
		return false;
	}

	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	dbus_error_init(&error);

	// Retrieve all devices
	msg = dbus_message_new_method_call(NM_SERVICE,
										NM_PATH,
										NM_IFACE,
										"GetDevices"
	);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (check_dbus_error(&error)) {
		dbus_connection_unref(conn);
		return false;
	}

	DBusMessageIter iter;
	DBusMessageIter array;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &array);

	bool is_secured = false;

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_OBJECT_PATH) {
		const char *device_path;
		dbus_message_iter_get_basic(&array, &device_path);

		// Check device type
		DBusMessage *type_msg = dbus_message_new_method_call(NM_SERVICE,
															device_path,
															"org.freedesktop.DBus.Properties",
															"Get"
		);
		const char *iface = NM_DEVICE_INTERFACE;
		const char *prop = "DeviceType";
		dbus_message_append_args(type_msg,
								DBUS_TYPE_STRING, &iface,
								DBUS_TYPE_STRING, &prop,
								DBUS_TYPE_INVALID
		);

		DBusMessage *type_reply = dbus_connection_send_with_reply_and_block(conn, type_msg, -1, &error);
		dbus_message_unref(type_msg);

		if (check_dbus_error(&error)) {
			dbus_message_iter_next(&array);
			continue;
		}

		DBusMessageIter type_iter;
		DBusMessageIter variant;
		dbus_message_iter_init(type_reply, &type_iter);
		dbus_message_iter_recurse(&type_iter, &variant);
		uint32_t dev_type;
		dbus_message_iter_get_basic(&variant, &dev_type);
		dbus_message_unref(type_reply);

		if (dev_type != 2) { // Not Wi-Fi
			dbus_message_iter_next(&array);
			continue;
		}

		// Get AccessPoints
		msg = dbus_message_new_method_call(NM_SERVICE,
											device_path,
											NM_WIRELESS_DEVICE_INTERFACE,
											"GetAccessPoints"
		);

		reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
		dbus_message_unref(msg);

		if (check_dbus_error(&error)) {
			dbus_message_iter_next(&array);
			continue;
		}

		DBusMessageIter ap_iter;
		DBusMessageIter ap_array;
		dbus_message_iter_init(reply, &ap_iter);
		dbus_message_iter_recurse(&ap_iter, &ap_array);

		while (dbus_message_iter_get_arg_type(&ap_array) == DBUS_TYPE_OBJECT_PATH) {
			const char *ap_path;
			dbus_message_iter_get_basic(&ap_array, &ap_path);

			// Get SSID
			char *ap_ssid = get_ssid(conn, ap_path);
			if (ap_ssid && strcmp(ap_ssid, ssid) == 0) {
				// Get Flags
				DBusMessage *flags_msg = dbus_message_new_method_call(NM_SERVICE,
																	ap_path,
																	"org.freedesktop.DBus.Properties",
																	"Get"
				);
				const char *ap_iface = NM_AP_IFACE;
				const char *flags_prop = "Flags";
				dbus_message_append_args(flags_msg,
										DBUS_TYPE_STRING, &ap_iface,
										DBUS_TYPE_STRING, &flags_prop,
										DBUS_TYPE_INVALID
				);

				DBusMessage *flags_reply = dbus_connection_send_with_reply_and_block(conn,
																					flags_msg,
																					-1,
																					&error);
				dbus_message_unref(flags_msg);

				if (!check_dbus_error(&error)) {
					DBusMessageIter flags_iter;
					DBusMessageIter flags_variant;
					dbus_message_iter_init(flags_reply, &flags_iter);
					dbus_message_iter_recurse(&flags_iter, &flags_variant);
					uint32_t flags;
					dbus_message_iter_get_basic(&flags_variant, &flags);
					dbus_message_unref(flags_reply);

					if (flags & 0x1) { // NM_802_11_AP_FLAGS_PRIVACY
						is_secured = true;
					}
				}
				if (ap_ssid) {
					free(ap_ssid);
					ap_ssid = NULL;
				}
				if (reply) {
					dbus_message_unref(reply);
					reply = NULL;
				}
				break;
			}
			if (ap_ssid) {
				free(ap_ssid);
				ap_ssid = NULL;
			}
			dbus_message_iter_next(&ap_array);
		}
		if (reply) {
			dbus_message_unref(reply);
			reply = NULL;
		}
		if (is_secured) {
			break;
		}
		dbus_message_iter_next(&array);
	}
	if (reply) {
		dbus_message_unref(reply);
		reply = NULL;
	}
	dbus_connection_unref(conn);
	return is_secured;
}

/**
 ********************************* Connect to free or secured networks *********************************
 */
 
static DBusConnection *connect_system_bus(void) {
	DBusError err;
	dbus_error_init(&err);
	DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (!conn) {
		fprintf(stderr, "Failed to connect to system bus: %s\n", err.message);
		dbus_error_free(&err);
	}
	return conn;
}

// Utility: convert SSID (from AP) to string
static char *ssid_to_string(const unsigned char *ssid, int len) {
	char *str = malloc(len + 1);
	if (!str) return NULL;
	memcpy(str, ssid, len);
	str[len] = '\0';
	return str;
}

// Find first wireless device
static char *find_wifi_device(DBusConnection *conn) {
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	dbus_error_init(&err);

	msg = dbus_message_new_method_call(NM_SERVICE, NM_PATH, NM_INTERFACE, "GetDevices");
	if (!msg) {
		return NULL;
	}

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
	dbus_message_unref(msg);
	if (!reply) {
		return NULL;
	}

	DBusMessageIter iter;
	DBusMessageIter array_iter;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &array_iter);

	while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_OBJECT_PATH) {
		const char *device_path = NULL;
		dbus_message_iter_get_basic(&array_iter, &device_path);

		// Check DeviceType
		DBusMessage *get_type = dbus_message_new_method_call(NM_SERVICE, device_path,
															"org.freedesktop.DBus.Properties", "Get");

		DBusMessageIter args;
		dbus_message_iter_init_append(get_type, &args);
		const char *iface = NM_DEVICE_INTERFACE;
		const char *prop = "DeviceType";
		dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &iface);
		dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &prop);

		DBusMessage *type_reply = dbus_connection_send_with_reply_and_block(conn, get_type, -1, &err);
		dbus_message_unref(get_type);
		if (!type_reply) {
			continue;
		}

		DBusMessageIter type_iter;
		dbus_message_iter_init(type_reply, &type_iter);
		dbus_message_iter_recurse(&type_iter, &type_iter);
		uint32_t type;
		dbus_message_iter_get_basic(&type_iter, &type);
		dbus_message_unref(type_reply);

		if (type == 2) { // NM_DEVICE_TYPE_WIFI
			dbus_message_unref(reply);
			return strdup(device_path);
		}

		dbus_message_iter_next(&array_iter);
	}

	dbus_message_unref(reply);
	return NULL;
}

// Find access point object path for SSID
static char *find_ap_by_ssid(DBusConnection *conn, const char *device_path, const char *target_ssid) {
	DBusMessage *msg = dbus_message_new_method_call(NM_SERVICE, device_path,
													NM_WIRELESS_DEVICE_INTERFACE,
													"GetAccessPoints");
	DBusError err;
	dbus_error_init(&err);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
	dbus_message_unref(msg);
	if (!reply) {
		return NULL;
	}

	DBusMessageIter iter;
	DBusMessageIter array;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &array);

	while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_OBJECT_PATH) {
		const char *ap_path = NULL;
		dbus_message_iter_get_basic(&array, &ap_path);

		// Get SSID
		DBusMessage *ssid_msg = dbus_message_new_method_call(NM_SERVICE, ap_path,
															"org.freedesktop.DBus.Properties",
															"Get");

		DBusMessageIter args;
		dbus_message_iter_init_append(ssid_msg, &args);
		const char *iface = NM_AP_INTERFACE;
		const char *prop = "Ssid";
		dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &iface);
		dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &prop);
		DBusMessage *ssid_reply = dbus_connection_send_with_reply_and_block(conn, ssid_msg, -1, &err);
		dbus_message_unref(ssid_msg);
		if (!ssid_reply) {
			dbus_message_iter_next(&array);
			continue;
		}

		DBusMessageIter val_iter;
		DBusMessageIter array_iter;
		dbus_message_iter_init(ssid_reply, &val_iter);
		dbus_message_iter_recurse(&val_iter, &val_iter);
		dbus_message_iter_recurse(&val_iter, &array_iter);

		unsigned char ssid[256];
		int len = 0;
		while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_BYTE && len < 255) {
			dbus_message_iter_get_basic(&array_iter, &ssid[len++]);
			dbus_message_iter_next(&array_iter);
		}
		ssid[len] = '\0';
		dbus_message_unref(ssid_reply);

		char *ssid_str = ssid_to_string(ssid, len);
		if (ssid_str && strcmp(ssid_str, target_ssid) == 0) {
			free(ssid_str);
			ssid_str = NULL;
			dbus_message_unref(reply);
			return strdup(ap_path);
		}
		free(ssid_str);
		ssid_str = NULL;
		dbus_message_iter_next(&array);
	}

	dbus_message_unref(reply);
	return NULL;
}

static char *find_existing_connection(DBusConnection *conn, const char *ssid) {
	DBusMessage *msg = dbus_message_new_method_call(NM_SERVICE,
													"/org/freedesktop/NetworkManager/Settings",
													NM_SETTINGS_INTERFACE,
													"ListConnections");

	DBusError err;
	dbus_error_init(&err);
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
	dbus_message_unref(msg);
	if (!reply) {
		fprintf(stderr, "Failed to call ListConnections: %s\n", err.message);
		dbus_error_free(&err);
		return NULL;
	}

	DBusMessageIter iter;
	DBusMessageIter array_iter;
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &array_iter);

	while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_OBJECT_PATH) {
		const char *conn_path = NULL;
		dbus_message_iter_get_basic(&array_iter, &conn_path);
		fprintf(stderr, "conn_path: %s\n", conn_path);

		// Call GetSettings on this connection path
		DBusMessage *get_msg = dbus_message_new_method_call(NM_SERVICE, conn_path,
													"org.freedesktop.NetworkManager.Settings.Connection",
													"GetSettings");

		DBusMessage *get_reply = dbus_connection_send_with_reply_and_block(conn, get_msg, -1, &err);
		dbus_message_unref(get_msg);
		if (!get_reply) {
			fprintf(stderr, "Failed to call GetSettings on %s: %s\n", conn_path, err.message);
			dbus_error_free(&err);
			dbus_message_iter_next(&array_iter);
			continue;
		}

		// The reply contains a{sa{sv}} dictionary (an array of dict entries)
		DBusMessageIter dict_iter;
		dbus_message_iter_init(get_reply, &dict_iter);

		// This is an array of dictionary entries (a{sa{sv}})
		if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_ARRAY) {
			fprintf(stderr, "Unexpected GetSettings reply type\n");
			dbus_message_unref(get_reply);
			dbus_message_iter_next(&array_iter);
			continue;
		}

		// Recurse into array
		DBusMessageIter array_settings_iter;
		dbus_message_iter_recurse(&dict_iter, &array_settings_iter);

		while (dbus_message_iter_get_arg_type(&array_settings_iter) == DBUS_TYPE_DICT_ENTRY) {
			DBusMessageIter dict_entry;
			dbus_message_iter_recurse(&array_settings_iter, &dict_entry);

			const char *setting_name = NULL;
			dbus_message_iter_get_basic(&dict_entry, &setting_name);
			fprintf(stderr, "Setting: %s\n", setting_name);

			if (strcmp(setting_name, "connection") == 0) {
				// Next is a{sv} dictionary for "connection" settings
				dbus_message_iter_next(&dict_entry);
				DBusMessageIter setting_dict;
				dbus_message_iter_recurse(&dict_entry, &setting_dict);

				while (dbus_message_iter_get_arg_type(&setting_dict) == DBUS_TYPE_DICT_ENTRY) {
					DBusMessageIter pair;
					dbus_message_iter_recurse(&setting_dict, &pair);

					const char *key = NULL;
					dbus_message_iter_get_basic(&pair, &key);

					if (strcmp(key, "id") == 0) {
						dbus_message_iter_next(&pair);
						DBusMessageIter variant;
						dbus_message_iter_recurse(&pair, &variant);

						const char *id_value = NULL;
						dbus_message_iter_get_basic(&variant, &id_value);

						fprintf(stderr, "Found id = %s\n", id_value);

						if (strcmp(id_value, ssid) == 0) {
							dbus_message_unref(get_reply);
							dbus_message_unref(reply);
							return strdup(conn_path); // Match found
						}
					}

					dbus_message_iter_next(&setting_dict);
				}
			}

			dbus_message_iter_next(&array_settings_iter);
		}

		dbus_message_unref(get_reply);
		dbus_message_iter_next(&array_iter);
	}

	dbus_message_unref(reply);
	return NULL;
}

// Connect to open network
void connect_to_open_ssid(const char *ssid) {
	DBusConnection *conn = connect_system_bus();
	if (!conn) return;

	char *device = find_wifi_device(conn);
	if (!device) {
		fprintf(stderr, "No Wi-Fi device found\n");
		return;
	}

	char *ap_path = find_ap_by_ssid(conn, device, ssid);
	if (!ap_path) {
		fprintf(stderr, "Access point '%s' not found\n", ssid);
		free(device);
		device = NULL;
		return;
	}

	// Build simple connection settings for open AP
	char *existing_conn = find_existing_connection(conn, ssid);
	fprintf(stderr, "\nexisting_conn: %s\n\n", existing_conn);
	if (existing_conn) {
		// Use ActivateConnection instead of AddAndActivateConnection
		DBusMessage *msg = dbus_message_new_method_call(NM_SERVICE,
														NM_PATH,
														NM_INTERFACE,
														"ActivateConnection");

		DBusMessageIter iter;
		dbus_message_iter_init_append(msg, &iter);

		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &existing_conn);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &device);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &ap_path);

		DBusError err;
		dbus_error_init(&err);
		DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
		dbus_message_unref(msg);
		free(existing_conn);
		existing_conn = NULL;

		if (!reply) {
			fprintf(stderr, "Failed to activate existing connection: %s\n", err.message);
			dbus_error_free(&err);
		}
		else {
			printf("Activated existing saved connection '%s'\n", ssid);
			dbus_message_unref(reply);
		}
		
		dbus_error_free(&err);      // Clears any existing error
		dbus_error_init(&err);      // Reinitialize.s.
	}
	else {
		DBusMessageIter iter;
		DBusMessageIter dict;
		DBusMessage *msg = dbus_message_new_method_call(NM_SERVICE,
														NM_PATH,
														NM_INTERFACE,
														"AddAndActivateConnection");

		dbus_message_iter_init_append(msg, &iter);

		// First arg: settings (a{sa{sv}})
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sa{sv}}", &dict);

		// "connection" setting
		DBusMessageIter con_entry;
		DBusMessageIter con_val;
		dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &con_entry);
		const char *con = "connection";
		dbus_message_iter_append_basic(&con_entry, DBUS_TYPE_STRING, &con);
		dbus_message_iter_open_container(&con_entry, DBUS_TYPE_ARRAY, "{sv}", &con_val);

		DBusMessageIter item;
		DBusMessageIter var;
		const char *id_key = "id", *id_val = ssid;
		dbus_message_iter_open_container(&con_val, DBUS_TYPE_DICT_ENTRY, NULL, &item);
		dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &id_key);
		dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "s", &var);
		dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &id_val);
		dbus_message_iter_close_container(&item, &var);
		dbus_message_iter_close_container(&con_val, &item);

		const char *type_key = "type", *type_val = "802-11-wireless";
		dbus_message_iter_open_container(&con_val, DBUS_TYPE_DICT_ENTRY, NULL, &item);
		dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &type_key);
		dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "s", &var);
		dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &type_val);
		dbus_message_iter_close_container(&item, &var);
		dbus_message_iter_close_container(&con_val, &item);

		dbus_message_iter_close_container(&con_entry, &con_val);
		dbus_message_iter_close_container(&dict, &con_entry);

		// "802-11-wireless" setting
		dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &con_entry);
		const char *wifi = "802-11-wireless";
		dbus_message_iter_append_basic(&con_entry, DBUS_TYPE_STRING, &wifi);
		dbus_message_iter_open_container(&con_entry, DBUS_TYPE_ARRAY, "{sv}", &con_val);

		const char *ssid_key = "ssid";
		dbus_message_iter_open_container(&con_val, DBUS_TYPE_DICT_ENTRY, NULL, &item);
		dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &ssid_key);
		dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "ay", &var);
		DBusMessageIter byte_array;
		dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "y", &byte_array);
		for (int i = 0; ssid[i]; i++) {
			dbus_message_iter_append_basic(&byte_array, DBUS_TYPE_BYTE, &ssid[i]);
		}
		dbus_message_iter_close_container(&var, &byte_array);
		dbus_message_iter_close_container(&item, &var);
		dbus_message_iter_close_container(&con_val, &item);

		dbus_message_iter_close_container(&con_entry, &con_val);
		dbus_message_iter_close_container(&dict, &con_entry);

		dbus_message_iter_close_container(&iter, &dict);

		// Second arg: device
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &device);
		// Third arg: access point
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &ap_path);

		DBusError err;
		dbus_error_init(&err);
		DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
		dbus_message_unref(msg);

		if (!reply) {
			fprintf(stderr, "Failed to connect to network: %s\n", err.message);
			dbus_error_free(&err);
		}
		else {
			printf("Successfully requested connection to '%s'\n", ssid);
			dbus_message_unref(reply);
		}
		
		dbus_error_free(&err);      // Clears any existing error
		dbus_error_init(&err);      // Reinitializes
	}
	free(device);
	free(ap_path);
	device = NULL;
	ap_path = NULL;
	dbus_connection_unref(conn);
}

static void append_setting_string(DBusMessageIter *settings, const char *key, const char *value) {
	DBusMessageIter item;
	DBusMessageIter var;
	dbus_message_iter_open_container(settings, DBUS_TYPE_DICT_ENTRY, NULL, &item);
	dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "s", &var);
	dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &value);
	dbus_message_iter_close_container(&item, &var);
	dbus_message_iter_close_container(settings, &item);
}

void connect_to_secured_ssid(const char *ssid, char *password) {
	DBusConnection *conn = connect_system_bus();
	if (!conn) {
		return;
	}

	char *device = find_wifi_device(conn);
	if (!device) {
		fprintf(stderr, "No Wi-Fi device found\n");
		return;
	}

	char *ap_path = find_ap_by_ssid(conn, device, ssid);
	if (!ap_path) {
		fprintf(stderr, "Access point '%s' not found\n", ssid);
		free(device);
		device = NULL;
		return;
	}

	char *existing_conn = find_existing_connection(conn, ssid);
	if (existing_conn) {
		DBusMessage *msg = dbus_message_new_method_call(NM_SERVICE,
														NM_PATH,
														NM_INTERFACE,
														"ActivateConnection");

		DBusMessageIter iter;
		dbus_message_iter_init_append(msg, &iter);

		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &existing_conn);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &device);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &ap_path);

		DBusError err;
		dbus_error_init(&err);
		DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
		dbus_message_unref(msg);
		free(existing_conn);
		existing_conn = NULL;

		if (!reply) {
			fprintf(stderr, "Failed to activate existing connection: %s\n", err.message);
			dbus_error_free(&err);
		}
		else {
			printf("Activated existing saved connection '%s'\n", ssid);
			dbus_message_unref(reply);
		}
		dbus_error_free(&err);
		dbus_error_init(&err);
	}
	else {
		DBusMessage *msg = dbus_message_new_method_call(NM_SERVICE,
														NM_PATH,
														NM_INTERFACE,
														"AddAndActivateConnection");

		DBusMessageIter iter;
		DBusMessageIter dict;
		dbus_message_iter_init_append(msg, &iter);
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sa{sv}}", &dict);

		// "connection"
		DBusMessageIter entry;
		DBusMessageIter settings;
		const char *section = "connection";
		dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
		dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &section);
		dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &settings);

		const char *id_key = "id", *type_key = "type";
		const char *id_val = ssid, *type_val = "802-11-wireless";

		append_setting_string(&settings, id_key, id_val);
		append_setting_string(&settings, type_key, type_val);

		dbus_message_iter_close_container(&entry, &settings);
		dbus_message_iter_close_container(&dict, &entry);

		// "802-11-wireless"
		section = "802-11-wireless";
		dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
		dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &section);
		dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &settings);

		// ssid (as 'ay')
		const char *ssid_key = "ssid";
		DBusMessageIter item;
		DBusMessageIter var;
		DBusMessageIter byte_array;
		dbus_message_iter_open_container(&settings, DBUS_TYPE_DICT_ENTRY, NULL, &item);
		dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &ssid_key);
		dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "ay", &var);
		dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "y", &byte_array);
		for (int i = 0; ssid[i]; i++) {
			dbus_message_iter_append_basic(&byte_array, DBUS_TYPE_BYTE, &ssid[i]);
		}
		dbus_message_iter_close_container(&var, &byte_array);
		dbus_message_iter_close_container(&item, &var);
		dbus_message_iter_close_container(&settings, &item);

		dbus_message_iter_close_container(&entry, &settings);
		dbus_message_iter_close_container(&dict, &entry);

		// "802-11-wireless-security"
		section = "802-11-wireless-security";
		dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
		dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &section);
		dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &settings);

		const char *key_mgmt = "key-mgmt", *psk_key = "psk";
		const char *wpa = "wpa-psk";
		append_setting_string(&settings, key_mgmt, wpa);
		append_setting_string(&settings, psk_key, password);

		dbus_message_iter_close_container(&entry, &settings);
		dbus_message_iter_close_container(&dict, &entry);
		dbus_message_iter_close_container(&iter, &dict);

		// device and AP
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &device);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &ap_path);

		DBusError err;
		dbus_error_init(&err);
		DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
		dbus_message_unref(msg);

		if (!reply) {
			fprintf(stderr, "Failed to connect to secured network: %s\n", err.message);
			dbus_error_free(&err);
		}
		else {
			printf("Successfully connected to secured network '%s'\n", ssid);
			dbus_message_unref(reply);
		}
		dbus_error_free(&err);
	}

	free(device);
	free(ap_path);
	device = NULL;
	ap_path = NULL;
	dbus_connection_unref(conn);
}
