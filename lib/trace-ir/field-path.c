/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "FIELD-PATH"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/field-classes.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/field-path-internal.h>
#include <babeltrace/trace-ir/field-path.h>
#include <babeltrace/object.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

static
void destroy_field_path(struct bt_object *obj)
{
	struct bt_field_path *field_path = (struct bt_field_path *) obj;

	BT_ASSERT(field_path);
	BT_LIB_LOGD("Destroying field path: %!+P", field_path);
	g_array_free(field_path->indexes, TRUE);
	field_path->indexes = NULL;
	g_free(field_path);
}

BT_HIDDEN
struct bt_field_path *bt_field_path_create(void)
{
	struct bt_field_path *field_path = NULL;

	BT_LOGD_STR("Creating empty field path object.");

	field_path = g_new0(struct bt_field_path, 1);
	if (!field_path) {
		BT_LOGE_STR("Failed to allocate one field path.");
		goto error;
	}

	bt_object_init_shared(&field_path->base, destroy_field_path);
	field_path->indexes = g_array_new(FALSE, FALSE, sizeof(uint64_t));
	if (!field_path->indexes) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LIB_LOGD("Created empty field path object: %!+P", field_path);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(field_path);

end:
	return field_path;
}

enum bt_scope bt_field_path_get_root_scope(struct bt_field_path *field_path)
{
	BT_ASSERT_PRE_NON_NULL(field_path, "Field path");
	return field_path->root;
}

uint64_t bt_field_path_get_index_count(struct bt_field_path *field_path)
{
	BT_ASSERT_PRE_NON_NULL(field_path, "Field path");
	return (uint64_t) field_path->indexes->len;
}

uint64_t bt_field_path_get_index_by_index(struct bt_field_path *field_path,
		uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(field_path, "Field path");
	BT_ASSERT_PRE_VALID_INDEX(index, field_path->indexes->len);
	return bt_field_path_get_index_by_index_inline(field_path, index);
}
