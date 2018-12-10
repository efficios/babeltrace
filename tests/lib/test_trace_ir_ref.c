/*
 * test_trace_ir_ref.c
 *
 * Trace IR Reference Count test
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
#include <babeltrace/babeltrace.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compat/stdlib-internal.h>
#include <babeltrace/assert-internal.h>
#include "common.h"

#define NR_TESTS 37

struct user {
	bt_trace_class *tc;
	bt_stream_class *sc;
	bt_event_class *ec;
	bt_stream *stream;
	bt_event *event;
};

struct writer_user {
	struct bt_ctf_writer *writer;
	struct bt_ctf_trace *tc;
	struct bt_ctf_stream_class *sc;
	struct bt_ctf_event_class *ec;
	struct bt_ctf_stream *stream;
	struct bt_ctf_event *event;
};

const char *writer_user_names[] = {
	"writer",
	"trace",
	"stream class",
	"event class",
	"stream",
	"event",
};

static const size_t WRITER_USER_NR_ELEMENTS =
	sizeof(struct writer_user) / sizeof(void *);

/**
 * Returns a structure containing the following fields:
 *     - uint8_t payload_8;
 *     - uint16_t payload_16;
 *     - uint32_t payload_32;
 */
static bt_field_class *create_integer_struct(bt_trace_class *trace_class)
{
	int ret;
	bt_field_class *structure = NULL;
	bt_field_class *ui8 = NULL, *ui16 = NULL, *ui32 = NULL;

	structure = bt_field_class_structure_create(trace_class);
	BT_ASSERT(structure);
	ui8 = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(ui8);
	bt_field_class_integer_set_field_value_range(ui8, 8);
	ret = bt_field_class_structure_append_member(structure,
		"payload_8", ui8);
	BT_ASSERT(ret == 0);
	ui16 = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(ui16);
	bt_field_class_integer_set_field_value_range(ui16, 16);
	ret = bt_field_class_structure_append_member(structure,
		"payload_16", ui16);
	BT_ASSERT(ret == 0);
	ui32 = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(ui32);
	bt_field_class_integer_set_field_value_range(ui32, 32);
	ret = bt_field_class_structure_append_member(structure,
		"payload_32", ui32);
	BT_ASSERT(ret == 0);
	BT_FIELD_CLASS_PUT_REF_AND_RESET(ui8);
	BT_FIELD_CLASS_PUT_REF_AND_RESET(ui16);
	BT_FIELD_CLASS_PUT_REF_AND_RESET(ui32);
	return structure;
}

static struct bt_ctf_field_type *create_writer_integer_struct(void)
{
	int ret;
	struct bt_ctf_field_type *structure = NULL;
	struct bt_ctf_field_type *ui8 = NULL, *ui16 = NULL, *ui32 = NULL;

	structure = bt_ctf_field_type_structure_create();
	BT_ASSERT(structure);
	ui8 = bt_ctf_field_type_integer_create(8);
	BT_ASSERT(ui8);
	ret = bt_ctf_field_type_structure_add_field(structure, ui8,
		        "payload_8");
	BT_ASSERT(ret == 0);
	ui16 = bt_ctf_field_type_integer_create(16);
	BT_ASSERT(ui16);
	ret = bt_ctf_field_type_structure_add_field(structure, ui16,
		        "payload_16");
	BT_ASSERT(ret == 0);
	ui32 = bt_ctf_field_type_integer_create(32);
	BT_ASSERT(ui32);
	ret = bt_ctf_field_type_structure_add_field(structure, ui32,
		        "payload_32");
	BT_ASSERT(ret == 0);
	BT_OBJECT_PUT_REF_AND_RESET(ui8);
	BT_OBJECT_PUT_REF_AND_RESET(ui16);
	BT_OBJECT_PUT_REF_AND_RESET(ui32);
	return structure;
}

/**
 * A simple event has the following payload:
 *     - uint8_t payload_8;
 *     - uint16_t payload_16;
 *     - uint32_t payload_32;
 */
static bt_event_class *create_simple_event(
		bt_stream_class *sc, const char *name)
{
	int ret;
	bt_event_class *event = NULL;
	bt_field_class *payload = NULL;

	BT_ASSERT(name);
	event = bt_event_class_create(sc);
	BT_ASSERT(event);
	ret = bt_event_class_set_name(event, name);
	BT_ASSERT(ret == 0);
	payload = create_integer_struct(bt_stream_class_borrow_trace_class(sc));
	BT_ASSERT(payload);
	ret = bt_event_class_set_payload_field_class(event, payload);
	BT_ASSERT(ret == 0);
	BT_FIELD_CLASS_PUT_REF_AND_RESET(payload);
	return event;
}

/**
 * A complex event has the following payload:
 *     - uint8_t payload_8;
 *     - uint16_t payload_16;
 *     - uint32_t payload_32;
 *     - struct payload_struct:
 *           - uint8_t payload_8;
 *           - uint16_t payload_16;
 *           - uint32_t payload_32;
 */
static bt_event_class *create_complex_event(bt_stream_class *sc,
		const char *name)
{
	int ret;
	bt_event_class *event = NULL;
	bt_field_class *inner = NULL, *outer = NULL;
	bt_trace_class *trace_class = bt_stream_class_borrow_trace_class(sc);

	BT_ASSERT(name);
	event = bt_event_class_create(sc);
	BT_ASSERT(event);
	ret = bt_event_class_set_name(event, name);
	BT_ASSERT(ret == 0);
	outer = create_integer_struct(trace_class);
	BT_ASSERT(outer);
	inner = create_integer_struct(trace_class);
	BT_ASSERT(inner);
	ret = bt_field_class_structure_append_member(outer,
		"payload_struct", inner);
	BT_ASSERT(ret == 0);
	ret = bt_event_class_set_payload_field_class(event, outer);
	BT_ASSERT(ret == 0);
	BT_FIELD_CLASS_PUT_REF_AND_RESET(inner);
	BT_FIELD_CLASS_PUT_REF_AND_RESET(outer);
	return event;
}

static void set_stream_class_field_classes(
		bt_stream_class *stream_class)
{
	bt_trace_class *trace_class =
		bt_stream_class_borrow_trace_class(stream_class);
	bt_field_class *packet_context_type;
	bt_field_class *event_header_type;
	bt_field_class *fc;
	int ret;

	packet_context_type = bt_field_class_structure_create(trace_class);
	BT_ASSERT(packet_context_type);
	fc = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(fc);
	bt_field_class_integer_set_field_value_range(fc, 32);
	ret = bt_field_class_structure_append_member(packet_context_type,
		"packet_size", fc);
	BT_ASSERT(ret == 0);
	bt_field_class_put_ref(fc);
	fc = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(fc);
	bt_field_class_integer_set_field_value_range(fc, 32);
	ret = bt_field_class_structure_append_member(packet_context_type,
		"content_size", fc);
	BT_ASSERT(ret == 0);
	bt_field_class_put_ref(fc);
	event_header_type = bt_field_class_structure_create(trace_class);
	BT_ASSERT(event_header_type);
	fc = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(fc);
	bt_field_class_integer_set_field_value_range(fc, 32);
	ret = bt_field_class_structure_append_member(event_header_type,
		"id", fc);
	BT_ASSERT(ret == 0);
	bt_field_class_put_ref(fc);
	ret = bt_stream_class_set_packet_context_field_class(
		stream_class, packet_context_type);
	BT_ASSERT(ret == 0);
	ret = bt_stream_class_set_event_header_field_class(
		stream_class, event_header_type);
	BT_ASSERT(ret == 0);
	bt_field_class_put_ref(packet_context_type);
	bt_field_class_put_ref(event_header_type);
}

static void create_sc1(bt_trace_class *trace_class)
{
	int ret;
	bt_event_class *ec1 = NULL, *ec2 = NULL;
	bt_stream_class *sc1 = NULL, *ret_stream = NULL;

	sc1 = bt_stream_class_create(trace_class);
	BT_ASSERT(sc1);
	ret = bt_stream_class_set_name(sc1, "sc1");
	BT_ASSERT(ret == 0);
	set_stream_class_field_classes(sc1);
	ec1 = create_complex_event(sc1, "ec1");
	BT_ASSERT(ec1);
	ec2 = create_simple_event(sc1, "ec2");
	BT_ASSERT(ec2);
	ret_stream = bt_event_class_borrow_stream_class(ec1);
	ok(ret_stream == sc1, "Borrow parent stream SC1 from EC1");
	ret_stream = bt_event_class_borrow_stream_class(ec2);
	ok(ret_stream == sc1, "Borrow parent stream SC1 from EC2");
	BT_EVENT_CLASS_PUT_REF_AND_RESET(ec1);
	BT_EVENT_CLASS_PUT_REF_AND_RESET(ec2);
	BT_STREAM_CLASS_PUT_REF_AND_RESET(sc1);
}

static void create_sc2(bt_trace_class *trace_class)
{
	int ret;
	bt_event_class *ec3 = NULL;
	bt_stream_class *sc2 = NULL, *ret_stream = NULL;

	sc2 = bt_stream_class_create(trace_class);
	BT_ASSERT(sc2);
	ret = bt_stream_class_set_name(sc2, "sc2");
	BT_ASSERT(ret == 0);
	set_stream_class_field_classes(sc2);
	ec3 = create_simple_event(sc2, "ec3");
	ret_stream = bt_event_class_borrow_stream_class(ec3);
	ok(ret_stream == sc2, "Borrow parent stream SC2 from EC3");
	BT_EVENT_CLASS_PUT_REF_AND_RESET(ec3);
	BT_STREAM_CLASS_PUT_REF_AND_RESET(sc2);
}

static void set_trace_packet_header(bt_trace_class *trace_class)
{
	bt_field_class *packet_header_type;
	bt_field_class *fc;
	int ret;

	packet_header_type = bt_field_class_structure_create(trace_class);
	BT_ASSERT(packet_header_type);
	fc = bt_field_class_unsigned_integer_create(trace_class);
	BT_ASSERT(fc);
	bt_field_class_integer_set_field_value_range(fc, 32);
	ret = bt_field_class_structure_append_member(packet_header_type,
		"stream_id", fc);
	BT_ASSERT(ret == 0);
	bt_field_class_put_ref(fc);
	ret = bt_trace_class_set_packet_header_field_class(trace_class,
		packet_header_type);
	BT_ASSERT(ret == 0);

	bt_field_class_put_ref(packet_header_type);
}

static bt_trace_class *create_tc1(bt_self_component_source *self_comp)
{
	bt_trace_class *tc1 = NULL;

	tc1 = bt_trace_class_create(
		bt_self_component_source_as_self_component(self_comp));
	BT_ASSERT(tc1);
	set_trace_packet_header(tc1);
	create_sc1(tc1);
	create_sc2(tc1);
	return tc1;
}

static void init_weak_refs(bt_trace_class *tc,
		bt_trace_class **tc1,
		bt_stream_class **sc1,
		bt_stream_class **sc2,
		bt_event_class **ec1,
		bt_event_class **ec2,
		bt_event_class **ec3)
{
	*tc1 = tc;
	*sc1 = bt_trace_class_borrow_stream_class_by_index(tc, 0);
	*sc2 = bt_trace_class_borrow_stream_class_by_index(tc, 1);
	*ec1 = bt_stream_class_borrow_event_class_by_index(*sc1, 0);
	*ec2 = bt_stream_class_borrow_event_class_by_index(*sc1, 1);
	*ec3 = bt_stream_class_borrow_event_class_by_index(*sc2, 0);
}

static void test_example_scenario(bt_self_component_source *self_comp)
{
	/*
	 * Weak pointers to trace IR objects are to be used very
	 * carefully. This is NOT a good practice and is strongly
	 * discouraged; this is only done to facilitate the validation
	 * of expected reference counts without affecting them by taking
	 * "real" references to the objects.
	 */
	bt_trace_class *tc1 = NULL, *weak_tc1 = NULL;
	bt_stream_class *weak_sc1 = NULL, *weak_sc2 = NULL;
	bt_event_class *weak_ec1 = NULL, *weak_ec2 = NULL,
			*weak_ec3 = NULL;
	struct user user_a = { 0 }, user_b = { 0 }, user_c = { 0 };

	/* The only reference which exists at this point is on TC1. */
	tc1 = create_tc1(self_comp);
	ok(tc1, "Initialize trace");
	BT_ASSERT(tc1);
	init_weak_refs(tc1, &weak_tc1, &weak_sc1, &weak_sc2, &weak_ec1,
			&weak_ec2, &weak_ec3);
	ok(bt_object_get_ref_count((void *) weak_sc1) == 0,
			"Initial SC1 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 0,
			"Initial SC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec1) == 0,
			"Initial EC1 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec2) == 0,
			"Initial EC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 0,
			"Initial EC3 reference count is 0");

	/* User A has ownership of the trace. */
	BT_OBJECT_MOVE_REF(user_a.tc, tc1);
	ok(bt_object_get_ref_count((void *) user_a.tc) == 1,
			"TC1 reference count is 1");

	/* User A acquires a reference to SC2 from TC1. */
	user_a.sc = bt_trace_class_borrow_stream_class_by_index(
			user_a.tc, 1);
	bt_stream_class_get_ref(user_a.sc);
	ok(user_a.sc, "User A acquires SC2 from TC1");
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 1,
			"SC2 reference count is 1");

	/* User A acquires a reference to EC3 from SC2. */
	user_a.ec = bt_stream_class_borrow_event_class_by_index(
			user_a.sc, 0);
	bt_event_class_get_ref(user_a.ec);
	ok(user_a.ec, "User A acquires EC3 from SC2");
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 2,
			"SC2 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 1,
			"EC3 reference count is 1");

	/* User A releases its reference to SC2. */
	diag("User A releases SC2");
	BT_STREAM_CLASS_PUT_REF_AND_RESET(user_a.sc);
	/*
	 * We keep the pointer to SC2 around to validate its reference
	 * count.
	 */
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 1,
			"SC2 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 1,
			"EC3 reference count is 1");

	/* User A releases its reference to TC1. */
	diag("User A releases TC1");
	BT_TRACE_CLASS_PUT_REF_AND_RESET(user_a.tc);
	/*
	 * We keep the pointer to TC1 around to validate its reference
	 * count.
	 */
	ok(bt_object_get_ref_count((void *) weak_tc1) == 1,
			"TC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 1,
			"SC2 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 1,
			"EC3 reference count is 1");

	/* User B acquires a reference to SC1. */
	diag("User B acquires a reference to SC1");
	user_b.sc = weak_sc1;
	bt_stream_class_get_ref(user_b.sc);
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc1) == 1,
			"SC1 reference count is 1");

	/* User C acquires a reference to EC1. */
	diag("User C acquires a reference to EC1");
	user_c.ec = bt_stream_class_borrow_event_class_by_index(
			user_b.sc, 0);
	bt_event_class_get_ref(user_c.ec);
	ok(bt_object_get_ref_count((void *) weak_ec1) == 1,
			"EC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc1) == 2,
			"SC1 reference count is 2");

	/* User A releases its reference on EC3. */
	diag("User A releases its reference on EC3");
	BT_EVENT_CLASS_PUT_REF_AND_RESET(user_a.ec);
	ok(bt_object_get_ref_count((void *) weak_ec3) == 0,
			"EC3 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 0,
			"SC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_tc1) == 1,
			"TC1 reference count is 1");

	/* User B releases its reference on SC1. */
	diag("User B releases its reference on SC1");
	BT_STREAM_CLASS_PUT_REF_AND_RESET(user_b.sc);
	ok(bt_object_get_ref_count((void *) weak_sc1) == 1,
			"SC1 reference count is 1");

	/*
	 * User C is the sole owner of an object and is keeping the whole
	 * trace hierarchy "alive" by holding a reference to EC1.
	 */
	ok(bt_object_get_ref_count((void *) weak_tc1) == 1,
			"TC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc1) == 1,
			"SC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 0,
			"SC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec1) == 1,
			"EC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_ec2) == 0,
			"EC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 0,
			"EC3 reference count is 0");

	/* Reclaim last reference held by User C. */
	BT_EVENT_CLASS_PUT_REF_AND_RESET(user_c.ec);
}

static
bt_self_component_status src_init(
	bt_self_component_source *self_comp,
	const bt_value *params, void *init_method_data)
{
	test_example_scenario(self_comp);
	return BT_SELF_COMPONENT_STATUS_OK;
}

static
bt_self_message_iterator_status src_iter_next(
		bt_self_message_iterator *self_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	return BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
}

static void test_example_scenario_in_graph(void)
{
	bt_component_class_source *comp_cls;
	bt_graph *graph;
	int ret;

	comp_cls = bt_component_class_source_create("src", src_iter_next);
	BT_ASSERT(comp_cls);
	ret = bt_component_class_source_set_init_method(comp_cls, src_init);
	BT_ASSERT(ret == 0);
	graph = bt_graph_create();
	ret = bt_graph_add_source_component(graph, comp_cls, "src-comp",
		NULL, NULL);
	BT_ASSERT(ret == 0);
	bt_graph_put_ref(graph);
	bt_component_class_source_put_ref(comp_cls);
}

static void create_writer_user_full(struct writer_user *user)
{
	gchar *trace_path;
	struct bt_ctf_field_type *ft;
	struct bt_ctf_field *field;
	struct bt_ctf_clock *clock;
	int ret;

	trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);
	if (!bt_mkdtemp(trace_path)) {
		perror("# perror");
	}

	user->writer = bt_ctf_writer_create(trace_path);
	BT_ASSERT(user->writer);
	ret = bt_ctf_writer_set_byte_order(user->writer,
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN);
	BT_ASSERT(ret == 0);
	user->tc = bt_ctf_writer_get_trace(user->writer);
	BT_ASSERT(user->tc);
	user->sc = bt_ctf_stream_class_create("sc");
	BT_ASSERT(user->sc);
	clock = bt_ctf_clock_create("the_clock");
	BT_ASSERT(clock);
	ret = bt_ctf_writer_add_clock(user->writer, clock);
	BT_ASSERT(!ret);
	ret = bt_ctf_stream_class_set_clock(user->sc, clock);
	BT_ASSERT(!ret);
	BT_OBJECT_PUT_REF_AND_RESET(clock);
	user->stream = bt_ctf_writer_create_stream(user->writer, user->sc);
	BT_ASSERT(user->stream);
	user->ec = bt_ctf_event_class_create("ec");
	BT_ASSERT(user->ec);
	ft = create_writer_integer_struct();
	BT_ASSERT(ft);
	ret = bt_ctf_event_class_set_payload_field_type(user->ec, ft);
	BT_OBJECT_PUT_REF_AND_RESET(ft);
	BT_ASSERT(!ret);
	ret = bt_ctf_stream_class_add_event_class(user->sc, user->ec);
	BT_ASSERT(!ret);
	user->event = bt_ctf_event_create(user->ec);
	BT_ASSERT(user->event);
	field = bt_ctf_event_get_payload(user->event, "payload_8");
	BT_ASSERT(field);
	ret = bt_ctf_field_integer_unsigned_set_value(field, 10);
	BT_ASSERT(!ret);
	BT_OBJECT_PUT_REF_AND_RESET(field);
	field = bt_ctf_event_get_payload(user->event, "payload_16");
	BT_ASSERT(field);
	ret = bt_ctf_field_integer_unsigned_set_value(field, 20);
	BT_ASSERT(!ret);
	BT_OBJECT_PUT_REF_AND_RESET(field);
	field = bt_ctf_event_get_payload(user->event, "payload_32");
	BT_ASSERT(field);
	ret = bt_ctf_field_integer_unsigned_set_value(field, 30);
	BT_ASSERT(!ret);
	BT_OBJECT_PUT_REF_AND_RESET(field);
	ret = bt_ctf_stream_append_event(user->stream, user->event);
	BT_ASSERT(!ret);
	recursive_rmdir(trace_path);
	g_free(trace_path);
}

static void test_put_order_swap(size_t *array, size_t a, size_t b)
{
	size_t temp = array[a];

	array[a] = array[b];
	array[b] = temp;
}

static void test_put_order_put_objects(size_t *array, size_t size)
{
	size_t i;
	struct writer_user user = { 0 };
	void **objects = (void *) &user;

	create_writer_user_full(&user);
	printf("# ");

	for (i = 0; i < size; ++i) {
		void *obj = objects[array[i]];

		printf("%s", writer_user_names[array[i]]);
		BT_OBJECT_PUT_REF_AND_RESET(obj);

		if (i < size - 1) {
			printf(" -> ");
		}
	}

	puts("");
}

static void test_put_order_permute(size_t *array, int k, size_t size)
{
	if (k == 0) {
		test_put_order_put_objects(array, size);
	} else {
		int i;

		for (i = k - 1; i >= 0; i--) {
			size_t next_k = k - 1;

			test_put_order_swap(array, i, next_k);
			test_put_order_permute(array, next_k, size);
			test_put_order_swap(array, i, next_k);
		}
	}
}

static void test_put_order(void)
{
	size_t i;
	size_t array[WRITER_USER_NR_ELEMENTS];

	/* Initialize array of indexes */
	for (i = 0; i < WRITER_USER_NR_ELEMENTS; ++i) {
		array[i] = i;
	}

	test_put_order_permute(array, WRITER_USER_NR_ELEMENTS,
		WRITER_USER_NR_ELEMENTS);
}

/**
 * The objective of this test is to implement and expand upon the scenario
 * described in the reference counting documentation and ensure that any node of
 * the Trace, Stream Class, Event Class, Stream and Event hiearchy keeps all
 * other "alive" and reachable.
 *
 * External tools (e.g. valgrind) should be used to confirm that this
 * known-good test does not leak memory.
 */
int main(int argc, char **argv)
{
	/* Initialize tap harness before any tests */
	plan_tests(NR_TESTS);

	test_example_scenario_in_graph();
	test_put_order();

	return exit_status();
}
