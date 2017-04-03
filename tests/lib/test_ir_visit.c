/*
 * test_ir_visit.c
 *
 * CTF IR visitor interface test
 *
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "tap/tap.h"
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/visitor.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NR_TESTS 13

struct visitor_state {
	int i;
};

struct expected_result {
	const char *object_name;
	enum bt_ctf_object_type object_type;
};

struct expected_result expected_results[] = {
	{ NULL, BT_CTF_OBJECT_TYPE_TRACE },
	{ "sc1", BT_CTF_OBJECT_TYPE_STREAM_CLASS },
	{ "ec1", BT_CTF_OBJECT_TYPE_EVENT_CLASS },
	{ "sc2", BT_CTF_OBJECT_TYPE_STREAM_CLASS },
	{ "ec2", BT_CTF_OBJECT_TYPE_EVENT_CLASS },
	{ "ec3", BT_CTF_OBJECT_TYPE_EVENT_CLASS },
};

const char *object_type_str(enum bt_ctf_object_type type)
{
	switch (type) {
	case BT_CTF_OBJECT_TYPE_TRACE:
		return "trace";
	case BT_CTF_OBJECT_TYPE_STREAM_CLASS:
		return "stream class";
	case BT_CTF_OBJECT_TYPE_STREAM:
		return "stream";
	case BT_CTF_OBJECT_TYPE_EVENT_CLASS:
		return "event class";
	case BT_CTF_OBJECT_TYPE_EVENT:
		return "event";
	default:
		return "unknown";
	}
}

struct bt_ctf_event_class *init_event_class(const char *name)
{
	int ret;
	struct bt_ctf_event_class *ec = bt_ctf_event_class_create(name);
	struct bt_ctf_field_type *int_field =
			bt_ctf_field_type_integer_create(8);

	if (!ec || !int_field) {
		goto error;
	}

	ret = bt_ctf_event_class_add_field(ec, int_field, "an_int_field");
	if (ret) {
		goto error;
	}

	BT_PUT(int_field);
	return ec;
error:
	BT_PUT(ec);
	BT_PUT(int_field);
	return NULL;
}

struct bt_ctf_trace *init_trace(void)
{
	int ret;
	struct bt_ctf_trace *trace = bt_ctf_trace_create();
	struct bt_ctf_stream_class *sc1 = bt_ctf_stream_class_create("sc1");
	struct bt_ctf_stream_class *sc2 = bt_ctf_stream_class_create("sc2");
	struct bt_ctf_event_class *ec1 = init_event_class("ec1");
	struct bt_ctf_event_class *ec2 = init_event_class("ec2");
	struct bt_ctf_event_class *ec3 = init_event_class("ec3");

	if (!trace || !sc1 || !sc2 || !ec1 || !ec2 || !ec3) {
		goto end;
	}

	ret = bt_ctf_trace_set_native_byte_order(trace,
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN);
	assert(ret == 0);
	ret = bt_ctf_stream_class_add_event_class(sc1, ec1);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_stream_class_add_event_class(sc2, ec2);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_stream_class_add_event_class(sc2, ec3);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_trace_add_stream_class(trace, sc1);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_trace_add_stream_class(trace, sc2);
	if (ret) {
		goto error;
	}
end:
	BT_PUT(sc1);
	BT_PUT(sc2);
	BT_PUT(ec1);
	BT_PUT(ec2);
	BT_PUT(ec3);
	return trace;
error:
	BT_PUT(trace);
	goto end;
}

int visitor(struct bt_ctf_object *object, void *data)
{
	int ret = 0;
	bool names_match;
	const char *object_name;
	struct visitor_state *state = data;
	struct expected_result *expected = &expected_results[state->i++];

	switch (bt_ctf_object_get_type(object)) {
	case BT_CTF_OBJECT_TYPE_TRACE:
		object_name = NULL;
		names_match = expected->object_name == NULL;
		break;
	case BT_CTF_OBJECT_TYPE_STREAM_CLASS:
		object_name = bt_ctf_stream_class_get_name(
				bt_ctf_object_get_object(object));
		if (!object_name) {
			ret = -1;
			goto end;
		}

		names_match = !strcmp(object_name, expected->object_name);
		break;
	case BT_CTF_OBJECT_TYPE_EVENT_CLASS:
		object_name = bt_ctf_event_class_get_name(
				bt_ctf_object_get_object(object));
		if (!object_name) {
			ret = -1;
			goto end;
		}

		names_match = !strcmp(object_name, expected->object_name);
		break;
	default:
		diag("Encountered an unexpected type while visiting trace");
		ret = -1;
		goto end;
	}

	ok(expected->object_type == bt_ctf_object_get_type(object),
			"Encoutered object type %s, expected %s",
			object_type_str(expected->object_type),
			object_type_str(bt_ctf_object_get_type(object)));
	ok(names_match, "Element name is %s, expected %s",
			object_name ? : "NULL",
			expected->object_name ? : "NULL");
end:
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	struct bt_ctf_trace *trace;
	struct visitor_state state = { 0 };

	plan_tests(NR_TESTS);

	/*
	 * Initialize a reference trace which we'll walk using the
	 * bt_ctf_*_visit() interface.
	 */
	trace = init_trace();
	if (!trace) {
		diag("Failed to initialize reference trace, aborting.");
		exit(-1);
	}

	ret = bt_ctf_trace_visit(trace, visitor, &state);
	ok(!ret, "bt_ctf_trace_visit returned success");

	BT_PUT(trace);
	return exit_status();
}

