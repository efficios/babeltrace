/*
 * test_ctf_writer.c
 *
 * CTF Writer test
 *
 * Copyright 2013 - 2017 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/values.h>
#include <glib.h>
#include <unistd.h>
#include <babeltrace/compat/stdlib-internal.h>
#include <stdio.h>
#include <babeltrace/compat/limits-internal.h>
#include <babeltrace/compat/stdio-internal.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include "tap/tap.h"
#include <math.h>
#include <float.h>
#include "common.h"

#define METADATA_LINE_SIZE 512
#define SEQUENCE_TEST_LENGTH 10
#define ARRAY_TEST_LENGTH 5
#define PACKET_RESIZE_TEST_DEF_LENGTH 100000

#define DEFAULT_CLOCK_FREQ 1000000000
#define DEFAULT_CLOCK_PRECISION 1
#define DEFAULT_CLOCK_OFFSET 0
#define DEFAULT_CLOCK_OFFSET_S 0
#define DEFAULT_CLOCK_IS_ABSOLUTE 0
#define DEFAULT_CLOCK_TIME 0
#define DEFAULT_CLOCK_VALUE 0

#define NR_TESTS 346

struct bt_utsname {
	char sysname[BABELTRACE_HOST_NAME_MAX];
	char nodename[BABELTRACE_HOST_NAME_MAX];
	char release[BABELTRACE_HOST_NAME_MAX];
	char version[BABELTRACE_HOST_NAME_MAX];
	char machine[BABELTRACE_HOST_NAME_MAX];
};

static int64_t current_time = 42;
static unsigned int packet_resize_test_length = PACKET_RESIZE_TEST_DEF_LENGTH;

/* Return 1 if uuids match, zero if different. */
static
int uuid_match(const unsigned char *uuid_a, const unsigned char *uuid_b)
{
	int ret = 0;
	int i;

	if (!uuid_a || !uuid_b) {
		goto end;
	}

	for (i = 0; i < 16; i++) {
		if (uuid_a[i] != uuid_b[i]) {
			goto end;
		}
	}

	ret = 1;
end:
	return ret;
}

static
void validate_trace(char *parser_path, char *trace_path)
{
	int ret = 0;
	gint exit_status;
	char *argv[] = {parser_path, trace_path, "-o", "dummy", NULL};

	if (!parser_path || !trace_path) {
		ret = -1;
		goto result;
	}

	if (!g_spawn_sync(NULL,
			argv,
			NULL,
			G_SPAWN_STDOUT_TO_DEV_NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			&exit_status,
			NULL)) {
		diag("Failed to spawn babeltrace.");
		ret = -1;
		goto result;
	}

	/* Replace by g_spawn_check_exit_status when we require glib >= 2.34 */
#ifdef G_OS_UNIX
	ret = WIFEXITED(exit_status) ? WEXITSTATUS(exit_status) : -1;
#else
	ret = exit_status;
#endif

	if (ret != 0) {
		diag("Babeltrace returned an error.");
		goto result;
	}

result:
	ok(ret == 0, "Babeltrace could read the resulting trace");
}

static
void append_simple_event(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
	/* Create and add a simple event class */
	struct bt_ctf_event_class *simple_event_class =
		bt_ctf_event_class_create("Simple Event");
	struct bt_ctf_field_type *uint_12_type =
		bt_ctf_field_type_integer_create(12);
	struct bt_ctf_field_type *int_64_type =
		bt_ctf_field_type_integer_create(64);
	struct bt_ctf_field_type *float_type =
		bt_ctf_field_type_floating_point_create();
	struct bt_ctf_field_type *enum_type;
	struct bt_ctf_field_type *enum_type_unsigned =
		bt_ctf_field_type_enumeration_create(uint_12_type);
	struct bt_ctf_field_type *event_context_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *event_payload_type = NULL;
	struct bt_ctf_field_type *returned_type;
	struct bt_ctf_event *simple_event;
	struct bt_ctf_field *integer_field;
	struct bt_ctf_field *float_field;
	struct bt_ctf_field *enum_field;
	struct bt_ctf_field *enum_field_unsigned;
	struct bt_ctf_field *enum_container_field;
	const char *mapping_name_test = "truie";
	const double double_test_value = 3.1415;
	struct bt_ctf_field *enum_container_field_unsigned;
	const char *mapping_name_negative_test = "negative_value";
	const char *ret_char;
	double ret_double;
	int64_t ret_range_start_int64_t, ret_range_end_int64_t;
	uint64_t ret_range_start_uint64_t, ret_range_end_uint64_t;
	struct bt_ctf_event_class *ret_event_class;
	struct bt_ctf_field *packet_context;
	struct bt_ctf_field *packet_context_field;
	struct bt_ctf_field *stream_event_context;
	struct bt_ctf_field *stream_event_context_field;
	struct bt_ctf_field *event_context;
	struct bt_ctf_field *event_context_field;
	struct bt_ctf_field_type *ep_integer_field_type = NULL;
	struct bt_ctf_field_type *ep_enum_field_type = NULL;
	struct bt_ctf_field_type *ep_enum_field_unsigned_type = NULL;
	struct bt_ctf_field_type_enumeration_mapping_iterator *iter = NULL;
	int ret;

	ok(uint_12_type, "Create an unsigned integer type");

	ok(!bt_ctf_field_type_integer_set_signed(int_64_type, 1),
		"Set signed 64 bit integer signedness to true");
	ok(int_64_type, "Create a signed integer type");
	enum_type = bt_ctf_field_type_enumeration_create(int_64_type);

	returned_type = bt_ctf_field_type_enumeration_get_container_field_type(enum_type);
	ok(returned_type == int_64_type, "bt_ctf_field_type_enumeration_get_container_field_type returns the right type");
	ok(!bt_ctf_field_type_enumeration_create(enum_type),
		"bt_ctf_field_enumeration_type_create rejects non-integer container field types");
	bt_put(returned_type);

	bt_ctf_field_type_set_alignment(float_type, 32);
	ok(bt_ctf_field_type_get_alignment(float_type) == 32,
		"bt_ctf_field_type_get_alignment returns a correct value");

	ok(bt_ctf_field_type_floating_point_set_exponent_digits(float_type, 11) == 0,
		"Set a floating point type's exponent digit count");
	ok(bt_ctf_field_type_floating_point_set_mantissa_digits(float_type, 53) == 0,
		"Set a floating point type's mantissa digit count");

	ok(bt_ctf_field_type_floating_point_get_exponent_digits(float_type) == 11,
		"bt_ctf_field_type_floating_point_get_exponent_digits returns the correct value");
	ok(bt_ctf_field_type_floating_point_get_mantissa_digits(float_type) == 53,
		"bt_ctf_field_type_floating_point_get_mantissa_digits returns the correct value");

	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		mapping_name_negative_test, -12345, 0) == 0,
		"bt_ctf_field_type_enumeration_add_mapping accepts negative enumeration mappings");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		"escaping; \"test\"", 1, 1) == 0,
		"bt_ctf_field_type_enumeration_add_mapping accepts enumeration mapping strings containing quotes");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		"\tanother \'escaping\'\n test\"", 2, 4) == 0,
		"bt_ctf_field_type_enumeration_add_mapping accepts enumeration mapping strings containing special characters");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		"event clock int float", 5, 22) == 0,
		"Accept enumeration mapping strings containing reserved keywords");
	bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		42, 42);
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		43, 51) == 0, "bt_ctf_field_type_enumeration_add_mapping accepts duplicate mapping names");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, "something",
		-500, -400) == 0, "bt_ctf_field_type_enumeration_add_mapping accepts overlapping enum entries");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		-54, -55), "bt_ctf_field_type_enumeration_add_mapping rejects mapping where end < start");
	bt_ctf_field_type_enumeration_add_mapping(enum_type, "another entry", -42000, -13000);

	ok(bt_ctf_event_class_add_field(simple_event_class, enum_type,
		"enum_field") == 0, "Add signed enumeration field to event");

	ok(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(enum_type, 0, NULL,
		&ret_range_start_int64_t, &ret_range_end_int64_t) == 0,
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index handles a NULL string correctly");
	ok(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(enum_type, 0, &ret_char,
		NULL, &ret_range_end_int64_t) == 0,
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index handles a NULL start correctly");
	ok(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(enum_type, 0, &ret_char,
		&ret_range_start_int64_t, NULL) == 0,
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index handles a NULL end correctly");
	/* Assumes entries are sorted by range_start values. */
	ok(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(enum_type, 6, &ret_char,
		&ret_range_start_int64_t, &ret_range_end_int64_t) == 0,
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index returns a value");
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index returns a correct mapping name");
	ok(ret_range_start_int64_t == 42,
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index returns a correct mapping start");
	ok(ret_range_end_int64_t == 42,
		"bt_ctf_field_type_enumeration_signed_get_mapping_by_index returns a correct mapping end");

	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
		"escaping; \"test\"", 0, 0) == 0,
		"bt_ctf_field_type_enumeration_unsigned_add_mapping accepts enumeration mapping strings containing quotes");
	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
		"\tanother \'escaping\'\n test\"", 1, 4) == 0,
		"bt_ctf_field_type_enumeration_unsigned_add_mapping accepts enumeration mapping strings containing special characters");
	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
		"event clock int float", 5, 22) == 0,
		"bt_ctf_field_type_enumeration_unsigned_add_mapping accepts enumeration mapping strings containing reserved keywords");
	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned, mapping_name_test,
		42, 42) == 0, "bt_ctf_field_type_enumeration_unsigned_add_mapping accepts single-value ranges");
	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned, mapping_name_test,
		43, 51) == 0, "bt_ctf_field_type_enumeration_unsigned_add_mapping accepts duplicate mapping names");
	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned, "something",
		7, 8) == 0, "bt_ctf_field_type_enumeration_unsigned_add_mapping accepts overlapping enum entries");
	ok(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned, mapping_name_test,
		55, 54), "bt_ctf_field_type_enumeration_unsigned_add_mapping rejects mapping where end < start");
	ok(bt_ctf_event_class_add_field(simple_event_class, enum_type_unsigned,
		"enum_field_unsigned") == 0, "Add unsigned enumeration field to event");

	ok(bt_ctf_field_type_enumeration_get_mapping_count(enum_type_unsigned) == 6,
		"bt_ctf_field_type_enumeration_get_mapping_count returns the correct value");

	ok(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(enum_type_unsigned, 0, NULL,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) == 0,
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index handles a NULL string correctly");
	ok(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(enum_type_unsigned, 0, &ret_char,
		NULL, &ret_range_end_uint64_t) == 0,
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index handles a NULL start correctly");
	ok(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(enum_type_unsigned, 0, &ret_char,
		&ret_range_start_uint64_t, NULL) == 0,
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index handles a NULL end correctly");
	ok(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(enum_type_unsigned, 4, &ret_char,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) == 0,
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index returns a value");
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index returns a correct mapping name");
	ok(ret_range_start_uint64_t == 42,
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index returns a correct mapping start");
	ok(ret_range_end_uint64_t == 42,
		"bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index returns a correct mapping end");

	bt_ctf_event_class_add_field(simple_event_class, uint_12_type,
		"integer_field");
	bt_ctf_event_class_add_field(simple_event_class, float_type,
		"float_field");

	assert(!bt_ctf_event_class_set_id(simple_event_class, 13));

	/* Set an event context type which will contain a single integer. */
	ok(!bt_ctf_field_type_structure_add_field(event_context_type, uint_12_type,
		"event_specific_context"),
		"Add event specific context field");

	ok(bt_ctf_event_class_set_context_field_type(NULL, event_context_type) < 0,
		"bt_ctf_event_class_set_context_field_type handles a NULL event class correctly");
	ok(!bt_ctf_event_class_set_context_field_type(simple_event_class, event_context_type),
		"Set an event class' context type successfully");
	returned_type = bt_ctf_event_class_get_context_field_type(simple_event_class);
	ok(returned_type == event_context_type,
		"bt_ctf_event_class_get_context_field_type returns the appropriate type");
	bt_put(returned_type);

	ok(!bt_ctf_stream_class_add_event_class(stream_class, simple_event_class),
		"Adding simple event class to stream class");

	/*
	 * bt_ctf_stream_class_add_event_class() copies the field types
	 * of simple_event_class, so we retrieve the new ones to create
	 * the appropriate fields.
	 */
	BT_PUT(event_context_type);
	BT_PUT(event_payload_type);
	event_payload_type = bt_ctf_event_class_get_payload_field_type(
		simple_event_class);
	assert(event_payload_type);
	event_context_type = bt_ctf_event_class_get_context_field_type(
		simple_event_class);
	assert(event_context_type);
	ep_integer_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
			event_payload_type, "integer_field");
	assert(ep_integer_field_type);
	ep_enum_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
			event_payload_type, "enum_field");
	assert(ep_enum_field_type);
	ep_enum_field_unsigned_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
			event_payload_type, "enum_field_unsigned");
	assert(ep_enum_field_unsigned_type);

	ok(bt_ctf_stream_class_get_event_class_count(stream_class) == 1,
		"bt_ctf_stream_class_get_event_class_count returns a correct number of event classes");
	ret_event_class = bt_ctf_stream_class_get_event_class_by_index(stream_class, 0);
	ok(ret_event_class == simple_event_class,
		"bt_ctf_stream_class_get_event_class returns the correct event class");
	bt_put(ret_event_class);
	ok(!bt_ctf_stream_class_get_event_class_by_id(stream_class, 2),
		"bt_ctf_stream_class_get_event_class_by_id returns NULL when the requested ID doesn't exist");
	ret_event_class =
		bt_ctf_stream_class_get_event_class_by_id(stream_class, 13);
	ok(ret_event_class == simple_event_class,
		"bt_ctf_stream_class_get_event_class_by_id returns a correct event class");
	bt_put(ret_event_class);

	simple_event = bt_ctf_event_create(simple_event_class);
	ok(simple_event,
		"Instantiate an event containing a single integer field");

	integer_field = bt_ctf_field_create(ep_integer_field_type);
	bt_ctf_field_integer_unsigned_set_value(integer_field, 42);
	ok(bt_ctf_event_set_payload(simple_event, "integer_field",
		integer_field) == 0, "Use bt_ctf_event_set_payload to set a manually allocated field");

	float_field = bt_ctf_event_get_payload(simple_event, "float_field");
	bt_ctf_field_floating_point_set_value(float_field, double_test_value);
	ok(!bt_ctf_field_floating_point_get_value(float_field, &ret_double),
		"bt_ctf_field_floating_point_get_value returns a double value");
	ok(fabs(ret_double - double_test_value) <= DBL_EPSILON,
		"bt_ctf_field_floating_point_get_value returns a correct value");

	enum_field = bt_ctf_field_create(ep_enum_field_type);
	assert(enum_field);

	enum_container_field = bt_ctf_field_enumeration_get_container(enum_field);
	ok(bt_ctf_field_integer_signed_set_value(
		enum_container_field, -42) == 0,
		"Set signed enumeration container value");
	ret = bt_ctf_event_set_payload(simple_event, "enum_field", enum_field);
	assert(!ret);
	BT_PUT(iter);

	enum_field_unsigned = bt_ctf_field_create(ep_enum_field_unsigned_type);
	assert(enum_field_unsigned);
	enum_container_field_unsigned = bt_ctf_field_enumeration_get_container(
		enum_field_unsigned);
	ok(bt_ctf_field_integer_unsigned_set_value(
		enum_container_field_unsigned, 42) == 0,
		"Set unsigned enumeration container value");
	ret = bt_ctf_event_set_payload(simple_event, "enum_field_unsigned",
		enum_field_unsigned);
	assert(!ret);

	ok(bt_ctf_clock_set_time(clock, current_time) == 0, "Set clock time");

	/* Populate stream event context */
	stream_event_context =
		bt_ctf_event_get_stream_event_context(simple_event);
	assert(stream_event_context);
	stream_event_context_field = bt_ctf_field_structure_get_field_by_name(
		stream_event_context, "common_event_context");
	bt_ctf_field_integer_unsigned_set_value(stream_event_context_field, 42);

	/* Populate the event's context */
	event_context = bt_ctf_event_get_context(simple_event);
	ok(event_context,
		"bt_ctf_event_get_context returns a field");
	returned_type = bt_ctf_field_get_type(event_context);
	ok(returned_type == event_context_type,
		"bt_ctf_event_get_context returns a field of the appropriate type");
	event_context_field = bt_ctf_field_structure_get_field_by_name(event_context,
		"event_specific_context");
	ok(!bt_ctf_field_integer_unsigned_set_value(event_context_field, 1234),
		"Successfully set an event context's value");
	ok(!bt_ctf_event_set_context(simple_event, event_context),
		"Set an event context successfully");

	ok(bt_ctf_stream_append_event(stream, simple_event) == 0,
		"Append simple event to trace stream");

	packet_context = bt_ctf_stream_get_packet_context(stream);
	ok(packet_context,
		"bt_ctf_stream_get_packet_context returns a packet context");

	packet_context_field = bt_ctf_field_structure_get_field_by_name(packet_context,
		"packet_size");
	ok(packet_context_field,
		"Packet context contains the default packet_size field.");
	bt_put(packet_context_field);
	packet_context_field = bt_ctf_field_structure_get_field_by_name(packet_context,
		"custom_packet_context_field");
	ok(bt_ctf_field_integer_unsigned_set_value(packet_context_field, 8) == 0,
		"Custom packet context field value successfully set.");

	ok(bt_ctf_stream_set_packet_context(stream, packet_context) == 0,
		"Successfully set a stream's packet context");

	ok(bt_ctf_stream_flush(stream) == 0,
		"Flush trace stream with one event");

	bt_put(simple_event_class);
	bt_put(simple_event);
	bt_put(uint_12_type);
	bt_put(int_64_type);
	bt_put(float_type);
	bt_put(enum_type);
	bt_put(enum_type_unsigned);
	bt_put(returned_type);
	bt_put(event_context_type);
	bt_put(integer_field);
	bt_put(float_field);
	bt_put(enum_field);
	bt_put(enum_field_unsigned);
	bt_put(enum_container_field);
	bt_put(enum_container_field_unsigned);
	bt_put(packet_context);
	bt_put(packet_context_field);
	bt_put(stream_event_context);
	bt_put(stream_event_context_field);
	bt_put(event_context);
	bt_put(event_context_field);
	bt_put(event_payload_type);
	bt_put(ep_integer_field_type);
	bt_put(ep_enum_field_type);
	bt_put(ep_enum_field_unsigned_type);
	bt_put(iter);
}

static
void append_complex_event(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
	int i;
	struct event_class_attrs_counts ;
	const char *complex_test_event_string = "Complex Test Event";
	const char *test_string_1 = "Test ";
	const char *test_string_2 = "string ";
	const char *test_string_3 = "abcdefghi";
	const char *test_string_4 = "abcd\0efg\0hi";
	const char *test_string_cat = "Test string abcdeefg";
	struct bt_ctf_field_type *uint_35_type =
		bt_ctf_field_type_integer_create(35);
	struct bt_ctf_field_type *int_16_type =
		bt_ctf_field_type_integer_create(16);
	struct bt_ctf_field_type *uint_3_type =
		bt_ctf_field_type_integer_create(3);
	struct bt_ctf_field_type *enum_variant_type =
		bt_ctf_field_type_enumeration_create(uint_3_type);
	struct bt_ctf_field_type *variant_type =
		bt_ctf_field_type_variant_create(enum_variant_type,
			"variant_selector");
	struct bt_ctf_field_type *string_type =
		bt_ctf_field_type_string_create();
	struct bt_ctf_field_type *sequence_type;
	struct bt_ctf_field_type *array_type;
	struct bt_ctf_field_type *inner_structure_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *complex_structure_type =
		bt_ctf_field_type_structure_create();
	struct bt_ctf_field_type *ret_field_type;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_event *event;
	struct bt_ctf_field *uint_35_field, *int_16_field, *a_string_field,
		*inner_structure_field, *complex_structure_field,
		*a_sequence_field, *enum_variant_field, *enum_container_field,
		*variant_field, *an_array_field, *stream_event_ctx_field,
		*stream_event_ctx_int_field;
	uint64_t ret_unsigned_int;
	int64_t ret_signed_int;
	const char *ret_string;
	struct bt_ctf_stream_class *ret_stream_class;
	struct bt_ctf_event_class *ret_event_class;
	struct bt_ctf_field *packet_context, *packet_context_field;

	ok(bt_ctf_field_type_set_alignment(int_16_type, 0),
		"bt_ctf_field_type_set_alignment handles 0-alignment correctly");
	ok(bt_ctf_field_type_set_alignment(int_16_type, 3),
		"bt_ctf_field_type_set_alignment handles wrong alignment correctly (3)");
	ok(bt_ctf_field_type_set_alignment(int_16_type, 24),
		"bt_ctf_field_type_set_alignment handles wrong alignment correctly (24)");
	ok(!bt_ctf_field_type_set_alignment(int_16_type, 4),
		"bt_ctf_field_type_set_alignment handles correct alignment correctly (4)");
	ok(!bt_ctf_field_type_set_alignment(int_16_type, 32),
		"Set alignment of signed 16 bit integer to 32");
	ok(!bt_ctf_field_type_integer_set_signed(int_16_type, 1),
		"Set integer signedness to true");
	ok(!bt_ctf_field_type_integer_set_base(uint_35_type,
		BT_CTF_INTEGER_BASE_HEXADECIMAL),
		"Set signed 16 bit integer base to hexadecimal");

	array_type = bt_ctf_field_type_array_create(int_16_type, ARRAY_TEST_LENGTH);
	sequence_type = bt_ctf_field_type_sequence_create(int_16_type,
		"seq_len");

	ret_field_type = bt_ctf_field_type_array_get_element_field_type(
		array_type);
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_array_get_element_field_type returns the correct type");
	bt_put(ret_field_type);

	ok(bt_ctf_field_type_array_get_length(array_type) == ARRAY_TEST_LENGTH,
		"bt_ctf_field_type_array_get_length returns the correct length");

	ok(bt_ctf_field_type_structure_add_field(inner_structure_type,
		inner_structure_type, "yes"), "Cannot add self to structure");
	ok(!bt_ctf_field_type_structure_add_field(inner_structure_type,
		uint_35_type, "seq_len"), "Add seq_len field to inner structure");
	ok(!bt_ctf_field_type_structure_add_field(inner_structure_type,
		sequence_type, "a_sequence"), "Add a_sequence field to inner structure");
	ok(!bt_ctf_field_type_structure_add_field(inner_structure_type,
		array_type, "an_array"), "Add an_array field to inner structure");

	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"UINT3_TYPE", 0, 0);
	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"INT16_TYPE", 1, 1);
	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"UINT35_TYPE", 2, 7);

	ok(bt_ctf_field_type_variant_add_field(variant_type, uint_3_type,
		"An unknown entry"), "Reject a variant field based on an unknown tag value");
	ok(bt_ctf_field_type_variant_add_field(variant_type, uint_3_type,
		"UINT3_TYPE") == 0, "Add a field to a variant");
	ok(!bt_ctf_field_type_variant_add_field(variant_type, int_16_type,
		"INT16_TYPE"), "Add INT16_TYPE field to variant");
	ok(!bt_ctf_field_type_variant_add_field(variant_type, uint_35_type,
		"UINT35_TYPE"), "Add UINT35_TYPE field to variant");

	ret_field_type = bt_ctf_field_type_variant_get_tag_field_type(variant_type);
	ok(ret_field_type == enum_variant_type,
		"bt_ctf_field_type_variant_get_tag_field_type returns a correct tag type");
	bt_put(ret_field_type);

	ret_string = bt_ctf_field_type_variant_get_tag_name(variant_type);
	ok(ret_string ? !strcmp(ret_string, "variant_selector") : 0,
		"bt_ctf_field_type_variant_get_tag_name returns the correct variant tag name");
	ret_field_type = bt_ctf_field_type_variant_get_field_type_by_name(
		variant_type, "INT16_TYPE");
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_variant_get_field_type_by_name returns a correct field type");
	bt_put(ret_field_type);

	ok(bt_ctf_field_type_variant_get_field_count(variant_type) == 3,
		"bt_ctf_field_type_variant_get_field_count returns the correct count");

	ok(bt_ctf_field_type_variant_get_field_by_index(variant_type, NULL, &ret_field_type, 0) == 0,
		"bt_ctf_field_type_variant_get_field handles a NULL field name correctly");
	bt_put(ret_field_type);
	ok(bt_ctf_field_type_variant_get_field_by_index(variant_type, &ret_string, NULL, 0) == 0,
		"bt_ctf_field_type_variant_get_field handles a NULL field type correctly");
	ok(bt_ctf_field_type_variant_get_field_by_index(variant_type, &ret_string, &ret_field_type, 1) == 0,
		"bt_ctf_field_type_variant_get_field returns a field");
	ok(!strcmp("INT16_TYPE", ret_string),
		"bt_ctf_field_type_variant_get_field returns a correct field name");
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_variant_get_field returns a correct field type");
	bt_put(ret_field_type);

	ok(!bt_ctf_field_type_structure_add_field(complex_structure_type,
		enum_variant_type, "variant_selector"),
		"Add variant_selector field to complex structure");
	ok(!bt_ctf_field_type_structure_add_field(complex_structure_type,
		string_type, "string"), "Add `string` field to complex structure");
	ok(!bt_ctf_field_type_structure_add_field(complex_structure_type,
		variant_type, "variant_value"),
		"Add variant_value field to complex structure");
	ok(!bt_ctf_field_type_structure_add_field(complex_structure_type,
		inner_structure_type, "inner_structure"),
		"Add inner_structure field to complex structure");

	event_class = bt_ctf_event_class_create(complex_test_event_string);
	ok(event_class, "Create an event class");
	ok(bt_ctf_event_class_add_field(event_class, uint_35_type, ""),
		"Reject addition of a field with an empty name to an event");
	ok(bt_ctf_event_class_add_field(event_class, NULL, "an_integer"),
		"Reject addition of a field with a NULL type to an event");
	ok(bt_ctf_event_class_add_field(event_class, uint_35_type,
		"int"),
		"Reject addition of a type with an illegal name to an event");
	ok(bt_ctf_event_class_add_field(event_class, uint_35_type,
		"uint_35") == 0,
		"Add field of type unsigned integer to an event");
	ok(bt_ctf_event_class_add_field(event_class, int_16_type,
		"int_16") == 0, "Add field of type signed integer to an event");
	ok(bt_ctf_event_class_add_field(event_class, complex_structure_type,
		"complex_structure") == 0,
		"Add composite structure to an event");

	ret_string = bt_ctf_event_class_get_name(event_class);
	ok(!strcmp(ret_string, complex_test_event_string),
		"bt_ctf_event_class_get_name returns a correct name");
	ok(bt_ctf_event_class_get_id(event_class) < 0,
		"bt_ctf_event_class_get_id returns a negative value when not set");
	ok(bt_ctf_event_class_set_id(NULL, 42) < 0,
		"bt_ctf_event_class_set_id handles NULL correctly");
	ok(bt_ctf_event_class_set_id(event_class, 42) == 0,
		"Set an event class' id");
	ok(bt_ctf_event_class_get_id(event_class) == 42,
		"bt_ctf_event_class_get_id returns the correct value");

	/* Test event class attributes */
	ok(bt_ctf_event_class_get_log_level(event_class) == BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED,
		"event class has the expected initial log level");
	ok(!bt_ctf_event_class_get_emf_uri(event_class),
		"as expected, event class has no initial EMF URI");
	ok(bt_ctf_event_class_set_log_level(NULL, BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO),
		"bt_ctf_event_class_set_log_level handles a NULL event class correctly");
	ok(bt_ctf_event_class_set_log_level(event_class, BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN),
		"bt_ctf_event_class_set_log_level handles an unknown log level correctly");
	ok(!bt_ctf_event_class_set_log_level(event_class, BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO),
		"bt_ctf_event_class_set_log_level succeeds with a valid log level");
	ok(bt_ctf_event_class_get_log_level(event_class) == BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO,
		"bt_ctf_event_class_get_log_level returns the expected log level");
	ok(bt_ctf_event_class_set_emf_uri(NULL, "http://diamon.org/babeltrace/"),
		"bt_ctf_event_class_set_emf_uri handles a NULL event class correctly");
	ok(!bt_ctf_event_class_set_emf_uri(event_class, "http://diamon.org/babeltrace/"),
		"bt_ctf_event_class_set_emf_uri succeeds with a valid EMF URI");
	ok(strcmp(bt_ctf_event_class_get_emf_uri(event_class), "http://diamon.org/babeltrace/") == 0,
		"bt_ctf_event_class_get_emf_uri returns the expected EMF URI");
	ok(!bt_ctf_event_class_set_emf_uri(event_class, NULL),
		"bt_ctf_event_class_set_emf_uri succeeds with NULL (to reset)");
	ok(!bt_ctf_event_class_get_emf_uri(event_class),
		"as expected, event class has no EMF URI after reset");

	/* Add event class to the stream class */
	ok(bt_ctf_stream_class_add_event_class(stream_class, NULL),
		"Reject addition of NULL event class to a stream class");
	ok(bt_ctf_stream_class_add_event_class(stream_class,
		event_class) == 0, "Add an event class to stream class");

	ret_stream_class = bt_ctf_event_class_get_stream_class(event_class);
	ok(ret_stream_class == stream_class,
		"bt_ctf_event_class_get_stream_class returns the correct stream class");
	bt_put(ret_stream_class);

	ok(bt_ctf_event_class_get_field_by_name(event_class, "truie") == NULL,
		"bt_ctf_event_class_get_field_by_name handles an invalid field name correctly");
	ret_field_type = bt_ctf_event_class_get_field_by_name(event_class,
		"complex_structure");
	bt_put(ret_field_type);

	event = bt_ctf_event_create(event_class);
	ok(event, "Instanciate a complex event");

	ret_event_class = bt_ctf_event_get_class(event);
	ok(ret_event_class == event_class,
		"bt_ctf_event_get_class returns the correct event class");
	bt_put(ret_event_class);

	uint_35_field = bt_ctf_event_get_payload(event, "uint_35");
	ok(uint_35_field, "Use bt_ctf_event_get_payload to get a field instance ");
	bt_ctf_field_integer_unsigned_set_value(uint_35_field, 0x0DDF00D);
	ok(bt_ctf_field_integer_unsigned_get_value(uint_35_field,
		&ret_unsigned_int) == 0,
		"bt_ctf_field_integer_unsigned_get_value succeeds after setting a value");
	ok(ret_unsigned_int == 0x0DDF00D,
		"bt_ctf_field_integer_unsigned_get_value returns the correct value");
	bt_put(uint_35_field);

	int_16_field = bt_ctf_event_get_payload(event, "int_16");
	bt_ctf_field_integer_signed_set_value(int_16_field, -12345);
	ok(bt_ctf_field_integer_signed_get_value(int_16_field,
		&ret_signed_int) == 0,
		"bt_ctf_field_integer_signed_get_value succeeds after setting a value");
	ok(ret_signed_int == -12345,
		"bt_ctf_field_integer_signed_get_value returns the correct value");
	bt_put(int_16_field);

	complex_structure_field = bt_ctf_event_get_payload(event,
		"complex_structure");

	inner_structure_field = bt_ctf_field_structure_get_field_by_index(
		complex_structure_field, 3);
	ret_field_type = bt_ctf_field_get_type(inner_structure_field);
	bt_put(inner_structure_field);
	bt_put(ret_field_type);

	inner_structure_field = bt_ctf_field_structure_get_field_by_name(
		complex_structure_field, "inner_structure");
	a_string_field = bt_ctf_field_structure_get_field_by_name(
		complex_structure_field, "string");
	enum_variant_field = bt_ctf_field_structure_get_field_by_name(
		complex_structure_field, "variant_selector");
	variant_field = bt_ctf_field_structure_get_field_by_name(
		complex_structure_field, "variant_value");
	uint_35_field = bt_ctf_field_structure_get_field_by_name(
		inner_structure_field, "seq_len");
	a_sequence_field = bt_ctf_field_structure_get_field_by_name(
		inner_structure_field, "a_sequence");
	an_array_field = bt_ctf_field_structure_get_field_by_name(
		inner_structure_field, "an_array");

	enum_container_field = bt_ctf_field_enumeration_get_container(
		enum_variant_field);
	bt_ctf_field_integer_unsigned_set_value(enum_container_field, 1);
	int_16_field = bt_ctf_field_variant_get_field(variant_field,
		enum_variant_field);
	bt_ctf_field_integer_signed_set_value(int_16_field, -200);
	bt_put(int_16_field);
	bt_ctf_field_string_set_value(a_string_field,
		test_string_1);
	ok(!bt_ctf_field_string_append(a_string_field, test_string_2),
		"bt_ctf_field_string_append succeeds");
	ok(!bt_ctf_field_string_append_len(a_string_field, test_string_3, 5),
		"bt_ctf_field_string_append_len succeeds (append 5 characters)");
	ok(!bt_ctf_field_string_append_len(a_string_field, &test_string_4[5], 3),
		"bt_ctf_field_string_append_len succeeds (append 0 characters)");
	ok(!bt_ctf_field_string_append_len(a_string_field, test_string_3, 0),
		"bt_ctf_field_string_append_len succeeds (append 0 characters)");

	ret_string = bt_ctf_field_string_get_value(a_string_field);
	ok(ret_string, "bt_ctf_field_string_get_value returns a string");
	ok(ret_string ? !strcmp(ret_string, test_string_cat) : 0,
		"bt_ctf_field_string_get_value returns a correct value");
	bt_ctf_field_integer_unsigned_set_value(uint_35_field,
		SEQUENCE_TEST_LENGTH);

	ret_field_type = bt_ctf_field_type_variant_get_field_type_from_tag(
		variant_type, enum_variant_field);
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_variant_get_field_type_from_tag returns the correct field type");

	ok(bt_ctf_field_sequence_set_length(a_sequence_field,
		uint_35_field) == 0, "Set a sequence field's length");

	for (i = 0; i < SEQUENCE_TEST_LENGTH; i++) {
		int_16_field = bt_ctf_field_sequence_get_field(
			a_sequence_field, i);
		bt_ctf_field_integer_signed_set_value(int_16_field, 4 - i);
		bt_put(int_16_field);
	}

	for (i = 0; i < ARRAY_TEST_LENGTH; i++) {
		int_16_field = bt_ctf_field_array_get_field(
			an_array_field, i);
		bt_ctf_field_integer_signed_set_value(int_16_field, i);
		bt_put(int_16_field);
	}

	stream_event_ctx_field = bt_ctf_event_get_stream_event_context(event);
	assert(stream_event_ctx_field);
	stream_event_ctx_int_field = bt_ctf_field_structure_get_field_by_name(
		stream_event_ctx_field, "common_event_context");
	BT_PUT(stream_event_ctx_field);
	bt_ctf_field_integer_unsigned_set_value(stream_event_ctx_int_field, 17);
	BT_PUT(stream_event_ctx_int_field);

	bt_ctf_clock_set_time(clock, ++current_time);
	ok(bt_ctf_stream_append_event(stream, event) == 0,
		"Append a complex event to a stream");

	/*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
	packet_context = bt_ctf_stream_get_packet_context(stream);
	packet_context_field = bt_ctf_field_structure_get_field_by_name(packet_context,
		"custom_packet_context_field");
	bt_ctf_field_integer_unsigned_set_value(packet_context_field, 1);

	ok(bt_ctf_stream_flush(stream) == 0,
		"Flush a stream containing a complex event");

	bt_put(uint_35_field);
	bt_put(a_string_field);
	bt_put(inner_structure_field);
	bt_put(complex_structure_field);
	bt_put(a_sequence_field);
	bt_put(an_array_field);
	bt_put(enum_variant_field);
	bt_put(enum_container_field);
	bt_put(variant_field);
	bt_put(packet_context_field);
	bt_put(packet_context);
	bt_put(uint_35_type);
	bt_put(int_16_type);
	bt_put(string_type);
	bt_put(sequence_type);
	bt_put(array_type);
	bt_put(inner_structure_type);
	bt_put(complex_structure_type);
	bt_put(uint_3_type);
	bt_put(enum_variant_type);
	bt_put(variant_type);
	bt_put(ret_field_type);
	bt_put(event_class);
	bt_put(event);
}

static
void type_field_tests()
{
	struct bt_ctf_field *uint_12;
	struct bt_ctf_field *int_16;
	struct bt_ctf_field *string;
	struct bt_ctf_field_type *composite_structure_type;
	struct bt_ctf_field_type *structure_seq_type;
	struct bt_ctf_field_type *string_type;
	struct bt_ctf_field_type *sequence_type;
	struct bt_ctf_field_type *uint_8_type;
	struct bt_ctf_field_type *int_16_type;
	struct bt_ctf_field_type *uint_12_type =
		bt_ctf_field_type_integer_create(12);
	struct bt_ctf_field_type *enumeration_type;
	struct bt_ctf_field_type *returned_type;
	const char *ret_string;

	ok(uint_12_type, "Create an unsigned integer type");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_BINARY) == 0,
		"Set integer type's base as binary");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_DECIMAL) == 0,
		"Set integer type's base as decimal");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_UNKNOWN),
		"Reject integer type's base set as unknown");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_OCTAL) == 0,
		"Set integer type's base as octal");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_HEXADECIMAL) == 0,
		"Set integer type's base as hexadecimal");
	ok(bt_ctf_field_type_integer_set_base(uint_12_type, 457417),
		"Reject unknown integer base value");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 952835) == 0,
		"Set integer type signedness to signed");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 0) == 0,
		"Set integer type signedness to unsigned");
	ok(bt_ctf_field_type_integer_get_size(uint_12_type) == 12,
		"bt_ctf_field_type_integer_get_size returns a correct value");
	ok(bt_ctf_field_type_integer_get_signed(uint_12_type) == 0,
		"bt_ctf_field_type_integer_get_signed returns a correct value for unsigned types");

	ok(bt_ctf_field_type_set_byte_order(NULL,
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN) < 0,
		"bt_ctf_field_type_set_byte_order handles NULL correctly");
	ok(bt_ctf_field_type_set_byte_order(uint_12_type,
		(enum bt_ctf_byte_order) 42) < 0,
		"bt_ctf_field_type_set_byte_order rejects invalid values");
	ok(bt_ctf_field_type_set_byte_order(uint_12_type,
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN) == 0,
		"Set an integer's byte order to little endian");
	ok(bt_ctf_field_type_set_byte_order(uint_12_type,
		BT_CTF_BYTE_ORDER_BIG_ENDIAN) == 0,
		"Set an integer's byte order to big endian");
	ok(bt_ctf_field_type_get_byte_order(uint_12_type) ==
		BT_CTF_BYTE_ORDER_BIG_ENDIAN,
		"bt_ctf_field_type_get_byte_order returns a correct value");

	ok(bt_ctf_field_type_get_type_id(uint_12_type) ==
		BT_CTF_FIELD_TYPE_ID_INTEGER,
		"bt_ctf_field_type_get_type_id returns a correct value with an integer type");

	ok(bt_ctf_field_type_integer_get_base(uint_12_type) ==
		BT_CTF_INTEGER_BASE_HEXADECIMAL,
		"bt_ctf_field_type_integer_get_base returns a correct value");

	ok(bt_ctf_field_type_integer_set_encoding(NULL,
		BT_CTF_STRING_ENCODING_ASCII) < 0,
		"bt_ctf_field_type_integer_set_encoding handles NULL correctly");
	ok(bt_ctf_field_type_integer_set_encoding(uint_12_type,
		(enum bt_ctf_string_encoding) 123) < 0,
		"bt_ctf_field_type_integer_set_encoding handles invalid encodings correctly");
	ok(bt_ctf_field_type_integer_set_encoding(uint_12_type,
		BT_CTF_STRING_ENCODING_UTF8) == 0,
		"Set integer type encoding to UTF8");
	ok(bt_ctf_field_type_integer_get_encoding(uint_12_type) ==
		BT_CTF_STRING_ENCODING_UTF8,
		"bt_ctf_field_type_integer_get_encoding returns a correct value");

	int_16_type = bt_ctf_field_type_integer_create(16);
	assert(int_16_type);
	ok(!bt_ctf_field_type_integer_set_signed(int_16_type, 1),
		"Set signedness of 16 bit integer to true");
	ok(bt_ctf_field_type_integer_get_signed(int_16_type) == 1,
		"bt_ctf_field_type_integer_get_signed returns a correct value for signed types");
	uint_8_type = bt_ctf_field_type_integer_create(8);
	sequence_type =
		bt_ctf_field_type_sequence_create(int_16_type, "seq_len");
	ok(sequence_type, "Create a sequence of int16_t type");
	ok(bt_ctf_field_type_get_type_id(sequence_type) ==
		BT_CTF_FIELD_TYPE_ID_SEQUENCE,
		"bt_ctf_field_type_get_type_id returns a correct value with a sequence type");

	ret_string = bt_ctf_field_type_sequence_get_length_field_name(
		sequence_type);
	ok(!strcmp(ret_string, "seq_len"),
		"bt_ctf_field_type_sequence_get_length_field_name returns the correct value");
	returned_type = bt_ctf_field_type_sequence_get_element_field_type(
		sequence_type);
	ok(returned_type == int_16_type,
		"bt_ctf_field_type_sequence_get_element_field_type returns the correct type");
	bt_put(returned_type);

	string_type = bt_ctf_field_type_string_create();
	ok(string_type, "Create a string type");
	ok(bt_ctf_field_type_string_set_encoding(string_type,
		BT_CTF_STRING_ENCODING_NONE),
		"Reject invalid \"None\" string encoding");
	ok(bt_ctf_field_type_string_set_encoding(string_type,
		42),
		"Reject invalid string encoding");
	ok(bt_ctf_field_type_string_set_encoding(string_type,
		BT_CTF_STRING_ENCODING_ASCII) == 0,
		"Set string encoding to ASCII");

	ok(bt_ctf_field_type_string_get_encoding(string_type) ==
		BT_CTF_STRING_ENCODING_ASCII,
		"bt_ctf_field_type_string_get_encoding returns the correct value");

	structure_seq_type = bt_ctf_field_type_structure_create();
	ok(bt_ctf_field_type_get_type_id(structure_seq_type) ==
		BT_CTF_FIELD_TYPE_ID_STRUCT,
		"bt_ctf_field_type_get_type_id returns a correct value with a structure type");
	ok(structure_seq_type, "Create a structure type");
	ok(bt_ctf_field_type_structure_add_field(structure_seq_type,
		uint_8_type, "seq_len") == 0,
		"Add a uint8_t type to a structure");
	ok(bt_ctf_field_type_structure_add_field(structure_seq_type,
		sequence_type, "a_sequence") == 0,
		"Add a sequence type to a structure");

	ok(bt_ctf_field_type_structure_get_field_count(structure_seq_type) == 2,
		"bt_ctf_field_type_structure_get_field_count returns a correct value");

	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		NULL, &returned_type, 1) == 0,
		"bt_ctf_field_type_structure_get_field handles a NULL name correctly");
	bt_put(returned_type);
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, NULL, 1) == 0,
		"bt_ctf_field_type_structure_get_field handles a NULL return type correctly");
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, &returned_type, 1) == 0,
		"bt_ctf_field_type_structure_get_field returns a field");
	ok(!strcmp(ret_string, "a_sequence"),
		"bt_ctf_field_type_structure_get_field returns a correct field name");
	ok(returned_type == sequence_type,
		"bt_ctf_field_type_structure_get_field returns a correct field type");
	bt_put(returned_type);

	returned_type = bt_ctf_field_type_structure_get_field_type_by_name(
		structure_seq_type, "a_sequence");
	ok(returned_type == sequence_type,
		"bt_ctf_field_type_structure_get_field_type_by_name returns the correct field type");
	bt_put(returned_type);

	composite_structure_type = bt_ctf_field_type_structure_create();
	ok(bt_ctf_field_type_structure_add_field(composite_structure_type,
		string_type, "a_string") == 0,
		"Add a string type to a structure");
	ok(bt_ctf_field_type_structure_add_field(composite_structure_type,
		structure_seq_type, "inner_structure") == 0,
		"Add a structure type to a structure");

	returned_type = bt_ctf_field_type_structure_get_field_type_by_name(
		structure_seq_type, "a_sequence");
	ok(returned_type == sequence_type,
		"bt_ctf_field_type_structure_get_field_type_by_name returns a correct type");
	bt_put(returned_type);

	int_16 = bt_ctf_field_create(int_16_type);
	ok(int_16, "Instanciate a signed 16-bit integer");
	uint_12 = bt_ctf_field_create(uint_12_type);
	ok(uint_12, "Instanciate an unsigned 12-bit integer");
	returned_type = bt_ctf_field_get_type(int_16);
	ok(returned_type == int_16_type,
		"bt_ctf_field_get_type returns the correct type");

	/* Can't modify types after instanciating them */
	ok(bt_ctf_field_type_integer_set_base(uint_12_type,
		BT_CTF_INTEGER_BASE_DECIMAL),
		"Check an integer type' base can't be modified after instanciation");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 0),
		"Check an integer type's signedness can't be modified after instanciation");

	/* Check overflows are properly tested for */
	ok(bt_ctf_field_integer_signed_set_value(int_16, -32768) == 0,
		"Check -32768 is allowed for a signed 16-bit integer");
	ok(bt_ctf_field_integer_signed_set_value(int_16, 32767) == 0,
		"Check 32767 is allowed for a signed 16-bit integer");
	ok(bt_ctf_field_integer_signed_set_value(int_16, -42) == 0,
		"Check -42 is allowed for a signed 16-bit integer");

	ok(bt_ctf_field_integer_unsigned_set_value(uint_12, 4095) == 0,
		"Check 4095 is allowed for an unsigned 12-bit integer");
	ok(bt_ctf_field_integer_unsigned_set_value(uint_12, 0) == 0,
		"Check 0 is allowed for an unsigned 12-bit integer");

	string = bt_ctf_field_create(string_type);
	ok(string, "Instanciate a string field");
	ok(bt_ctf_field_string_set_value(string, "A value") == 0,
		"Set a string's value");

	enumeration_type = bt_ctf_field_type_enumeration_create(uint_12_type);
	ok(enumeration_type,
		"Create an enumeration type with an unsigned 12-bit integer as container");

	bt_put(string);
	bt_put(uint_12);
	bt_put(int_16);
	bt_put(composite_structure_type);
	bt_put(structure_seq_type);
	bt_put(string_type);
	bt_put(sequence_type);
	bt_put(uint_8_type);
	bt_put(int_16_type);
	bt_put(uint_12_type);
	bt_put(enumeration_type);
	bt_put(returned_type);
}

static
void packet_resize_test(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
	/*
	 * Append enough events to force the underlying packet to be resized.
	 * Also tests that a new event can be declared after a stream has been
	 * instantiated and used/flushed.
	 */
	int ret = 0;
	int i;
	struct bt_ctf_event_class *event_class = bt_ctf_event_class_create(
		"Spammy_Event");
	struct bt_ctf_field_type *integer_type =
		bt_ctf_field_type_integer_create(17);
	struct bt_ctf_field_type *string_type =
		bt_ctf_field_type_string_create();
	struct bt_ctf_event *event = NULL;
	struct bt_ctf_field *ret_field = NULL;
	struct bt_ctf_field_type *ret_field_type = NULL;
	uint64_t ret_uint64;
	int events_appended = 0;
	struct bt_ctf_field *packet_context = NULL,
		*packet_context_field = NULL, *stream_event_context = NULL;
	struct bt_ctf_field_type *ep_field_1_type = NULL;
	struct bt_ctf_field_type *ep_a_string_type = NULL;
	struct bt_ctf_field_type *ep_type = NULL;

	ret |= bt_ctf_event_class_add_field(event_class, integer_type,
		"field_1");
	ret |= bt_ctf_event_class_add_field(event_class, string_type,
		"a_string");
	ret |= bt_ctf_stream_class_add_event_class(stream_class, event_class);
	ok(ret == 0, "Add a new event class to a stream class after writing an event");
	if (ret) {
		goto end;
	}

	/*
	 * bt_ctf_stream_class_add_event_class() copies the field types
	 * of event_class, so we retrieve the new ones to create the
	 * appropriate fields.
	 */
	ep_type = bt_ctf_event_class_get_payload_field_type(event_class);
	assert(ep_type);
	ep_field_1_type = bt_ctf_field_type_structure_get_field_type_by_name(
		ep_type, "field_1");
	assert(ep_field_1_type);
	ep_a_string_type = bt_ctf_field_type_structure_get_field_type_by_name(
		ep_type, "a_string");
	assert(ep_a_string_type);

	event = bt_ctf_event_create(event_class);
	ret_field = bt_ctf_event_get_payload(event, 0);
	ret_field_type = bt_ctf_field_get_type(ret_field);
	bt_put(ret_field_type);
	bt_put(ret_field);
	bt_put(event);

	for (i = 0; i < packet_resize_test_length; i++) {
		event = bt_ctf_event_create(event_class);
		struct bt_ctf_field *integer =
			bt_ctf_field_create(ep_field_1_type);
		struct bt_ctf_field *string =
			bt_ctf_field_create(ep_a_string_type);

		ret |= bt_ctf_clock_set_time(clock, ++current_time);
		ret |= bt_ctf_field_integer_unsigned_set_value(integer, i);
		ret |= bt_ctf_event_set_payload(event, "field_1",
			integer);
		bt_put(integer);
		ret |= bt_ctf_field_string_set_value(string, "This is a test");
		ret |= bt_ctf_event_set_payload(event, "a_string",
			string);
		bt_put(string);

		/* Populate stream event context */
		stream_event_context =
			bt_ctf_event_get_stream_event_context(event);
		integer = bt_ctf_field_structure_get_field_by_name(stream_event_context,
			"common_event_context");
		BT_PUT(stream_event_context);
		ret |= bt_ctf_field_integer_unsigned_set_value(integer,
			i % 42);
		bt_put(integer);

		ret |= bt_ctf_stream_append_event(stream, event);
		bt_put(event);

		if (ret) {
			break;
		}
	}

	events_appended = !!(i == packet_resize_test_length);
	ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 0,
		"bt_ctf_stream_get_discarded_events_count returns a correct number of discarded events when none were discarded");
	bt_ctf_stream_append_discarded_events(stream, 1000);
	ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 1000,
		"bt_ctf_stream_get_discarded_events_count returns a correct number of discarded events when some were discarded");

end:
	ok(events_appended, "Append 100 000 events to a stream");

	/*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
	packet_context = bt_ctf_stream_get_packet_context(stream);
	packet_context_field = bt_ctf_field_structure_get_field_by_name(packet_context,
		"custom_packet_context_field");
	bt_ctf_field_integer_unsigned_set_value(packet_context_field, 2);

	ok(bt_ctf_stream_flush(stream) == 0,
		"Flush a stream that forces a packet resize");
	ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 1000,
		"bt_ctf_stream_get_discarded_events_count returns a correct number of discarded events after a flush");
	bt_put(integer_type);
	bt_put(string_type);
	bt_put(packet_context);
	bt_put(packet_context_field);
	bt_put(stream_event_context);
	bt_put(event_class);
	bt_put(ep_field_1_type);
	bt_put(ep_a_string_type);
	bt_put(ep_type);
}

static
void test_empty_stream(struct bt_ctf_writer *writer)
{
	int ret = 0;
	struct bt_ctf_trace *trace = NULL, *ret_trace = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream *stream = NULL;

	trace = bt_ctf_writer_get_trace(writer);
	if (!trace) {
		diag("Failed to get trace from writer");
		ret = -1;
		goto end;
	}

	stream_class = bt_ctf_stream_class_create("empty_stream");
	if (!stream_class) {
		diag("Failed to create stream class");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_stream_class_set_packet_context_type(stream_class, NULL);
	assert(ret == 0);
	ret = bt_ctf_stream_class_set_event_header_type(stream_class, NULL);
	assert(ret == 0);

	ok(bt_ctf_stream_class_get_trace(stream_class) == NULL,
		"bt_ctf_stream_class_get_trace returns NULL when stream class is orphaned");

	stream = bt_ctf_writer_create_stream(writer, stream_class);
	if (!stream) {
		diag("Failed to create writer stream");
		ret = -1;
		goto end;
	}

	ret_trace = bt_ctf_stream_class_get_trace(stream_class);
	ok(ret_trace == trace,
		"bt_ctf_stream_class_get_trace returns the correct trace after a stream has been created");
end:
	ok(ret == 0,
		"Created a stream class with default attributes and an empty stream");
	bt_put(trace);
	bt_put(ret_trace);
	bt_put(stream);
	bt_put(stream_class);
}

static
void test_custom_event_header_stream(struct bt_ctf_writer *writer,
			struct bt_ctf_clock *clock)
{
	int i, ret;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_field_type *integer_type = NULL,
		*sequence_type = NULL, *event_header_type = NULL;
	struct bt_ctf_field *integer = NULL, *sequence = NULL,
		*event_header = NULL, *packet_header = NULL;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_event *event = NULL;

	stream_class = bt_ctf_stream_class_create("custom_event_header_stream");
	if (!stream_class) {
		fail("Failed to create stream class");
		goto end;
	}

	ret = bt_ctf_stream_class_set_clock(stream_class, clock);
	if (ret) {
		fail("Failed to set stream class clock");
		goto end;
	}

	/*
	 * Customize event header to add an "seq_len" integer member
	 * which will be used as the length of a sequence in an event of this
	 * stream.
	 */
	event_header_type = bt_ctf_stream_class_get_event_header_type(
		stream_class);
	if (!event_header_type) {
		fail("Failed to get event header type");
		goto end;
	}

	integer_type = bt_ctf_field_type_integer_create(13);
	if (!integer_type) {
		fail("Failed to create length integer type");
		goto end;
	}

	ret = bt_ctf_field_type_structure_add_field(event_header_type,
		integer_type, "seq_len");
	if (ret) {
		fail("Failed to add a new field to stream event header");
		goto end;
	}

	event_class = bt_ctf_event_class_create("sequence_event");
	if (!event_class) {
		fail("Failed to create event class");
		goto end;
	}

	/*
	 * This event's payload will contain a sequence which references
	 * stream.event.header.seq_len as its length field.
	 */
	sequence_type = bt_ctf_field_type_sequence_create(integer_type,
		"stream.event.header.seq_len");
	if (!sequence_type) {
		fail("Failed to create a sequence");
		goto end;
	}

	ret = bt_ctf_event_class_add_field(event_class, sequence_type,
		"some_sequence");
	if (ret) {
		fail("Failed to add a sequence to an event class");
		goto end;
	}

	ret = bt_ctf_stream_class_add_event_class(stream_class, event_class);
	if (ret) {
		fail("Failed to add event class to stream class");
		goto end;
	}

	stream = bt_ctf_writer_create_stream(writer, stream_class);
	if (!stream) {
		fail("Failed to create stream")
		goto end;
	}

	/*
	 * We have defined a custom packet header field. We have to populate it
	 * explicitly.
	 */
	packet_header = bt_ctf_stream_get_packet_header(stream);
	if (!packet_header) {
		fail("Failed to get stream packet header");
		goto end;
	}

	integer = bt_ctf_field_structure_get_field_by_name(packet_header,
		"custom_trace_packet_header_field");
	if (!integer) {
		fail("Failed to retrieve custom_trace_packet_header_field");
		goto end;
	}

	ret = bt_ctf_field_integer_unsigned_set_value(integer, 3487);
	if (ret) {
		fail("Failed to set custom_trace_packet_header_field value");
		goto end;
	}
	bt_put(integer);

	event = bt_ctf_event_create(event_class);
	if (!event) {
		fail("Failed to create event");
		goto end;
	}

	event_header = bt_ctf_event_get_header(event);
	if (!event_header) {
		fail("Failed to get event header");
		goto end;
	}

	integer = bt_ctf_field_structure_get_field_by_name(event_header,
		"seq_len");
	if (!integer) {
		fail("Failed to get seq_len field from event header");
		goto end;
	}

	ret = bt_ctf_field_integer_unsigned_set_value(integer, 2);
	if (ret) {
		fail("Failed to set seq_len value in event header");
		goto end;
	}

	/* Populate both sequence integer fields */
	sequence = bt_ctf_event_get_payload(event, "some_sequence");
	if (!sequence) {
		fail("Failed to retrieve sequence from event");
		goto end;
	}

	ret = bt_ctf_field_sequence_set_length(sequence, integer);
	if (ret) {
		fail("Failed to set sequence length");
		goto end;
	}
	bt_put(integer);

	for (i = 0; i < 2; i++) {
		integer = bt_ctf_field_sequence_get_field(sequence, i);
		if (ret) {
			fail("Failed to retrieve sequence element");
			goto end;
		}

		ret = bt_ctf_field_integer_unsigned_set_value(integer, i);
		if (ret) {
			fail("Failed to set sequence element value");
			goto end;
		}

		bt_put(integer);
		integer = NULL;
	}

	ret = bt_ctf_stream_append_event(stream, event);
	if (ret) {
		fail("Failed to append event to stream");
		goto end;
	}

	ret = bt_ctf_stream_flush(stream);
	if (ret) {
		fail("Failed to flush custom_event_header stream");
	}
end:
	bt_put(stream);
	bt_put(stream_class);
	bt_put(event_class);
	bt_put(event);
	bt_put(integer);
	bt_put(sequence);
	bt_put(event_header);
	bt_put(packet_header);
	bt_put(sequence_type);
	bt_put(integer_type);
	bt_put(event_header_type);
}

static
void test_instanciate_event_before_stream(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock)
{
	int ret = 0;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream *stream = NULL,
		*ret_stream = NULL;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_event *event = NULL;
	struct bt_ctf_field_type *integer_type = NULL;
	struct bt_ctf_field *payload_field = NULL;
	struct bt_ctf_field *integer = NULL;

	stream_class = bt_ctf_stream_class_create("event_before_stream_test");
	if (!stream_class) {
		diag("Failed to create stream class");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_stream_class_set_clock(stream_class, clock);
	if (ret) {
		diag("Failed to set stream class clock");
		goto end;
	}

	event_class = bt_ctf_event_class_create("some_event_class_name");
	integer_type = bt_ctf_field_type_integer_create(32);
	if (!integer_type) {
		diag("Failed to create integer field type");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_event_class_add_field(event_class, integer_type,
		"integer_field");
	if (ret) {
		diag("Failed to add field to event class");
		goto end;
	}

	ret = bt_ctf_stream_class_add_event_class(stream_class,
		event_class);
	if (ret) {
		diag("Failed to add event class to stream class");
	}

	event = bt_ctf_event_create(event_class);
	if (!event) {
		diag("Failed to create event");
		ret = -1;
		goto end;
	}

	payload_field = bt_ctf_event_get_payload_field(event);
	if (!payload_field) {
		diag("Failed to get event's payload field");
		ret = -1;
		goto end;
	}

	integer = bt_ctf_field_structure_get_field_by_index(payload_field, 0);
	if (!integer) {
		diag("Failed to get integer field payload from event");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_integer_unsigned_set_value(integer, 1234);
	if (ret) {
		diag("Failed to set integer field value");
		goto end;
	}

	stream = bt_ctf_writer_create_stream(writer, stream_class);
	if (!stream) {
		diag("Failed to create writer stream");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_stream_append_event(stream, event);
	if (ret) {
		diag("Failed to append event to stream");
		goto end;
	}

	ret_stream = bt_ctf_event_get_stream(event);
	ok(ret_stream == stream,
		"bt_ctf_event_get_stream returns an event's stream after it has been appended");
end:
	ok(ret == 0,
		"Create an event before instanciating its associated stream");
	bt_put(stream);
	bt_put(ret_stream);
	bt_put(stream_class);
	bt_put(event_class);
	bt_put(event);
	bt_put(integer_type);
	bt_put(integer);
	bt_put(payload_field);
}

static
void append_existing_event_class(struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_event_class *event_class;

	event_class = bt_ctf_event_class_create("Simple Event");
	assert(event_class);
	ok(bt_ctf_stream_class_add_event_class(stream_class, event_class) == 0,
		"two event classes with the same name may cohabit within the same stream class");
	bt_put(event_class);

	event_class = bt_ctf_event_class_create("different name, ok");
	assert(event_class);
	assert(!bt_ctf_event_class_set_id(event_class, 13));
	ok(bt_ctf_stream_class_add_event_class(stream_class, event_class),
		"two event classes with the same ID cannot cohabit within the same stream class");
	bt_put(event_class);
}

static
void test_clock_utils(void)
{
	int ret;
	struct bt_ctf_clock *clock = NULL;

	clock = bt_ctf_clock_create("water");
	assert(clock);
	ret = bt_ctf_clock_set_offset_s(clock, 1234);
	assert(!ret);
	ret = bt_ctf_clock_set_offset(clock, 1000);
	assert(!ret);
	ret = bt_ctf_clock_set_frequency(clock, 1000000000);
	assert(!ret);
	ret = bt_ctf_clock_set_frequency(clock, 1534);
	assert(!ret);

	BT_PUT(clock);
}

int main(int argc, char **argv)
{
	const char *env_resize_length;
	gchar *trace_path;
	gchar *metadata_path;
	const char *clock_name = "test_clock";
	const char *clock_description = "This is a test clock";
	const char *returned_clock_name;
	const char *returned_clock_description;
	const uint64_t frequency = 1123456789;
	const int64_t offset_s = 13515309;
	const int64_t offset = 1234567;
	int64_t get_offset_s,
		get_offset;
	const uint64_t precision = 10;
	const int is_absolute = 0xFF;
	char *metadata_string;
	struct bt_ctf_writer *writer;
	struct bt_utsname name = {"GNU/Linux", "testhost", "4.4.0-87-generic",
		"#110-Ubuntu SMP Tue Jul 18 12:55:35 UTC 2017", "x86_64"};
	struct bt_ctf_clock *clock, *ret_clock;
	struct bt_ctf_clock_class *ret_clock_class;
	struct bt_ctf_stream_class *stream_class, *ret_stream_class;
	struct bt_ctf_stream *stream1;
	struct bt_ctf_stream *stream;
	const char *ret_string;
	const unsigned char *ret_uuid;
	unsigned char tmp_uuid[16] = { 0 };
	struct bt_ctf_field_type *packet_context_type,
		*packet_context_field_type,
		*packet_header_type,
		*packet_header_field_type,
		*integer_type,
		*stream_event_context_type,
		*ret_field_type,
		*event_header_field_type;
	struct bt_ctf_field *packet_header, *packet_header_field;
	struct bt_ctf_trace *trace;
	int ret;
	int64_t ret_int64_t;
	struct bt_value *obj;

	if (argc < 2) {
		printf("Usage: tests-ctf-writer path_to_babeltrace\n");
		return -1;
	}

	env_resize_length = getenv("PACKET_RESIZE_TEST_LENGTH");
	if (env_resize_length) {
		packet_resize_test_length =
			(unsigned int) atoi(env_resize_length);
	}

	plan_tests(NR_TESTS);

	trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);
	if (!bt_mkdtemp(trace_path)) {
		perror("# perror");
	}

	metadata_path = g_build_filename(trace_path, "metadata", NULL);

	writer = bt_ctf_writer_create(trace_path);
	ok(writer, "bt_ctf_create succeeds in creating trace with path");

	ok(!bt_ctf_writer_get_trace(NULL),
		"bt_ctf_writer_get_trace correctly handles NULL");
	trace = bt_ctf_writer_get_trace(writer);
	ok(bt_ctf_trace_set_native_byte_order(trace, BT_CTF_BYTE_ORDER_NATIVE),
		"Cannot set a trace's byte order to BT_CTF_BYTE_ORDER_NATIVE");
	ok(bt_ctf_trace_set_native_byte_order(trace, BT_CTF_BYTE_ORDER_UNSPECIFIED),
		"Cannot set a trace's byte order to BT_CTF_BYTE_ORDER_UNSPECIFIED");
	ok(trace,
		"bt_ctf_writer_get_trace returns a bt_ctf_trace object");
	ok(bt_ctf_trace_set_native_byte_order(trace, BT_CTF_BYTE_ORDER_BIG_ENDIAN) == 0,
		"Set a trace's byte order to big endian");
	ok(bt_ctf_trace_get_native_byte_order(trace) == BT_CTF_BYTE_ORDER_BIG_ENDIAN,
		"bt_ctf_trace_get_native_byte_order returns a correct endianness");

	/* Add environment context to the trace */
	ok(bt_ctf_writer_add_environment_field(writer, "host", name.nodename) == 0,
		"Add host (%s) environment field to writer instance",
		name.nodename);
	ok(bt_ctf_writer_add_environment_field(NULL, "test_field",
		"test_value"),
		"bt_ctf_writer_add_environment_field error with NULL writer");
	ok(bt_ctf_writer_add_environment_field(writer, NULL,
		"test_value"),
		"bt_ctf_writer_add_environment_field error with NULL field name");
	ok(bt_ctf_writer_add_environment_field(writer, "test_field",
		NULL),
		"bt_ctf_writer_add_environment_field error with NULL field value");

	/* Test bt_ctf_trace_set_environment_field with an integer object */
	obj = bt_value_integer_create_init(23);
	assert(obj);
	ok(bt_ctf_trace_set_environment_field(NULL, "test_env_int_obj", obj),
		"bt_ctf_trace_set_environment_field handles a NULL trace correctly");
	ok(bt_ctf_trace_set_environment_field(trace, NULL, obj),
		"bt_ctf_trace_set_environment_field handles a NULL name correctly");
	ok(bt_ctf_trace_set_environment_field(trace, "test_env_int_obj", NULL),
		"bt_ctf_trace_set_environment_field handles a NULL value correctly");
	ok(!bt_ctf_trace_set_environment_field(trace, "test_env_int_obj", obj),
		"bt_ctf_trace_set_environment_field succeeds in adding an integer object");
	BT_PUT(obj);

	/* Test bt_ctf_trace_set_environment_field with a string object */
	obj = bt_value_string_create_init("the value");
	assert(obj);
	ok(!bt_ctf_trace_set_environment_field(trace, "test_env_str_obj", obj),
		"bt_ctf_trace_set_environment_field succeeds in adding a string object");
	BT_PUT(obj);

	/* Test bt_ctf_trace_set_environment_field_integer */
	ok(bt_ctf_trace_set_environment_field_integer(NULL, "test_env_int",
		-194875),
		"bt_ctf_trace_set_environment_field_integer handles a NULL trace correctly");
	ok(bt_ctf_trace_set_environment_field_integer(trace, NULL, -194875),
		"bt_ctf_trace_set_environment_field_integer handles a NULL name correctly");
	ok(!bt_ctf_trace_set_environment_field_integer(trace, "test_env_int",
		-164973),
		"bt_ctf_trace_set_environment_field_integer succeeds");

	/* Test bt_ctf_trace_set_environment_field_string */
	ok(bt_ctf_trace_set_environment_field_string(NULL, "test_env_str",
		"yeah"),
		"bt_ctf_trace_set_environment_field_string handles a NULL trace correctly");
	ok(bt_ctf_trace_set_environment_field_string(trace, NULL, "yeah"),
		"bt_ctf_trace_set_environment_field_string handles a NULL name correctly");
	ok(bt_ctf_trace_set_environment_field_string(trace, "test_env_str",
		NULL),
		"bt_ctf_trace_set_environment_field_string handles a NULL value correctly");
	ok(!bt_ctf_trace_set_environment_field_string(trace, "test_env_str",
		"oh yeah"),
		"bt_ctf_trace_set_environment_field_string succeeds");

	/* Test bt_ctf_trace_get_environment_field_count */
	ok(bt_ctf_trace_get_environment_field_count(trace) == 5,
		"bt_ctf_trace_get_environment_field_count returns a correct number of environment fields");

	/* Test bt_ctf_trace_get_environment_field_name */
	ret_string = bt_ctf_trace_get_environment_field_name_by_index(trace, 0);
	ok(ret_string && !strcmp(ret_string, "host"),
		"bt_ctf_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_ctf_trace_get_environment_field_name_by_index(trace, 1);
	ok(ret_string && !strcmp(ret_string, "test_env_int_obj"),
		"bt_ctf_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_ctf_trace_get_environment_field_name_by_index(trace, 2);
	ok(ret_string && !strcmp(ret_string, "test_env_str_obj"),
		"bt_ctf_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_ctf_trace_get_environment_field_name_by_index(trace, 3);
	ok(ret_string && !strcmp(ret_string, "test_env_int"),
		"bt_ctf_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_ctf_trace_get_environment_field_name_by_index(trace, 4);
	ok(ret_string && !strcmp(ret_string, "test_env_str"),
		"bt_ctf_trace_get_environment_field_name returns a correct field name");

	/* Test bt_ctf_trace_get_environment_field_value */
	obj = bt_ctf_trace_get_environment_field_value_by_index(trace, 1);
	ret = bt_value_integer_get(obj, &ret_int64_t);
	ok(!ret && ret_int64_t == 23,
		"bt_ctf_trace_get_environment_field_value succeeds in getting an integer value");
	BT_PUT(obj);
	obj = bt_ctf_trace_get_environment_field_value_by_index(trace, 2);
	ret = bt_value_string_get(obj, &ret_string);
	ok(!ret && ret_string && !strcmp(ret_string, "the value"),
		"bt_ctf_trace_get_environment_field_value succeeds in getting a string value");
	BT_PUT(obj);

	/* Test bt_ctf_trace_get_environment_field_value_by_name */
	ok(!bt_ctf_trace_get_environment_field_value_by_name(trace, "oh oh"),
		"bt_ctf_trace_get_environment_field_value_by_name returns NULL or an unknown field name");
	obj = bt_ctf_trace_get_environment_field_value_by_name(trace,
		"test_env_str");
	ret = bt_value_string_get(obj, &ret_string);
	ok(!ret && ret_string && !strcmp(ret_string, "oh yeah"),
		"bt_ctf_trace_get_environment_field_value_by_name succeeds in getting an existing field");
	BT_PUT(obj);

	/* Test environment field replacement */
	ok(!bt_ctf_trace_set_environment_field_integer(trace, "test_env_int",
		654321),
		"bt_ctf_trace_set_environment_field_integer succeeds with an existing name");
	ok(bt_ctf_trace_get_environment_field_count(trace) == 5,
		"bt_ctf_trace_set_environment_field_integer with an existing key does not increase the environment size");
	obj = bt_ctf_trace_get_environment_field_value_by_index(trace, 3);
	ret = bt_value_integer_get(obj, &ret_int64_t);
	ok(!ret && ret_int64_t == 654321,
		"bt_ctf_trace_get_environment_field_value successfully replaces an existing field");
	BT_PUT(obj);

	ok(bt_ctf_writer_add_environment_field(writer, "sysname", name.sysname)
		== 0, "Add sysname (%s) environment field to writer instance",
		name.sysname);
	ok(bt_ctf_writer_add_environment_field(writer, "nodename",
		name.nodename) == 0,
		"Add nodename (%s) environment field to writer instance",
		name.nodename);
	ok(bt_ctf_writer_add_environment_field(writer, "release", name.release)
		== 0, "Add release (%s) environment field to writer instance",
		name.release);
	ok(bt_ctf_writer_add_environment_field(writer, "version", name.version)
		== 0, "Add version (%s) environment field to writer instance",
		name.version);
	ok(bt_ctf_writer_add_environment_field(writer, "machine", name.machine)
		== 0, "Add machine (%s) environment field to writer istance",
		name.machine);

	/* Define a clock and add it to the trace */
	ok(bt_ctf_clock_create("signed") == NULL,
		"Illegal clock name rejected");
	clock = bt_ctf_clock_create(clock_name);
	ok(clock, "Clock created sucessfully");
	returned_clock_name = bt_ctf_clock_get_name(clock);
	ok(returned_clock_name, "bt_ctf_clock_get_name returns a clock name");
	ok(returned_clock_name ? !strcmp(returned_clock_name, clock_name) : 0,
		"Returned clock name is valid");

	returned_clock_description = bt_ctf_clock_get_description(clock);
	ok(!returned_clock_description, "bt_ctf_clock_get_description returns NULL on an unset description");
	ok(bt_ctf_clock_set_description(clock, clock_description) == 0,
		"Clock description set successfully");

	returned_clock_description = bt_ctf_clock_get_description(clock);
	ok(returned_clock_description,
		"bt_ctf_clock_get_description returns a description.");
	ok(returned_clock_description ?
		!strcmp(returned_clock_description, clock_description) : 0,
		"Returned clock description is valid");

	ok(bt_ctf_clock_get_frequency(clock) == DEFAULT_CLOCK_FREQ,
		"bt_ctf_clock_get_frequency returns the correct default frequency");
	ok(bt_ctf_clock_set_frequency(clock, frequency) == 0,
		"Set clock frequency");
	ok(bt_ctf_clock_get_frequency(clock) == frequency,
		"bt_ctf_clock_get_frequency returns the correct frequency once it is set");

	ok(bt_ctf_clock_get_offset_s(clock, &get_offset_s) == 0,
		"bt_ctf_clock_get_offset_s succeeds");
	ok(get_offset_s == DEFAULT_CLOCK_OFFSET_S,
		"bt_ctf_clock_get_offset_s returns the correct default offset (in seconds)");
	ok(bt_ctf_clock_set_offset_s(clock, offset_s) == 0,
		"Set clock offset (seconds)");
	ok(bt_ctf_clock_get_offset_s(clock, &get_offset_s) == 0,
		"bt_ctf_clock_get_offset_s succeeds");
	ok(get_offset_s == offset_s,
		"bt_ctf_clock_get_offset_s returns the correct default offset (in seconds) once it is set");

	ok(bt_ctf_clock_get_offset(clock, &get_offset) == 0,
		"bt_ctf_clock_get_offset succeeds");
	ok(get_offset == DEFAULT_CLOCK_OFFSET,
		"bt_ctf_clock_get_offset returns the correct default offset (in ticks)");
	ok(bt_ctf_clock_set_offset(clock, offset) == 0, "Set clock offset");
	ok(bt_ctf_clock_get_offset(clock, &get_offset) == 0,
		"bt_ctf_clock_get_offset succeeds");
	ok(get_offset == offset,
		"bt_ctf_clock_get_offset returns the correct default offset (in ticks) once it is set");

	ok(bt_ctf_clock_get_precision(clock) == DEFAULT_CLOCK_PRECISION,
		"bt_ctf_clock_get_precision returns the correct default precision");
	ok(bt_ctf_clock_set_precision(clock, precision) == 0,
		"Set clock precision");
	ok(bt_ctf_clock_get_precision(clock) == precision,
		"bt_ctf_clock_get_precision returns the correct precision once it is set");

	ok(bt_ctf_clock_get_is_absolute(clock) == DEFAULT_CLOCK_IS_ABSOLUTE,
		"bt_ctf_clock_get_precision returns the correct default is_absolute attribute");
	ok(bt_ctf_clock_set_is_absolute(clock, is_absolute) == 0,
		"Set clock absolute property");
	ok(bt_ctf_clock_get_is_absolute(clock) == !!is_absolute,
		"bt_ctf_clock_get_precision returns the correct is_absolute attribute once it is set");
	ok(bt_ctf_clock_set_time(clock, current_time) == 0,
		"Set clock time");
	ret_uuid = bt_ctf_clock_get_uuid(clock);
	ok(ret_uuid,
		"bt_ctf_clock_get_uuid returns a UUID");
	if (ret_uuid) {
		memcpy(tmp_uuid, ret_uuid, sizeof(tmp_uuid));
		/* Slightly modify UUID */
		tmp_uuid[sizeof(tmp_uuid) - 1]++;
	}

	ok(bt_ctf_clock_set_uuid(clock, tmp_uuid) == 0,
		"bt_ctf_clock_set_uuid sets a new uuid successfully");
	ret_uuid = bt_ctf_clock_get_uuid(clock);
	ok(ret_uuid,
		"bt_ctf_clock_get_uuid returns a UUID after setting a new one");
	ok(uuid_match(ret_uuid, tmp_uuid),
		"bt_ctf_clock_get_uuid returns the correct UUID after setting a new one");

	/* Define a stream class */
	stream_class = bt_ctf_stream_class_create("test_stream");
	ret_string = bt_ctf_stream_class_get_name(stream_class);
	ok(ret_string && !strcmp(ret_string, "test_stream"),
		"bt_ctf_stream_class_get_name returns a correct stream class name");

	ok(bt_ctf_stream_class_get_clock(stream_class) == NULL,
		"bt_ctf_stream_class_get_clock returns NULL when a clock was not set");
	ok(bt_ctf_stream_class_get_clock(NULL) == NULL,
		"bt_ctf_stream_class_get_clock handles NULL correctly");

	ok(stream_class, "Create stream class");
	ok(bt_ctf_stream_class_set_clock(stream_class, clock) == 0,
		"Set a stream class' clock");
	ret_clock = bt_ctf_stream_class_get_clock(stream_class);
	ok(ret_clock == clock,
		"bt_ctf_stream_class_get_clock returns a correct clock");
	bt_put(ret_clock);

	/* Test the event fields and event types APIs */
	type_field_tests();

	ok(bt_ctf_stream_class_get_id(stream_class) < 0,
		"bt_ctf_stream_class_get_id returns an error when no id is set");
	ok(bt_ctf_stream_class_set_id(NULL, 123) < 0,
		"bt_ctf_stream_class_set_id handles NULL correctly");
	ok(bt_ctf_stream_class_set_id(stream_class, 123) == 0,
		"Set an stream class' id");
	ok(bt_ctf_stream_class_get_id(stream_class) == 123,
		"bt_ctf_stream_class_get_id returns the correct value");

	/* Validate default event header fields */
	ret_field_type = bt_ctf_stream_class_get_event_header_type(
		stream_class);
	ok(ret_field_type,
		"bt_ctf_stream_class_get_event_header_type returns an event header type");
	ok(bt_ctf_field_type_get_type_id(ret_field_type) == BT_CTF_FIELD_TYPE_ID_STRUCT,
		"Default event header type is a structure");
	event_header_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
		ret_field_type, "id");
	ok(event_header_field_type,
		"Default event header type contains an \"id\" field");
	ok(bt_ctf_field_type_get_type_id(
		event_header_field_type) == BT_CTF_FIELD_TYPE_ID_INTEGER,
		"Default event header \"id\" field is an integer");
	bt_put(event_header_field_type);
	event_header_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
		ret_field_type, "timestamp");
	ok(event_header_field_type,
		"Default event header type contains a \"timestamp\" field");
	ok(bt_ctf_field_type_get_type_id(
		event_header_field_type) == BT_CTF_FIELD_TYPE_ID_INTEGER,
		"Default event header \"timestamp\" field is an integer");
	bt_put(event_header_field_type);
	bt_put(ret_field_type);

	/* Add a custom trace packet header field */
	packet_header_type = bt_ctf_trace_get_packet_header_field_type(trace);
	ok(packet_header_type,
		"bt_ctf_trace_get_packet_header_field_type returns a packet header");
	ok(bt_ctf_field_type_get_type_id(packet_header_type) == BT_CTF_FIELD_TYPE_ID_STRUCT,
		"bt_ctf_trace_get_packet_header_field_type returns a packet header of type struct");
	ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		packet_header_type, "magic");
	ok(ret_field_type, "Default packet header type contains a \"magic\" field");
	bt_put(ret_field_type);
	ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		packet_header_type, "uuid");
	ok(ret_field_type, "Default packet header type contains a \"uuid\" field");
	bt_put(ret_field_type);
	ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		packet_header_type, "stream_id");
	ok(ret_field_type, "Default packet header type contains a \"stream_id\" field");
	bt_put(ret_field_type);

	packet_header_field_type = bt_ctf_field_type_integer_create(22);
	ok(!bt_ctf_field_type_structure_add_field(packet_header_type,
		packet_header_field_type, "custom_trace_packet_header_field"),
		"Added a custom trace packet header field successfully");

	ok(bt_ctf_trace_set_packet_header_field_type(NULL, packet_header_type) < 0,
		"bt_ctf_trace_set_packet_header_field_type handles a NULL trace correctly");
	ok(!bt_ctf_trace_set_packet_header_field_type(trace, packet_header_type),
		"Set a trace packet_header_type successfully");

	/* Add a custom field to the stream class' packet context */
	packet_context_type = bt_ctf_stream_class_get_packet_context_type(stream_class);
	ok(packet_context_type,
		"bt_ctf_stream_class_get_packet_context_type returns a packet context type.");
	ok(bt_ctf_field_type_get_type_id(packet_context_type) == BT_CTF_FIELD_TYPE_ID_STRUCT,
		"Packet context is a structure");

	ok(bt_ctf_stream_class_set_packet_context_type(NULL, packet_context_type),
		"bt_ctf_stream_class_set_packet_context_type handles a NULL stream class correctly");

	integer_type = bt_ctf_field_type_integer_create(32);

	ok(bt_ctf_stream_class_set_packet_context_type(stream_class,
		integer_type) < 0,
		"bt_ctf_stream_class_set_packet_context_type rejects a packet context that is not a structure");

	/* Create a "uint5_t" equivalent custom packet context field */
	packet_context_field_type = bt_ctf_field_type_integer_create(5);

	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		packet_context_field_type, "custom_packet_context_field");
	ok(ret == 0, "Packet context field added successfully");

	/* Define a stream event context containing a my_integer field. */
	stream_event_context_type = bt_ctf_field_type_structure_create();
	bt_ctf_field_type_structure_add_field(stream_event_context_type,
		integer_type, "common_event_context");

	ok(bt_ctf_stream_class_set_event_context_type(NULL,
		stream_event_context_type) < 0,
		"bt_ctf_stream_class_set_event_context_type handles a NULL stream_class correctly");
	ok(bt_ctf_stream_class_set_event_context_type(stream_class,
		integer_type) < 0,
		"bt_ctf_stream_class_set_event_context_type validates that the event context os a structure");

	ok(bt_ctf_stream_class_set_event_context_type(
		stream_class, stream_event_context_type) == 0,
		"Set a new stream event context type");

	ret_field_type = bt_ctf_stream_class_get_event_context_type(
		stream_class);
	ok(ret_field_type == stream_event_context_type,
		"bt_ctf_stream_class_get_event_context_type returns the correct field type.");
	bt_put(ret_field_type);


	/* Instantiate a stream and append events */
	ret = bt_ctf_writer_add_clock(writer, clock);
	assert(ret == 0);

	ok(bt_ctf_trace_get_stream_count(trace) == 0,
		"bt_ctf_trace_get_stream_count() succeeds and returns the correct value (0)");
	stream1 = bt_ctf_writer_create_stream(writer, stream_class);
	ok(stream1, "Instanciate a stream class from writer");
	ok(bt_ctf_trace_get_stream_count(trace) == 1,
		"bt_ctf_trace_get_stream_count() succeeds and returns the correct value (1)");
	stream = bt_ctf_trace_get_stream_by_index(trace, 0);
	ok(stream == stream1,
		"bt_ctf_trace_get_stream_by_index() succeeds and returns the correct value");
	BT_PUT(stream);

	/*
	 * Creating a stream through a writer adds the given stream
	 * class to the writer's trace, thus registering the stream
	 * class's clock to the trace.
	 */
	ok(bt_ctf_trace_get_clock_class_count(trace) == 1,
		"bt_ctf_trace_get_clock_class_count returns the correct number of clocks");
	ret_clock_class = bt_ctf_trace_get_clock_class_by_index(trace, 0);
	ok(strcmp(bt_ctf_clock_class_get_name(ret_clock_class),
		bt_ctf_clock_get_name(clock)) == 0,
		"bt_ctf_trace_get_clock_class returns the right clock instance");
	bt_put(ret_clock_class);
	ret_clock_class = bt_ctf_trace_get_clock_class_by_name(trace, clock_name);
	ok(strcmp(bt_ctf_clock_class_get_name(ret_clock_class),
		bt_ctf_clock_get_name(clock)) == 0,
		"bt_ctf_trace_get_clock_class returns the right clock instance");
	bt_put(ret_clock_class);
	ok(!bt_ctf_trace_get_clock_class_by_name(trace, "random"),
		"bt_ctf_trace_get_clock_by_name fails when the requested clock doesn't exist");

	ret_stream_class = bt_ctf_stream_get_class(stream1);
	ok(ret_stream_class,
		"bt_ctf_stream_get_class returns a stream class");
	ok(ret_stream_class == stream_class,
		"Returned stream class is of the correct type");

	/*
	 * Packet header, packet context, event header, and stream
	 * event context types were copied for the resolving
	 * process
	 */
	BT_PUT(packet_header_type);
	BT_PUT(packet_context_type);
	BT_PUT(stream_event_context_type);
	packet_header_type = bt_ctf_trace_get_packet_header_field_type(trace);
	assert(packet_header_type);
	packet_context_type =
		bt_ctf_stream_class_get_packet_context_type(stream_class);
	assert(packet_context_type);
	stream_event_context_type =
		bt_ctf_stream_class_get_event_context_type(stream_class);
	assert(stream_event_context_type);

	/*
	 * Try to modify the packet context type after a stream has been
	 * created.
	 */
	ret = bt_ctf_field_type_structure_add_field(packet_header_type,
		packet_header_field_type, "should_fail");
	ok(ret < 0,
		"Trace packet header type can't be modified once a stream has been instanciated");

	/*
	 * Try to modify the packet context type after a stream has been
	 * created.
	 */
	ret = bt_ctf_field_type_structure_add_field(packet_context_type,
		packet_context_field_type, "should_fail");
	ok(ret < 0,
		"Packet context type can't be modified once a stream has been instanciated");

	/*
	 * Try to modify the stream event context type after a stream has been
	 * created.
	 */
	ret = bt_ctf_field_type_structure_add_field(stream_event_context_type,
		integer_type, "should_fail");
	ok(ret < 0,
		"Stream event context type can't be modified once a stream has been instanciated");

	/* Should fail after instanciating a stream (frozen) */
	ok(bt_ctf_stream_class_set_clock(stream_class, clock),
		"Changes to a stream class that was already instantiated fail");

	/* Populate the custom packet header field only once for all tests */
	ok(bt_ctf_stream_get_packet_header(NULL) == NULL,
		"bt_ctf_stream_get_packet_header handles NULL correctly");
	packet_header = bt_ctf_stream_get_packet_header(stream1);
	ok(packet_header,
		"bt_ctf_stream_get_packet_header returns a packet header");
	ret_field_type = bt_ctf_field_get_type(packet_header);
	ok(ret_field_type == packet_header_type,
		"Stream returns a packet header of the appropriate type");
	bt_put(ret_field_type);
	packet_header_field = bt_ctf_field_structure_get_field_by_name(packet_header,
		"custom_trace_packet_header_field");
	ok(packet_header_field,
		"Packet header structure contains a custom field with the appropriate name");
	ret_field_type = bt_ctf_field_get_type(packet_header_field);
	ok(!bt_ctf_field_integer_unsigned_set_value(packet_header_field,
		54321), "Set custom packet header value successfully");
	ok(bt_ctf_stream_set_packet_header(stream1, NULL) < 0,
		"bt_ctf_stream_set_packet_header handles a NULL packet header correctly");
	ok(bt_ctf_stream_set_packet_header(NULL, packet_header) < 0,
		"bt_ctf_stream_set_packet_header handles a NULL stream correctly");
	ok(bt_ctf_stream_set_packet_header(stream1, packet_header_field) < 0,
		"bt_ctf_stream_set_packet_header rejects a packet header of the wrong type");
	ok(!bt_ctf_stream_set_packet_header(stream1, packet_header),
		"Successfully set a stream's packet header");

	ok(bt_ctf_writer_add_environment_field(writer, "new_field", "test") == 0,
		"Add environment field to writer after stream creation");

	test_clock_utils();

	test_instanciate_event_before_stream(writer, clock);

	append_simple_event(stream_class, stream1, clock);

	packet_resize_test(stream_class, stream1, clock);

	append_complex_event(stream_class, stream1, clock);

	append_existing_event_class(stream_class);

	test_empty_stream(writer);

	test_custom_event_header_stream(writer, clock);

	metadata_string = bt_ctf_writer_get_metadata_string(writer);
	ok(metadata_string, "Get metadata string");

	bt_ctf_writer_flush_metadata(writer);

	bt_put(clock);
	bt_put(ret_stream_class);
	bt_put(writer);
	bt_put(stream1);
	bt_put(packet_context_type);
	bt_put(packet_context_field_type);
	bt_put(integer_type);
	bt_put(stream_event_context_type);
	bt_put(ret_field_type);
	bt_put(packet_header_type);
	bt_put(packet_header_field_type);
	bt_put(packet_header);
	bt_put(packet_header_field);
	bt_put(trace);
	free(metadata_string);
	bt_put(stream_class);

	validate_trace(argv[1], trace_path);

	recursive_rmdir(trace_path);
	g_free(trace_path);
	g_free(metadata_path);

	return 0;
}
