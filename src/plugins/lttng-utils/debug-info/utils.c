/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace - Debug info utilities
 */

#include <stdbool.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#include "debug-info.h"
#include "utils.h"

BT_HIDDEN
const char *get_filename_from_path(const char *path)
{
	size_t i = strlen(path);

	if (i == 0) {
		goto end;
	}

	if (path[i - 1] == '/') {
		/*
		 * Path ends with a trailing slash, no filename to return.
		 * Return the original path.
		 */
		goto end;
	}

	while (i-- > 0) {
		if (path[i] == '/') {
			path = &path[i + 1];
			goto end;
		}
	}
end:
	return path;
}

BT_HIDDEN
bool is_event_common_ctx_dbg_info_compatible(const bt_field_class *in_field_class,
		const char *debug_info_field_class_name)
{
	const bt_field_class_structure_member *member;
	const bt_field_class *ip_fc, *vpid_fc;
	bool match = false;

	/*
	 * If the debug info field is already present in the event common
	 * context. Do not try to add it.
	 */
	member = bt_field_class_structure_borrow_member_by_name_const(
		in_field_class, debug_info_field_class_name);
	if (member) {
		goto end;
	}

	/*
	 * Verify that the ip and vpid field are present and of the right field
	 * class.
	 */
	member = bt_field_class_structure_borrow_member_by_name_const(
		in_field_class, IP_FIELD_NAME);
	if (!member) {
		goto end;
	}

	ip_fc = bt_field_class_structure_member_borrow_field_class_const(
		member);
	if (bt_field_class_get_type(ip_fc) !=
			BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER) {
		goto end;
	}

	member = bt_field_class_structure_borrow_member_by_name_const(
		in_field_class, VPID_FIELD_NAME);
	if (!member) {
		goto end;
	}

	vpid_fc = bt_field_class_structure_member_borrow_field_class_const(
		member);
	if (bt_field_class_get_type(vpid_fc) !=
			BT_FIELD_CLASS_TYPE_SIGNED_INTEGER) {
		goto end;
	}

	if (bt_field_class_integer_get_field_value_range(vpid_fc) != 32) {
		goto end;
	}

	match = true;

end:
	return match;
}
