/*
 * component.c
 *
 * Babeltrace Plugin Component
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/plugin/component-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler.h>

static void bt_component_destroy(struct bt_ctf_ref *ref);

const char *bt_component_get_name(struct bt_component *component)
{
	const char *ret = NULL;

	if (!component) {
		goto end;
	}

	ret = component->name->str;
end:
	return ret;
}

enum bt_component_status bt_component_set_name(struct bt_component *component,
		const char *name)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !name || name[0] == '\0') {
		ret = BT_COMPONENT_STATUS_INVAL;
		goto end;
	}

	g_string_assign(component->name, name);
end:
	return ret;
}

enum bt_component_type bt_component_get_type(struct bt_component *component)
{
	enum bt_component_type type = BT_COMPONENT_TYPE_UNKNOWN;

	if (!component) {
		goto end;
	}

	type = component->type;
end:
	return type;
}

enum bt_component_status bt_component_set_error_stream(
		struct bt_component *component, FILE *stream)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVAL;
		goto end;
	}

	component->error_stream = stream;
end:
	return ret;
}

void bt_component_get(struct bt_component *component)
{
	if (!component) {
		return;
	}

	bt_ctf_ref_get(&component->ref_count);
}

void bt_component_put(struct bt_component *component)
{
	if (!component) {
		return;
	}

	bt_ctf_ref_put(&component->ref_count, bt_component_destroy);
}

BT_HIDDEN
enum bt_component_status bt_component_init(struct bt_component *component,
		const char *name, void *user_data,
		bt_component_destroy_cb user_destroy_func,
		enum bt_component_type component_type,
		bt_component_destroy_cb component_destroy)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !name || name[0] == '\0' ||
		!user_destroy_func || !user_data || !component_destroy) {
		ret = BT_COMPONENT_STATUS_INVAL;
		goto end;
	}

	bt_ctf_ref_init(&component->ref_count);
	component->type = component_type;
	component->user_data = user_data;
	component->user_data_destroy = user_destroy_func;
	component->destroy = component_destroy;

	component->name = g_string_new(name);
	if (!component->name) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}
end:
	return ret;
}

void *bt_component_get_private_data(struct bt_component *component)
{
	void *ret = NULL;

	if (!component) {
		goto end;
	}

	ret = component->user_data;
end:
	return ret;
}

enum bt_component_status
bt_component_set_private_data(struct bt_component *component,
		void *data)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVAL;
		goto end;
	}

	component->user_data = data;
end:
	return ret;
}

static
void bt_component_destroy(struct bt_ctf_ref *ref)
{
	struct bt_component *component = NULL;

	if (!ref) {
		return;
	}

	component = container_of(ref, struct bt_component, ref_count);

	/**
	 * User data is destroyed first, followed by the concrete component
	 * instance.
	 */
	assert(!component->user_data || component->user_data_destroy);
	component->user_data_destroy(component->user_data);

	g_string_free(component->name, TRUE);

	assert(component->destroy);
	component->destroy(component);
}
