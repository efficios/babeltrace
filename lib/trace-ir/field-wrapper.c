/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "FIELD-WRAPPER"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/trace-ir/field-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/object-internal.h>
#include <glib.h>

BT_HIDDEN
struct bt_field_wrapper *bt_field_wrapper_new(void *data)
{
	struct bt_field_wrapper *field_wrapper =
		g_new0(struct bt_field_wrapper, 1);

	BT_LOGD_STR("Creating empty field wrapper object.");

	if (!field_wrapper) {
		BT_LOGE("Failed to allocate one field wrapper.");
		goto end;
	}

	bt_object_init_unique(&field_wrapper->base);
	BT_LOGD("Created empty field wrapper object: addr=%p",
		field_wrapper);

end:
	return field_wrapper;
}

BT_HIDDEN
void bt_field_wrapper_destroy(struct bt_field_wrapper *field_wrapper)
{
	BT_LOGD("Destroying field wrapper: addr=%p", field_wrapper);

	if (field_wrapper->field) {
		BT_LOGD_STR("Destroying field.");
		bt_field_destroy((void *) field_wrapper->field);
		field_wrapper->field = NULL;
	}

	BT_LOGD_STR("Putting stream class.");
	g_free(field_wrapper);
}

BT_HIDDEN
struct bt_field_wrapper *bt_field_wrapper_create(
		struct bt_object_pool *pool, struct bt_field_class *fc)
{
	struct bt_field_wrapper *field_wrapper = NULL;

	BT_ASSERT(pool);
	BT_ASSERT(fc);
	field_wrapper = bt_object_pool_create_object(pool);
	if (!field_wrapper) {
		BT_LIB_LOGE("Cannot allocate one field wrapper from field wrapper pool: "
			"%![pool-]+o", pool);
		goto error;
	}

	if (!field_wrapper->field) {
		field_wrapper->field = (void *) bt_field_create(fc);
		if (!field_wrapper->field) {
			BT_LIB_LOGE("Cannot create field wrapper from field class: "
				"%![fc-]+F", fc);
			goto error;
		}

		BT_LIB_LOGD("Created initial field wrapper object: "
			"wrapper-addr=%p, %![field-]+f", field_wrapper,
			field_wrapper->field);
	}

	goto end;

error:
	if (field_wrapper) {
		bt_field_wrapper_destroy(field_wrapper);
		field_wrapper = NULL;
	}

end:
	return field_wrapper;
}
