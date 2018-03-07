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
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/trace.h>
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

#define NR_TESTS 476

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
	gchar *standard_error = NULL;
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
			&standard_error,
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
		diag_multiline(standard_error);
		goto result;
	}

result:
	ok(ret == 0, "Babeltrace could read the resulting trace");
	g_free(standard_error);
}

static
void append_simple_event(struct bt_stream_class *stream_class,
		struct bt_stream *stream, struct bt_ctf_clock *clock)
{
	/* Create and add a simple event class */
	struct bt_event_class *simple_event_class =
		bt_event_class_create("Simple Event");
	struct bt_field_type *uint_12_type =
		bt_field_type_integer_create(12);
	struct bt_field_type *int_64_type =
		bt_field_type_integer_create(64);
	struct bt_field_type *float_type =
		bt_field_type_floating_point_create();
	struct bt_field_type *enum_type;
	struct bt_field_type *enum_type_unsigned =
		bt_field_type_enumeration_create(uint_12_type);
	struct bt_field_type *event_context_type =
		bt_field_type_structure_create();
	struct bt_field_type *event_payload_type = NULL;
	struct bt_field_type *returned_type;
	struct bt_event *simple_event;
	struct bt_field *integer_field;
	struct bt_field *float_field;
	struct bt_field *enum_field;
	struct bt_field *enum_field_unsigned;
	struct bt_field *enum_container_field;
	const char *mapping_name_test = "truie";
	const double double_test_value = 3.1415;
	struct bt_field *enum_container_field_unsigned;
	const char *mapping_name_negative_test = "negative_value";
	const char *ret_char;
	double ret_double;
	int64_t ret_range_start_int64_t, ret_range_end_int64_t;
	uint64_t ret_range_start_uint64_t, ret_range_end_uint64_t;
	struct bt_event_class *ret_event_class;
	struct bt_field *packet_context;
	struct bt_field *packet_context_field;
	struct bt_field *stream_event_context;
	struct bt_field *stream_event_context_field;
	struct bt_field *event_context;
	struct bt_field *event_context_field;
	struct bt_field_type *ep_integer_field_type = NULL;
	struct bt_field_type *ep_enum_field_type = NULL;
	struct bt_field_type *ep_enum_field_unsigned_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;
	int ret;

	ok(uint_12_type, "Create an unsigned integer type");

	ok(!bt_ctf_field_type_integer_set_signed(int_64_type, 1),
		"Set signed 64 bit integer signedness to true");
	ok(int_64_type, "Create a signed integer type");
	enum_type = bt_field_type_enumeration_create(int_64_type);

	returned_type = bt_field_type_enumeration_get_container_type(enum_type);
	ok(returned_type == int_64_type, "bt_field_type_enumeration_get_container_type returns the right type");
	ok(!bt_field_type_enumeration_create(enum_type),
		"bt_field_enumeration_type_create rejects non-integer container field types");
	bt_put(returned_type);

	bt_field_type_set_alignment(float_type, 32);
	ok(bt_field_type_get_alignment(float_type) == 32,
		"bt_field_type_get_alignment returns a correct value");

	ok(bt_field_type_floating_point_set_exponent_digits(float_type, 11) == 0,
		"Set a floating point type's exponent digit count");
	ok(bt_field_type_floating_point_set_mantissa_digits(float_type, 53) == 0,
		"Set a floating point type's mantissa digit count");

	ok(bt_field_type_floating_point_get_exponent_digits(float_type) == 11,
		"bt_field_type_floating_point_get_exponent_digits returns the correct value");
	ok(bt_field_type_floating_point_get_mantissa_digits(float_type) == 53,
		"bt_field_type_floating_point_get_mantissa_digits returns the correct value");

	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		mapping_name_negative_test, -12345, 0) == 0,
		"bt_field_type_enumeration_add_mapping accepts negative enumeration mappings");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		"escaping; \"test\"", 1, 1) == 0,
		"bt_field_type_enumeration_add_mapping accepts enumeration mapping strings containing quotes");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		"\tanother \'escaping\'\n test\"", 2, 4) == 0,
		"bt_field_type_enumeration_add_mapping accepts enumeration mapping strings containing special characters");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type,
		"event clock int float", 5, 22) == 0,
		"Accept enumeration mapping strings containing reserved keywords");
	bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		42, 42);
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		43, 51) == 0, "bt_field_type_enumeration_add_mapping accepts duplicate mapping names");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, "something",
		-500, -400) == 0, "bt_field_type_enumeration_add_mapping accepts overlapping enum entries");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		-54, -55), "bt_field_type_enumeration_add_mapping rejects mapping where end < start");
	bt_ctf_field_type_enumeration_add_mapping(enum_type, "another entry", -42000, -13000);

	iter = bt_field_type_enumeration_find_mappings_by_signed_value(enum_type, -4200000);
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	ok(iter && ret, "bt_field_type_enumeration_find_mappings_by_signed_value rejects non-mapped values");
	BT_PUT(iter);

	iter = bt_field_type_enumeration_find_mappings_by_signed_value(enum_type, 3);
	ok(iter != NULL, "bt_field_type_enumeration_find_mappings_by_signed_value succeeds with mapped value");
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	ok(!ret && bt_field_type_enumeration_mapping_iterator_get_signed(iter, NULL, NULL, NULL) == 0,
		"bt_field_type_enumeration_mapping_iterator_get_signed handles mapped values correctly");
	BT_PUT(iter);

	ok(bt_event_class_add_field(simple_event_class, enum_type,
		"enum_field") == 0, "Add signed enumeration field to event");

	ok(bt_field_type_enumeration_get_mapping_signed(enum_type, 0, NULL,
		&ret_range_start_int64_t, &ret_range_end_int64_t) == 0,
		"bt_field_type_enumeration_get_mapping_signed handles a NULL string correctly");
	ok(bt_field_type_enumeration_get_mapping_signed(enum_type, 0, &ret_char,
		NULL, &ret_range_end_int64_t) == 0,
		"bt_field_type_enumeration_get_mapping_signed handles a NULL start correctly");
	ok(bt_field_type_enumeration_get_mapping_signed(enum_type, 0, &ret_char,
		&ret_range_start_int64_t, NULL) == 0,
		"bt_field_type_enumeration_get_mapping_signed handles a NULL end correctly");
	/* Assumes entries are sorted by range_start values. */
	ok(bt_field_type_enumeration_get_mapping_signed(enum_type, 6, &ret_char,
		&ret_range_start_int64_t, &ret_range_end_int64_t) == 0,
		"bt_field_type_enumeration_get_mapping_signed returns a value");
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_field_type_enumeration_get_mapping_signed returns a correct mapping name");
	ok(ret_range_start_int64_t == 42,
		"bt_field_type_enumeration_get_mapping_signed returns a correct mapping start");
	ok(ret_range_end_int64_t == 42,
		"bt_field_type_enumeration_get_mapping_signed returns a correct mapping end");

	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned,
		"escaping; \"test\"", 0, 0) == 0,
		"bt_field_type_enumeration_add_mapping_unsigned accepts enumeration mapping strings containing quotes");
	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned,
		"\tanother \'escaping\'\n test\"", 1, 4) == 0,
		"bt_field_type_enumeration_add_mapping_unsigned accepts enumeration mapping strings containing special characters");
	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned,
		"event clock int float", 5, 22) == 0,
		"bt_field_type_enumeration_add_mapping_unsigned accepts enumeration mapping strings containing reserved keywords");
	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, mapping_name_test,
		42, 42) == 0, "bt_field_type_enumeration_add_mapping_unsigned accepts single-value ranges");
	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, mapping_name_test,
		43, 51) == 0, "bt_field_type_enumeration_add_mapping_unsigned accepts duplicate mapping names");
	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, "something",
		7, 8) == 0, "bt_field_type_enumeration_add_mapping_unsigned accepts overlapping enum entries");
	ok(bt_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, mapping_name_test,
		55, 54), "bt_field_type_enumeration_add_mapping_unsigned rejects mapping where end < start");
	ok(bt_event_class_add_field(simple_event_class, enum_type_unsigned,
		"enum_field_unsigned") == 0, "Add unsigned enumeration field to event");

	ok(bt_field_type_enumeration_get_mapping_count(enum_type_unsigned) == 6,
		"bt_field_type_enumeration_get_mapping_count returns the correct value");

	ok(bt_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 0, NULL,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) == 0,
		"bt_field_type_enumeration_get_mapping_unsigned handles a NULL string correctly");
	ok(bt_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 0, &ret_char,
		NULL, &ret_range_end_uint64_t) == 0,
		"bt_field_type_enumeration_get_mapping_unsigned handles a NULL start correctly");
	ok(bt_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 0, &ret_char,
		&ret_range_start_uint64_t, NULL) == 0,
		"bt_field_type_enumeration_get_mapping_unsigned handles a NULL end correctly");
	ok(bt_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 4, &ret_char,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) == 0,
		"bt_field_type_enumeration_get_mapping_unsigned returns a value");
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_field_type_enumeration_get_mapping_unsigned returns a correct mapping name");
	ok(ret_range_start_uint64_t == 42,
		"bt_field_type_enumeration_get_mapping_unsigned returns a correct mapping start");
	ok(ret_range_end_uint64_t == 42,
		"bt_field_type_enumeration_get_mapping_unsigned returns a correct mapping end");

	bt_event_class_add_field(simple_event_class, uint_12_type,
		"integer_field");
	bt_event_class_add_field(simple_event_class, float_type,
		"float_field");

	assert(!bt_event_class_set_id(simple_event_class, 13));

	/* Set an event context type which will contain a single integer. */
	ok(!bt_field_type_structure_add_field(event_context_type, uint_12_type,
		"event_specific_context"),
		"Add event specific context field");

	ok(bt_event_class_set_context_type(NULL, event_context_type) < 0,
		"bt_event_class_set_context_type handles a NULL event class correctly");
	ok(!bt_event_class_set_context_type(simple_event_class, event_context_type),
		"Set an event class' context type successfully");
	returned_type = bt_event_class_get_context_type(simple_event_class);
	ok(returned_type == event_context_type,
		"bt_event_class_get_context_type returns the appropriate type");
	bt_put(returned_type);

	ok(!bt_stream_class_add_event_class(stream_class, simple_event_class),
		"Adding simple event class to stream class");

	/*
	 * bt_stream_class_add_event_class() copies the field types
	 * of simple_event_class, so we retrieve the new ones to create
	 * the appropriate fields.
	 */
	BT_PUT(event_context_type);
	BT_PUT(event_payload_type);
	event_payload_type = bt_event_class_get_payload_type(
		simple_event_class);
	assert(event_payload_type);
	event_context_type = bt_event_class_get_context_type(
		simple_event_class);
	assert(event_context_type);
	ep_integer_field_type =
		bt_field_type_structure_get_field_type_by_name(
			event_payload_type, "integer_field");
	assert(ep_integer_field_type);
	ep_enum_field_type =
		bt_field_type_structure_get_field_type_by_name(
			event_payload_type, "enum_field");
	assert(ep_enum_field_type);
	ep_enum_field_unsigned_type =
		bt_field_type_structure_get_field_type_by_name(
			event_payload_type, "enum_field_unsigned");
	assert(ep_enum_field_unsigned_type);

	ok(bt_stream_class_get_event_class_count(stream_class) == 1,
		"bt_stream_class_get_event_class_count returns a correct number of event classes");
	ret_event_class = bt_stream_class_get_event_class_by_index(stream_class, 0);
	ok(ret_event_class == simple_event_class,
		"bt_stream_class_get_event_class returns the correct event class");
	bt_put(ret_event_class);
	ok(!bt_stream_class_get_event_class_by_id(stream_class, 2),
		"bt_stream_class_get_event_class_by_id returns NULL when the requested ID doesn't exist");
	ret_event_class =
		bt_stream_class_get_event_class_by_id(stream_class, 13);
	ok(ret_event_class == simple_event_class,
		"bt_stream_class_get_event_class_by_id returns a correct event class");
	bt_put(ret_event_class);

	simple_event = bt_event_create(simple_event_class);
	ok(simple_event,
		"Instantiate an event containing a single integer field");

	integer_field = bt_field_create(ep_integer_field_type);
	bt_field_unsigned_integer_set_value(integer_field, 42);
	ok(bt_event_set_payload(simple_event, "integer_field",
		integer_field) == 0, "Use bt_event_set_payload to set a manually allocated field");

	float_field = bt_event_get_payload(simple_event, "float_field");
	bt_field_floating_point_set_value(float_field, double_test_value);
	ok(!bt_field_floating_point_get_value(float_field, &ret_double),
		"bt_field_floating_point_get_value returns a double value");
	ok(fabs(ret_double - double_test_value) <= DBL_EPSILON,
		"bt_field_floating_point_get_value returns a correct value");

	enum_field = bt_field_create(ep_enum_field_type);
	assert(enum_field);

	enum_container_field = bt_field_enumeration_get_container(enum_field);
	ok(bt_field_signed_integer_set_value(
		enum_container_field, -42) == 0,
		"Set signed enumeration container value");
	iter = bt_field_enumeration_get_mappings(enum_field);
	ok(iter, "bt_field_enumeration_get_mappings returns an iterator to matching mappings");
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	ok(!ret, "bt_field_enumeration_get_mappings returned a non-empty match");
	ret = bt_field_type_enumeration_mapping_iterator_get_signed(iter, &ret_char, NULL, NULL);
	ok(!ret && ret_char, "bt_field_type_enumeration_mapping_iterator_get_signed return a mapping name");
	assert(ret_char);
	ok(!strcmp(ret_char, mapping_name_negative_test),
		"bt_field_enumeration_get_single_mapping_name returns the correct mapping name with an signed container");
	ret = bt_event_set_payload(simple_event, "enum_field", enum_field);
	assert(!ret);
	BT_PUT(iter);

	enum_field_unsigned = bt_field_create(ep_enum_field_unsigned_type);
	assert(enum_field_unsigned);
	enum_container_field_unsigned = bt_field_enumeration_get_container(
		enum_field_unsigned);
	ok(bt_field_unsigned_integer_set_value(
		enum_container_field_unsigned, 42) == 0,
		"Set unsigned enumeration container value");
	ret = bt_event_set_payload(simple_event, "enum_field_unsigned",
		enum_field_unsigned);
	assert(!ret);
	iter = bt_field_enumeration_get_mappings(enum_field_unsigned);
	assert(iter);
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	assert(!ret);
	(void) bt_field_type_enumeration_mapping_iterator_get_unsigned(iter, &ret_char, NULL, NULL);
	ok(ret_char && !strcmp(ret_char, mapping_name_test),
		"bt_field_type_enumeration_mapping_iterator_get_unsigned returns the correct mapping name with an unsigned container");

	ok(bt_ctf_clock_set_time(clock, current_time) == 0, "Set clock time");

	/* Populate stream event context */
	stream_event_context =
		bt_event_get_stream_event_context(simple_event);
	assert(stream_event_context);
	stream_event_context_field = bt_field_structure_get_field_by_name(
		stream_event_context, "common_event_context");
	bt_field_unsigned_integer_set_value(stream_event_context_field, 42);

	/* Populate the event's context */
	event_context = bt_event_get_event_context(simple_event);
	ok(event_context,
		"bt_event_get_event_context returns a field");
	returned_type = bt_field_get_type(event_context);
	ok(returned_type == event_context_type,
		"bt_event_get_event_context returns a field of the appropriate type");
	event_context_field = bt_field_structure_get_field_by_name(event_context,
		"event_specific_context");
	ok(!bt_field_unsigned_integer_set_value(event_context_field, 1234),
		"Successfully set an event context's value");
	ok(!bt_event_set_event_context(simple_event, event_context),
		"Set an event context successfully");

	ok(bt_stream_append_event(stream, simple_event) == 0,
		"Append simple event to trace stream");

	packet_context = bt_stream_get_packet_context(stream);
	ok(packet_context,
		"bt_stream_get_packet_context returns a packet context");

	packet_context_field = bt_field_structure_get_field_by_name(packet_context,
		"packet_size");
	ok(packet_context_field,
		"Packet context contains the default packet_size field.");
	bt_put(packet_context_field);
	packet_context_field = bt_field_structure_get_field_by_name(packet_context,
		"custom_packet_context_field");
	ok(bt_field_unsigned_integer_set_value(packet_context_field, 8) == 0,
		"Custom packet context field value successfully set.");

	ok(bt_stream_set_packet_context(stream, packet_context) == 0,
		"Successfully set a stream's packet context");

	ok(bt_stream_flush(stream) == 0,
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
void append_complex_event(struct bt_stream_class *stream_class,
		struct bt_stream *stream, struct bt_ctf_clock *clock)
{
	int i, ret;
	struct event_class_attrs_counts ;
	const char *complex_test_event_string = "Complex Test Event";
	const char *test_string_1 = "Test ";
	const char *test_string_2 = "string ";
	const char *test_string_3 = "abcdefghi";
	const char *test_string_4 = "abcd\0efg\0hi";
	const char *test_string_cat = "Test string abcdeefg";
	struct bt_field_type *uint_35_type =
		bt_field_type_integer_create(35);
	struct bt_field_type *int_16_type =
		bt_field_type_integer_create(16);
	struct bt_field_type *uint_3_type =
		bt_field_type_integer_create(3);
	struct bt_field_type *enum_variant_type =
		bt_field_type_enumeration_create(uint_3_type);
	struct bt_field_type *variant_type =
		bt_field_type_variant_create(enum_variant_type,
			"variant_selector");
	struct bt_field_type *string_type =
		bt_field_type_string_create();
	struct bt_field_type *sequence_type;
	struct bt_field_type *array_type;
	struct bt_field_type *inner_structure_type =
		bt_field_type_structure_create();
	struct bt_field_type *complex_structure_type =
		bt_field_type_structure_create();
	struct bt_field_type *ret_field_type;
	struct bt_event_class *event_class;
	struct bt_event *event;
	struct bt_field *uint_35_field, *int_16_field, *a_string_field,
		*inner_structure_field, *complex_structure_field,
		*a_sequence_field, *enum_variant_field, *enum_container_field,
		*variant_field, *an_array_field, *stream_event_ctx_field,
		*stream_event_ctx_int_field, *ret_field;
	uint64_t ret_unsigned_int;
	int64_t ret_signed_int;
	const char *ret_string;
	struct bt_stream_class *ret_stream_class;
	struct bt_event_class *ret_event_class;
	struct bt_field *packet_context, *packet_context_field;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	ok(bt_field_type_set_alignment(int_16_type, 0),
		"bt_field_type_set_alignment handles 0-alignment correctly");
	ok(bt_field_type_set_alignment(int_16_type, 3),
		"bt_field_type_set_alignment handles wrong alignment correctly (3)");
	ok(bt_field_type_set_alignment(int_16_type, 24),
		"bt_field_type_set_alignment handles wrong alignment correctly (24)");
	ok(!bt_field_type_set_alignment(int_16_type, 4),
		"bt_field_type_set_alignment handles correct alignment correctly (4)");
	ok(!bt_field_type_set_alignment(int_16_type, 32),
		"Set alignment of signed 16 bit integer to 32");
	ok(!bt_ctf_field_type_integer_set_signed(int_16_type, 1),
		"Set integer signedness to true");
	ok(!bt_field_type_integer_set_base(uint_35_type,
		BT_INTEGER_BASE_HEXADECIMAL),
		"Set signed 16 bit integer base to hexadecimal");

	array_type = bt_field_type_array_create(int_16_type, ARRAY_TEST_LENGTH);
	sequence_type = bt_field_type_sequence_create(int_16_type,
		"seq_len");

	ret_field_type = bt_field_type_array_get_element_type(
		array_type);
	ok(ret_field_type == int_16_type,
		"bt_field_type_array_get_element_type returns the correct type");
	bt_put(ret_field_type);

	ok(bt_field_type_array_get_length(array_type) == ARRAY_TEST_LENGTH,
		"bt_field_type_array_get_length returns the correct length");

	ok(bt_field_type_structure_add_field(inner_structure_type,
		inner_structure_type, "yes"), "Cannot add self to structure");
	ok(!bt_field_type_structure_add_field(inner_structure_type,
		uint_35_type, "seq_len"), "Add seq_len field to inner structure");
	ok(!bt_field_type_structure_add_field(inner_structure_type,
		sequence_type, "a_sequence"), "Add a_sequence field to inner structure");
	ok(!bt_field_type_structure_add_field(inner_structure_type,
		array_type, "an_array"), "Add an_array field to inner structure");

	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"UINT3_TYPE", 0, 0);
	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"INT16_TYPE", 1, 1);
	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"UINT35_TYPE", 2, 7);

	iter = bt_field_type_enumeration_find_mappings_by_name(enum_variant_type, "INT16_TYPE");
	ok(iter != NULL, "bt_field_type_enumeration_find_mappings_by_name returns a non-NULL iterator");
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	ok(!ret, "bt_field_type_enumeration_find_mappings_by_name handles an existing mapping correctly");
	ok(bt_field_type_enumeration_mapping_iterator_get_unsigned(iter, NULL, NULL, NULL) == 0,
		"bt_field_type_enumeration_mapping_iterator_get_unsigned handles mapped values correctly");
	BT_PUT(iter);

	iter = bt_field_type_enumeration_find_mappings_by_unsigned_value(enum_variant_type, -42);
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	ok(iter && ret, "bt_field_type_enumeration_find_mappings_by_unsigned_value handles invalid values correctly");
	BT_PUT(iter);

	iter = bt_field_type_enumeration_find_mappings_by_unsigned_value(enum_variant_type, 5);
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	ok(iter != NULL && !ret, "bt_field_type_enumeration_find_mappings_by_unsigned_value handles valid values correctly");
	BT_PUT(iter);

	ok(bt_field_type_variant_add_field(variant_type, uint_3_type,
		"An unknown entry"), "Reject a variant field based on an unknown tag value");
	ok(bt_field_type_variant_add_field(variant_type, uint_3_type,
		"UINT3_TYPE") == 0, "Add a field to a variant");
	ok(!bt_field_type_variant_add_field(variant_type, int_16_type,
		"INT16_TYPE"), "Add INT16_TYPE field to variant");
	ok(!bt_field_type_variant_add_field(variant_type, uint_35_type,
		"UINT35_TYPE"), "Add UINT35_TYPE field to variant");

	ret_field_type = bt_field_type_variant_get_tag_type(variant_type);
	ok(ret_field_type == enum_variant_type,
		"bt_field_type_variant_get_tag_type returns a correct tag type");
	bt_put(ret_field_type);

	ret_string = bt_field_type_variant_get_tag_name(variant_type);
	ok(ret_string ? !strcmp(ret_string, "variant_selector") : 0,
		"bt_field_type_variant_get_tag_name returns the correct variant tag name");
	ret_field_type = bt_field_type_variant_get_field_type_by_name(
		variant_type, "INT16_TYPE");
	ok(ret_field_type == int_16_type,
		"bt_field_type_variant_get_field_type_by_name returns a correct field type");
	bt_put(ret_field_type);

	ok(bt_field_type_variant_get_field_count(variant_type) == 3,
		"bt_field_type_variant_get_field_count returns the correct count");

	ok(bt_field_type_variant_get_field_by_index(variant_type, NULL, &ret_field_type, 0) == 0,
		"bt_field_type_variant_get_field handles a NULL field name correctly");
	bt_put(ret_field_type);
	ok(bt_field_type_variant_get_field_by_index(variant_type, &ret_string, NULL, 0) == 0,
		"bt_field_type_variant_get_field handles a NULL field type correctly");
	ok(bt_field_type_variant_get_field_by_index(variant_type, &ret_string, &ret_field_type, 1) == 0,
		"bt_field_type_variant_get_field returns a field");
	ok(!strcmp("INT16_TYPE", ret_string),
		"bt_field_type_variant_get_field returns a correct field name");
	ok(ret_field_type == int_16_type,
		"bt_field_type_variant_get_field returns a correct field type");
	bt_put(ret_field_type);

	ok(!bt_field_type_structure_add_field(complex_structure_type,
		enum_variant_type, "variant_selector"),
		"Add variant_selector field to complex structure");
	ok(!bt_field_type_structure_add_field(complex_structure_type,
		string_type, "string"), "Add `string` field to complex structure");
	ok(!bt_field_type_structure_add_field(complex_structure_type,
		variant_type, "variant_value"),
		"Add variant_value field to complex structure");
	ok(!bt_field_type_structure_add_field(complex_structure_type,
		inner_structure_type, "inner_structure"),
		"Add inner_structure field to complex structure");

	event_class = bt_event_class_create(complex_test_event_string);
	ok(event_class, "Create an event class");
	ok(bt_event_class_add_field(event_class, uint_35_type, ""),
		"Reject addition of a field with an empty name to an event");
	ok(bt_event_class_add_field(event_class, NULL, "an_integer"),
		"Reject addition of a field with a NULL type to an event");
	ok(bt_event_class_add_field(event_class, uint_35_type,
		"int"),
		"Reject addition of a type with an illegal name to an event");
	ok(bt_event_class_add_field(event_class, uint_35_type,
		"uint_35") == 0,
		"Add field of type unsigned integer to an event");
	ok(bt_event_class_add_field(event_class, int_16_type,
		"int_16") == 0, "Add field of type signed integer to an event");
	ok(bt_event_class_add_field(event_class, complex_structure_type,
		"complex_structure") == 0,
		"Add composite structure to an event");

	ret_string = bt_event_class_get_name(event_class);
	ok(!strcmp(ret_string, complex_test_event_string),
		"bt_event_class_get_name returns a correct name");
	ok(bt_event_class_get_id(event_class) < 0,
		"bt_event_class_get_id returns a negative value when not set");
	ok(bt_event_class_set_id(NULL, 42) < 0,
		"bt_event_class_set_id handles NULL correctly");
	ok(bt_event_class_set_id(event_class, 42) == 0,
		"Set an event class' id");
	ok(bt_event_class_get_id(event_class) == 42,
		"bt_event_class_get_id returns the correct value");

	/* Test event class attributes */
	ok(bt_event_class_get_log_level(event_class) == BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED,
		"event class has the expected initial log level");
	ok(!bt_event_class_get_emf_uri(event_class),
		"as expected, event class has no initial EMF URI");
	ok(bt_event_class_set_log_level(NULL, BT_EVENT_CLASS_LOG_LEVEL_INFO),
		"bt_event_class_set_log_level handles a NULL event class correctly");
	ok(bt_event_class_set_log_level(event_class, BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN),
		"bt_event_class_set_log_level handles an unknown log level correctly");
	ok(!bt_event_class_set_log_level(event_class, BT_EVENT_CLASS_LOG_LEVEL_INFO),
		"bt_event_class_set_log_level succeeds with a valid log level");
	ok(bt_event_class_get_log_level(event_class) == BT_EVENT_CLASS_LOG_LEVEL_INFO,
		"bt_event_class_get_log_level returns the expected log level");
	ok(bt_event_class_set_emf_uri(NULL, "http://diamon.org/babeltrace/"),
		"bt_event_class_set_emf_uri handles a NULL event class correctly");
	ok(!bt_event_class_set_emf_uri(event_class, "http://diamon.org/babeltrace/"),
		"bt_event_class_set_emf_uri succeeds with a valid EMF URI");
	ok(strcmp(bt_event_class_get_emf_uri(event_class), "http://diamon.org/babeltrace/") == 0,
		"bt_event_class_get_emf_uri returns the expected EMF URI");
	ok(!bt_event_class_set_emf_uri(event_class, NULL),
		"bt_event_class_set_emf_uri succeeds with NULL (to reset)");
	ok(!bt_event_class_get_emf_uri(event_class),
		"as expected, event class has no EMF URI after reset");

	/* Add event class to the stream class */
	ok(bt_stream_class_add_event_class(stream_class, NULL),
		"Reject addition of NULL event class to a stream class");
	ok(bt_stream_class_add_event_class(stream_class,
		event_class) == 0, "Add an event class to stream class");

	ret_stream_class = bt_event_class_get_stream_class(event_class);
	ok(ret_stream_class == stream_class,
		"bt_event_class_get_stream_class returns the correct stream class");
	bt_put(ret_stream_class);

	ok(bt_event_class_get_payload_type_field_count(event_class) == 3,
		"bt_event_class_get_field_count returns a correct value");

	ok(bt_event_class_get_payload_type_field_by_index(event_class, &ret_string,
		NULL, 0) == 0,
		"bt_event_class_get_field handles a NULL field type correctly");
	ok(bt_event_class_get_payload_type_field_by_index(event_class, &ret_string,
		&ret_field_type, 0) == 0,
		"bt_event_class_get_field returns a field");
	ok(bt_field_type_compare(ret_field_type, uint_35_type) == 0,
		"bt_event_class_get_field returns a correct field type");
	bt_put(ret_field_type);
	ok(!strcmp(ret_string, "uint_35"),
		"bt_event_class_get_field returns a correct field name");
	ok(bt_ctf_event_class_get_field_by_name(event_class, "truie") == NULL,
		"bt_event_class_get_field_by_name handles an invalid field name correctly");
	ret_field_type = bt_ctf_event_class_get_field_by_name(event_class,
		"complex_structure");
	ok(bt_field_type_compare(ret_field_type, complex_structure_type) == 0,
		"bt_event_class_get_field_by_name returns a correct field type");
	bt_put(ret_field_type);

	event = bt_event_create(event_class);
	ok(event, "Instanciate a complex event");

	ret_event_class = bt_event_get_class(event);
	ok(ret_event_class == event_class,
		"bt_event_get_class returns the correct event class");
	bt_put(ret_event_class);

	uint_35_field = bt_event_get_payload(event, "uint_35");
	ok(uint_35_field, "Use bt_event_get_payload to get a field instance ");
	bt_field_unsigned_integer_set_value(uint_35_field, 0x0DDF00D);
	ok(bt_field_unsigned_integer_get_value(uint_35_field,
		&ret_unsigned_int) == 0,
		"bt_field_unsigned_integer_get_value succeeds after setting a value");
	ok(ret_unsigned_int == 0x0DDF00D,
		"bt_field_unsigned_integer_get_value returns the correct value");
	bt_put(uint_35_field);

	int_16_field = bt_event_get_payload(event, "int_16");
	bt_field_signed_integer_set_value(int_16_field, -12345);
	ok(bt_field_signed_integer_get_value(int_16_field,
		&ret_signed_int) == 0,
		"bt_field_signed_integer_get_value succeeds after setting a value");
	ok(ret_signed_int == -12345,
		"bt_field_signed_integer_get_value returns the correct value");
	bt_put(int_16_field);

	complex_structure_field = bt_event_get_payload(event,
		"complex_structure");

	inner_structure_field = bt_field_structure_get_field_by_index(
		complex_structure_field, 3);
	ret_field_type = bt_field_get_type(inner_structure_field);
	bt_put(inner_structure_field);
	ok(bt_field_type_compare(ret_field_type, inner_structure_type) == 0,
		"bt_field_structure_get_field_by_index returns a correct field");
	bt_put(ret_field_type);

	inner_structure_field = bt_field_structure_get_field_by_name(
		complex_structure_field, "inner_structure");
	a_string_field = bt_field_structure_get_field_by_name(
		complex_structure_field, "string");
	enum_variant_field = bt_field_structure_get_field_by_name(
		complex_structure_field, "variant_selector");
	variant_field = bt_field_structure_get_field_by_name(
		complex_structure_field, "variant_value");
	uint_35_field = bt_field_structure_get_field_by_name(
		inner_structure_field, "seq_len");
	a_sequence_field = bt_field_structure_get_field_by_name(
		inner_structure_field, "a_sequence");
	an_array_field = bt_field_structure_get_field_by_name(
		inner_structure_field, "an_array");

	enum_container_field = bt_field_enumeration_get_container(
		enum_variant_field);
	bt_field_unsigned_integer_set_value(enum_container_field, 1);
	int_16_field = bt_field_variant_get_field(variant_field,
		enum_variant_field);
	bt_field_signed_integer_set_value(int_16_field, -200);
	bt_put(int_16_field);
	bt_field_string_set_value(a_string_field,
		test_string_1);
	ok(!bt_field_string_append(a_string_field, test_string_2),
		"bt_field_string_append succeeds");
	ok(!bt_field_string_append_len(a_string_field, test_string_3, 5),
		"bt_field_string_append_len succeeds (append 5 characters)");
	ok(!bt_field_string_append_len(a_string_field, &test_string_4[5], 3),
		"bt_field_string_append_len succeeds (append 0 characters)");
	ok(!bt_field_string_append_len(a_string_field, test_string_3, 0),
		"bt_field_string_append_len succeeds (append 0 characters)");

	ret_string = bt_field_string_get_value(a_string_field);
	ok(ret_string, "bt_field_string_get_value returns a string");
	ok(ret_string ? !strcmp(ret_string, test_string_cat) : 0,
		"bt_field_string_get_value returns a correct value");
	bt_field_unsigned_integer_set_value(uint_35_field,
		SEQUENCE_TEST_LENGTH);

	ret_field_type = bt_field_type_variant_get_field_type_from_tag(
		variant_type, enum_variant_field);
	ok(ret_field_type == int_16_type,
		"bt_field_type_variant_get_field_type_from_tag returns the correct field type");

	ok(bt_field_sequence_set_length(a_sequence_field,
		uint_35_field) == 0, "Set a sequence field's length");
	ret_field = bt_field_sequence_get_length(a_sequence_field);
	ok(ret_field == uint_35_field,
		"bt_field_sequence_get_length returns the correct length field");

	for (i = 0; i < SEQUENCE_TEST_LENGTH; i++) {
		int_16_field = bt_field_sequence_get_field(
			a_sequence_field, i);
		bt_field_signed_integer_set_value(int_16_field, 4 - i);
		bt_put(int_16_field);
	}

	for (i = 0; i < ARRAY_TEST_LENGTH; i++) {
		int_16_field = bt_field_array_get_field(
			an_array_field, i);
		bt_field_signed_integer_set_value(int_16_field, i);
		bt_put(int_16_field);
	}

	stream_event_ctx_field = bt_event_get_stream_event_context(event);
	assert(stream_event_ctx_field);
	stream_event_ctx_int_field = bt_field_structure_get_field_by_name(
		stream_event_ctx_field, "common_event_context");
	BT_PUT(stream_event_ctx_field);
	bt_field_unsigned_integer_set_value(stream_event_ctx_int_field, 17);
	BT_PUT(stream_event_ctx_int_field);

	bt_ctf_clock_set_time(clock, ++current_time);
	ok(bt_stream_append_event(stream, event) == 0,
		"Append a complex event to a stream");

	/*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
	packet_context = bt_stream_get_packet_context(stream);
	packet_context_field = bt_field_structure_get_field_by_name(packet_context,
		"custom_packet_context_field");
	bt_field_unsigned_integer_set_value(packet_context_field, 1);

	ok(bt_stream_flush(stream) == 0,
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
	bt_put(ret_field);
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
void field_copy_tests_validate_same_type(struct bt_field *field,
		struct bt_field_type *expected_type, const char *name)
{
	struct bt_field_type *copy_type;

	copy_type = bt_field_get_type(field);
	ok(copy_type == expected_type,
		"bt_field_copy does not copy the type (%s)", name);
	bt_put(copy_type);
}

static
void field_copy_tests_validate_diff_ptrs(struct bt_field *field_a,
		struct bt_field *field_b, const char *name)
{
	ok(field_a != field_b,
		"bt_field_copy creates different pointers (%s)", name);
}

static
void field_copy_tests()
{
	struct bt_field_type *len_type = NULL;
	struct bt_field_type *fp_type = NULL;
	struct bt_field_type *s_type = NULL;
	struct bt_field_type *e_int_type = NULL;
	struct bt_field_type *e_type = NULL;
	struct bt_field_type *v_type = NULL;
	struct bt_field_type *v_label1_type = NULL;
	struct bt_field_type *v_label1_array_type = NULL;
	struct bt_field_type *v_label2_type = NULL;
	struct bt_field_type *v_label2_seq_type = NULL;
	struct bt_field_type *strct_type = NULL;
	struct bt_field *len = NULL;
	struct bt_field *fp = NULL;
	struct bt_field *s = NULL;
	struct bt_field *e_int = NULL;
	struct bt_field *e = NULL;
	struct bt_field *v = NULL;
	struct bt_field *v_selected = NULL;
	struct bt_field *v_selected_cur = NULL;
	struct bt_field *v_selected_0 = NULL;
	struct bt_field *v_selected_1 = NULL;
	struct bt_field *v_selected_2 = NULL;
	struct bt_field *v_selected_3 = NULL;
	struct bt_field *v_selected_4 = NULL;
	struct bt_field *v_selected_5 = NULL;
	struct bt_field *v_selected_6 = NULL;
	struct bt_field *a = NULL;
	struct bt_field *a_0 = NULL;
	struct bt_field *a_1 = NULL;
	struct bt_field *a_2 = NULL;
	struct bt_field *a_3 = NULL;
	struct bt_field *a_4 = NULL;
	struct bt_field *strct = NULL;
	struct bt_field *len_copy = NULL;
	struct bt_field *fp_copy = NULL;
	struct bt_field *s_copy = NULL;
	struct bt_field *e_int_copy = NULL;
	struct bt_field *e_copy = NULL;
	struct bt_field *v_copy = NULL;
	struct bt_field *v_selected_copy = NULL;
	struct bt_field *v_selected_copy_len = NULL;
	struct bt_field *v_selected_0_copy = NULL;
	struct bt_field *v_selected_1_copy = NULL;
	struct bt_field *v_selected_2_copy = NULL;
	struct bt_field *v_selected_3_copy = NULL;
	struct bt_field *v_selected_4_copy = NULL;
	struct bt_field *v_selected_5_copy = NULL;
	struct bt_field *v_selected_6_copy = NULL;
	struct bt_field *a_copy = NULL;
	struct bt_field *a_0_copy = NULL;
	struct bt_field *a_1_copy = NULL;
	struct bt_field *a_2_copy = NULL;
	struct bt_field *a_3_copy = NULL;
	struct bt_field *a_4_copy = NULL;
	struct bt_field *strct_copy = NULL;
	struct bt_field_type_enumeration_mapping_iterator *e_iter = NULL;
	uint64_t uint64_t_val;
	const char *str_val;
	double double_val;
	int ret;

	/* create len type */
	len_type = bt_field_type_integer_create(32);
	assert(len_type);

	/* create fp type */
	fp_type = bt_field_type_floating_point_create();
	assert(fp_type);

	/* create s type */
	s_type = bt_field_type_string_create();
	assert(s_type);

	/* create e_int type */
	e_int_type = bt_field_type_integer_create(8);
	assert(e_int_type);

	/* create e type */
	e_type = bt_field_type_enumeration_create(e_int_type);
	assert(e_type);
	ret = bt_ctf_field_type_enumeration_add_mapping(e_type, "LABEL1",
		10, 15);
	assert(!ret);
	ret = bt_ctf_field_type_enumeration_add_mapping(e_type, "LABEL2",
		23, 23);
	assert(!ret);

	/* create v_label1 type */
	v_label1_type = bt_field_type_string_create();
	assert(v_label1_type);

	/* create v_label1_array type */
	v_label1_array_type = bt_field_type_array_create(v_label1_type, 5);
	assert(v_label1_array_type);

	/* create v_label2 type */
	v_label2_type = bt_field_type_integer_create(16);
	assert(v_label2_type);

	/* create v_label2_seq type */
	v_label2_seq_type = bt_field_type_sequence_create(v_label2_type,
		"len");
	assert(v_label2_seq_type);

	/* create v type */
	v_type = bt_field_type_variant_create(e_type, "e");
	assert(v_type);
	ret = bt_field_type_variant_add_field(v_type, v_label1_array_type,
		"LABEL1");
	assert(!ret);
	ret = bt_field_type_variant_add_field(v_type, v_label2_seq_type,
		"LABEL2");
	assert(!ret);

	/* create strct type */
	strct_type = bt_field_type_structure_create();
	assert(strct_type);
	ret = bt_field_type_structure_add_field(strct_type, len_type,
		"len");
	assert(!ret);
	ret = bt_field_type_structure_add_field(strct_type, fp_type, "fp");
	assert(!ret);
	ret = bt_field_type_structure_add_field(strct_type, s_type, "s");
	assert(!ret);
	ret = bt_field_type_structure_add_field(strct_type, e_type, "e");
	assert(!ret);
	ret = bt_field_type_structure_add_field(strct_type, v_type, "v");
	assert(!ret);
	ret = bt_field_type_structure_add_field(strct_type,
		v_label1_array_type, "a");
	assert(!ret);

	/* create strct */
	strct = bt_field_create(strct_type);
	assert(strct);

	/* get len field */
	len = bt_field_structure_get_field_by_name(strct, "len");
	assert(len);

	/* get fp field */
	fp = bt_field_structure_get_field_by_name(strct, "fp");
	assert(fp);

	/* get s field */
	s = bt_field_structure_get_field_by_name(strct, "s");
	assert(s);

	/* get e field */
	e = bt_field_structure_get_field_by_name(strct, "e");
	assert(e);

	/* get e_int (underlying integer) */
	e_int = bt_field_enumeration_get_container(e);
	assert(e_int);

	/* get v field */
	v = bt_field_structure_get_field_by_name(strct, "v");
	assert(v);

	/* get a field */
	a = bt_field_structure_get_field_by_name(strct, "a");
	assert(a);

	/* set len field */
	ret = bt_field_unsigned_integer_set_value(len, 7);
	assert(!ret);

	/* set fp field */
	ret = bt_field_floating_point_set_value(fp, 3.14);
	assert(!ret);

	/* set s field */
	ret = bt_field_string_set_value(s, "btbt");
	assert(!ret);

	/* set e field (LABEL2) */
	ret = bt_field_unsigned_integer_set_value(e_int, 23);
	assert(!ret);

	/* set v field */
	v_selected = bt_field_variant_get_field(v, e);
	assert(v_selected);
	v_selected_cur = bt_field_variant_get_current_field(v);
	ok(v_selected_cur == v_selected,
		"bt_field_variant_get_current_field returns the current field");
	bt_put(v_selected_cur);

	/* set selected v field */
	ret = bt_field_sequence_set_length(v_selected, len);
	assert(!ret);
	v_selected_0 = bt_field_sequence_get_field(v_selected, 0);
	assert(v_selected_0);
	ret = bt_field_unsigned_integer_set_value(v_selected_0, 7);
	assert(!ret);
	v_selected_1 = bt_field_sequence_get_field(v_selected, 1);
	assert(v_selected_1);
	ret = bt_field_unsigned_integer_set_value(v_selected_1, 6);
	assert(!ret);
	v_selected_2 = bt_field_sequence_get_field(v_selected, 2);
	assert(v_selected_2);
	ret = bt_field_unsigned_integer_set_value(v_selected_2, 5);
	assert(!ret);
	v_selected_3 = bt_field_sequence_get_field(v_selected, 3);
	assert(v_selected_3);
	ret = bt_field_unsigned_integer_set_value(v_selected_3, 4);
	assert(!ret);
	v_selected_4 = bt_field_sequence_get_field(v_selected, 4);
	assert(v_selected_4);
	ret = bt_field_unsigned_integer_set_value(v_selected_4, 3);
	assert(!ret);
	v_selected_5 = bt_field_sequence_get_field(v_selected, 5);
	assert(v_selected_5);
	ret = bt_field_unsigned_integer_set_value(v_selected_5, 2);
	assert(!ret);
	v_selected_6 = bt_field_sequence_get_field(v_selected, 6);
	assert(v_selected_6);
	ret = bt_field_unsigned_integer_set_value(v_selected_6, 1);
	assert(!ret);

	/* set a field */
	a_0 = bt_field_array_get_field(a, 0);
	assert(a_0);
	ret = bt_field_string_set_value(a_0, "a_0");
	assert(!ret);
	a_1 = bt_field_array_get_field(a, 1);
	assert(a_1);
	ret = bt_field_string_set_value(a_1, "a_1");
	assert(!ret);
	a_2 = bt_field_array_get_field(a, 2);
	assert(a_2);
	ret = bt_field_string_set_value(a_2, "a_2");
	assert(!ret);
	a_3 = bt_field_array_get_field(a, 3);
	assert(a_3);
	ret = bt_field_string_set_value(a_3, "a_3");
	assert(!ret);
	a_4 = bt_field_array_get_field(a, 4);
	assert(a_4);
	ret = bt_field_string_set_value(a_4, "a_4");
	assert(!ret);

	/* create copy of strct */
	strct_copy = bt_field_copy(strct);
	ok(strct_copy,
		"bt_field_copy returns a valid pointer");

	/* get all copied fields */
	len_copy = bt_field_structure_get_field_by_name(strct_copy, "len");
	assert(len_copy);
	fp_copy = bt_field_structure_get_field_by_name(strct_copy, "fp");
	assert(fp_copy);
	s_copy = bt_field_structure_get_field_by_name(strct_copy, "s");
	assert(s_copy);
	e_copy = bt_field_structure_get_field_by_name(strct_copy, "e");
	assert(e_copy);
	e_int_copy = bt_field_enumeration_get_container(e_copy);
	assert(e_int_copy);
	v_copy = bt_field_structure_get_field_by_name(strct_copy, "v");
	assert(v_copy);
	v_selected_copy = bt_field_variant_get_field(v_copy, e_copy);
	assert(v_selected_copy);
	v_selected_0_copy = bt_field_sequence_get_field(v_selected_copy, 0);
	assert(v_selected_0_copy);
	v_selected_1_copy = bt_field_sequence_get_field(v_selected_copy, 1);
	assert(v_selected_1_copy);
	v_selected_2_copy = bt_field_sequence_get_field(v_selected_copy, 2);
	assert(v_selected_2_copy);
	v_selected_3_copy = bt_field_sequence_get_field(v_selected_copy, 3);
	assert(v_selected_3_copy);
	v_selected_4_copy = bt_field_sequence_get_field(v_selected_copy, 4);
	assert(v_selected_4_copy);
	v_selected_5_copy = bt_field_sequence_get_field(v_selected_copy, 5);
	assert(v_selected_5_copy);
	v_selected_6_copy = bt_field_sequence_get_field(v_selected_copy, 6);
	assert(v_selected_6_copy);
	a_copy = bt_field_structure_get_field_by_name(strct_copy, "a");
	assert(a_copy);
	a_0_copy = bt_field_array_get_field(a_copy, 0);
	assert(a_0_copy);
	a_1_copy = bt_field_array_get_field(a_copy, 1);
	assert(a_1_copy);
	a_2_copy = bt_field_array_get_field(a_copy, 2);
	assert(a_2_copy);
	a_3_copy = bt_field_array_get_field(a_copy, 3);
	assert(a_3_copy);
	a_4_copy = bt_field_array_get_field(a_copy, 4);
	assert(a_4_copy);

	/* make sure copied fields are different pointers */
	field_copy_tests_validate_diff_ptrs(strct_copy, strct, "strct");
	field_copy_tests_validate_diff_ptrs(len_copy, len, "len");
	field_copy_tests_validate_diff_ptrs(fp_copy, fp, "fp");
	field_copy_tests_validate_diff_ptrs(s_copy, s, "s");
	field_copy_tests_validate_diff_ptrs(e_int_copy, e_int, "e_int");
	field_copy_tests_validate_diff_ptrs(e_copy, e, "e");
	field_copy_tests_validate_diff_ptrs(v_copy, v, "v");
	field_copy_tests_validate_diff_ptrs(v_selected_copy, v_selected,
		"v_selected");
	field_copy_tests_validate_diff_ptrs(v_selected_0_copy, v_selected_0,
		"v_selected_0");
	field_copy_tests_validate_diff_ptrs(v_selected_1_copy, v_selected_1,
		"v_selected_1");
	field_copy_tests_validate_diff_ptrs(v_selected_2_copy, v_selected_2,
		"v_selected_2");
	field_copy_tests_validate_diff_ptrs(v_selected_3_copy, v_selected_3,
		"v_selected_3");
	field_copy_tests_validate_diff_ptrs(v_selected_4_copy, v_selected_4,
		"v_selected_4");
	field_copy_tests_validate_diff_ptrs(v_selected_5_copy, v_selected_5,
		"v_selected_5");
	field_copy_tests_validate_diff_ptrs(v_selected_6_copy, v_selected_6,
		"v_selected_6");
	field_copy_tests_validate_diff_ptrs(a_copy, a, "a");
	field_copy_tests_validate_diff_ptrs(a_0_copy, a_0, "a_0");
	field_copy_tests_validate_diff_ptrs(a_1_copy, a_1, "a_1");
	field_copy_tests_validate_diff_ptrs(a_2_copy, a_2, "a_2");
	field_copy_tests_validate_diff_ptrs(a_3_copy, a_3, "a_3");
	field_copy_tests_validate_diff_ptrs(a_4_copy, a_4, "a_4");

	/* make sure copied fields share the same types */
	field_copy_tests_validate_same_type(strct_copy, strct_type, "strct");
	field_copy_tests_validate_same_type(len_copy, len_type, "len");
	field_copy_tests_validate_same_type(fp_copy, fp_type, "fp");
	field_copy_tests_validate_same_type(e_int_copy, e_int_type, "e_int");
	field_copy_tests_validate_same_type(e_copy, e_type, "e");
	field_copy_tests_validate_same_type(v_copy, v_type, "v");
	field_copy_tests_validate_same_type(v_selected_copy, v_label2_seq_type,
		"v_selected");
	field_copy_tests_validate_same_type(v_selected_0_copy, v_label2_type,
		"v_selected_0");
	field_copy_tests_validate_same_type(v_selected_1_copy, v_label2_type,
		"v_selected_1");
	field_copy_tests_validate_same_type(v_selected_2_copy, v_label2_type,
		"v_selected_2");
	field_copy_tests_validate_same_type(v_selected_3_copy, v_label2_type,
		"v_selected_3");
	field_copy_tests_validate_same_type(v_selected_4_copy, v_label2_type,
		"v_selected_4");
	field_copy_tests_validate_same_type(v_selected_5_copy, v_label2_type,
		"v_selected_5");
	field_copy_tests_validate_same_type(v_selected_6_copy, v_label2_type,
		"v_selected_6");
	field_copy_tests_validate_same_type(a_copy, v_label1_array_type, "a");
	field_copy_tests_validate_same_type(a_0_copy, v_label1_type, "a_0");
	field_copy_tests_validate_same_type(a_1_copy, v_label1_type, "a_1");
	field_copy_tests_validate_same_type(a_2_copy, v_label1_type, "a_2");
	field_copy_tests_validate_same_type(a_3_copy, v_label1_type, "a_3");
	field_copy_tests_validate_same_type(a_4_copy, v_label1_type, "a_4");

	/* validate len copy */
	ret = bt_field_unsigned_integer_get_value(len_copy, &uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 7,
		"bt_field_copy creates a valid integer field copy");

	/* validate fp copy */
	ret = bt_field_floating_point_get_value(fp_copy, &double_val);
	assert(!ret);
	ok(double_val == 3.14,
		"bt_field_copy creates a valid floating point number field copy");

	/* validate s copy */
	str_val = bt_field_string_get_value(s_copy);
	ok(str_val && !strcmp(str_val, "btbt"),
		"bt_field_copy creates a valid string field copy");

	/* validate e_int copy */
	ret = bt_field_unsigned_integer_get_value(e_int_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 23,
		"bt_field_copy creates a valid enum's integer field copy");

	/* validate e copy */
	e_iter = bt_field_enumeration_get_mappings(e_copy);
	(void) bt_field_type_enumeration_mapping_iterator_next(e_iter);
	(void) bt_field_type_enumeration_mapping_iterator_get_signed(e_iter,
		&str_val, NULL, NULL);
	ok(str_val && !strcmp(str_val, "LABEL2"),
		"bt_field_copy creates a valid enum field copy");

	/* validate v_selected copy */
	v_selected_copy_len = bt_field_sequence_get_length(v_selected);
	assert(v_selected_copy_len);
	ret = bt_field_unsigned_integer_get_value(v_selected_copy_len,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 7,
		"bt_field_copy creates a sequence field copy with the proper length");
	bt_put(v_selected_copy_len);
	v_selected_copy_len = NULL;

	/* validate v_selected copy fields */
	ret = bt_field_unsigned_integer_get_value(v_selected_0_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 7,
		"bt_field_copy creates a valid sequence field element copy (v_selected_0)");
	ret = bt_field_unsigned_integer_get_value(v_selected_1_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 6,
		"bt_field_copy creates a valid sequence field element copy (v_selected_1)");
	ret = bt_field_unsigned_integer_get_value(v_selected_2_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 5,
		"bt_field_copy creates a valid sequence field element copy (v_selected_2)");
	ret = bt_field_unsigned_integer_get_value(v_selected_3_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 4,
		"bt_field_copy creates a valid sequence field element copy (v_selected_3)");
	ret = bt_field_unsigned_integer_get_value(v_selected_4_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 3,
		"bt_field_copy creates a valid sequence field element copy (v_selected_4)");
	ret = bt_field_unsigned_integer_get_value(v_selected_5_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 2,
		"bt_field_copy creates a valid sequence field element copy (v_selected_5)");
	ret = bt_field_unsigned_integer_get_value(v_selected_6_copy,
		&uint64_t_val);
	assert(!ret);
	ok(uint64_t_val == 1,
		"bt_field_copy creates a valid sequence field element copy (v_selected_6)");

	/* validate a copy fields */
	str_val = bt_field_string_get_value(a_0_copy);
	ok(str_val && !strcmp(str_val, "a_0"),
		"bt_field_copy creates a valid array field element copy (a_0)");
	str_val = bt_field_string_get_value(a_1_copy);
	ok(str_val && !strcmp(str_val, "a_1"),
		"bt_field_copy creates a valid array field element copy (a_1)");
	str_val = bt_field_string_get_value(a_2_copy);
	ok(str_val && !strcmp(str_val, "a_2"),
		"bt_field_copy creates a valid array field element copy (a_2)");
	str_val = bt_field_string_get_value(a_3_copy);
	ok(str_val && !strcmp(str_val, "a_3"),
		"bt_field_copy creates a valid array field element copy (a_3)");
	str_val = bt_field_string_get_value(a_4_copy);
	ok(str_val && !strcmp(str_val, "a_4"),
		"bt_field_copy creates a valid array field element copy (a_4)");

	/* put everything */
	bt_put(len_type);
	bt_put(fp_type);
	bt_put(s_type);
	bt_put(e_int_type);
	bt_put(e_type);
	bt_put(v_type);
	bt_put(v_label1_type);
	bt_put(v_label1_array_type);
	bt_put(v_label2_type);
	bt_put(v_label2_seq_type);
	bt_put(strct_type);
	bt_put(len);
	bt_put(fp);
	bt_put(s);
	bt_put(e_int);
	bt_put(e);
	bt_put(v);
	bt_put(v_selected);
	bt_put(v_selected_0);
	bt_put(v_selected_1);
	bt_put(v_selected_2);
	bt_put(v_selected_3);
	bt_put(v_selected_4);
	bt_put(v_selected_5);
	bt_put(v_selected_6);
	bt_put(a);
	bt_put(a_0);
	bt_put(a_1);
	bt_put(a_2);
	bt_put(a_3);
	bt_put(a_4);
	bt_put(strct);
	bt_put(len_copy);
	bt_put(fp_copy);
	bt_put(s_copy);
	bt_put(e_int_copy);
	bt_put(e_copy);
	bt_put(v_copy);
	bt_put(v_selected_copy);
	bt_put(v_selected_0_copy);
	bt_put(v_selected_1_copy);
	bt_put(v_selected_2_copy);
	bt_put(v_selected_3_copy);
	bt_put(v_selected_4_copy);
	bt_put(v_selected_5_copy);
	bt_put(v_selected_6_copy);
	bt_put(a_copy);
	bt_put(a_0_copy);
	bt_put(a_1_copy);
	bt_put(a_2_copy);
	bt_put(a_3_copy);
	bt_put(a_4_copy);
	bt_put(strct_copy);
	bt_put(e_iter);
}

static
void type_field_tests()
{
	struct bt_field *uint_12;
	struct bt_field *int_16;
	struct bt_field *string;
	struct bt_field_type *composite_structure_type;
	struct bt_field_type *structure_seq_type;
	struct bt_field_type *string_type;
	struct bt_field_type *sequence_type;
	struct bt_field_type *uint_8_type;
	struct bt_field_type *int_16_type;
	struct bt_field_type *uint_12_type =
		bt_field_type_integer_create(12);
	struct bt_field_type *enumeration_type;
	struct bt_field_type *returned_type;
	const char *ret_string;

	ok(uint_12_type, "Create an unsigned integer type");
	ok(bt_field_type_integer_set_base(uint_12_type,
		BT_INTEGER_BASE_BINARY) == 0,
		"Set integer type's base as binary");
	ok(bt_field_type_integer_set_base(uint_12_type,
		BT_INTEGER_BASE_DECIMAL) == 0,
		"Set integer type's base as decimal");
	ok(bt_field_type_integer_set_base(uint_12_type,
		BT_INTEGER_BASE_UNKNOWN),
		"Reject integer type's base set as unknown");
	ok(bt_field_type_integer_set_base(uint_12_type,
		BT_INTEGER_BASE_OCTAL) == 0,
		"Set integer type's base as octal");
	ok(bt_field_type_integer_set_base(uint_12_type,
		BT_INTEGER_BASE_HEXADECIMAL) == 0,
		"Set integer type's base as hexadecimal");
	ok(bt_field_type_integer_set_base(uint_12_type, 457417),
		"Reject unknown integer base value");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 952835) == 0,
		"Set integer type signedness to signed");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 0) == 0,
		"Set integer type signedness to unsigned");
	ok(bt_field_type_integer_get_size(uint_12_type) == 12,
		"bt_field_type_integer_get_size returns a correct value");
	ok(bt_ctf_field_type_integer_get_signed(uint_12_type) == 0,
		"bt_field_type_integer_get_signed returns a correct value for unsigned types");

	ok(bt_field_type_set_byte_order(NULL,
		BT_BYTE_ORDER_LITTLE_ENDIAN) < 0,
		"bt_field_type_set_byte_order handles NULL correctly");
	ok(bt_field_type_set_byte_order(uint_12_type,
		(enum bt_byte_order) 42) < 0,
		"bt_field_type_set_byte_order rejects invalid values");
	ok(bt_field_type_set_byte_order(uint_12_type,
		BT_BYTE_ORDER_LITTLE_ENDIAN) == 0,
		"Set an integer's byte order to little endian");
	ok(bt_field_type_set_byte_order(uint_12_type,
		BT_BYTE_ORDER_BIG_ENDIAN) == 0,
		"Set an integer's byte order to big endian");
	ok(bt_field_type_get_byte_order(uint_12_type) ==
		BT_BYTE_ORDER_BIG_ENDIAN,
		"bt_field_type_get_byte_order returns a correct value");

	ok(bt_field_type_get_type_id(uint_12_type) ==
		BT_FIELD_TYPE_ID_INTEGER,
		"bt_field_type_get_type_id returns a correct value with an integer type");

	ok(bt_field_type_integer_get_base(uint_12_type) ==
		BT_INTEGER_BASE_HEXADECIMAL,
		"bt_field_type_integer_get_base returns a correct value");

	ok(bt_field_type_integer_set_encoding(NULL,
		BT_STRING_ENCODING_ASCII) < 0,
		"bt_field_type_integer_set_encoding handles NULL correctly");
	ok(bt_field_type_integer_set_encoding(uint_12_type,
		(enum bt_string_encoding) 123) < 0,
		"bt_field_type_integer_set_encoding handles invalid encodings correctly");
	ok(bt_field_type_integer_set_encoding(uint_12_type,
		BT_STRING_ENCODING_UTF8) == 0,
		"Set integer type encoding to UTF8");
	ok(bt_field_type_integer_get_encoding(uint_12_type) ==
		BT_STRING_ENCODING_UTF8,
		"bt_field_type_integer_get_encoding returns a correct value");

	int_16_type = bt_field_type_integer_create(16);
	assert(int_16_type);
	ok(!bt_ctf_field_type_integer_set_signed(int_16_type, 1),
		"Set signedness of 16 bit integer to true");
	ok(bt_ctf_field_type_integer_get_signed(int_16_type) == 1,
		"bt_field_type_integer_get_signed returns a correct value for signed types");
	uint_8_type = bt_field_type_integer_create(8);
	sequence_type =
		bt_field_type_sequence_create(int_16_type, "seq_len");
	ok(sequence_type, "Create a sequence of int16_t type");
	ok(bt_field_type_get_type_id(sequence_type) ==
		BT_FIELD_TYPE_ID_SEQUENCE,
		"bt_field_type_get_type_id returns a correct value with a sequence type");

	ret_string = bt_field_type_sequence_get_length_field_name(
		sequence_type);
	ok(!strcmp(ret_string, "seq_len"),
		"bt_field_type_sequence_get_length_field_name returns the correct value");
	returned_type = bt_field_type_sequence_get_element_type(
		sequence_type);
	ok(returned_type == int_16_type,
		"bt_field_type_sequence_get_element_type returns the correct type");
	bt_put(returned_type);

	string_type = bt_field_type_string_create();
	ok(string_type, "Create a string type");
	ok(bt_field_type_string_set_encoding(string_type,
		BT_STRING_ENCODING_NONE),
		"Reject invalid \"None\" string encoding");
	ok(bt_field_type_string_set_encoding(string_type,
		42),
		"Reject invalid string encoding");
	ok(bt_field_type_string_set_encoding(string_type,
		BT_STRING_ENCODING_ASCII) == 0,
		"Set string encoding to ASCII");

	ok(bt_field_type_string_get_encoding(string_type) ==
		BT_STRING_ENCODING_ASCII,
		"bt_field_type_string_get_encoding returns the correct value");

	structure_seq_type = bt_field_type_structure_create();
	ok(bt_field_type_get_type_id(structure_seq_type) ==
		BT_FIELD_TYPE_ID_STRUCT,
		"bt_field_type_get_type_id returns a correct value with a structure type");
	ok(structure_seq_type, "Create a structure type");
	ok(bt_field_type_structure_add_field(structure_seq_type,
		uint_8_type, "seq_len") == 0,
		"Add a uint8_t type to a structure");
	ok(bt_field_type_structure_add_field(structure_seq_type,
		sequence_type, "a_sequence") == 0,
		"Add a sequence type to a structure");

	ok(bt_field_type_structure_get_field_count(structure_seq_type) == 2,
		"bt_field_type_structure_get_field_count returns a correct value");

	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		NULL, &returned_type, 1) == 0,
		"bt_field_type_structure_get_field handles a NULL name correctly");
	bt_put(returned_type);
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, NULL, 1) == 0,
		"bt_field_type_structure_get_field handles a NULL return type correctly");
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, &returned_type, 1) == 0,
		"bt_field_type_structure_get_field returns a field");
	ok(!strcmp(ret_string, "a_sequence"),
		"bt_field_type_structure_get_field returns a correct field name");
	ok(returned_type == sequence_type,
		"bt_field_type_structure_get_field returns a correct field type");
	bt_put(returned_type);

	returned_type = bt_field_type_structure_get_field_type_by_name(
		structure_seq_type, "a_sequence");
	ok(returned_type == sequence_type,
		"bt_field_type_structure_get_field_type_by_name returns the correct field type");
	bt_put(returned_type);

	composite_structure_type = bt_field_type_structure_create();
	ok(bt_field_type_structure_add_field(composite_structure_type,
		string_type, "a_string") == 0,
		"Add a string type to a structure");
	ok(bt_field_type_structure_add_field(composite_structure_type,
		structure_seq_type, "inner_structure") == 0,
		"Add a structure type to a structure");

	returned_type = bt_field_type_structure_get_field_type_by_name(
		structure_seq_type, "a_sequence");
	ok(returned_type == sequence_type,
		"bt_field_type_structure_get_field_type_by_name returns a correct type");
	bt_put(returned_type);

	int_16 = bt_field_create(int_16_type);
	ok(int_16, "Instanciate a signed 16-bit integer");
	uint_12 = bt_field_create(uint_12_type);
	ok(uint_12, "Instanciate an unsigned 12-bit integer");
	returned_type = bt_field_get_type(int_16);
	ok(returned_type == int_16_type,
		"bt_field_get_type returns the correct type");

	/* Can't modify types after instanciating them */
	ok(bt_field_type_integer_set_base(uint_12_type,
		BT_INTEGER_BASE_DECIMAL),
		"Check an integer type' base can't be modified after instanciation");
	ok(bt_ctf_field_type_integer_set_signed(uint_12_type, 0),
		"Check an integer type's signedness can't be modified after instanciation");

	/* Check overflows are properly tested for */
	ok(bt_field_signed_integer_set_value(int_16, -32768) == 0,
		"Check -32768 is allowed for a signed 16-bit integer");
	ok(bt_field_signed_integer_set_value(int_16, 32767) == 0,
		"Check 32767 is allowed for a signed 16-bit integer");
	ok(bt_field_signed_integer_set_value(int_16, -42) == 0,
		"Check -42 is allowed for a signed 16-bit integer");

	ok(bt_field_unsigned_integer_set_value(uint_12, 4095) == 0,
		"Check 4095 is allowed for an unsigned 12-bit integer");
	ok(bt_field_unsigned_integer_set_value(uint_12, 0) == 0,
		"Check 0 is allowed for an unsigned 12-bit integer");

	string = bt_field_create(string_type);
	ok(string, "Instanciate a string field");
	ok(bt_field_string_set_value(string, "A value") == 0,
		"Set a string's value");

	enumeration_type = bt_field_type_enumeration_create(uint_12_type);
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
void packet_resize_test(struct bt_stream_class *stream_class,
		struct bt_stream *stream, struct bt_ctf_clock *clock)
{
	/*
	 * Append enough events to force the underlying packet to be resized.
	 * Also tests that a new event can be declared after a stream has been
	 * instantiated and used/flushed.
	 */
	int ret = 0;
	int i;
	struct bt_event_class *event_class = bt_event_class_create(
		"Spammy_Event");
	struct bt_field_type *integer_type =
		bt_field_type_integer_create(17);
	struct bt_field_type *string_type =
		bt_field_type_string_create();
	struct bt_event *event = NULL;
	struct bt_field *ret_field = NULL;
	struct bt_field_type *ret_field_type = NULL;
	uint64_t ret_uint64;
	int events_appended = 0;
	struct bt_field *packet_context = NULL,
		*packet_context_field = NULL, *stream_event_context = NULL;
	struct bt_field_type *ep_field_1_type = NULL;
	struct bt_field_type *ep_a_string_type = NULL;
	struct bt_field_type *ep_type = NULL;

	ret |= bt_event_class_add_field(event_class, integer_type,
		"field_1");
	ret |= bt_event_class_add_field(event_class, string_type,
		"a_string");
	ret |= bt_stream_class_add_event_class(stream_class, event_class);
	ok(ret == 0, "Add a new event class to a stream class after writing an event");
	if (ret) {
		goto end;
	}

	/*
	 * bt_stream_class_add_event_class() copies the field types
	 * of event_class, so we retrieve the new ones to create the
	 * appropriate fields.
	 */
	ep_type = bt_event_class_get_payload_type(event_class);
	assert(ep_type);
	ep_field_1_type = bt_field_type_structure_get_field_type_by_name(
		ep_type, "field_1");
	assert(ep_field_1_type);
	ep_a_string_type = bt_field_type_structure_get_field_type_by_name(
		ep_type, "a_string");
	assert(ep_a_string_type);

	event = bt_event_create(event_class);
	ret_field = bt_event_get_payload_by_index(event, 0);
	ret_field_type = bt_field_get_type(ret_field);
	ok(bt_field_type_compare(ret_field_type, integer_type) == 0,
		"bt_event_get_payload_by_index returns a correct field");
	bt_put(ret_field_type);
	bt_put(ret_field);
	bt_put(event);

	for (i = 0; i < packet_resize_test_length; i++) {
		event = bt_event_create(event_class);
		struct bt_field *integer =
			bt_field_create(ep_field_1_type);
		struct bt_field *string =
			bt_field_create(ep_a_string_type);

		ret |= bt_ctf_clock_set_time(clock, ++current_time);
		ret |= bt_field_unsigned_integer_set_value(integer, i);
		ret |= bt_event_set_payload(event, "field_1",
			integer);
		bt_put(integer);
		ret |= bt_field_string_set_value(string, "This is a test");
		ret |= bt_event_set_payload(event, "a_string",
			string);
		bt_put(string);

		/* Populate stream event context */
		stream_event_context =
			bt_event_get_stream_event_context(event);
		integer = bt_field_structure_get_field_by_name(stream_event_context,
			"common_event_context");
		BT_PUT(stream_event_context);
		ret |= bt_field_unsigned_integer_set_value(integer,
			i % 42);
		bt_put(integer);

		ret |= bt_stream_append_event(stream, event);
		bt_put(event);

		if (ret) {
			break;
		}
	}

	events_appended = !!(i == packet_resize_test_length);
	ret = bt_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 0,
		"bt_stream_get_discarded_events_count returns a correct number of discarded events when none were discarded");
	bt_stream_append_discarded_events(stream, 1000);
	ret = bt_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 1000,
		"bt_stream_get_discarded_events_count returns a correct number of discarded events when some were discarded");

end:
	ok(events_appended, "Append 100 000 events to a stream");

	/*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
	packet_context = bt_stream_get_packet_context(stream);
	packet_context_field = bt_field_structure_get_field_by_name(packet_context,
		"custom_packet_context_field");
	bt_field_unsigned_integer_set_value(packet_context_field, 2);

	ok(bt_stream_flush(stream) == 0,
		"Flush a stream that forces a packet resize");
	ret = bt_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 1000,
		"bt_stream_get_discarded_events_count returns a correct number of discarded events after a flush");
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
	struct bt_trace *trace = NULL, *ret_trace = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_stream *stream = NULL;

	trace = bt_ctf_writer_get_trace(writer);
	if (!trace) {
		diag("Failed to get trace from writer");
		ret = -1;
		goto end;
	}

	stream_class = bt_stream_class_create("empty_stream");
	if (!stream_class) {
		diag("Failed to create stream class");
		ret = -1;
		goto end;
	}

	ret = bt_stream_class_set_packet_context_type(stream_class, NULL);
	assert(ret == 0);
	ret = bt_stream_class_set_event_header_type(stream_class, NULL);
	assert(ret == 0);

	ok(bt_stream_class_get_trace(stream_class) == NULL,
		"bt_stream_class_get_trace returns NULL when stream class is orphaned");

	stream = bt_ctf_writer_create_stream(writer, stream_class);
	if (!stream) {
		diag("Failed to create writer stream");
		ret = -1;
		goto end;
	}

	ret_trace = bt_stream_class_get_trace(stream_class);
	ok(ret_trace == trace,
		"bt_stream_class_get_trace returns the correct trace after a stream has been created");
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
	struct bt_stream_class *stream_class = NULL;
	struct bt_stream *stream = NULL;
	struct bt_field_type *integer_type = NULL,
		*sequence_type = NULL, *event_header_type = NULL;
	struct bt_field *integer = NULL, *sequence = NULL,
		*event_header = NULL, *packet_header = NULL;
	struct bt_event_class *event_class = NULL;
	struct bt_event *event = NULL;

	stream_class = bt_stream_class_create("custom_event_header_stream");
	if (!stream_class) {
		fail("Failed to create stream class");
		goto end;
	}

	ret = bt_stream_class_set_clock(stream_class, clock);
	if (ret) {
		fail("Failed to set stream class clock");
		goto end;
	}

	/*
	 * Customize event header to add an "seq_len" integer member
	 * which will be used as the length of a sequence in an event of this
	 * stream.
	 */
	event_header_type = bt_stream_class_get_event_header_type(
		stream_class);
	if (!event_header_type) {
		fail("Failed to get event header type");
		goto end;
	}

	integer_type = bt_field_type_integer_create(13);
	if (!integer_type) {
		fail("Failed to create length integer type");
		goto end;
	}

	ret = bt_field_type_structure_add_field(event_header_type,
		integer_type, "seq_len");
	if (ret) {
		fail("Failed to add a new field to stream event header");
		goto end;
	}

	event_class = bt_event_class_create("sequence_event");
	if (!event_class) {
		fail("Failed to create event class");
		goto end;
	}

	/*
	 * This event's payload will contain a sequence which references
	 * stream.event.header.seq_len as its length field.
	 */
	sequence_type = bt_field_type_sequence_create(integer_type,
		"stream.event.header.seq_len");
	if (!sequence_type) {
		fail("Failed to create a sequence");
		goto end;
	}

	ret = bt_event_class_add_field(event_class, sequence_type,
		"some_sequence");
	if (ret) {
		fail("Failed to add a sequence to an event class");
		goto end;
	}

	ret = bt_stream_class_add_event_class(stream_class, event_class);
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
	packet_header = bt_stream_get_packet_header(stream);
	if (!packet_header) {
		fail("Failed to get stream packet header");
		goto end;
	}

	integer = bt_field_structure_get_field_by_name(packet_header,
		"custom_trace_packet_header_field");
	if (!integer) {
		fail("Failed to retrieve custom_trace_packet_header_field");
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(integer, 3487);
	if (ret) {
		fail("Failed to set custom_trace_packet_header_field value");
		goto end;
	}
	bt_put(integer);

	event = bt_event_create(event_class);
	if (!event) {
		fail("Failed to create event");
		goto end;
	}

	event_header = bt_event_get_header(event);
	if (!event_header) {
		fail("Failed to get event header");
		goto end;
	}

	integer = bt_field_structure_get_field_by_name(event_header,
		"seq_len");
	if (!integer) {
		fail("Failed to get seq_len field from event header");
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(integer, 2);
	if (ret) {
		fail("Failed to set seq_len value in event header");
		goto end;
	}

	/* Populate both sequence integer fields */
	sequence = bt_event_get_payload(event, "some_sequence");
	if (!sequence) {
		fail("Failed to retrieve sequence from event");
		goto end;
	}

	ret = bt_field_sequence_set_length(sequence, integer);
	if (ret) {
		fail("Failed to set sequence length");
		goto end;
	}
	bt_put(integer);

	for (i = 0; i < 2; i++) {
		integer = bt_field_sequence_get_field(sequence, i);
		if (ret) {
			fail("Failed to retrieve sequence element");
			goto end;
		}

		ret = bt_field_unsigned_integer_set_value(integer, i);
		if (ret) {
			fail("Failed to set sequence element value");
			goto end;
		}

		bt_put(integer);
		integer = NULL;
	}

	ret = bt_stream_append_event(stream, event);
	if (ret) {
		fail("Failed to append event to stream");
		goto end;
	}

	ret = bt_stream_flush(stream);
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
	struct bt_stream_class *stream_class = NULL;
	struct bt_stream *stream = NULL,
		*ret_stream = NULL;
	struct bt_event_class *event_class = NULL;
	struct bt_event *event = NULL;
	struct bt_field_type *integer_type = NULL;
	struct bt_field *integer = NULL;

	stream_class = bt_stream_class_create("event_before_stream_test");
	if (!stream_class) {
		diag("Failed to create stream class");
		ret = -1;
		goto end;
	}

	ret = bt_stream_class_set_clock(stream_class, clock);
	if (ret) {
		diag("Failed to set stream class clock");
		goto end;
	}

	event_class = bt_event_class_create("some_event_class_name");
	integer_type = bt_field_type_integer_create(32);
	if (!integer_type) {
		diag("Failed to create integer field type");
		ret = -1;
		goto end;
	}

	ret = bt_event_class_add_field(event_class, integer_type,
		"integer_field");
	if (ret) {
		diag("Failed to add field to event class");
		goto end;
	}

	ret = bt_stream_class_add_event_class(stream_class,
		event_class);
	if (ret) {
		diag("Failed to add event class to stream class");
	}

	event = bt_event_create(event_class);
	if (!event) {
		diag("Failed to create event");
		ret = -1;
		goto end;
	}

	integer = bt_event_get_payload_by_index(event, 0);
	if (!integer) {
		diag("Failed to get integer field payload from event");
		ret = -1;
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(integer, 1234);
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

	ret = bt_stream_append_event(stream, event);
	if (ret) {
		diag("Failed to append event to stream");
		goto end;
	}

	ret_stream = bt_event_get_stream(event);
	ok(ret_stream == stream,
		"bt_event_get_stream returns an event's stream after it has been appended");
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
}

static
void append_existing_event_class(struct bt_stream_class *stream_class)
{
	struct bt_event_class *event_class;

	event_class = bt_event_class_create("Simple Event");
	assert(event_class);
	ok(bt_stream_class_add_event_class(stream_class, event_class) == 0,
		"two event classes with the same name may cohabit within the same stream class");
	bt_put(event_class);

	event_class = bt_event_class_create("different name, ok");
	assert(event_class);
	assert(!bt_event_class_set_id(event_class, 13));
	ok(bt_stream_class_add_event_class(stream_class, event_class),
		"two event classes with the same ID cannot cohabit within the same stream class");
	bt_put(event_class);
}

static
struct bt_event_class *create_minimal_event_class(void)
{
	struct bt_event_class *ec = NULL;
	struct bt_field_type *int_ft = NULL;
	int ret;

	int_ft = bt_field_type_integer_create(23);
	assert(int_ft);
	ec = bt_event_class_create("minimal");
	assert(ec);
	ret = bt_event_class_add_field(ec, int_ft, "field");
	assert(!ret);
	BT_PUT(int_ft);

	return ec;
}

static
void test_create_writer_vs_non_writer_mode(void)
{
	int ret;
	gchar *trace_path;
	const char *writer_stream_name = "writer stream instance";
	struct bt_ctf_writer *writer = NULL;
	struct bt_trace *writer_trace = NULL;
	struct bt_stream_class *writer_sc = NULL;
	struct bt_stream *writer_stream = NULL;
	struct bt_stream *writer_stream2 = NULL;
	struct bt_stream *packet_stream = NULL;
	struct bt_trace *non_writer_trace = NULL;
	struct bt_stream_class *non_writer_sc = NULL;
	struct bt_stream *non_writer_stream = NULL;
	struct bt_stream *non_writer_stream2 = NULL;
	struct bt_event_class *writer_ec = NULL;
	struct bt_event_class *non_writer_ec = NULL;
	struct bt_event *event = NULL;
	struct bt_event *event2 = NULL;
	struct bt_field_type *empty_struct_ft = NULL;
	struct bt_field *int_field = NULL;
	struct bt_ctf_clock *writer_clock = NULL;
	struct bt_clock_class *non_writer_clock_class = NULL;
	struct bt_packet *packet = NULL;
	struct bt_packet *packet2 = NULL;

	trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);
	if (!bt_mkdtemp(trace_path)) {
		perror("# perror");
	}

	/* Create empty structure field type (event header) */
	empty_struct_ft = bt_field_type_structure_create();
	assert(empty_struct_ft);

	/* Create writer, writer stream class, stream, and clock */
	writer = bt_ctf_writer_create(trace_path);
	assert(writer);
	writer_clock = bt_ctf_clock_create("writer_clock");
	assert(writer_clock);
	ret = bt_ctf_writer_add_clock(writer, writer_clock);
	assert(!ret);
	ret = bt_ctf_writer_set_byte_order(writer, BT_BYTE_ORDER_LITTLE_ENDIAN);
	assert(!ret);
	writer_trace = bt_ctf_writer_get_trace(writer);
	ok(writer_trace, "bt_ctf_writer_get_trace() returns a trace");
	writer_sc = bt_stream_class_create("writer_sc");
	assert(writer_sc);
	ret = bt_stream_class_set_event_header_type(writer_sc,
		empty_struct_ft);
	assert(!ret);
	ret = bt_stream_class_set_clock(writer_sc, writer_clock);
	assert(!ret);
	ret = bt_trace_add_stream_class(writer_trace, writer_sc);
	assert(!ret);
	writer_stream = bt_stream_create(writer_sc, writer_stream_name);
	assert(writer_stream);
	ok(!strcmp(bt_stream_get_name(writer_stream), writer_stream_name),
		"bt_stream_get_name() returns the stream's name");

	/* Create non-writer trace, stream class, stream, and clock */
	non_writer_trace = bt_trace_create();
	assert(non_writer_trace);
	non_writer_sc = bt_stream_class_create("nonwriter_sc");
	assert(non_writer_sc);
	ret = bt_stream_class_set_event_header_type(non_writer_sc,
		empty_struct_ft);
	assert(!ret);
	ret = bt_stream_class_set_packet_context_type(non_writer_sc, NULL);
	assert(!ret);
	ret = bt_trace_add_stream_class(non_writer_trace, non_writer_sc);
	assert(!ret);
	non_writer_stream = bt_stream_create(non_writer_sc, NULL);
	assert(non_writer_stream);
	non_writer_clock_class =
		bt_clock_class_create("non_writer_clock_class",
			1000000000);
	assert(non_writer_clock_class);
	ret = bt_trace_add_clock_class(non_writer_trace,
		non_writer_clock_class);
	assert(!ret);

	/* Create event class and event */
	writer_ec = create_minimal_event_class();
	assert(writer_ec);
	ret = bt_stream_class_add_event_class(writer_sc, writer_ec);
	assert(!ret);
	event = bt_event_create(writer_ec);
	assert(event);
	int_field = bt_event_get_payload_by_index(event, 0);
	assert(int_field);
	bt_field_unsigned_integer_set_value(int_field, 17);

	/*
	 * Verify non-writer stream: it should be impossible to append
	 * an event to it.
	 */
	ok(bt_stream_append_event(non_writer_stream, event),
		"bt_stream_append_event() fails with a non-writer stream");

	/*
	 * Verify writer stream: it should be possible to append an
	 * event to it.
	 */
	ok(!bt_stream_append_event(writer_stream, event),
		"bt_stream_append_event() succeeds with a writer stream");

	/*
	 * It should be possible to create a packet from a non-writer
	 * stream, but not from a writer stream.
	 */
	packet = bt_packet_create(non_writer_stream);
	ok(packet, "bt_packet_create() succeeds with a non-writer stream");
	packet_stream = bt_packet_get_stream(packet);
	ok(packet_stream == non_writer_stream,
		"bt_packet_get_stream() returns the correct stream");

	/*
	 * It should not be possible to append an event associated to
	 * a stream to a different stream.
	 */
	writer_stream2 = bt_stream_create(writer_sc, "zoo");
	assert(writer_stream2);

	/*
	 * It should be possible to set the packet of a fresh event, as
	 * long as the originating stream classes are the same.
	 */
	event2 = bt_event_create(writer_ec);
	assert(event2);
	non_writer_ec = create_minimal_event_class();
	assert(non_writer_ec);
	ret = bt_stream_class_add_event_class(non_writer_sc, non_writer_ec);
	assert(!ret);
	BT_PUT(event2);
	event2 = bt_event_create(non_writer_ec);
	assert(event2);
	ok(!bt_event_set_packet(event2, packet),
		"bt_event_set_packet() succeeds when the event's and the packet's stream class are the same");

	/*
	 * It should be possible to set a packet created from the same
	 * stream to an event with an existing packet.
	 */
	packet2 = bt_packet_create(non_writer_stream);
	assert(packet2);
	ok(!bt_event_set_packet(event2, packet2),
		"bt_event_set_packet() succeeds when the event's current packet has the same stream");
	BT_PUT(packet2);

	/*
	 * It should not be possible to set a packet created from a
	 * different stream to an event with an existing packet.
	 */
	non_writer_stream2 = bt_stream_create(non_writer_sc, "rj45");
	assert(non_writer_stream2);
	packet2 = bt_packet_create(non_writer_stream);
	assert(packet2);

	bt_put(writer);
	bt_put(writer_trace);
	bt_put(writer_sc);
	bt_put(writer_stream);
	bt_put(writer_stream2);
	bt_put(non_writer_trace);
	bt_put(non_writer_sc);
	bt_put(non_writer_stream);
	bt_put(non_writer_stream2);
	bt_put(packet_stream);
	bt_put(writer_ec);
	bt_put(non_writer_ec);
	bt_put(event);
	bt_put(event2);
	bt_put(int_field);
	bt_put(empty_struct_ft);
	bt_put(writer_clock);
	bt_put(non_writer_clock_class);
	bt_put(packet);
	bt_put(packet2);
	recursive_rmdir(trace_path);
	g_free(trace_path);
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

void test_set_clock_non_writer_stream_class(void)
{
	struct bt_ctf_clock *clock;
	struct bt_trace *trace;
	struct bt_stream_class *sc;
	int ret;

	clock = bt_ctf_clock_create("the_clock");
	assert(clock);

	trace = bt_trace_create();
	assert(trace);

	sc = bt_stream_class_create(NULL);
	assert(sc);

	ret = bt_stream_class_set_clock(sc, clock);
	assert(ret == 0);

	ret = bt_trace_add_stream_class(trace, sc);
	ok(ret < 0,
		"bt_trace_add_stream_class() fails with a stream class with a registered clock");

	bt_put(clock);
	bt_put(trace);
	bt_put(sc);
}

static
void test_static_trace(void)
{
	struct bt_trace *trace;
	struct bt_stream_class *stream_class;
	struct bt_stream_class *stream_class2;
	struct bt_stream *stream;
	struct bt_clock_class *clock_class;
	int ret;

	trace = bt_trace_create();
	assert(trace);
	stream_class = bt_stream_class_create(NULL);
	assert(stream_class);
	ret = bt_stream_class_set_packet_context_type(stream_class, NULL);
	assert(ret == 0);
	ret = bt_trace_add_stream_class(trace, stream_class);
	assert(ret == 0);
	stream = bt_stream_create(stream_class, "hello");
	ok(stream, "bt_stream_create() succeeds with a non-static trace");
	bt_put(stream);
	ok(!bt_trace_is_static(trace),
		"bt_trace_is_static() returns the expected value");
	ok(bt_trace_set_is_static(trace) == 0,
		"bt_trace_set_is_static() succeeds");
	ok(bt_trace_is_static(trace),
		"bt_trace_is_static() returns the expected value");
	clock_class = bt_clock_class_create("yes", 1000000000);
	assert(clock_class);
	stream_class2 = bt_stream_class_create(NULL);
	assert(stream_class2);
	ok(bt_trace_add_stream_class(trace, stream_class2),
		"bt_trace_add_stream_class() fails with a static trace");
	ok(bt_trace_add_clock_class(trace, clock_class),
		"bt_trace_add_clock_class() fails with a static trace");
	ok(!bt_stream_create(stream_class, "hello2"),
		"bt_stream_create() fails with a static trace");

	bt_put(trace);
	bt_put(stream_class);
	bt_put(stream_class2);
	bt_put(clock_class);
}

static
void trace_is_static_listener(struct bt_trace *trace, void *data)
{
	*((int *) data) |= 1;
}

static
void trace_listener_removed(struct bt_trace *trace, void *data)
{
	*((int *) data) |= 2;
}

static
void test_trace_is_static_listener(void)
{
	struct bt_trace *trace;
	int ret;
	int called1 = 0;
	int called2 = 0;
	int called3 = 0;
	int called4 = 0;
	int listener1_id;
	int listener2_id;
	int listener3_id;
	int listener4_id;

	trace = bt_trace_create();
	assert(trace);
	ret = bt_trace_add_is_static_listener(NULL,
		trace_is_static_listener, trace_listener_removed, &called1);
	ok(ret < 0, "bt_trace_add_is_static_listener() handles NULL (trace)");
	ret = bt_trace_add_is_static_listener(trace, NULL,
		trace_listener_removed, &called1);
	ok(ret < 0, "bt_trace_add_is_static_listener() handles NULL (listener)");
	listener1_id = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, &called1);
	ok(listener1_id >= 0, "bt_trace_add_is_static_listener() succeeds (1)");
	listener2_id = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, &called2);
	ok(listener2_id >= 0, "bt_trace_add_is_static_listener() succeeds (2)");
	listener3_id = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, &called3);
	ok(listener3_id >= 0, "bt_trace_add_is_static_listener() succeeds (3)");
	ret = bt_trace_remove_is_static_listener(NULL, 0);
	ok(ret < 0, "bt_trace_remove_is_static_listener() handles NULL (trace)");
	ret = bt_trace_remove_is_static_listener(trace, -2);
	ok(ret < 0, "bt_trace_remove_is_static_listener() handles invalid ID (negative)");
	ret = bt_trace_remove_is_static_listener(trace, 77);
	ok(ret < 0, "bt_trace_remove_is_static_listener() handles invalid ID (non existing)");
	ret = bt_trace_remove_is_static_listener(trace, listener2_id);
	ok(ret == 0, "bt_trace_remove_is_static_listener() succeeds");
	ok(called2 == 2, "bt_trace_remove_is_static_listener() calls the remove listener");
	listener4_id = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, NULL, &called4);
	ok(listener4_id >= 0, "bt_trace_add_is_static_listener() succeeds (4)");
	ok(called1 == 0, "\"trace is static\" listener not called before the trace is made static (1)");
	ok(called2 == 2, "\"trace is static\" listener not called before the trace is made static (2)");
	ok(called3 == 0, "\"trace is static\" listener not called before the trace is made static (3)");
	ok(called4 == 0, "\"trace is static\" listener not called before the trace is made static (4)");
	ret = bt_trace_set_is_static(trace);
	assert(ret == 0);
	ret = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, &called1);
	ok(ret < 0,
		"bt_trace_add_is_static_listener() fails when the trace is static");
	ok(called1 == 1, "\"trace is static\" listener called when the trace is made static (1)");
	ok(called2 == 2, "\"trace is static\" listener not called when the trace is made static (2)");
	ok(called3 == 1, "\"trace is static\" listener called when the trace is made static (3)");
	ok(called4 == 1, "\"trace is static\" listener called when the trace is made static (4)");
	called1 = 0;
	called2 = 0;
	called3 = 0;
	called4 = 0;
	bt_put(trace);
	ok(called1 == 2, "\"trace is static\" listener not called after the trace is put (1)");
	ok(called2 == 0, "\"trace is static\" listener not called after the trace is put (2)");
	ok(called3 == 2, "\"trace is static\" listener not called after the trace is put (3)");
	ok(called4 == 0, "\"trace is static\" listener not called after the trace is put (4)");
}

static
void test_trace_uuid(void)
{
	struct bt_trace *trace;
	const unsigned char uuid[] = {
		0x35, 0x92, 0x63, 0xab, 0xb4, 0xbe, 0x40, 0xb4,
		0xb2, 0x60, 0xd3, 0xf1, 0x3b, 0xb0, 0xd8, 0x59,
	};
	const unsigned char *ret_uuid;

	trace = bt_trace_create();
	assert(trace);
	ok(!bt_trace_get_uuid(trace),
		"bt_trace_get_uuid() returns NULL initially");
	ok(bt_trace_set_uuid(NULL, uuid),
		"bt_trace_set_uuid() handles NULL (trace)");
	ok(bt_trace_set_uuid(trace, NULL),
		"bt_trace_set_uuid() handles NULL (UUID)");
	ok(bt_trace_set_uuid(trace, uuid) == 0,
		"bt_trace_set_uuid() succeeds with a valid UUID");
	ret_uuid = bt_trace_get_uuid(trace);
	ok(ret_uuid, "bt_trace_get_uuid() returns a UUID");
	assert(ret_uuid);
	ok(memcmp(uuid, ret_uuid, 16) == 0,
		"bt_trace_get_uuid() returns the expected UUID");

	bt_put(trace);
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
	struct bt_clock_class *ret_clock_class;
	struct bt_stream_class *stream_class, *ret_stream_class;
	struct bt_stream *stream1;
	struct bt_stream *stream;
	const char *ret_string;
	const unsigned char *ret_uuid;
	unsigned char tmp_uuid[16] = { 0 };
	struct bt_field_type *packet_context_type,
		*packet_context_field_type,
		*packet_header_type,
		*packet_header_field_type,
		*integer_type,
		*stream_event_context_type,
		*ret_field_type,
		*event_header_field_type;
	struct bt_field *packet_header, *packet_header_field;
	struct bt_trace *trace;
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
	ok(writer, "bt_create succeeds in creating trace with path");

	ok(!bt_ctf_writer_get_trace(NULL),
		"bt_ctf_writer_get_trace correctly handles NULL");
	trace = bt_ctf_writer_get_trace(writer);
	ok(bt_trace_set_native_byte_order(trace, BT_BYTE_ORDER_NATIVE),
		"Cannot set a trace's byte order to BT_BYTE_ORDER_NATIVE");
	ok(bt_trace_set_native_byte_order(trace, BT_BYTE_ORDER_UNSPECIFIED),
		"Cannot set a trace's byte order to BT_BYTE_ORDER_UNSPECIFIED");
	ok(trace,
		"bt_ctf_writer_get_trace returns a bt_trace object");
	ok(bt_trace_set_native_byte_order(trace, BT_BYTE_ORDER_BIG_ENDIAN) == 0,
		"Set a trace's byte order to big endian");
	ok(bt_trace_get_native_byte_order(trace) == BT_BYTE_ORDER_BIG_ENDIAN,
		"bt_trace_get_native_byte_order returns a correct endianness");

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

	/* Test bt_trace_set_environment_field with an integer object */
	obj = bt_value_integer_create_init(23);
	assert(obj);
	ok(bt_trace_set_environment_field(NULL, "test_env_int_obj", obj),
		"bt_trace_set_environment_field handles a NULL trace correctly");
	ok(bt_trace_set_environment_field(trace, NULL, obj),
		"bt_trace_set_environment_field handles a NULL name correctly");
	ok(bt_trace_set_environment_field(trace, "test_env_int_obj", NULL),
		"bt_trace_set_environment_field handles a NULL value correctly");
	ok(!bt_trace_set_environment_field(trace, "test_env_int_obj", obj),
		"bt_trace_set_environment_field succeeds in adding an integer object");
	BT_PUT(obj);

	/* Test bt_trace_set_environment_field with a string object */
	obj = bt_value_string_create_init("the value");
	assert(obj);
	ok(!bt_trace_set_environment_field(trace, "test_env_str_obj", obj),
		"bt_trace_set_environment_field succeeds in adding a string object");
	BT_PUT(obj);

	/* Test bt_trace_set_environment_field_integer */
	ok(bt_trace_set_environment_field_integer(NULL, "test_env_int",
		-194875),
		"bt_trace_set_environment_field_integer handles a NULL trace correctly");
	ok(bt_trace_set_environment_field_integer(trace, NULL, -194875),
		"bt_trace_set_environment_field_integer handles a NULL name correctly");
	ok(!bt_trace_set_environment_field_integer(trace, "test_env_int",
		-164973),
		"bt_trace_set_environment_field_integer succeeds");

	/* Test bt_trace_set_environment_field_string */
	ok(bt_trace_set_environment_field_string(NULL, "test_env_str",
		"yeah"),
		"bt_trace_set_environment_field_string handles a NULL trace correctly");
	ok(bt_trace_set_environment_field_string(trace, NULL, "yeah"),
		"bt_trace_set_environment_field_string handles a NULL name correctly");
	ok(bt_trace_set_environment_field_string(trace, "test_env_str",
		NULL),
		"bt_trace_set_environment_field_string handles a NULL value correctly");
	ok(!bt_trace_set_environment_field_string(trace, "test_env_str",
		"oh yeah"),
		"bt_trace_set_environment_field_string succeeds");

	/* Test bt_trace_get_environment_field_count */
	ok(bt_trace_get_environment_field_count(trace) == 5,
		"bt_trace_get_environment_field_count returns a correct number of environment fields");

	/* Test bt_trace_get_environment_field_name */
	ret_string = bt_trace_get_environment_field_name_by_index(trace, 0);
	ok(ret_string && !strcmp(ret_string, "host"),
		"bt_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_trace_get_environment_field_name_by_index(trace, 1);
	ok(ret_string && !strcmp(ret_string, "test_env_int_obj"),
		"bt_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_trace_get_environment_field_name_by_index(trace, 2);
	ok(ret_string && !strcmp(ret_string, "test_env_str_obj"),
		"bt_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_trace_get_environment_field_name_by_index(trace, 3);
	ok(ret_string && !strcmp(ret_string, "test_env_int"),
		"bt_trace_get_environment_field_name returns a correct field name");
	ret_string = bt_trace_get_environment_field_name_by_index(trace, 4);
	ok(ret_string && !strcmp(ret_string, "test_env_str"),
		"bt_trace_get_environment_field_name returns a correct field name");

	/* Test bt_trace_get_environment_field_value */
	obj = bt_trace_get_environment_field_value_by_index(trace, 1);
	ret = bt_value_integer_get(obj, &ret_int64_t);
	ok(!ret && ret_int64_t == 23,
		"bt_trace_get_environment_field_value succeeds in getting an integer value");
	BT_PUT(obj);
	obj = bt_trace_get_environment_field_value_by_index(trace, 2);
	ret = bt_value_string_get(obj, &ret_string);
	ok(!ret && ret_string && !strcmp(ret_string, "the value"),
		"bt_trace_get_environment_field_value succeeds in getting a string value");
	BT_PUT(obj);

	/* Test bt_trace_get_environment_field_value_by_name */
	ok(!bt_trace_get_environment_field_value_by_name(trace, "oh oh"),
		"bt_trace_get_environment_field_value_by_name returns NULL or an unknown field name");
	obj = bt_trace_get_environment_field_value_by_name(trace,
		"test_env_str");
	ret = bt_value_string_get(obj, &ret_string);
	ok(!ret && ret_string && !strcmp(ret_string, "oh yeah"),
		"bt_trace_get_environment_field_value_by_name succeeds in getting an existing field");
	BT_PUT(obj);

	/* Test environment field replacement */
	ok(!bt_trace_set_environment_field_integer(trace, "test_env_int",
		654321),
		"bt_trace_set_environment_field_integer succeeds with an existing name");
	ok(bt_trace_get_environment_field_count(trace) == 5,
		"bt_trace_set_environment_field_integer with an existing key does not increase the environment size");
	obj = bt_trace_get_environment_field_value_by_index(trace, 3);
	ret = bt_value_integer_get(obj, &ret_int64_t);
	ok(!ret && ret_int64_t == 654321,
		"bt_trace_get_environment_field_value successfully replaces an existing field");
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
	stream_class = bt_stream_class_create("test_stream");

	ret_string = bt_stream_class_get_name(stream_class);
	ok(ret_string && !strcmp(ret_string, "test_stream"),
		"bt_stream_class_get_name returns a correct stream class name");

	ok(bt_stream_class_get_clock(stream_class) == NULL,
		"bt_stream_class_get_clock returns NULL when a clock was not set");
	ok(bt_stream_class_get_clock(NULL) == NULL,
		"bt_stream_class_get_clock handles NULL correctly");

	ok(stream_class, "Create stream class");
	ok(bt_stream_class_set_clock(stream_class, clock) == 0,
		"Set a stream class' clock");
	ret_clock = bt_stream_class_get_clock(stream_class);
	ok(ret_clock == clock,
		"bt_stream_class_get_clock returns a correct clock");
	bt_put(ret_clock);

	/* Test the event fields and event types APIs */
	type_field_tests();

	/* Test fields copying */
	field_copy_tests();

	ok(bt_stream_class_get_id(stream_class) < 0,
		"bt_stream_class_get_id returns an error when no id is set");
	ok(bt_stream_class_set_id(NULL, 123) < 0,
		"bt_stream_class_set_id handles NULL correctly");
	ok(bt_stream_class_set_id(stream_class, 123) == 0,
		"Set an stream class' id");
	ok(bt_stream_class_get_id(stream_class) == 123,
		"bt_stream_class_get_id returns the correct value");

	/* Validate default event header fields */
	ret_field_type = bt_stream_class_get_event_header_type(
		stream_class);
	ok(ret_field_type,
		"bt_stream_class_get_event_header_type returns an event header type");
	ok(bt_field_type_get_type_id(ret_field_type) == BT_FIELD_TYPE_ID_STRUCT,
		"Default event header type is a structure");
	event_header_field_type =
		bt_field_type_structure_get_field_type_by_name(
		ret_field_type, "id");
	ok(event_header_field_type,
		"Default event header type contains an \"id\" field");
	ok(bt_field_type_get_type_id(
		event_header_field_type) == BT_FIELD_TYPE_ID_INTEGER,
		"Default event header \"id\" field is an integer");
	bt_put(event_header_field_type);
	event_header_field_type =
		bt_field_type_structure_get_field_type_by_name(
		ret_field_type, "timestamp");
	ok(event_header_field_type,
		"Default event header type contains a \"timestamp\" field");
	ok(bt_field_type_get_type_id(
		event_header_field_type) == BT_FIELD_TYPE_ID_INTEGER,
		"Default event header \"timestamp\" field is an integer");
	bt_put(event_header_field_type);
	bt_put(ret_field_type);

	/* Add a custom trace packet header field */
	packet_header_type = bt_trace_get_packet_header_type(trace);
	ok(packet_header_type,
		"bt_trace_get_packet_header_type returns a packet header");
	ok(bt_field_type_get_type_id(packet_header_type) == BT_FIELD_TYPE_ID_STRUCT,
		"bt_trace_get_packet_header_type returns a packet header of type struct");
	ret_field_type = bt_field_type_structure_get_field_type_by_name(
		packet_header_type, "magic");
	ok(ret_field_type, "Default packet header type contains a \"magic\" field");
	bt_put(ret_field_type);
	ret_field_type = bt_field_type_structure_get_field_type_by_name(
		packet_header_type, "uuid");
	ok(ret_field_type, "Default packet header type contains a \"uuid\" field");
	bt_put(ret_field_type);
	ret_field_type = bt_field_type_structure_get_field_type_by_name(
		packet_header_type, "stream_id");
	ok(ret_field_type, "Default packet header type contains a \"stream_id\" field");
	bt_put(ret_field_type);

	packet_header_field_type = bt_field_type_integer_create(22);
	ok(!bt_field_type_structure_add_field(packet_header_type,
		packet_header_field_type, "custom_trace_packet_header_field"),
		"Added a custom trace packet header field successfully");

	ok(bt_trace_set_packet_header_type(NULL, packet_header_type) < 0,
		"bt_trace_set_packet_header_type handles a NULL trace correctly");
	ok(!bt_trace_set_packet_header_type(trace, packet_header_type),
		"Set a trace packet_header_type successfully");

	/* Add a custom field to the stream class' packet context */
	packet_context_type = bt_stream_class_get_packet_context_type(stream_class);
	ok(packet_context_type,
		"bt_stream_class_get_packet_context_type returns a packet context type.");
	ok(bt_field_type_get_type_id(packet_context_type) == BT_FIELD_TYPE_ID_STRUCT,
		"Packet context is a structure");

	ok(bt_stream_class_set_packet_context_type(NULL, packet_context_type),
		"bt_stream_class_set_packet_context_type handles a NULL stream class correctly");

	integer_type = bt_field_type_integer_create(32);
	ok(bt_stream_class_set_packet_context_type(stream_class,
		integer_type) < 0,
		"bt_stream_class_set_packet_context_type rejects a packet context that is not a structure");
	/* Create a "uint5_t" equivalent custom packet context field */
	packet_context_field_type = bt_field_type_integer_create(5);

	ret = bt_field_type_structure_add_field(packet_context_type,
		packet_context_field_type, "custom_packet_context_field");
	ok(ret == 0, "Packet context field added successfully");

	/* Define a stream event context containing a my_integer field. */
	stream_event_context_type = bt_field_type_structure_create();
	bt_field_type_structure_add_field(stream_event_context_type,
		integer_type, "common_event_context");

	ok(bt_stream_class_set_event_context_type(NULL,
		stream_event_context_type) < 0,
		"bt_stream_class_set_event_context_type handles a NULL stream_class correctly");
	ok(bt_stream_class_set_event_context_type(stream_class,
		integer_type) < 0,
		"bt_stream_class_set_event_context_type validates that the event context os a structure");

	ok(bt_stream_class_set_event_context_type(
		stream_class, stream_event_context_type) == 0,
		"Set a new stream event context type");
	ret_field_type = bt_stream_class_get_event_context_type(
		stream_class);
	ok(ret_field_type == stream_event_context_type,
		"bt_stream_class_get_event_context_type returns the correct field type.");
	bt_put(ret_field_type);

	/* Instantiate a stream and append events */
	ret = bt_ctf_writer_add_clock(writer, clock);
	assert(ret == 0);
	ok(bt_trace_get_stream_count(trace) == 0,
		"bt_trace_get_stream_count() succeeds and returns the correct value (0)");
	stream1 = bt_ctf_writer_create_stream(writer, stream_class);
	ok(stream1, "Instanciate a stream class from writer");
	ok(bt_trace_get_stream_count(trace) == 1,
		"bt_trace_get_stream_count() succeeds and returns the correct value (1)");
	stream = bt_trace_get_stream_by_index(trace, 0);
	ok(stream == stream1,
		"bt_trace_get_stream_by_index() succeeds and returns the correct value");
	BT_PUT(stream);

	/*
	 * Creating a stream through a writer adds the given stream
	 * class to the writer's trace, thus registering the stream
	 * class's clock to the trace.
	 */
	ok(bt_trace_get_clock_class_count(trace) == 1,
		"bt_trace_get_clock_class_count returns the correct number of clocks");
	ret_clock_class = bt_trace_get_clock_class_by_index(trace, 0);
	ok(strcmp(bt_clock_class_get_name(ret_clock_class),
		bt_ctf_clock_get_name(clock)) == 0,
		"bt_trace_get_clock_class returns the right clock instance");
	bt_put(ret_clock_class);
	ret_clock_class = bt_trace_get_clock_class_by_name(trace, clock_name);
	ok(strcmp(bt_clock_class_get_name(ret_clock_class),
		bt_ctf_clock_get_name(clock)) == 0,
		"bt_trace_get_clock_class returns the right clock instance");
	bt_put(ret_clock_class);
	ok(!bt_trace_get_clock_class_by_name(trace, "random"),
		"bt_trace_get_clock_by_name fails when the requested clock doesn't exist");

	ret_stream_class = bt_stream_get_class(stream1);
	ok(ret_stream_class,
		"bt_stream_get_class returns a stream class");
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
	packet_header_type = bt_trace_get_packet_header_type(trace);
	assert(packet_header_type);
	packet_context_type =
		bt_stream_class_get_packet_context_type(stream_class);
	assert(packet_context_type);
	stream_event_context_type =
		bt_stream_class_get_event_context_type(stream_class);
	assert(stream_event_context_type);

	/*
	 * Try to modify the packet context type after a stream has been
	 * created.
	 */
	ret = bt_field_type_structure_add_field(packet_header_type,
		packet_header_field_type, "should_fail");
	ok(ret < 0,
		"Trace packet header type can't be modified once a stream has been instanciated");

	/*
	 * Try to modify the packet context type after a stream has been
	 * created.
	 */
	ret = bt_field_type_structure_add_field(packet_context_type,
		packet_context_field_type, "should_fail");
	ok(ret < 0,
		"Packet context type can't be modified once a stream has been instanciated");

	/*
	 * Try to modify the stream event context type after a stream has been
	 * created.
	 */
	ret = bt_field_type_structure_add_field(stream_event_context_type,
		integer_type, "should_fail");
	ok(ret < 0,
		"Stream event context type can't be modified once a stream has been instanciated");

	/* Should fail after instanciating a stream (frozen) */
	ok(bt_stream_class_set_clock(stream_class, clock),
		"Changes to a stream class that was already instantiated fail");

	/* Populate the custom packet header field only once for all tests */
	ok(bt_stream_get_packet_header(NULL) == NULL,
		"bt_stream_get_packet_header handles NULL correctly");
	packet_header = bt_stream_get_packet_header(stream1);
	ok(packet_header,
		"bt_stream_get_packet_header returns a packet header");
	ret_field_type = bt_field_get_type(packet_header);
	ok(ret_field_type == packet_header_type,
		"Stream returns a packet header of the appropriate type");
	bt_put(ret_field_type);
	packet_header_field = bt_field_structure_get_field_by_name(packet_header,
		"custom_trace_packet_header_field");
	ok(packet_header_field,
		"Packet header structure contains a custom field with the appropriate name");
	ret_field_type = bt_field_get_type(packet_header_field);
	ok(bt_field_type_compare(ret_field_type, packet_header_field_type) == 0,
		"Custom packet header field is of the expected type");
	ok(!bt_field_unsigned_integer_set_value(packet_header_field,
		54321), "Set custom packet header value successfully");
	ok(bt_stream_set_packet_header(stream1, NULL) < 0,
		"bt_stream_set_packet_header handles a NULL packet header correctly");
	ok(bt_stream_set_packet_header(NULL, packet_header) < 0,
		"bt_stream_set_packet_header handles a NULL stream correctly");
	ok(bt_stream_set_packet_header(stream1, packet_header_field) < 0,
		"bt_stream_set_packet_header rejects a packet header of the wrong type");
	ok(!bt_stream_set_packet_header(stream1, packet_header),
		"Successfully set a stream's packet header");

	ok(bt_ctf_writer_add_environment_field(writer, "new_field", "test") == 0,
		"Add environment field to writer after stream creation");

	test_clock_utils();

	test_create_writer_vs_non_writer_mode();

	test_set_clock_non_writer_stream_class();

	test_instanciate_event_before_stream(writer, clock);

	append_simple_event(stream_class, stream1, clock);

	packet_resize_test(stream_class, stream1, clock);

	append_complex_event(stream_class, stream1, clock);

	append_existing_event_class(stream_class);

	test_empty_stream(writer);

	test_custom_event_header_stream(writer, clock);

	test_static_trace();

	test_trace_is_static_listener();

	test_trace_uuid();

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

	//recursive_rmdir(trace_path);
	g_free(trace_path);
	g_free(metadata_path);

	return 0;
}
