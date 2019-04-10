/*
 * Babeltrace - Debug info utilities
 *
 * Copyright (c) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
bt_bool is_event_common_ctx_dbg_info_compatible(const bt_field_class *in_field_class,
		const char *debug_info_field_class_name)
{
	const bt_field_class_structure_member *member;
	const bt_field_class *ip_fc, *vpid_fc;
	bt_bool match = BT_FALSE;

	/*
	 * If the debug info field is already present in the event common
	 * context. Do not try to add it.
	 */
	member =
		bt_field_class_structure_borrow_member_by_name_const(
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
		match = BT_FALSE;
		goto end;
	}

	if (bt_field_class_integer_get_field_value_range(ip_fc) != 64) {
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

	match = BT_TRUE;

end:
	return match;
}
