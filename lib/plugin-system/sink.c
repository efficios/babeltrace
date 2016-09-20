/*
 * sink.c
 *
 * Babeltrace Sink Component
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

#include <babeltrace/compiler.h>
#include <babeltrace/values.h>
#include <babeltrace/plugin/sink-internal.h>
#include <babeltrace/plugin/component-internal.h>
#include <babeltrace/plugin/notification/notification.h>

BT_HIDDEN
enum bt_component_status bt_component_sink_validate(
		struct bt_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_sink *sink;

	sink = container_of(component, struct bt_component_sink, parent);
	if (!sink->consume) {
		printf_error("Invalid sink component; no notification consumption callback defined.");
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	ret = component_input_validate(&sink->input);
	if (ret) {
		goto end;
	}
end:
	return ret;
}

static
void bt_component_sink_destroy(struct bt_component *component)
{
	struct bt_component_sink *sink = container_of(component,
			struct bt_component_sink, parent);

	component_input_fini(&sink->input);
}

BT_HIDDEN
struct bt_component *bt_component_sink_create(
		struct bt_component_class *class, struct bt_value *params)
{
	struct bt_component_sink *sink = NULL;
	enum bt_component_status ret;

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		goto end;
	}

	sink->parent.class = bt_get(class);
	ret = bt_component_init(&sink->parent, bt_component_sink_destroy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

/*
	ret = bt_component_sink_register_notification_type(&sink->parent,
		BT_NOTIFICATION_TYPE_EVENT);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
*/
	if (component_input_init(&sink->input)) {
		goto error;
	}
end:
	return sink ? &sink->parent : NULL;
error:
	BT_PUT(sink);
	return NULL;
}

static
enum bt_component_status validate_inputs(struct bt_component_sink *sink)
{
	size_t array_size = sink->input.iterators->len;

	if (array_size < sink->input.min_count ||
			array_size > sink->input.max_count) {
		return BT_COMPONENT_STATUS_INVALID;
	}

	return BT_COMPONENT_STATUS_OK;
}

enum bt_component_status bt_component_sink_consume(
		struct bt_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_sink *sink = NULL;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	if (!sink->input.validated) {
		ret = validate_inputs(sink);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
		sink->input.validated = true;
	}

	assert(sink->consume);
	ret = sink->consume(component);
end:
	return ret;
}
/*
static
enum bt_component_status bt_component_sink_register_notification_type(
		struct bt_component *component, enum bt_notification_type type)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_sink *sink = NULL;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (type <= BT_NOTIFICATION_TYPE_UNKNOWN ||
		type >= BT_NOTIFICATION_TYPE_NR) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}
	sink = container_of(component, struct bt_component_sink, parent);
	if (type == BT_NOTIFICATION_TYPE_ALL) {
		sink->registered_notifications_mask = ~(notification_mask_t) 0;
	} else {
		sink->registered_notifications_mask |=
			(notification_mask_t) 1 << type;
	}
end:
	return ret;
}
*/
enum bt_component_status bt_component_sink_set_consume_cb(
		struct bt_component *component,
		bt_component_sink_consume_cb consume)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (!component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	sink->consume = consume;
end:
	return ret;
}

enum bt_component_status bt_component_sink_set_add_iterator_cb(
		struct bt_component *component,
		bt_component_sink_add_iterator_cb add_iterator)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (!component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	sink->add_iterator = add_iterator;
end:
	return ret;
}

enum bt_component_status bt_component_sink_set_minimum_input_count(
		struct bt_component *component,
		unsigned int minimum)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (!component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	sink->input.min_count = minimum;
end:
	return ret;
}

enum bt_component_status bt_component_sink_set_maximum_input_count(
		struct bt_component *component,
		unsigned int maximum)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (!component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	sink->input.max_count = maximum;
end:
	return ret;
}

enum bt_component_status
bt_component_sink_get_input_count(struct bt_component *component,
		unsigned int *count)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !count) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	*count = (unsigned int) sink->input.iterators->len;
end:
	return ret;
}

enum bt_component_status
bt_component_sink_get_input_iterator(struct bt_component *component,
		unsigned int input, struct bt_notification_iterator **iterator)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !iterator) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	if (input >= (unsigned int) sink->input.iterators->len) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	*iterator = bt_get(g_ptr_array_index(sink->input.iterators, input));
end:
	return ret;
}

enum bt_component_status
bt_component_sink_add_iterator(struct bt_component *component,
		struct bt_notification_iterator *iterator)
{
	struct bt_component_sink *sink;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !iterator) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	if (sink->input.iterators->len == sink->input.max_count) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (sink->add_iterator) {
		ret = sink->add_iterator(component, iterator);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	}

	g_ptr_array_add(sink->input.iterators, bt_get(iterator));
end:
	return ret;
}
