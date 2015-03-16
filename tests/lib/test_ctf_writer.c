/*
 * test-ctf-writer.c
 *
 * CTF Writer test
 *
 * Copyright 2013 - 2015 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define _GNU_SOURCE
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf/events.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <babeltrace/compat/limits.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include "tap/tap.h"
#include <math.h>
#include <float.h>

#define METADATA_LINE_SIZE 512
#define SEQUENCE_TEST_LENGTH 10
#define ARRAY_TEST_LENGTH 5
#define PACKET_RESIZE_TEST_LENGTH 100000

#define DEFAULT_CLOCK_FREQ 1000000000
#define DEFAULT_CLOCK_PRECISION 1
#define DEFAULT_CLOCK_OFFSET 0
#define DEFAULT_CLOCK_OFFSET_S 0
#define DEFAULT_CLOCK_IS_ABSOLUTE 0
#define DEFAULT_CLOCK_TIME 0

static uint64_t current_time = 42;

/* Return 1 if uuids match, zero if different. */
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

void validate_metadata(char *parser_path, char *metadata_path)
{
	int ret = 0;
	char parser_output_path[] = "/tmp/parser_output_XXXXXX";
	int parser_output_fd = -1, metadata_fd = -1;

	if (!metadata_path) {
		ret = -1;
		goto result;
	}

	parser_output_fd = mkstemp(parser_output_path);
	metadata_fd = open(metadata_path, O_RDONLY);

	unlink(parser_output_path);

	if (parser_output_fd == -1 || metadata_fd == -1) {
		diag("Failed create temporary files for metadata parsing.");
		ret = -1;
		goto result;
	}

	pid_t pid = fork();
	if (pid) {
		int status = 0;
		waitpid(pid, &status, 0);
		ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	} else {
		/* ctf-parser-test expects a metadata string on stdin. */
		ret = dup2(metadata_fd, STDIN_FILENO);
		if (ret < 0) {
			perror("# dup2 metadata_fd to STDIN");
			goto result;
		}

		ret = dup2(parser_output_fd, STDOUT_FILENO);
		if (ret < 0) {
			perror("# dup2 parser_output_fd to STDOUT");
			goto result;
		}

		ret = dup2(parser_output_fd, STDERR_FILENO);
		if (ret < 0) {
			perror("# dup2 parser_output_fd to STDERR");
			goto result;
		}

		execl(parser_path, "ctf-parser-test", NULL);
		perror("# Could not launch the ctf metadata parser process");
		exit(-1);
	}
result:
	ok(ret == 0, "Metadata string is valid");

	if (ret && metadata_fd >= 0 && parser_output_fd >= 0) {
		char *line;
		size_t len = METADATA_LINE_SIZE;
		FILE *metadata_fp = NULL, *parser_output_fp = NULL;

		metadata_fp = fdopen(metadata_fd, "r");
		if (!metadata_fp) {
			perror("fdopen on metadata_fd");
			goto close_fp;
		}
		metadata_fd = -1;

		parser_output_fp = fdopen(parser_output_fd, "r");
		if (!parser_output_fp) {
			perror("fdopen on parser_output_fd");
			goto close_fp;
		}
		parser_output_fd = -1;

		line = malloc(len);
		if (!line) {
			diag("malloc failure");
		}

		rewind(metadata_fp);

		/* Output the metadata and parser output as diagnostic */
		while (getline(&line, &len, metadata_fp) > 0) {
			fprintf(stderr, "# %s", line);
		}

		rewind(parser_output_fp);
		while (getline(&line, &len, parser_output_fp) > 0) {
			fprintf(stderr, "# %s", line);
		}

		free(line);
close_fp:
		if (metadata_fp) {
			if (fclose(metadata_fp)) {
				diag("fclose failure");
			}
		}
		if (parser_output_fp) {
			if (fclose(parser_output_fp)) {
				diag("fclose failure");
			}
		}
	}

	if (parser_output_fd >= 0) {
		if (close(parser_output_fd)) {
			diag("close error");
		}
	}
	if (metadata_fd >= 0) {
		if (close(metadata_fd)) {
			diag("close error");
		}
	}
}

void validate_trace(char *parser_path, char *trace_path)
{
	int ret = 0;
	char babeltrace_output_path[] = "/tmp/babeltrace_output_XXXXXX";
	int babeltrace_output_fd = -1;

	if (!trace_path) {
		ret = -1;
		goto result;
	}

	babeltrace_output_fd = mkstemp(babeltrace_output_path);
	unlink(babeltrace_output_path);

	if (babeltrace_output_fd == -1) {
		diag("Failed to create a temporary file for trace parsing.");
		ret = -1;
		goto result;
	}

	pid_t pid = fork();
	if (pid) {
		int status = 0;
		waitpid(pid, &status, 0);
		ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	} else {
		ret = dup2(babeltrace_output_fd, STDOUT_FILENO);
		if (ret < 0) {
			perror("# dup2 babeltrace_output_fd to STDOUT");
			goto result;
		}

		ret = dup2(babeltrace_output_fd, STDERR_FILENO);
		if (ret < 0) {
			perror("# dup2 babeltrace_output_fd to STDERR");
			goto result;
		}

		execl(parser_path, "babeltrace", trace_path, NULL);
		perror("# Could not launch the babeltrace process");
		exit(-1);
	}
result:
	ok(ret == 0, "Babeltrace could read the resulting trace");

	if (ret && babeltrace_output_fd >= 0) {
		char *line;
		size_t len = METADATA_LINE_SIZE;
		FILE *babeltrace_output_fp = NULL;

		babeltrace_output_fp = fdopen(babeltrace_output_fd, "r");
		if (!babeltrace_output_fp) {
			perror("fdopen on babeltrace_output_fd");
			goto close_fp;
		}
		babeltrace_output_fd = -1;

		line = malloc(len);
		if (!line) {
			diag("malloc error");
		}
		rewind(babeltrace_output_fp);
		while (getline(&line, &len, babeltrace_output_fp) > 0) {
			diag("%s", line);
		}

		free(line);
close_fp:
		if (babeltrace_output_fp) {
			if (fclose(babeltrace_output_fp)) {
				diag("fclose error");
			}
		}
	}

	if (babeltrace_output_fd >= 0) {
		if (close(babeltrace_output_fd)) {
			diag("close error");
		}
	}
}

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
	struct bt_ctf_clock *ret_clock;
	struct bt_ctf_event_class *ret_event_class;
	struct bt_ctf_field *packet_context;
	struct bt_ctf_field *packet_context_field;
	struct bt_ctf_field *stream_event_context;
	struct bt_ctf_field *stream_event_context_field;
	struct bt_ctf_field *event_context;
	struct bt_ctf_field *event_context_field;

	ok(uint_12_type, "Create an unsigned integer type");

	bt_ctf_field_type_integer_set_signed(int_64_type, 1);
	ok(int_64_type, "Create a signed integer type");
	enum_type = bt_ctf_field_type_enumeration_create(int_64_type);

	returned_type = bt_ctf_field_type_enumeration_get_container_type(enum_type);
	ok(returned_type == int_64_type, "bt_ctf_field_type_enumeration_get_container_type returns the right type");
	ok(!bt_ctf_field_type_enumeration_get_container_type(NULL), "bt_ctf_field_type_enumeration_get_container_type handles NULL correctly");
	ok(!bt_ctf_field_type_enumeration_create(enum_type),
		"bt_ctf_field_enumeration_type_create rejects non-integer container field types");
	bt_ctf_field_type_put(returned_type);

	bt_ctf_field_type_set_alignment(float_type, 32);
	ok(bt_ctf_field_type_get_alignment(NULL) < 0,
		"bt_ctf_field_type_get_alignment handles NULL correctly");
	ok(bt_ctf_field_type_get_alignment(float_type) == 32,
		"bt_ctf_field_type_get_alignment returns a correct value");

	ok(bt_ctf_field_type_floating_point_set_exponent_digits(float_type, 11) == 0,
		"Set a floating point type's exponent digit count");
	ok(bt_ctf_field_type_floating_point_set_mantissa_digits(float_type, 53) == 0,
		"Set a floating point type's mantissa digit count");

	ok(bt_ctf_field_type_floating_point_get_exponent_digits(NULL) < 0,
		"bt_ctf_field_type_floating_point_get_exponent_digits handles NULL properly");
	ok(bt_ctf_field_type_floating_point_get_mantissa_digits(NULL) < 0,
		"bt_ctf_field_type_floating_point_get_mantissa_digits handles NULL properly");
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
		43, 51), "bt_ctf_field_type_enumeration_add_mapping rejects duplicate mapping names");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, "something",
		-500, -400), "bt_ctf_field_type_enumeration_add_mapping rejects overlapping enum entries");
	ok(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test,
		-54, -55), "bt_ctf_field_type_enumeration_add_mapping rejects mapping where end < start");
	bt_ctf_field_type_enumeration_add_mapping(enum_type, "another entry", -42000, -13000);

	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_value(NULL, -42) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_value handles a NULL field type correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_value(enum_type, 1000000) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_value handles invalid values correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_value(enum_type, -55) == 1,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_value returns the correct index");

	ok(bt_ctf_event_class_add_field(simple_event_class, enum_type,
		"enum_field") == 0, "Add signed enumeration field to event");

	ok(bt_ctf_field_type_enumeration_get_mapping(NULL, 0, &ret_char,
		&ret_range_start_int64_t, &ret_range_end_int64_t) < 0,
		"bt_ctf_field_type_enumeration_get_mapping handles a NULL enumeration correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping(enum_type, 0, NULL,
		&ret_range_start_int64_t, &ret_range_end_int64_t) < 0,
		"bt_ctf_field_type_enumeration_get_mapping handles a NULL string correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping(enum_type, 0, &ret_char,
		NULL, &ret_range_end_int64_t) < 0,
		"bt_ctf_field_type_enumeration_get_mapping handles a NULL start correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping(enum_type, 0, &ret_char,
		&ret_range_start_int64_t, NULL) < 0,
		"bt_ctf_field_type_enumeration_get_mapping handles a NULL end correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping(enum_type, 5, &ret_char,
		&ret_range_start_int64_t, &ret_range_end_int64_t) == 0,
		"bt_ctf_field_type_enumeration_get_mapping returns a value");
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_ctf_field_type_enumeration_get_mapping returns a correct mapping name");
	ok(ret_range_start_int64_t == 42,
		"bt_ctf_field_type_enumeration_get_mapping returns a correct mapping start");
	ok(ret_range_end_int64_t == 42,
		"bt_ctf_field_type_enumeration_get_mapping returns a correct mapping end");

	ok(bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned,
		"escaping; \"test\"", 0, 0) == 0,
		"bt_ctf_field_type_enumeration_add_mapping_unsigned accepts enumeration mapping strings containing quotes");
	ok(bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned,
		"\tanother \'escaping\'\n test\"", 1, 4) == 0,
		"bt_ctf_field_type_enumeration_add_mapping_unsigned accepts enumeration mapping strings containing special characters");
	ok(bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned,
		"event clock int float", 5, 22) == 0,
		"bt_ctf_field_type_enumeration_add_mapping_unsigned accepts enumeration mapping strings containing reserved keywords");
	bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, mapping_name_test,
		42, 42);
	ok(bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, mapping_name_test,
		43, 51), "bt_ctf_field_type_enumeration_add_mapping_unsigned rejects duplicate mapping names");
	ok(bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, "something",
		7, 8), "bt_ctf_field_type_enumeration_add_mapping_unsigned rejects overlapping enum entries");
	ok(bt_ctf_field_type_enumeration_add_mapping_unsigned(enum_type_unsigned, mapping_name_test,
		55, 54), "bt_ctf_field_type_enumeration_add_mapping_unsigned rejects mapping where end < start");
	ok(bt_ctf_event_class_add_field(simple_event_class, enum_type_unsigned,
		"enum_field_unsigned") == 0, "Add unsigned enumeration field to event");

	ok(bt_ctf_field_type_enumeration_get_mapping_count(NULL) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_count handles NULL correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_count(enum_type_unsigned) == 4,
		"bt_ctf_field_type_enumeration_get_mapping_count returns the correct value");

	ok(bt_ctf_field_type_enumeration_get_mapping_unsigned(NULL, 0, &ret_char,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned handles a NULL enumeration correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 0, NULL,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned handles a NULL string correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 0, &ret_char,
		NULL, &ret_range_end_uint64_t) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned handles a NULL start correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 0, &ret_char,
		&ret_range_start_uint64_t, NULL) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned handles a NULL end correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_unsigned(enum_type_unsigned, 3, &ret_char,
		&ret_range_start_uint64_t, &ret_range_end_uint64_t) == 0,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned returns a value");
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_ctf_field_type_enumeration_get_mapping_unsigned returns a correct mapping name");
	ok(ret_range_start_uint64_t == 42,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned returns a correct mapping start");
	ok(ret_range_end_uint64_t == 42,
		"bt_ctf_field_type_enumeration_get_mapping_unsigned returns a correct mapping end");

	bt_ctf_event_class_add_field(simple_event_class, uint_12_type,
		"integer_field");
	bt_ctf_event_class_add_field(simple_event_class, float_type,
		"float_field");

	/* Set an event context type which will contain a single integer*/
	ok(!bt_ctf_field_type_structure_add_field(event_context_type, uint_12_type,
		"event_specific_context"),
		"Add event specific context field");
	ok(bt_ctf_event_class_get_context_type(NULL) == NULL,
		"bt_ctf_event_class_get_context_type handles NULL correctly");
	ok(bt_ctf_event_class_get_context_type(simple_event_class) == NULL,
		"bt_ctf_event_class_get_context_type returns NULL when no event context type is set");

	ok(bt_ctf_event_class_set_context_type(simple_event_class, NULL) < 0,
		"bt_ctf_event_class_set_context_type handles a NULL context type correctly");
	ok(bt_ctf_event_class_set_context_type(NULL, event_context_type) < 0,
		"bt_ctf_event_class_set_context_type handles a NULL event class correctly");
	ok(!bt_ctf_event_class_set_context_type(simple_event_class, event_context_type),
		"Set an event class' context type successfully");
	returned_type = bt_ctf_event_class_get_context_type(simple_event_class);
	ok(returned_type == event_context_type,
		"bt_ctf_event_class_get_context_type returns the appropriate type");
	bt_ctf_field_type_put(returned_type);

	bt_ctf_stream_class_add_event_class(stream_class, simple_event_class);

	ok(bt_ctf_stream_class_get_event_class_count(NULL) < 0,
		"bt_ctf_stream_class_get_event_class_count handles NULL correctly");
	ok(bt_ctf_stream_class_get_event_class_count(stream_class) == 1,
		"bt_ctf_stream_class_get_event_class_count returns a correct number of event classes");
	ok(bt_ctf_stream_class_get_event_class(NULL, 0) == NULL,
		"bt_ctf_stream_class_get_event_class handles NULL correctly");
	ok(bt_ctf_stream_class_get_event_class(stream_class, 8724) == NULL,
		"bt_ctf_stream_class_get_event_class handles invalid indexes correctly");
	ret_event_class = bt_ctf_stream_class_get_event_class(stream_class, 0);
	ok(ret_event_class == simple_event_class,
		"bt_ctf_stream_class_get_event_class returns the correct event class");
	bt_ctf_event_class_put(ret_event_class);

	ok(bt_ctf_stream_class_get_event_class_by_name(NULL, "some event name") == NULL,
		"bt_ctf_stream_class_get_event_class_by_name handles a NULL stream class correctly");
	ok(bt_ctf_stream_class_get_event_class_by_name(stream_class, NULL) == NULL,
		"bt_ctf_stream_class_get_event_class_by_name handles a NULL event class name correctly");
	ok(bt_ctf_stream_class_get_event_class_by_name(stream_class, "some event name") == NULL,
		"bt_ctf_stream_class_get_event_class_by_name handles non-existing event class names correctly");
	ret_event_class = bt_ctf_stream_class_get_event_class_by_name(stream_class, "Simple Event");
	ok(ret_event_class == simple_event_class,
		"bt_ctf_stream_class_get_event_class_by_name returns a correct event class");
	bt_ctf_event_class_put(ret_event_class);

	simple_event = bt_ctf_event_create(simple_event_class);
	ok(simple_event,
		"Instantiate an event containing a single integer field");

	ok(bt_ctf_event_get_clock(NULL) == NULL,
		"bt_ctf_event_get_clock handles NULL correctly");
	ret_clock = bt_ctf_event_get_clock(simple_event);
	ok(ret_clock == clock,
		"bt_ctf_event_get_clock returns a correct clock");
	bt_ctf_clock_put(clock);

	integer_field = bt_ctf_field_create(uint_12_type);
	bt_ctf_field_unsigned_integer_set_value(integer_field, 42);
	ok(bt_ctf_event_set_payload(simple_event, "integer_field",
		integer_field) == 0, "Use bt_ctf_event_set_payload to set a manually allocated field");

	float_field = bt_ctf_event_get_payload(simple_event, "float_field");
	ok(bt_ctf_field_floating_point_get_value(float_field, &ret_double),
		"bt_ctf_field_floating_point_get_value fails on an unset float field");
	bt_ctf_field_floating_point_set_value(float_field, double_test_value);
	ok(bt_ctf_field_floating_point_get_value(NULL, &ret_double),
		"bt_ctf_field_floating_point_get_value properly handles a NULL field");
	ok(bt_ctf_field_floating_point_get_value(float_field, NULL),
		"bt_ctf_field_floating_point_get_value properly handles a NULL return value pointer");
	ok(!bt_ctf_field_floating_point_get_value(float_field, &ret_double),
		"bt_ctf_field_floating_point_get_value returns a double value");
	ok(fabs(ret_double - double_test_value) <= DBL_EPSILON,
		"bt_ctf_field_floating_point_get_value returns a correct value");

	enum_field = bt_ctf_field_create(enum_type);
	ret_char = bt_ctf_field_enumeration_get_mapping_name(NULL);
	ok(!ret_char, "bt_ctf_field_enumeration_get_mapping_name handles NULL correctly");
	ret_char = bt_ctf_field_enumeration_get_mapping_name(enum_field);
	ok(!ret_char, "bt_ctf_field_enumeration_get_mapping_name returns NULL if the enumeration's container field is unset");
	enum_container_field = bt_ctf_field_enumeration_get_container(
		enum_field);
	ok(bt_ctf_field_signed_integer_set_value(
		enum_container_field, -42) == 0,
		"Set signed enumeration container value");
	ret_char = bt_ctf_field_enumeration_get_mapping_name(enum_field);
	ok(!strcmp(ret_char, mapping_name_negative_test),
		"bt_ctf_field_enumeration_get_mapping_name returns the correct mapping name with an signed container");
	bt_ctf_event_set_payload(simple_event, "enum_field", enum_field);

	enum_field_unsigned = bt_ctf_field_create(enum_type_unsigned);
	enum_container_field_unsigned = bt_ctf_field_enumeration_get_container(
		enum_field_unsigned);
	ok(bt_ctf_field_unsigned_integer_set_value(
		enum_container_field_unsigned, 42) == 0,
		"Set unsigned enumeration container value");
	bt_ctf_event_set_payload(simple_event, "enum_field_unsigned",
		enum_field_unsigned);
	ret_char = bt_ctf_field_enumeration_get_mapping_name(enum_field_unsigned);
	ok(!strcmp(ret_char, mapping_name_test),
		"bt_ctf_field_enumeration_get_mapping_name returns the correct mapping name with an unsigned container");

	ok(bt_ctf_clock_set_time(clock, current_time) == 0, "Set clock time");

	/* Populate stream event context */
	stream_event_context = bt_ctf_stream_get_event_context(stream);
	stream_event_context_field = bt_ctf_field_structure_get_field(
		stream_event_context, "common_event_context");
	bt_ctf_field_unsigned_integer_set_value(stream_event_context_field, 42);

	/* Populate the event's context */
	ok(bt_ctf_event_get_event_context(NULL) == NULL,
		"bt_ctf_event_get_event_context handles NULL correctly");
	event_context = bt_ctf_event_get_event_context(simple_event);
	ok(event_context,
		"bt_ctf_event_get_event_context returns a field");
	returned_type = bt_ctf_field_get_type(event_context);
	ok(returned_type == event_context_type,
		"bt_ctf_event_get_event_context returns a field of the appropriate type");
	event_context_field = bt_ctf_field_structure_get_field(event_context,
		"event_specific_context");
	ok(!bt_ctf_field_unsigned_integer_set_value(event_context_field, 1234),
		"Successfully set an event context's value");
	ok(bt_ctf_event_set_event_context(NULL, event_context) < 0,
		"bt_ctf_event_set_event_context handles a NULL event correctly");
	ok(bt_ctf_event_set_event_context(simple_event, NULL) < 0,
		"bt_ctf_event_set_event_context handles a NULL event context correctly");
	ok(bt_ctf_event_set_event_context(simple_event, event_context_field) < 0,
		"bt_ctf_event_set_event_context rejects a context of the wrong type");
	ok(!bt_ctf_event_set_event_context(simple_event, event_context),
		"Set an event context successfully");

	ok(bt_ctf_stream_append_event(stream, simple_event) == 0,
		"Append simple event to trace stream");

	ok(bt_ctf_stream_get_packet_context(NULL) == NULL,
		"bt_ctf_stream_get_packet_context handles NULL correctly");
	packet_context = bt_ctf_stream_get_packet_context(stream);
	ok(packet_context,
		"bt_ctf_stream_get_packet_context returns a packet context");

	packet_context_field = bt_ctf_field_structure_get_field(packet_context,
		"packet_size");
	ok(packet_context_field,
		"Packet context contains the default packet_size field.");
	bt_ctf_field_put(packet_context_field);
	packet_context_field = bt_ctf_field_structure_get_field(packet_context,
		"custom_packet_context_field");
	ok(bt_ctf_field_unsigned_integer_set_value(packet_context_field, 8) == 0,
		"Custom packet context field value successfully set.");

	ok(bt_ctf_stream_set_packet_context(NULL, packet_context_field) < 0,
		"bt_ctf_stream_set_packet_context handles a NULL stream correctly");
	ok(bt_ctf_stream_set_packet_context(stream, NULL) < 0,
		"bt_ctf_stream_set_packet_context handles a NULL packet context correctly");
	ok(bt_ctf_stream_set_packet_context(stream, packet_context) == 0,
		"Successfully set a stream's packet context");

	ok(bt_ctf_stream_flush(stream) == 0,
		"Flush trace stream with one event");

	bt_ctf_event_class_put(simple_event_class);
	bt_ctf_event_put(simple_event);
	bt_ctf_field_type_put(uint_12_type);
	bt_ctf_field_type_put(int_64_type);
	bt_ctf_field_type_put(float_type);
	bt_ctf_field_type_put(enum_type);
	bt_ctf_field_type_put(enum_type_unsigned);
	bt_ctf_field_type_put(returned_type);
	bt_ctf_field_type_put(event_context_type);
	bt_ctf_field_put(integer_field);
	bt_ctf_field_put(float_field);
	bt_ctf_field_put(enum_field);
	bt_ctf_field_put(enum_field_unsigned);
	bt_ctf_field_put(enum_container_field);
	bt_ctf_field_put(enum_container_field_unsigned);
	bt_ctf_field_put(packet_context);
	bt_ctf_field_put(packet_context_field);
	bt_ctf_field_put(stream_event_context);
	bt_ctf_field_put(stream_event_context_field);
	bt_ctf_field_put(event_context);
	bt_ctf_field_put(event_context_field);
}

void append_complex_event(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
	int i;
	const char *complex_test_event_string = "Complex Test Event";
	const char *test_string = "Test string";
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
		*variant_field, *an_array_field, *ret_field;
	uint64_t ret_unsigned_int;
	int64_t ret_signed_int;
	const char *ret_string;
	struct bt_ctf_stream_class *ret_stream_class;
	struct bt_ctf_event_class *ret_event_class;
	struct bt_ctf_field *packet_context, *packet_context_field;

	bt_ctf_field_type_set_alignment(int_16_type, 32);
	bt_ctf_field_type_integer_set_signed(int_16_type, 1);
	bt_ctf_field_type_integer_set_base(uint_35_type,
		BT_CTF_INTEGER_BASE_HEXADECIMAL);

	array_type = bt_ctf_field_type_array_create(int_16_type, ARRAY_TEST_LENGTH);
	sequence_type = bt_ctf_field_type_sequence_create(int_16_type,
		"seq_len");

	ok(bt_ctf_field_type_array_get_element_type(NULL) == NULL,
		"bt_ctf_field_type_array_get_element_type handles NULL correctly");
	ret_field_type = bt_ctf_field_type_array_get_element_type(
		array_type);
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_array_get_element_type returns the correct type");
	bt_ctf_field_type_put(ret_field_type);

	ok(bt_ctf_field_type_array_get_length(NULL) < 0,
		"bt_ctf_field_type_array_get_length handles NULL correctly");
	ok(bt_ctf_field_type_array_get_length(array_type) == ARRAY_TEST_LENGTH,
		"bt_ctf_field_type_array_get_length returns the correct length");

	bt_ctf_field_type_structure_add_field(inner_structure_type,
		uint_35_type, "seq_len");
	bt_ctf_field_type_structure_add_field(inner_structure_type,
		sequence_type, "a_sequence");
	bt_ctf_field_type_structure_add_field(inner_structure_type,
		array_type, "an_array");

	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"UINT3_TYPE", 0, 0);
	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"INT16_TYPE", 1, 1);
	bt_ctf_field_type_enumeration_add_mapping(enum_variant_type,
		"UINT35_TYPE", 2, 7);

	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_name(NULL,
		"INT16_TYPE") < 0,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_name handles a NULL field type correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_name(
		enum_variant_type, NULL) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_name handles a NULL name correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_name(
		enum_variant_type, "INT16_TYPE") == 1,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_name returns the correct index");

	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(NULL, 1) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value handles a NULL field type correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(enum_variant_type, -42) < 0,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value handles invalid values correctly");
	ok(bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(enum_variant_type, 5) == 2,
		"bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value returns the correct index");

	ok(bt_ctf_field_type_variant_add_field(variant_type, uint_3_type,
		"An unknown entry"), "Reject a variant field based on an unknown tag value");
	ok(bt_ctf_field_type_variant_add_field(variant_type, uint_3_type,
		"UINT3_TYPE") == 0, "Add a field to a variant");
	bt_ctf_field_type_variant_add_field(variant_type, int_16_type,
		"INT16_TYPE");
	bt_ctf_field_type_variant_add_field(variant_type, uint_35_type,
		"UINT35_TYPE");

	ok(bt_ctf_field_type_variant_get_tag_type(NULL) == NULL,
		"bt_ctf_field_type_variant_get_tag_type handles NULL correctly");
	ret_field_type = bt_ctf_field_type_variant_get_tag_type(variant_type);
	ok(ret_field_type == enum_variant_type,
		"bt_ctf_field_type_variant_get_tag_type returns a correct tag type");
	bt_ctf_field_type_put(ret_field_type);

	ok(bt_ctf_field_type_variant_get_tag_name(NULL) == NULL,
		"bt_ctf_field_type_variant_get_tag_name handles NULL correctly");
	ret_string = bt_ctf_field_type_variant_get_tag_name(variant_type);
	ok(!strcmp(ret_string, "variant_selector"),
		"bt_ctf_field_type_variant_get_tag_name returns the correct variant tag name");
	ok(bt_ctf_field_type_variant_get_field_type_by_name(NULL,
		"INT16_TYPE") == NULL,
		"bt_ctf_field_type_variant_get_field_type_by_name handles a NULL variant_type correctly");
	ok(bt_ctf_field_type_variant_get_field_type_by_name(variant_type,
		NULL) == NULL,
		"bt_ctf_field_type_variant_get_field_type_by_name handles a NULL field name correctly");
	ret_field_type = bt_ctf_field_type_variant_get_field_type_by_name(
		variant_type, "INT16_TYPE");
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_variant_get_field_type_by_name returns a correct field type");
	bt_ctf_field_type_put(ret_field_type);

	ok(bt_ctf_field_type_variant_get_field_count(NULL) < 0,
		"bt_ctf_field_type_variant_get_field_count handles NULL correctly");
	ok(bt_ctf_field_type_variant_get_field_count(variant_type) == 3,
		"bt_ctf_field_type_variant_get_field_count returns the correct count");

	ok(bt_ctf_field_type_variant_get_field(NULL, &ret_string, &ret_field_type, 0) < 0,
		"bt_ctf_field_type_variant_get_field handles a NULL type correctly");
	ok(bt_ctf_field_type_variant_get_field(variant_type, NULL, &ret_field_type, 0) < 0,
		"bt_ctf_field_type_variant_get_field handles a NULL field name correctly");
	ok(bt_ctf_field_type_variant_get_field(variant_type, &ret_string, NULL, 0) < 0,
		"bt_ctf_field_type_variant_get_field handles a NULL field type correctly");
	ok(bt_ctf_field_type_variant_get_field(variant_type, &ret_string, &ret_field_type, 200) < 0,
		"bt_ctf_field_type_variant_get_field handles an invalid index correctly");
	ok(bt_ctf_field_type_variant_get_field(variant_type, &ret_string, &ret_field_type, 1) == 0,
		"bt_ctf_field_type_variant_get_field returns a field");
	ok(!strcmp("INT16_TYPE", ret_string),
		"bt_ctf_field_type_variant_get_field returns a correct field name");
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_variant_get_field returns a correct field type");
	bt_ctf_field_type_put(ret_field_type);

	bt_ctf_field_type_structure_add_field(complex_structure_type,
		enum_variant_type, "variant_selector");
	bt_ctf_field_type_structure_add_field(complex_structure_type,
		string_type, "a_string");
	bt_ctf_field_type_structure_add_field(complex_structure_type,
		variant_type, "variant_value");
	bt_ctf_field_type_structure_add_field(complex_structure_type,
		inner_structure_type, "inner_structure");

	ok(bt_ctf_event_class_create("clock") == NULL,
		"Reject creation of an event class with an illegal name");
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

	ok(bt_ctf_event_class_get_name(NULL) == NULL,
		"bt_ctf_event_class_get_name handles NULL correctly");
	ret_string = bt_ctf_event_class_get_name(event_class);
	ok(!strcmp(ret_string, complex_test_event_string),
		"bt_ctf_event_class_get_name returns a correct name");
	ok(bt_ctf_event_class_get_id(event_class) < 0,
		"bt_ctf_event_class_get_id returns a negative value when not set");
	ok(bt_ctf_event_class_get_id(NULL) < 0,
		"bt_ctf_event_class_get_id handles NULL correctly");
	ok(bt_ctf_event_class_set_id(NULL, 42) < 0,
		"bt_ctf_event_class_set_id handles NULL correctly");
	ok(bt_ctf_event_class_set_id(event_class, 42) == 0,
		"Set an event class' id");
	ok(bt_ctf_event_class_get_id(event_class) == 42,
		"bt_ctf_event_class_get_id returns the correct value");

	/* Add event class to the stream class */
	ok(bt_ctf_stream_class_add_event_class(stream_class, NULL),
		"Reject addition of NULL event class to a stream class");
	ok(bt_ctf_stream_class_add_event_class(stream_class,
		event_class) == 0, "Add an event class to stream class");

	ok(bt_ctf_event_class_get_stream_class(NULL) == NULL,
		"bt_ctf_event_class_get_stream_class handles NULL correctly");
	ret_stream_class = bt_ctf_event_class_get_stream_class(event_class);
	ok(ret_stream_class == stream_class,
		"bt_ctf_event_class_get_stream_class returns the correct stream class");
	bt_ctf_stream_class_put(ret_stream_class);

	ok(bt_ctf_event_class_get_field_count(NULL) < 0,
		"bt_ctf_event_class_get_field_count handles NULL correctly");
	ok(bt_ctf_event_class_get_field_count(event_class) == 3,
		"bt_ctf_event_class_get_field_count returns a correct value");

	ok(bt_ctf_event_class_get_field(NULL, &ret_string,
		&ret_field_type, 0) < 0,
		"bt_ctf_event_class_get_field handles a NULL event class correctly");
	ok(bt_ctf_event_class_get_field(event_class, NULL,
		&ret_field_type, 0) < 0,
		"bt_ctf_event_class_get_field handles a NULL field name correctly");
	ok(bt_ctf_event_class_get_field(event_class, &ret_string,
		NULL, 0) < 0,
		"bt_ctf_event_class_get_field handles a NULL field type correctly");
	ok(bt_ctf_event_class_get_field(event_class, &ret_string,
		&ret_field_type, 42) < 0,
		"bt_ctf_event_class_get_field handles an invalid index correctly");
	ok(bt_ctf_event_class_get_field(event_class, &ret_string,
		&ret_field_type, 0) == 0,
		"bt_ctf_event_class_get_field returns a field");
	ok(ret_field_type == uint_35_type,
		"bt_ctf_event_class_get_field returns a correct field type");
	bt_ctf_field_type_put(ret_field_type);
	ok(!strcmp(ret_string, "uint_35"),
		"bt_ctf_event_class_get_field returns a correct field name");
	ok(bt_ctf_event_class_get_field_by_name(NULL, "") == NULL,
		"bt_ctf_event_class_get_field_by_name handles a NULL event class correctly");
	ok(bt_ctf_event_class_get_field_by_name(event_class, NULL) == NULL,
		"bt_ctf_event_class_get_field_by_name handles a NULL field name correctly");
	ok(bt_ctf_event_class_get_field_by_name(event_class, "truie") == NULL,
		"bt_ctf_event_class_get_field_by_name handles an invalid field name correctly");
	ret_field_type = bt_ctf_event_class_get_field_by_name(event_class,
		"complex_structure");
	ok(ret_field_type == complex_structure_type,
		"bt_ctf_event_class_get_field_by_name returns a correct field type");
	bt_ctf_field_type_put(ret_field_type);

	event = bt_ctf_event_create(event_class);
	ok(event, "Instanciate a complex event");

	ok(bt_ctf_event_get_class(NULL) == NULL,
		"bt_ctf_event_get_class handles NULL correctly");
	ret_event_class = bt_ctf_event_get_class(event);
	ok(ret_event_class == event_class,
		"bt_ctf_event_get_class returns the correct event class");
	bt_ctf_event_class_put(ret_event_class);

	uint_35_field = bt_ctf_event_get_payload(event, "uint_35");
	if (!uint_35_field) {
		printf("uint_35_field is NULL\n");
	}

	ok(uint_35_field, "Use bt_ctf_event_get_payload to get a field instance ");
	bt_ctf_field_unsigned_integer_set_value(uint_35_field, 0x0DDF00D);
	ok(bt_ctf_field_unsigned_integer_get_value(NULL, &ret_unsigned_int) < 0,
		"bt_ctf_field_unsigned_integer_get_value properly properly handles a NULL field.");
	ok(bt_ctf_field_unsigned_integer_get_value(uint_35_field, NULL) < 0,
		"bt_ctf_field_unsigned_integer_get_value properly handles a NULL return value");
	ok(bt_ctf_field_unsigned_integer_get_value(uint_35_field,
		&ret_unsigned_int) == 0,
		"bt_ctf_field_unsigned_integer_get_value succeeds after setting a value");
	ok(ret_unsigned_int == 0x0DDF00D,
		"bt_ctf_field_unsigned_integer_get_value returns the correct value");
	ok(bt_ctf_field_signed_integer_get_value(uint_35_field,
		&ret_signed_int) < 0,
		"bt_ctf_field_signed_integer_get_value fails on an unsigned field");
	bt_ctf_field_put(uint_35_field);

	int_16_field = bt_ctf_event_get_payload(event, "int_16");
	bt_ctf_field_signed_integer_set_value(int_16_field, -12345);
	ok(bt_ctf_field_signed_integer_get_value(NULL, &ret_signed_int) < 0,
		"bt_ctf_field_signed_integer_get_value properly handles a NULL field");
	ok(bt_ctf_field_signed_integer_get_value(int_16_field, NULL) < 0,
		"bt_ctf_field_signed_integer_get_value properly handles a NULL return value");
	ok(bt_ctf_field_signed_integer_get_value(int_16_field,
		&ret_signed_int) == 0,
		"bt_ctf_field_signed_integer_get_value succeeds after setting a value");
	ok(ret_signed_int == -12345,
		"bt_ctf_field_signed_integer_get_value returns the correct value");
	ok(bt_ctf_field_unsigned_integer_get_value(int_16_field,
		&ret_unsigned_int) < 0,
		"bt_ctf_field_unsigned_integer_get_value fails on a signed field");
	bt_ctf_field_put(int_16_field);

	complex_structure_field = bt_ctf_event_get_payload(event,
		"complex_structure");

	ok(bt_ctf_field_structure_get_field_by_index(NULL, 0) == NULL,
		"bt_ctf_field_structure_get_field_by_index handles NULL correctly");
	ok(bt_ctf_field_structure_get_field_by_index(NULL, 9) == NULL,
		"bt_ctf_field_structure_get_field_by_index handles an invalid index correctly");
	inner_structure_field = bt_ctf_field_structure_get_field_by_index(
		complex_structure_field, 3);
	ret_field_type = bt_ctf_field_get_type(inner_structure_field);
	bt_ctf_field_put(inner_structure_field);
	ok(ret_field_type == inner_structure_type,
		"bt_ctf_field_structure_get_field_by_index returns a correct field");
	bt_ctf_field_type_put(ret_field_type);

	inner_structure_field = bt_ctf_field_structure_get_field(
		complex_structure_field, "inner_structure");
	a_string_field = bt_ctf_field_structure_get_field(
		complex_structure_field, "a_string");
	enum_variant_field = bt_ctf_field_structure_get_field(
		complex_structure_field, "variant_selector");
	variant_field = bt_ctf_field_structure_get_field(
		complex_structure_field, "variant_value");
	uint_35_field = bt_ctf_field_structure_get_field(
		inner_structure_field, "seq_len");
	a_sequence_field = bt_ctf_field_structure_get_field(
		inner_structure_field, "a_sequence");
	an_array_field = bt_ctf_field_structure_get_field(
		inner_structure_field, "an_array");

	enum_container_field = bt_ctf_field_enumeration_get_container(
		enum_variant_field);
	bt_ctf_field_unsigned_integer_set_value(enum_container_field, 1);
	int_16_field = bt_ctf_field_variant_get_field(variant_field,
		enum_variant_field);
	bt_ctf_field_signed_integer_set_value(int_16_field, -200);
	bt_ctf_field_put(int_16_field);
	ok(!bt_ctf_field_string_get_value(a_string_field),
		"bt_ctf_field_string_get_value returns NULL on an unset field");
	bt_ctf_field_string_set_value(a_string_field,
		test_string);
	ok(!bt_ctf_field_string_get_value(NULL),
		"bt_ctf_field_string_get_value correctly handles NULL");
	ret_string = bt_ctf_field_string_get_value(a_string_field);
	ok(ret_string, "bt_ctf_field_string_get_value returns a string");
	ok(ret_string ? !strcmp(ret_string, test_string) : 0,
		"bt_ctf_field_string_get_value returns a correct value");
	bt_ctf_field_unsigned_integer_set_value(uint_35_field,
		SEQUENCE_TEST_LENGTH);

	ok(bt_ctf_field_type_variant_get_field_type_from_tag(NULL,
		enum_container_field) == NULL,
		"bt_ctf_field_type_variant_get_field_type_from_tag handles a NULL variant type correctly");
	ok(bt_ctf_field_type_variant_get_field_type_from_tag(variant_type,
		NULL) == NULL,
		"bt_ctf_field_type_variant_get_field_type_from_tag handles a NULL tag correctly");
	ret_field_type = bt_ctf_field_type_variant_get_field_type_from_tag(
		variant_type, enum_variant_field);
	ok(ret_field_type == int_16_type,
		"bt_ctf_field_type_variant_get_field_type_from_tag returns the correct field type");

	ok(bt_ctf_field_sequence_get_length(a_sequence_field) == NULL,
		"bt_ctf_field_sequence_get_length returns NULL when length is unset");
	ok(bt_ctf_field_sequence_set_length(a_sequence_field,
		uint_35_field) == 0, "Set a sequence field's length");
	ret_field = bt_ctf_field_sequence_get_length(a_sequence_field);
	ok(ret_field == uint_35_field,
		"bt_ctf_field_sequence_get_length returns the correct length field");
	ok(bt_ctf_field_sequence_get_length(NULL) == NULL,
		"bt_ctf_field_sequence_get_length properly handles NULL");

	for (i = 0; i < SEQUENCE_TEST_LENGTH; i++) {
		int_16_field = bt_ctf_field_sequence_get_field(
			a_sequence_field, i);
		bt_ctf_field_signed_integer_set_value(int_16_field, 4 - i);
		bt_ctf_field_put(int_16_field);
	}

	for (i = 0; i < ARRAY_TEST_LENGTH; i++) {
		int_16_field = bt_ctf_field_array_get_field(
			an_array_field, i);
		bt_ctf_field_signed_integer_set_value(int_16_field, i);
		bt_ctf_field_put(int_16_field);
	}

	bt_ctf_clock_set_time(clock, ++current_time);
	ok(bt_ctf_stream_append_event(stream, event) == 0,
		"Append a complex event to a stream");

	/*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
	packet_context = bt_ctf_stream_get_packet_context(stream);
	packet_context_field = bt_ctf_field_structure_get_field(packet_context,
		"custom_packet_context_field");
	bt_ctf_field_unsigned_integer_set_value(packet_context_field, 1);

	ok(bt_ctf_stream_flush(stream) == 0,
		"Flush a stream containing a complex event");

	bt_ctf_field_put(uint_35_field);
	bt_ctf_field_put(a_string_field);
	bt_ctf_field_put(inner_structure_field);
	bt_ctf_field_put(complex_structure_field);
	bt_ctf_field_put(a_sequence_field);
	bt_ctf_field_put(an_array_field);
	bt_ctf_field_put(enum_variant_field);
	bt_ctf_field_put(enum_container_field);
	bt_ctf_field_put(variant_field);
	bt_ctf_field_put(ret_field);
	bt_ctf_field_put(packet_context_field);
	bt_ctf_field_put(packet_context);
	bt_ctf_field_type_put(uint_35_type);
	bt_ctf_field_type_put(int_16_type);
	bt_ctf_field_type_put(string_type);
	bt_ctf_field_type_put(sequence_type);
	bt_ctf_field_type_put(array_type);
	bt_ctf_field_type_put(inner_structure_type);
	bt_ctf_field_type_put(complex_structure_type);
	bt_ctf_field_type_put(uint_3_type);
	bt_ctf_field_type_put(enum_variant_type);
	bt_ctf_field_type_put(variant_type);
	bt_ctf_field_type_put(ret_field_type);
	bt_ctf_event_class_put(event_class);
	bt_ctf_event_put(event);
}

void type_field_tests()
{
	struct bt_ctf_field *uint_12;
	struct bt_ctf_field *int_16;
	struct bt_ctf_field *string;
	struct bt_ctf_field *enumeration;
	struct bt_ctf_field_type *composite_structure_type;
	struct bt_ctf_field_type *structure_seq_type;
	struct bt_ctf_field_type *string_type;
	struct bt_ctf_field_type *sequence_type;
	struct bt_ctf_field_type *uint_8_type;
	struct bt_ctf_field_type *int_16_type;
	struct bt_ctf_field_type *uint_12_type =
		bt_ctf_field_type_integer_create(12);
	struct bt_ctf_field_type *enumeration_type;
	struct bt_ctf_field_type *enumeration_sequence_type;
	struct bt_ctf_field_type *enumeration_array_type;
	struct bt_ctf_field_type *returned_type;
	const char *ret_string;

	returned_type = bt_ctf_field_get_type(NULL);
	ok(!returned_type, "bt_ctf_field_get_type handles NULL correctly");

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
	ok(bt_ctf_field_type_integer_get_size(NULL) < 0,
		"bt_ctf_field_type_integer_get_size handles NULL correctly");
	ok(bt_ctf_field_type_integer_get_size(uint_12_type) == 12,
		"bt_ctf_field_type_integer_get_size returns a correct value");
	ok(bt_ctf_field_type_integer_get_signed(NULL) < 0,
		"bt_ctf_field_type_integer_get_signed handles NULL correctly");
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
	ok(bt_ctf_field_type_get_byte_order(NULL) ==
		BT_CTF_BYTE_ORDER_UNKNOWN,
		"bt_ctf_field_type_get_byte_order handles NULL correctly");

	ok(bt_ctf_field_type_get_type_id(NULL) ==
		CTF_TYPE_UNKNOWN,
		"bt_ctf_field_type_get_type_id handles NULL correctly");
	ok(bt_ctf_field_type_get_type_id(uint_12_type) ==
		CTF_TYPE_INTEGER,
		"bt_ctf_field_type_get_type_id returns a correct value with an integer type");

	ok(bt_ctf_field_type_integer_get_base(NULL) ==
		BT_CTF_INTEGER_BASE_UNKNOWN,
		"bt_ctf_field_type_integer_get_base handles NULL correctly");
	ok(bt_ctf_field_type_integer_get_base(uint_12_type) ==
		BT_CTF_INTEGER_BASE_HEXADECIMAL,
		"bt_ctf_field_type_integer_get_base returns a correct value");

	ok(bt_ctf_field_type_integer_set_encoding(NULL, CTF_STRING_ASCII) < 0,
		"bt_ctf_field_type_integer_set_encoding handles NULL correctly");
	ok(bt_ctf_field_type_integer_set_encoding(uint_12_type,
		(enum ctf_string_encoding) 123) < 0,
		"bt_ctf_field_type_integer_set_encoding handles invalid encodings correctly");
	ok(bt_ctf_field_type_integer_set_encoding(uint_12_type,
		CTF_STRING_UTF8) == 0,
		"Set integer type encoding to UTF8");
	ok(bt_ctf_field_type_integer_get_encoding(NULL) == CTF_STRING_UNKNOWN,
		"bt_ctf_field_type_integer_get_encoding handles NULL correctly");
	ok(bt_ctf_field_type_integer_get_encoding(uint_12_type) == CTF_STRING_UTF8,
		"bt_ctf_field_type_integer_get_encoding returns a correct value");

	int_16_type = bt_ctf_field_type_integer_create(16);
	bt_ctf_field_type_integer_set_signed(int_16_type, 1);
	ok(bt_ctf_field_type_integer_get_signed(int_16_type) == 1,
		"bt_ctf_field_type_integer_get_signed returns a correct value for signed types");
	uint_8_type = bt_ctf_field_type_integer_create(8);
	sequence_type =
		bt_ctf_field_type_sequence_create(int_16_type, "seq_len");
	ok(sequence_type, "Create a sequence of int16_t type");
	ok(bt_ctf_field_type_get_type_id(sequence_type) ==
		CTF_TYPE_SEQUENCE,
		"bt_ctf_field_type_get_type_id returns a correct value with a sequence type");

	ok(bt_ctf_field_type_sequence_get_length_field_name(NULL) == NULL,
		"bt_ctf_field_type_sequence_get_length_field_name handles NULL correctly");
	ret_string = bt_ctf_field_type_sequence_get_length_field_name(
		sequence_type);
	ok(!strcmp(ret_string, "seq_len"),
		"bt_ctf_field_type_sequence_get_length_field_name returns the correct value");
	ok(bt_ctf_field_type_sequence_get_element_type(NULL) == NULL,
		"bt_ctf_field_type_sequence_get_element_type handles NULL correctly");
	returned_type = bt_ctf_field_type_sequence_get_element_type(
		sequence_type);
	ok(returned_type == int_16_type,
		"bt_ctf_field_type_sequence_get_element_type returns the correct type");
	bt_ctf_field_type_put(returned_type);

	string_type = bt_ctf_field_type_string_create();
	ok(string_type, "Create a string type");
	ok(bt_ctf_field_type_string_set_encoding(string_type,
		CTF_STRING_NONE),
		"Reject invalid \"None\" string encoding");
	ok(bt_ctf_field_type_string_set_encoding(string_type,
		42),
		"Reject invalid string encoding");
	ok(bt_ctf_field_type_string_set_encoding(string_type,
		CTF_STRING_ASCII) == 0,
		"Set string encoding to ASCII");

	ok(bt_ctf_field_type_string_get_encoding(NULL) ==
		CTF_STRING_UNKNOWN,
		"bt_ctf_field_type_string_get_encoding handles NULL correctly");
	ok(bt_ctf_field_type_string_get_encoding(string_type) ==
		CTF_STRING_ASCII,
		"bt_ctf_field_type_string_get_encoding returns the correct value");

	structure_seq_type = bt_ctf_field_type_structure_create();
	ok(bt_ctf_field_type_get_type_id(structure_seq_type) ==
		CTF_TYPE_STRUCT,
		"bt_ctf_field_type_get_type_id returns a correct value with a structure type");
	ok(structure_seq_type, "Create a structure type");
	ok(bt_ctf_field_type_structure_add_field(structure_seq_type,
		uint_8_type, "seq_len") == 0,
		"Add a uint8_t type to a structure");
	ok(bt_ctf_field_type_structure_add_field(structure_seq_type,
		sequence_type, "a_sequence") == 0,
		"Add a sequence type to a structure");

	ok(bt_ctf_field_type_structure_get_field_count(NULL) < 0,
		"bt_ctf_field_type_structure_get_field_count handles NULL correctly");
	ok(bt_ctf_field_type_structure_get_field_count(structure_seq_type) == 2,
		"bt_ctf_field_type_structure_get_field_count returns a correct value");

	ok(bt_ctf_field_type_structure_get_field(NULL,
		&ret_string, &returned_type, 1) < 0,
		"bt_ctf_field_type_structure_get_field handles a NULL type correctly");
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		NULL, &returned_type, 1) < 0,
		"bt_ctf_field_type_structure_get_field handles a NULL name correctly");
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, NULL, 1) < 0,
		"bt_ctf_field_type_structure_get_field handles a NULL return type correctly");
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, &returned_type, 10) < 0,
		"bt_ctf_field_type_structure_get_field handles an invalid index correctly");
	ok(bt_ctf_field_type_structure_get_field(structure_seq_type,
		&ret_string, &returned_type, 1) == 0,
		"bt_ctf_field_type_structure_get_field returns a field");
	ok(!strcmp(ret_string, "a_sequence"),
		"bt_ctf_field_type_structure_get_field returns a correct field name");
	ok(returned_type == sequence_type,
		"bt_ctf_field_type_structure_get_field returns a correct field type");
	bt_ctf_field_type_put(returned_type);

	ok(bt_ctf_field_type_structure_get_field_type_by_name(NULL, "a_sequence") == NULL,
		"bt_ctf_field_type_structure_get_field_type_by_name handles a NULL structure correctly");
	ok(bt_ctf_field_type_structure_get_field_type_by_name(structure_seq_type, NULL) == NULL,
		"bt_ctf_field_type_structure_get_field_type_by_name handles a NULL field name correctly");
	returned_type = bt_ctf_field_type_structure_get_field_type_by_name(
		structure_seq_type, "a_sequence");
	ok(returned_type == sequence_type,
		"bt_ctf_field_type_structure_get_field_type_by_name returns the correct field type");
	bt_ctf_field_type_put(returned_type);

	composite_structure_type = bt_ctf_field_type_structure_create();
	ok(bt_ctf_field_type_structure_add_field(composite_structure_type,
		string_type, "a_string") == 0,
		"Add a string type to a structure");
	ok(bt_ctf_field_type_structure_add_field(composite_structure_type,
		structure_seq_type, "inner_structure") == 0,
		"Add a structure type to a structure");

	ok(bt_ctf_field_type_structure_get_field_type_by_name(
		NULL, "a_sequence") == NULL,
		"bt_ctf_field_type_structure_get_field_type_by_name handles a NULL field correctly");
	ok(bt_ctf_field_type_structure_get_field_type_by_name(
		structure_seq_type, NULL) == NULL,
		"bt_ctf_field_type_structure_get_field_type_by_name handles a NULL field name correctly");
	returned_type = bt_ctf_field_type_structure_get_field_type_by_name(
		structure_seq_type, "a_sequence");
	ok(returned_type == sequence_type,
		"bt_ctf_field_type_structure_get_field_type_by_name returns a correct type");
	bt_ctf_field_type_put(returned_type);

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

	/* Check signed property is checked */
	ok(bt_ctf_field_signed_integer_set_value(uint_12, -52),
		"Check bt_ctf_field_signed_integer_set_value is not allowed on an unsigned integer");
	ok(bt_ctf_field_unsigned_integer_set_value(int_16, 42),
		"Check bt_ctf_field_unsigned_integer_set_value is not allowed on a signed integer");

	/* Check overflows are properly tested for */
	ok(bt_ctf_field_signed_integer_set_value(int_16, -32768) == 0,
		"Check -32768 is allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, 32767) == 0,
		"Check 32767 is allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, 32768),
		"Check 32768 is not allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, -32769),
		"Check -32769 is not allowed for a signed 16-bit integer");
	ok(bt_ctf_field_signed_integer_set_value(int_16, -42) == 0,
		"Check -42 is allowed for a signed 16-bit integer");

	ok(bt_ctf_field_unsigned_integer_set_value(uint_12, 4095) == 0,
		"Check 4095 is allowed for an unsigned 12-bit integer");
	ok(bt_ctf_field_unsigned_integer_set_value(uint_12, 4096),
		"Check 4096 is not allowed for a unsigned 12-bit integer");
	ok(bt_ctf_field_unsigned_integer_set_value(uint_12, 0) == 0,
		"Check 0 is allowed for an unsigned 12-bit integer");

	string = bt_ctf_field_create(string_type);
	ok(string, "Instanciate a string field");
	ok(bt_ctf_field_string_set_value(string, "A value") == 0,
		"Set a string's value");

	enumeration_type = bt_ctf_field_type_enumeration_create(uint_12_type);
	ok(enumeration_type,
		"Create an enumeration type with an unsigned 12-bit integer as container");
	enumeration_sequence_type = bt_ctf_field_type_sequence_create(
		enumeration_type, "count");
	ok(!enumeration_sequence_type,
		"Check enumeration types are validated when creating a sequence");
	enumeration_array_type = bt_ctf_field_type_array_create(
		enumeration_type, 10);
	ok(!enumeration_array_type,
		"Check enumeration types are validated when creating an array");
	ok(bt_ctf_field_type_structure_add_field(composite_structure_type,
		enumeration_type, "enumeration"),
		"Check enumeration types are validated when adding them as structure members");
	enumeration = bt_ctf_field_create(enumeration_type);
	ok(!enumeration,
		"Check enumeration types are validated before instantiation");

	bt_ctf_field_put(string);
	bt_ctf_field_put(uint_12);
	bt_ctf_field_put(int_16);
	bt_ctf_field_put(enumeration);
	bt_ctf_field_type_put(composite_structure_type);
	bt_ctf_field_type_put(structure_seq_type);
	bt_ctf_field_type_put(string_type);
	bt_ctf_field_type_put(sequence_type);
	bt_ctf_field_type_put(uint_8_type);
	bt_ctf_field_type_put(int_16_type);
	bt_ctf_field_type_put(uint_12_type);
	bt_ctf_field_type_put(enumeration_type);
	bt_ctf_field_type_put(enumeration_sequence_type);
	bt_ctf_field_type_put(enumeration_array_type);
	bt_ctf_field_type_put(returned_type);
}

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
		*packet_context_field = NULL, *event_context = NULL;

	ret |= bt_ctf_event_class_add_field(event_class, integer_type,
		"field_1");
	ret |= bt_ctf_event_class_add_field(event_class, string_type,
		"a_string");
	ret |= bt_ctf_stream_class_add_event_class(stream_class, event_class);
	ok(ret == 0, "Add a new event class to a stream class after writing an event");
	if (ret) {
		goto end;
	}

	event = bt_ctf_event_create(event_class);
	ret_field = bt_ctf_event_get_payload_by_index(event, 0);
	ret_field_type = bt_ctf_field_get_type(ret_field);
	ok(ret_field_type == integer_type,
		"bt_ctf_event_get_payload_by_index returns a correct field");
	bt_ctf_field_type_put(ret_field_type);
	bt_ctf_field_put(ret_field);

	ok(bt_ctf_event_get_payload_by_index(NULL, 0) == NULL,
		"bt_ctf_event_get_payload_by_index handles NULL correctly");
	ok(bt_ctf_event_get_payload_by_index(event, 4) == NULL,
		"bt_ctf_event_get_payload_by_index handles an invalid index correctly");
	bt_ctf_event_put(event);

	ok(bt_ctf_stream_get_event_context(NULL) == NULL,
		"bt_ctf_stream_get_event_context handles NULL correctly");
	event_context = bt_ctf_stream_get_event_context(stream);
	ok(event_context,
		"bt_ctf_stream_get_event_context returns a stream event context");
	ok(bt_ctf_stream_set_event_context(NULL, event_context) < 0,
		"bt_ctf_stream_set_event_context handles a NULL stream correctly");
	ok(bt_ctf_stream_set_event_context(stream, NULL) < 0,
		"bt_ctf_stream_set_event_context handles a NULL stream event context correctly");
	ok(!bt_ctf_stream_set_event_context(stream, event_context),
		"bt_ctf_stream_set_event_context correctly set a stream event context");
	ret_field = bt_ctf_field_create(integer_type);
	ok(bt_ctf_stream_set_event_context(stream, ret_field) < 0,
		"bt_ctf_stream_set_event_context rejects an event context of incorrect type");
	bt_ctf_field_put(ret_field);

	for (i = 0; i < PACKET_RESIZE_TEST_LENGTH; i++) {
		event = bt_ctf_event_create(event_class);
		struct bt_ctf_field *integer =
			bt_ctf_field_create(integer_type);
		struct bt_ctf_field *string =
			bt_ctf_field_create(string_type);

		ret |= bt_ctf_clock_set_time(clock, ++current_time);
		ret |= bt_ctf_field_unsigned_integer_set_value(integer, i);
		ret |= bt_ctf_event_set_payload(event, "field_1",
			integer);
		bt_ctf_field_put(integer);
		ret |= bt_ctf_field_string_set_value(string, "This is a test");
		ret |= bt_ctf_event_set_payload(event, "a_string",
			string);
		bt_ctf_field_put(string);

		/* Populate stream event context */
		integer = bt_ctf_field_structure_get_field(event_context,
			"common_event_context");
		ret |= bt_ctf_field_unsigned_integer_set_value(integer,
			i % 42);
		bt_ctf_field_put(integer);

		ret |= bt_ctf_stream_append_event(stream, event);
		bt_ctf_event_put(event);

		if (ret) {
			break;
		}
	}

	events_appended = !!(i == PACKET_RESIZE_TEST_LENGTH);
	ok(bt_ctf_stream_get_discarded_events_count(NULL, &ret_uint64) < 0,
		"bt_ctf_stream_get_discarded_events_count handles a NULL stream correctly");
	ok(bt_ctf_stream_get_discarded_events_count(stream, NULL) < 0,
		"bt_ctf_stream_get_discarded_events_count handles a NULL return pointer correctly");
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
	packet_context_field = bt_ctf_field_structure_get_field(packet_context,
		"custom_packet_context_field");
	bt_ctf_field_unsigned_integer_set_value(packet_context_field, 2);

	ok(bt_ctf_stream_flush(stream) == 0,
		"Flush a stream that forces a packet resize");
	ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
	ok(ret == 0 && ret_uint64 == 1000,
		"bt_ctf_stream_get_discarded_events_count returns a correct number of discarded events after a flush");
	bt_ctf_field_type_put(integer_type);
	bt_ctf_field_type_put(string_type);
	bt_ctf_field_put(packet_context);
	bt_ctf_field_put(packet_context_field);
	bt_ctf_field_put(event_context);
	bt_ctf_event_class_put(event_class);
}

void test_empty_stream(struct bt_ctf_writer *writer)
{
	int ret = 0;
	struct bt_ctf_trace *trace = NULL;
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

	stream = bt_ctf_writer_create_stream(writer, stream_class);
	if (!stream) {
		diag("Failed to create writer stream");
		ret = -1;
		goto end;
	}
end:
	ok(ret == 0,
		"Created a stream class with default attributes and an empty stream");
	bt_ctf_trace_put(trace);
	bt_ctf_stream_put(stream);
	bt_ctf_stream_class_put(stream_class);
}

void test_instanciate_event_before_stream(struct bt_ctf_writer *writer)
{
	int ret = 0;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_clock *clock = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_event *event = NULL;
	struct bt_ctf_field_type *integer_type = NULL;
	struct bt_ctf_field *integer = NULL;

	trace = bt_ctf_writer_get_trace(writer);
	if (!trace) {
		diag("Failed to get trace from writer");
		ret = -1;
		goto end;
	}

	clock = bt_ctf_trace_get_clock(trace, 0);
	if (!clock) {
		diag("Failed to get clock from trace");
		ret = -1;
		goto end;
	}

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

	integer = bt_ctf_event_get_payload_by_index(event, 0);
	if (!integer) {
		diag("Failed to get integer field payload from event");
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_unsigned_integer_set_value(integer, 1234);
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
end:
	ok(ret == 0,
		"Create an event before instanciating its associated stream");
	bt_ctf_trace_put(trace);
	bt_ctf_stream_put(stream);
	bt_ctf_stream_class_put(stream_class);
	bt_ctf_event_class_put(event_class);
	bt_ctf_event_put(event);
	bt_ctf_field_type_put(integer_type);
	bt_ctf_field_put(integer);
	bt_ctf_clock_put(clock);
}

int main(int argc, char **argv)
{
	char trace_path[] = "/tmp/ctfwriter_XXXXXX";
	char metadata_path[sizeof(trace_path) + 9];
	const char *clock_name = "test_clock";
	const char *clock_description = "This is a test clock";
	const char *returned_clock_name;
	const char *returned_clock_description;
	const uint64_t frequency = 1123456789;
	const uint64_t offset_s = 1351530929945824323;
	const uint64_t offset = 1234567;
	const uint64_t precision = 10;
	const int is_absolute = 0xFF;
	char *metadata_string;
	struct bt_ctf_writer *writer;
	struct utsname name;
	char hostname[BABELTRACE_HOST_NAME_MAX];
	struct bt_ctf_clock *clock, *ret_clock;
	struct bt_ctf_stream_class *stream_class, *ret_stream_class;
	struct bt_ctf_stream *stream1;
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

	if (argc < 3) {
		printf("Usage: tests-ctf-writer path_to_ctf_parser_test path_to_babeltrace\n");
		return -1;
	}

	plan_no_plan();

	if (!mkdtemp(trace_path)) {
		perror("# perror");
	}

	strcpy(metadata_path, trace_path);
	strcat(metadata_path + sizeof(trace_path) - 1, "/metadata");

	writer = bt_ctf_writer_create(trace_path);
	ok(writer, "bt_ctf_create succeeds in creating trace with path");

	ok(!bt_ctf_writer_get_trace(NULL),
		"bt_ctf_writer_get_trace correctly handles NULL");
	trace = bt_ctf_writer_get_trace(writer);
	ok(trace,
		"bt_ctf_writer_get_trace returns a bt_ctf_trace object");
	ok(bt_ctf_trace_set_byte_order(trace, BT_CTF_BYTE_ORDER_BIG_ENDIAN) == 0,
		"Set a trace's byte order to big endian");
	ok(bt_ctf_trace_get_byte_order(trace) == BT_CTF_BYTE_ORDER_BIG_ENDIAN,
		"bt_ctf_trace_get_byte_order returns a correct endianness");

	/* Add environment context to the trace */
	ret = gethostname(hostname, sizeof(hostname));
	if (ret < 0) {
		return ret;
	}
	ok(bt_ctf_writer_add_environment_field(writer, "host", hostname) == 0,
		"Add host (%s) environment field to writer instance",
		hostname);
	ok(bt_ctf_writer_add_environment_field(NULL, "test_field",
		"test_value"),
		"bt_ctf_writer_add_environment_field error with NULL writer");
	ok(bt_ctf_writer_add_environment_field(writer, NULL,
		"test_value"),
		"bt_ctf_writer_add_environment_field error with NULL field name");
	ok(bt_ctf_writer_add_environment_field(writer, "test_field",
		NULL),
		"bt_ctf_writer_add_environment_field error with NULL field value");
	ok(bt_ctf_trace_add_environment_field_integer(NULL, "test_env", 0),
		"bt_ctf_trace_add_environment_field_integer handles a NULL trace correctly");
	ok(bt_ctf_trace_add_environment_field_integer(trace, NULL, 0),
		"bt_ctf_trace_add_environment_field_integer handles a NULL environment field name");
	ok(!bt_ctf_trace_add_environment_field_integer(trace, "test_env",
		123456),
		"Add an integer environment field to a trace instance");

	/* Test bt_ctf_trace_get_environment_field_count */
	ok(bt_ctf_trace_get_environment_field_count(NULL) < 0,
		"bt_ctf_trace_get_environment_field_count handles a NULL trace correctly");
	ok(bt_ctf_trace_get_environment_field_count(trace) == 2,
		"bt_ctf_trace_get_environment_field_count returns a correct number of environment fields");

	/* Test bt_ctf_trace_get_environment_field_type */
	ok(bt_ctf_trace_get_environment_field_type(trace, 2) ==
		BT_ENVIRONMENT_FIELD_TYPE_UNKNOWN,
		"bt_ctf_trace_get_environment_field_type handles an invalid index correctly");
	ok(bt_ctf_trace_get_environment_field_type(NULL, 0) ==
		BT_ENVIRONMENT_FIELD_TYPE_UNKNOWN,
		"bt_ctf_trace_get_environment_field_type handles a NULL trace correctly");
	ok(bt_ctf_trace_get_environment_field_type(trace, 1) ==
		BT_ENVIRONMENT_FIELD_TYPE_INTEGER,
		"bt_ctf_trace_get_environment_field_type the correct type of environment field");

	/* Test bt_ctf_trace_get_environment_field_name */
	ok(bt_ctf_trace_get_environment_field_name(NULL, 0) == NULL,
		"bt_ctf_trace_get_environment_field_name handles a NULL trace correctly");
	ok(bt_ctf_trace_get_environment_field_name(trace, -1) == NULL,
		"bt_ctf_trace_get_environment_field_name handles an invalid index correctly");
	ret_string = bt_ctf_trace_get_environment_field_name(trace, 0);
	ok(ret_string && !strcmp(ret_string, "host"),
		"bt_ctf_trace_get_environment_field_name returns a correct field name");

	/* Test bt_ctf_trace_get_environment_field_value_string */
	ok(bt_ctf_trace_get_environment_field_value_string(NULL, 0) == NULL,
		"bt_ctf_trace_get_environment_field_value_string handles a NULL trace correctly");
	ok(bt_ctf_trace_get_environment_field_value_string(trace, -1) == NULL,
		"bt_ctf_trace_get_environment_field_value_string handles an invalid index correctly");
	ok(bt_ctf_trace_get_environment_field_value_string(trace, 1) == NULL,
		"bt_ctf_trace_get_environment_field_value_string validates environment field type");
	ret_string = bt_ctf_trace_get_environment_field_value_string(trace, 0);
	ok(ret_string && !strcmp(ret_string, hostname),
		"bt_ctf_trace_get_environment_field_value_string returns a correct value");

	/* Test bt_ctf_trace_get_environment_field_value_integer */
	ok(bt_ctf_trace_get_environment_field_value_integer(NULL, 0, &ret_int64_t) < 0,
		"bt_ctf_trace_get_environment_field_value_integer handles a NULL trace correctly");
	ok(bt_ctf_trace_get_environment_field_value_integer(trace, 42, &ret_int64_t) < 0,
		"bt_ctf_trace_get_environment_field_value_integer handles an invalid index correctly");
	ok(bt_ctf_trace_get_environment_field_value_integer(trace, 1, NULL) < 0,
		"bt_ctf_trace_get_environment_field_value_integer handles a NULL value argument correctly");
	ok(bt_ctf_trace_get_environment_field_value_integer(trace, 0, &ret_int64_t) < 0,
		"bt_ctf_trace_get_environment_field_value_integer validates environment field type");
	ok(!bt_ctf_trace_get_environment_field_value_integer(trace, 1, &ret_int64_t),
		"bt_ctf_trace_get_environment_field_value_integer returns a value");
	ok(ret_int64_t == 123456,
		"bt_ctf_trace_get_environment_field_value_integer returned a correct value");

	if (uname(&name)) {
		perror("uname");
		return -1;
	}

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
	ok(bt_ctf_clock_create(NULL) == NULL, "NULL clock name rejected");
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

	ok(bt_ctf_clock_get_offset_s(clock) == DEFAULT_CLOCK_OFFSET_S,
		"bt_ctf_clock_get_offset_s returns the correct default offset (in seconds)");
	ok(bt_ctf_clock_set_offset_s(clock, offset_s) == 0,
		"Set clock offset (seconds)");
	ok(bt_ctf_clock_get_offset_s(clock) == offset_s,
		"bt_ctf_clock_get_offset_s returns the correct default offset (in seconds) once it is set");

	ok(bt_ctf_clock_get_offset(clock) == DEFAULT_CLOCK_OFFSET,
		"bt_ctf_clock_get_frequency returns the correct default offset (in ticks)");
	ok(bt_ctf_clock_set_offset(clock, offset) == 0, "Set clock offset");
	ok(bt_ctf_clock_get_offset(clock) == offset,
		"bt_ctf_clock_get_frequency returns the correct default offset (in ticks) once it is set");

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

	ok(bt_ctf_clock_get_time(clock) == DEFAULT_CLOCK_TIME,
		"bt_ctf_clock_get_time returns the correct default time");
	ok(bt_ctf_clock_set_time(clock, current_time) == 0,
		"Set clock time");
	ok(bt_ctf_clock_get_time(clock) == current_time,
		"bt_ctf_clock_get_time returns the correct time once it is set");

	ok(bt_ctf_writer_add_clock(writer, clock) == 0,
		"Add clock to writer instance");
	ok(bt_ctf_writer_add_clock(writer, clock),
		"Verify a clock can't be added twice to a writer instance");

	ok(bt_ctf_trace_get_clock_count(NULL) < 0,
		"bt_ctf_trace_get_clock_count correctly handles NULL");
	ok(bt_ctf_trace_get_clock_count(trace) == 1,
		"bt_ctf_trace_get_clock_count returns the correct number of clocks");
	ok(!bt_ctf_trace_get_clock(NULL, 0),
		"bt_ctf_trace_get_clock correctly handles NULL");
	ok(!bt_ctf_trace_get_clock(trace, -1),
		"bt_ctf_trace_get_clock correctly handles negative indexes");
	ok(!bt_ctf_trace_get_clock(trace, 1),
		"bt_ctf_trace_get_clock correctly handles out of bound accesses");
	ret_clock = bt_ctf_trace_get_clock(trace, 0);
	ok(ret_clock == clock,
		"bt_ctf_trace_get_clock returns the right clock instance");
	bt_ctf_clock_put(ret_clock);
	ok(!bt_ctf_trace_get_clock_by_name(trace, NULL),
		"bt_ctf_trace_get_clock_by_name correctly handles NULL (trace)");
	ok(!bt_ctf_trace_get_clock_by_name(NULL, clock_name),
		"bt_ctf_trace_get_clock_by_name correctly handles NULL (clock name)");
	ok(!bt_ctf_trace_get_clock_by_name(NULL, NULL),
		"bt_ctf_trace_get_clock_by_name correctly handles NULL (both)");
	ret_clock = bt_ctf_trace_get_clock_by_name(trace, clock_name);
	ok(ret_clock == clock,
		"bt_ctf_trace_get_clock_by_name returns the right clock instance");
	bt_ctf_clock_put(ret_clock);
	ok(!bt_ctf_trace_get_clock_by_name(trace, "random"),
		"bt_ctf_trace_get_clock_by_name fails when the requested clock doesn't exist");

	ok(!bt_ctf_clock_get_name(NULL),
		"bt_ctf_clock_get_name correctly handles NULL");
	ok(!bt_ctf_clock_get_description(NULL),
		"bt_ctf_clock_get_description correctly handles NULL");
	ok(bt_ctf_clock_get_frequency(NULL) == -1ULL,
		"bt_ctf_clock_get_frequency correctly handles NULL");
	ok(bt_ctf_clock_get_precision(NULL) == -1ULL,
		"bt_ctf_clock_get_precision correctly handles NULL");
	ok(bt_ctf_clock_get_offset_s(NULL) == -1ULL,
		"bt_ctf_clock_get_offset_s correctly handles NULL");
	ok(bt_ctf_clock_get_offset(NULL) == -1ULL,
		"bt_ctf_clock_get_offset correctly handles NULL");
	ok(bt_ctf_clock_get_is_absolute(NULL) < 0,
		"bt_ctf_clock_get_is_absolute correctly handles NULL");
	ok(bt_ctf_clock_get_time(NULL) == -1ULL,
		"bt_ctf_clock_get_time correctly handles NULL");

	ok(bt_ctf_clock_set_description(NULL, NULL) < 0,
		"bt_ctf_clock_set_description correctly handles NULL clock");
	ok(bt_ctf_clock_set_frequency(NULL, frequency) < 0,
		"bt_ctf_clock_set_frequency correctly handles NULL clock");
	ok(bt_ctf_clock_set_precision(NULL, precision) < 0,
		"bt_ctf_clock_get_precision correctly handles NULL clock");
	ok(bt_ctf_clock_set_offset_s(NULL, offset_s) < 0,
		"bt_ctf_clock_set_offset_s correctly handles NULL clock");
	ok(bt_ctf_clock_set_offset(NULL, offset) < 0,
		"bt_ctf_clock_set_offset correctly handles NULL clock");
	ok(bt_ctf_clock_set_is_absolute(NULL, is_absolute) < 0,
		"bt_ctf_clock_set_is_absolute correctly handles NULL clock");
	ok(bt_ctf_clock_set_time(NULL, current_time) < 0,
		"bt_ctf_clock_set_time correctly handles NULL clock");
	ok(bt_ctf_clock_get_uuid(NULL) == NULL,
		"bt_ctf_clock_get_uuid correctly handles NULL clock");
	ret_uuid = bt_ctf_clock_get_uuid(clock);
	ok(ret_uuid,
		"bt_ctf_clock_get_uuid returns a UUID");
	if (ret_uuid) {
		memcpy(tmp_uuid, ret_uuid, sizeof(tmp_uuid));
		/* Slightly modify UUID */
		tmp_uuid[sizeof(tmp_uuid) - 1]++;
	}

	ok(bt_ctf_clock_set_uuid(NULL, tmp_uuid) < 0,
		"bt_ctf_clock_set_uuid correctly handles a NULL clock");
	ok(bt_ctf_clock_set_uuid(clock, NULL) < 0,
		"bt_ctf_clock_set_uuid correctly handles a NULL UUID");
	ok(bt_ctf_clock_set_uuid(clock, tmp_uuid) == 0,
		"bt_ctf_clock_set_uuid sets a new uuid succesfully");
	ret_uuid = bt_ctf_clock_get_uuid(clock);
	ok(ret_uuid,
		"bt_ctf_clock_get_uuid returns a UUID after setting a new one");
	ok(uuid_match(ret_uuid, tmp_uuid),
		"bt_ctf_clock_get_uuid returns the correct UUID after setting a new one");

	/* Define a stream class */
	stream_class = bt_ctf_stream_class_create("test_stream");

	ok(bt_ctf_stream_class_get_name(NULL) == NULL,
		"bt_ctf_stream_class_get_name handles NULL correctly");
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
	bt_ctf_clock_put(ret_clock);

	/* Test the event fields and event types APIs */
	type_field_tests();

	ok(bt_ctf_stream_class_get_id(stream_class) < 0,
		"bt_ctf_stream_class_get_id returns an error when no id is set");
	ok(bt_ctf_stream_class_get_id(NULL) < 0,
		"bt_ctf_stream_class_get_id handles NULL correctly");
	ok(bt_ctf_stream_class_set_id(NULL, 123) < 0,
		"bt_ctf_stream_class_set_id handles NULL correctly");
	ok(bt_ctf_stream_class_set_id(stream_class, 123) == 0,
		"Set an stream class' id");
	ok(bt_ctf_stream_class_get_id(stream_class) == 123,
		"bt_ctf_stream_class_get_id returns the correct value");

	/* Validate default event header fields */
	ok(bt_ctf_stream_class_get_event_header_type(NULL) == NULL,
		"bt_ctf_stream_class_get_event_header_type handles NULL correctly");
	ret_field_type = bt_ctf_stream_class_get_event_header_type(
		stream_class);
	ok(ret_field_type,
		"bt_ctf_stream_class_get_event_header_type returns an event header type");
	ok(bt_ctf_field_type_get_type_id(ret_field_type) == CTF_TYPE_STRUCT,
		"Default event header type is a structure");
	event_header_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
		ret_field_type, "id");
	ok(event_header_field_type,
		"Default event header type contains an \"id\" field");
	ok(bt_ctf_field_type_get_type_id(
		event_header_field_type) == CTF_TYPE_INTEGER,
		"Default event header \"id\" field is an integer");
	bt_ctf_field_type_put(event_header_field_type);
	event_header_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(
		ret_field_type, "timestamp");
	ok(event_header_field_type,
		"Default event header type contains a \"timestamp\" field");
	ok(bt_ctf_field_type_get_type_id(
		event_header_field_type) == CTF_TYPE_INTEGER,
		"Default event header \"timestamp\" field is an integer");
	bt_ctf_field_type_put(event_header_field_type);
	bt_ctf_field_type_put(ret_field_type);

	/* Add a custom trace packet header field */
	ok(bt_ctf_trace_get_packet_header_type(NULL) == NULL,
		"bt_ctf_trace_get_packet_header_type handles NULL correctly");
	packet_header_type = bt_ctf_trace_get_packet_header_type(trace);
	ok(packet_header_type,
		"bt_ctf_trace_get_packet_header_type returns a packet header");
	ok(bt_ctf_field_type_get_type_id(packet_header_type) == CTF_TYPE_STRUCT,
		"bt_ctf_trace_get_packet_header_type returns a packet header of type struct");
	ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		packet_header_type, "magic");
	ok(ret_field_type, "Default packet header type contains a \"magic\" field");
	bt_ctf_field_type_put(ret_field_type);
	ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		packet_header_type, "uuid");
	ok(ret_field_type, "Default packet header type contains a \"uuid\" field");
	bt_ctf_field_type_put(ret_field_type);
	ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		packet_header_type, "stream_id");
	ok(ret_field_type, "Default packet header type contains a \"stream_id\" field");
	bt_ctf_field_type_put(ret_field_type);

	packet_header_field_type = bt_ctf_field_type_integer_create(22);
	ok(!bt_ctf_field_type_structure_add_field(packet_header_type,
		packet_header_field_type, "custom_trace_packet_header_field"),
		"Added a custom trace packet header field successfully");

	ok(bt_ctf_trace_set_packet_header_type(NULL, packet_header_type) < 0,
		"bt_ctf_trace_set_packet_header_type handles a NULL trace correctly");
	ok(bt_ctf_trace_set_packet_header_type(trace, NULL) < 0,
		"bt_ctf_trace_set_packet_header_type handles a NULL packet_header_type correctly");
	ok(!bt_ctf_trace_set_packet_header_type(trace, packet_header_type),
		"Set a trace packet_header_type successfully");

	ok(bt_ctf_stream_class_get_packet_context_type(NULL) == NULL,
		"bt_ctf_stream_class_get_packet_context_type handles NULL correctly");

	/* Add a custom field to the stream class' packet context */
	packet_context_type = bt_ctf_stream_class_get_packet_context_type(stream_class);
	ok(packet_context_type,
		"bt_ctf_stream_class_get_packet_context_type returns a packet context type.");
	ok(bt_ctf_field_type_get_type_id(packet_context_type) == CTF_TYPE_STRUCT,
		"Packet context is a structure");

	ok(bt_ctf_stream_class_set_packet_context_type(NULL, packet_context_type),
		"bt_ctf_stream_class_set_packet_context_type handles a NULL stream class correctly");
	ok(bt_ctf_stream_class_set_packet_context_type(stream_class, NULL),
		"bt_ctf_stream_class_set_packet_context_type handles a NULL packet context type correctly");

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
	ok(bt_ctf_stream_class_get_event_context_type(NULL) == NULL,
		"bt_ctf_stream_class_get_event_context_type handles NULL correctly");
	ok(bt_ctf_stream_class_get_event_context_type(
		stream_class) == NULL,
		"bt_ctf_stream_class_get_event_context_type returns NULL when no stream event context type was set.");
	stream_event_context_type = bt_ctf_field_type_structure_create();
	bt_ctf_field_type_structure_add_field(stream_event_context_type,
		integer_type, "common_event_context");

	ok(bt_ctf_stream_class_set_event_context_type(NULL,
		stream_event_context_type) < 0,
		"bt_ctf_stream_class_set_event_context_type handles a NULL stream_class correctly");
	ok(bt_ctf_stream_class_set_event_context_type(stream_class,
		NULL) < 0,
		"bt_ctf_stream_class_set_event_context_type handles a NULL event_context correctly");
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
	bt_ctf_field_type_put(ret_field_type);

	/* Instantiate a stream and append events */
	stream1 = bt_ctf_writer_create_stream(writer, stream_class);
	ok(stream1, "Instanciate a stream class from writer");

	ok(bt_ctf_stream_get_class(NULL) == NULL,
		"bt_ctf_stream_get_class correctly handles NULL");
	ret_stream_class = bt_ctf_stream_get_class(stream1);
	ok(ret_stream_class,
		"bt_ctf_stream_get_class returns a stream class");
	ok(ret_stream_class == stream_class,
		"Returned stream class is of the correct type");

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
	bt_ctf_field_type_put(ret_field_type);
	packet_header_field = bt_ctf_field_structure_get_field(packet_header,
		"custom_trace_packet_header_field");
	ok(packet_header_field,
		"Packet header structure contains a custom field with the appropriate name");
	ret_field_type = bt_ctf_field_get_type(packet_header_field);
	ok(ret_field_type == packet_header_field_type,
		"Custom packet header field is of the expected type");
	ok(!bt_ctf_field_unsigned_integer_set_value(packet_header_field,
		54321), "Set custom packet header value successfully");
	ok(bt_ctf_stream_set_packet_header(stream1, NULL) < 0,
		"bt_ctf_stream_set_packet_header handles a NULL packet header correctly");
	ok(bt_ctf_stream_set_packet_header(NULL, packet_header) < 0,
		"bt_ctf_stream_set_packet_header handles a NULL stream correctly");
	ok(bt_ctf_stream_set_packet_header(stream1, packet_header_field) < 0,
		"bt_ctf_stream_set_packet_header rejects a packet header of the wrong type");
	ok(!bt_ctf_stream_set_packet_header(stream1, packet_header),
		"Successfully set a stream's packet header");

	test_instanciate_event_before_stream(writer);

	append_simple_event(stream_class, stream1, clock);

	packet_resize_test(stream_class, stream1, clock);

	append_complex_event(stream_class, stream1, clock);

	test_empty_stream(writer);

	metadata_string = bt_ctf_writer_get_metadata_string(writer);
	ok(metadata_string, "Get metadata string");

	bt_ctf_writer_flush_metadata(writer);
	validate_metadata(argv[1], metadata_path);
	validate_trace(argv[2], trace_path);

	bt_ctf_clock_put(clock);
	bt_ctf_stream_class_put(stream_class);
	bt_ctf_stream_class_put(ret_stream_class);
	bt_ctf_writer_put(writer);
	bt_ctf_stream_put(stream1);
	bt_ctf_field_type_put(packet_context_type);
	bt_ctf_field_type_put(packet_context_field_type);
	bt_ctf_field_type_put(integer_type);
	bt_ctf_field_type_put(stream_event_context_type);
	bt_ctf_field_type_put(ret_field_type);
	bt_ctf_field_type_put(packet_header_type);
	bt_ctf_field_type_put(packet_header_field_type);
	bt_ctf_field_put(packet_header);
	bt_ctf_field_put(packet_header_field);
	bt_ctf_trace_put(trace);
	free(metadata_string);

	/* Remove all trace files and delete temporary trace directory */
	DIR *trace_dir = opendir(trace_path);
	if (!trace_dir) {
		perror("# opendir");
		return -1;
	}

	struct dirent *entry;
	while ((entry = readdir(trace_dir))) {
		if (entry->d_type == DT_REG) {
			unlinkat(dirfd(trace_dir), entry->d_name, 0);
		}
	}

	rmdir(trace_path);
	closedir(trace_dir);
	return 0;
}
