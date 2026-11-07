/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2013-2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#include <babeltrace2-ctf-writer/clock-class.h>
#include <babeltrace2-ctf-writer/clock.h>
#include <babeltrace2-ctf-writer/event-fields.h>
#include <babeltrace2-ctf-writer/event-types.h>
#include <babeltrace2-ctf-writer/event.h>
#include <babeltrace2-ctf-writer/stream-class.h>
#include <babeltrace2-ctf-writer/stream.h>
#include <babeltrace2-ctf-writer/trace.h>
#include <babeltrace2-ctf-writer/writer.h>
#include <fcntl.h>
#include <float.h>
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <babeltrace2/babeltrace.h>

#include "common/uuid.h"
#include "compat/limits.h"

#include "catch2/catch_test_macros.hpp"

extern "C" {

#include "common.h"
}

#ifdef __FreeBSD__
/* Required for WIFEXITED and WEXITSTATUS */
#    include <sys/wait.h>
#endif

#define METADATA_LINE_SIZE            512
#define SEQUENCE_TEST_LENGTH          10
#define ARRAY_TEST_LENGTH             5
#define PACKET_RESIZE_TEST_DEF_LENGTH 100000

#define DEFAULT_CLOCK_FREQ        1000000000
#define DEFAULT_CLOCK_PRECISION   1
#define DEFAULT_CLOCK_OFFSET      0
#define DEFAULT_CLOCK_OFFSET_S    0
#define DEFAULT_CLOCK_IS_ABSOLUTE 0
#define DEFAULT_CLOCK_TIME        0
#define DEFAULT_CLOCK_VALUE       0

#define NR_TESTS 325

struct bt_utsname
{
    char sysname[BABELTRACE_HOST_NAME_MAX];
    char nodename[BABELTRACE_HOST_NAME_MAX];
    char release[BABELTRACE_HOST_NAME_MAX];
    char version[BABELTRACE_HOST_NAME_MAX];
    char machine[BABELTRACE_HOST_NAME_MAX];
};

static int64_t current_time = 42;
static unsigned int packet_resize_test_length = PACKET_RESIZE_TEST_DEF_LENGTH;

/* Return 1 if uuids match, zero if different. */
static int uuid_match(const uint8_t *uuid_a, const uint8_t *uuid_b)
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

static void append_simple_event(struct bt_ctf_stream_class *stream_class,
                                struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
    /* Create and add a simple event class */
    struct bt_ctf_event_class *simple_event_class = bt_ctf_event_class_create("Simple Event");
    struct bt_ctf_field_type *uint_12_type = bt_ctf_field_type_integer_create(12);
    struct bt_ctf_field_type *int_64_type = bt_ctf_field_type_integer_create(64);
    struct bt_ctf_field_type *float_type = bt_ctf_field_type_floating_point_create();
    struct bt_ctf_field_type *enum_type;
    struct bt_ctf_field_type *enum_type_unsigned =
        bt_ctf_field_type_enumeration_create(uint_12_type);
    struct bt_ctf_field_type *event_context_type = bt_ctf_field_type_structure_create();
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

    REQUIRE(uint_12_type);

    REQUIRE(!bt_ctf_field_type_integer_set_signed(int_64_type, 1));
    REQUIRE(int_64_type);
    enum_type = bt_ctf_field_type_enumeration_create(int_64_type);

    returned_type = bt_ctf_field_type_enumeration_get_container_field_type(enum_type);
    REQUIRE(returned_type == int_64_type);
    REQUIRE(!bt_ctf_field_type_enumeration_create(enum_type));
    bt_ctf_object_put_ref(returned_type);

    bt_ctf_field_type_set_alignment(float_type, 32);
    REQUIRE(bt_ctf_field_type_get_alignment(float_type) == 32);

    REQUIRE(bt_ctf_field_type_floating_point_set_exponent_digits(float_type, 11) == 0);
    REQUIRE(bt_ctf_field_type_floating_point_set_mantissa_digits(float_type, 53) == 0);

    REQUIRE(bt_ctf_field_type_floating_point_get_exponent_digits(float_type) == 11);
    REQUIRE(bt_ctf_field_type_floating_point_get_mantissa_digits(float_type) == 53);

    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_negative_test, -12345,
                                                      0) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, "escaping; \"test\"", 1, 1) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, "\tanother \'escaping\'\n test\"",
                                                      2, 4) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, "event clock int float", 5, 22) ==
            0);
    bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test, 42, 42);
    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test, 43, 51) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, "something", -500, -400) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_add_mapping(enum_type, mapping_name_test, -54, -55));
    bt_ctf_field_type_enumeration_add_mapping(enum_type, "another entry", -42000, -13000);

    REQUIRE(bt_ctf_event_class_add_field(simple_event_class, enum_type, "enum_field") == 0);

    REQUIRE(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(
                enum_type, 0, NULL, &ret_range_start_int64_t, &ret_range_end_int64_t) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(enum_type, 0, &ret_char, NULL,
                                                                      &ret_range_end_int64_t) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(
                enum_type, 0, &ret_char, &ret_range_start_int64_t, NULL) == 0);
    /* Assumes entries are sorted by range_start values. */
    REQUIRE(bt_ctf_field_type_enumeration_signed_get_mapping_by_index(
                enum_type, 6, &ret_char, &ret_range_start_int64_t, &ret_range_end_int64_t) == 0);
    REQUIRE(strcmp(ret_char, mapping_name_test) == 0);
    REQUIRE(ret_range_start_int64_t == 42);
    REQUIRE(ret_range_end_int64_t == 42);

    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
                                                               "escaping; \"test\"", 0, 0) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(
                enum_type_unsigned, "\tanother \'escaping\'\n test\"", 1, 4) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(
                enum_type_unsigned, "event clock int float", 5, 22) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
                                                               mapping_name_test, 42, 42) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
                                                               mapping_name_test, 43, 51) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned, "something", 7,
                                                               8) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(enum_type_unsigned,
                                                               mapping_name_test, 55, 54));
    REQUIRE(bt_ctf_event_class_add_field(simple_event_class, enum_type_unsigned,
                                         "enum_field_unsigned") == 0);

    REQUIRE(bt_ctf_field_type_enumeration_get_mapping_count(enum_type_unsigned) == 6);

    REQUIRE(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(
                enum_type_unsigned, 0, NULL, &ret_range_start_uint64_t, &ret_range_end_uint64_t) ==
            0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(
                enum_type_unsigned, 0, &ret_char, NULL, &ret_range_end_uint64_t) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(
                enum_type_unsigned, 0, &ret_char, &ret_range_start_uint64_t, NULL) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_get_mapping_by_index(
                enum_type_unsigned, 4, &ret_char, &ret_range_start_uint64_t,
                &ret_range_end_uint64_t) == 0);
    REQUIRE(strcmp(ret_char, mapping_name_test) == 0);
    REQUIRE(ret_range_start_uint64_t == 42);
    REQUIRE(ret_range_end_uint64_t == 42);

    bt_ctf_event_class_add_field(simple_event_class, uint_12_type, "integer_field");
    bt_ctf_event_class_add_field(simple_event_class, float_type, "float_field");

    ret = bt_ctf_event_class_set_id(simple_event_class, 13);
    REQUIRE(ret == 0);

    /* Set an event context type which will contain a single integer. */
    REQUIRE(!bt_ctf_field_type_structure_add_field(event_context_type, uint_12_type,
                                                   "event_specific_context"));

    REQUIRE(bt_ctf_event_class_set_context_field_type(NULL, event_context_type) < 0);
    REQUIRE(!bt_ctf_event_class_set_context_field_type(simple_event_class, event_context_type));
    returned_type = bt_ctf_event_class_get_context_field_type(simple_event_class);
    REQUIRE(returned_type == event_context_type);
    bt_ctf_object_put_ref(returned_type);

    REQUIRE(!bt_ctf_stream_class_add_event_class(stream_class, simple_event_class));

    /*
	 * bt_ctf_stream_class_add_event_class() copies the field types
	 * of simple_event_class, so we retrieve the new ones to create
	 * the appropriate fields.
	 */
    BT_CTF_OBJECT_PUT_REF_AND_RESET(event_context_type);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(event_payload_type);
    event_payload_type = bt_ctf_event_class_get_payload_field_type(simple_event_class);
    REQUIRE(event_payload_type);
    event_context_type = bt_ctf_event_class_get_context_field_type(simple_event_class);
    REQUIRE(event_context_type);
    ep_integer_field_type =
        bt_ctf_field_type_structure_get_field_type_by_name(event_payload_type, "integer_field");
    REQUIRE(ep_integer_field_type);
    ep_enum_field_type =
        bt_ctf_field_type_structure_get_field_type_by_name(event_payload_type, "enum_field");
    REQUIRE(ep_enum_field_type);
    ep_enum_field_unsigned_type = bt_ctf_field_type_structure_get_field_type_by_name(
        event_payload_type, "enum_field_unsigned");
    REQUIRE(ep_enum_field_unsigned_type);

    REQUIRE(bt_ctf_stream_class_get_event_class_count(stream_class) == 1);
    ret_event_class = bt_ctf_stream_class_get_event_class_by_index(stream_class, 0);
    REQUIRE(ret_event_class == simple_event_class);
    bt_ctf_object_put_ref(ret_event_class);
    REQUIRE(!bt_ctf_stream_class_get_event_class_by_id(stream_class, 2));
    ret_event_class = bt_ctf_stream_class_get_event_class_by_id(stream_class, 13);
    REQUIRE(ret_event_class == simple_event_class);
    bt_ctf_object_put_ref(ret_event_class);

    simple_event = bt_ctf_event_create(simple_event_class);
    REQUIRE(simple_event);

    integer_field = bt_ctf_field_create(ep_integer_field_type);
    bt_ctf_field_integer_unsigned_set_value(integer_field, 42);
    REQUIRE(bt_ctf_event_set_payload(simple_event, "integer_field", integer_field) == 0);

    float_field = bt_ctf_event_get_payload(simple_event, "float_field");
    bt_ctf_field_floating_point_set_value(float_field, double_test_value);
    REQUIRE(!bt_ctf_field_floating_point_get_value(float_field, &ret_double));
    REQUIRE(fabs(ret_double - double_test_value) <= DBL_EPSILON);

    enum_field = bt_ctf_field_create(ep_enum_field_type);
    REQUIRE(enum_field);

    enum_container_field = bt_ctf_field_enumeration_get_container(enum_field);
    REQUIRE(bt_ctf_field_integer_signed_set_value(enum_container_field, -42) == 0);
    ret = bt_ctf_event_set_payload(simple_event, "enum_field", enum_field);
    REQUIRE(!ret);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(iter);

    enum_field_unsigned = bt_ctf_field_create(ep_enum_field_unsigned_type);
    REQUIRE(enum_field_unsigned);
    enum_container_field_unsigned = bt_ctf_field_enumeration_get_container(enum_field_unsigned);
    REQUIRE(bt_ctf_field_integer_unsigned_set_value(enum_container_field_unsigned, 42) == 0);
    ret = bt_ctf_event_set_payload(simple_event, "enum_field_unsigned", enum_field_unsigned);
    REQUIRE(!ret);

    REQUIRE(bt_ctf_clock_set_time(clock, current_time) == 0);

    /* Populate stream event context */
    stream_event_context = bt_ctf_event_get_stream_event_context(simple_event);
    REQUIRE(stream_event_context);
    stream_event_context_field =
        bt_ctf_field_structure_get_field_by_name(stream_event_context, "common_event_context");
    bt_ctf_field_integer_unsigned_set_value(stream_event_context_field, 42);

    /* Populate the event's context */
    event_context = bt_ctf_event_get_context(simple_event);
    REQUIRE(event_context);
    returned_type = bt_ctf_field_get_type(event_context);
    REQUIRE(returned_type == event_context_type);
    event_context_field =
        bt_ctf_field_structure_get_field_by_name(event_context, "event_specific_context");
    REQUIRE(!bt_ctf_field_integer_unsigned_set_value(event_context_field, 1234));
    REQUIRE(!bt_ctf_event_set_context(simple_event, event_context));

    REQUIRE(bt_ctf_stream_append_event(stream, simple_event) == 0);

    packet_context = bt_ctf_stream_get_packet_context(stream);
    REQUIRE(packet_context);

    packet_context_field = bt_ctf_field_structure_get_field_by_name(packet_context, "packet_size");
    REQUIRE(packet_context_field);
    bt_ctf_object_put_ref(packet_context_field);
    packet_context_field =
        bt_ctf_field_structure_get_field_by_name(packet_context, "custom_packet_context_field");
    REQUIRE(bt_ctf_field_integer_unsigned_set_value(packet_context_field, 8) == 0);

    REQUIRE(bt_ctf_stream_set_packet_context(stream, packet_context) == 0);

    REQUIRE(bt_ctf_stream_flush(stream) == 0);

    bt_ctf_object_put_ref(simple_event_class);
    bt_ctf_object_put_ref(simple_event);
    bt_ctf_object_put_ref(uint_12_type);
    bt_ctf_object_put_ref(int_64_type);
    bt_ctf_object_put_ref(float_type);
    bt_ctf_object_put_ref(enum_type);
    bt_ctf_object_put_ref(enum_type_unsigned);
    bt_ctf_object_put_ref(returned_type);
    bt_ctf_object_put_ref(event_context_type);
    bt_ctf_object_put_ref(integer_field);
    bt_ctf_object_put_ref(float_field);
    bt_ctf_object_put_ref(enum_field);
    bt_ctf_object_put_ref(enum_field_unsigned);
    bt_ctf_object_put_ref(enum_container_field);
    bt_ctf_object_put_ref(enum_container_field_unsigned);
    bt_ctf_object_put_ref(packet_context);
    bt_ctf_object_put_ref(packet_context_field);
    bt_ctf_object_put_ref(stream_event_context);
    bt_ctf_object_put_ref(stream_event_context_field);
    bt_ctf_object_put_ref(event_context);
    bt_ctf_object_put_ref(event_context_field);
    bt_ctf_object_put_ref(event_payload_type);
    bt_ctf_object_put_ref(ep_integer_field_type);
    bt_ctf_object_put_ref(ep_enum_field_type);
    bt_ctf_object_put_ref(ep_enum_field_unsigned_type);
    bt_ctf_object_put_ref(iter);
}

static void append_complex_event(struct bt_ctf_stream_class *stream_class,
                                 struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
    int i;
    struct event_class_attrs_counts;
    const char *complex_test_event_string = "Complex Test Event";
    const char *test_string_1 = "Test ";
    const char *test_string_2 = "string ";
    const char *test_string_3 = "abcdefghi";
    const char *test_string_4 = "abcd\0efg\0hi";
    const char *test_string_cat = "Test string abcdeefg";
    struct bt_ctf_field_type *uint_35_type = bt_ctf_field_type_integer_create(35);
    struct bt_ctf_field_type *int_16_type = bt_ctf_field_type_integer_create(16);
    struct bt_ctf_field_type *uint_3_type = bt_ctf_field_type_integer_create(3);
    struct bt_ctf_field_type *enum_variant_type = bt_ctf_field_type_enumeration_create(uint_3_type);
    struct bt_ctf_field_type *variant_type =
        bt_ctf_field_type_variant_create(enum_variant_type, "variant_selector");
    struct bt_ctf_field_type *string_type = bt_ctf_field_type_string_create();
    struct bt_ctf_field_type *sequence_type;
    struct bt_ctf_field_type *array_type;
    struct bt_ctf_field_type *inner_structure_type = bt_ctf_field_type_structure_create();
    struct bt_ctf_field_type *complex_structure_type = bt_ctf_field_type_structure_create();
    struct bt_ctf_field_type *ret_field_type;
    struct bt_ctf_event_class *event_class;
    struct bt_ctf_event *event;
    struct bt_ctf_field *uint_35_field, *int_16_field, *a_string_field, *inner_structure_field,
        *complex_structure_field, *a_sequence_field, *enum_variant_field, *enum_container_field,
        *variant_field, *an_array_field, *stream_event_ctx_field, *stream_event_ctx_int_field;
    uint64_t ret_unsigned_int;
    int64_t ret_signed_int;
    const char *ret_string;
    struct bt_ctf_stream_class *ret_stream_class;
    struct bt_ctf_event_class *ret_event_class;
    struct bt_ctf_field *packet_context, *packet_context_field;

    REQUIRE(bt_ctf_field_type_set_alignment(int_16_type, 0));
    REQUIRE(bt_ctf_field_type_set_alignment(int_16_type, 3));
    REQUIRE(bt_ctf_field_type_set_alignment(int_16_type, 24));
    REQUIRE(!bt_ctf_field_type_set_alignment(int_16_type, 4));
    REQUIRE(!bt_ctf_field_type_set_alignment(int_16_type, 32));
    REQUIRE(!bt_ctf_field_type_integer_set_signed(int_16_type, 1));
    REQUIRE(!bt_ctf_field_type_integer_set_base(uint_35_type, BT_CTF_INTEGER_BASE_HEXADECIMAL));

    array_type = bt_ctf_field_type_array_create(int_16_type, ARRAY_TEST_LENGTH);
    sequence_type = bt_ctf_field_type_sequence_create(int_16_type, "seq_len");

    ret_field_type = bt_ctf_field_type_array_get_element_field_type(array_type);
    REQUIRE(ret_field_type == int_16_type);
    bt_ctf_object_put_ref(ret_field_type);

    REQUIRE(bt_ctf_field_type_array_get_length(array_type) == ARRAY_TEST_LENGTH);

    REQUIRE(
        bt_ctf_field_type_structure_add_field(inner_structure_type, inner_structure_type, "yes"));
    REQUIRE(!bt_ctf_field_type_structure_add_field(inner_structure_type, uint_35_type, "seq_len"));
    REQUIRE(
        !bt_ctf_field_type_structure_add_field(inner_structure_type, sequence_type, "a_sequence"));
    REQUIRE(!bt_ctf_field_type_structure_add_field(inner_structure_type, array_type, "an_array"));

    bt_ctf_field_type_enumeration_add_mapping(enum_variant_type, "UINT3_TYPE", 0, 0);
    bt_ctf_field_type_enumeration_add_mapping(enum_variant_type, "INT16_TYPE", 1, 1);
    bt_ctf_field_type_enumeration_add_mapping(enum_variant_type, "UINT35_TYPE", 2, 7);

    REQUIRE(bt_ctf_field_type_variant_add_field(variant_type, uint_3_type, "An unknown entry"));
    REQUIRE(bt_ctf_field_type_variant_add_field(variant_type, uint_3_type, "UINT3_TYPE") == 0);
    REQUIRE(!bt_ctf_field_type_variant_add_field(variant_type, int_16_type, "INT16_TYPE"));
    REQUIRE(!bt_ctf_field_type_variant_add_field(variant_type, uint_35_type, "UINT35_TYPE"));

    ret_field_type = bt_ctf_field_type_variant_get_tag_field_type(variant_type);
    REQUIRE(ret_field_type == enum_variant_type);
    bt_ctf_object_put_ref(ret_field_type);

    ret_string = bt_ctf_field_type_variant_get_tag_name(variant_type);
    REQUIRE((ret_string ? strcmp(ret_string, "variant_selector") == 0 : 0));
    ret_field_type = bt_ctf_field_type_variant_get_field_type_by_name(variant_type, "INT16_TYPE");
    REQUIRE(ret_field_type == int_16_type);
    bt_ctf_object_put_ref(ret_field_type);

    REQUIRE(bt_ctf_field_type_variant_get_field_count(variant_type) == 3);

    REQUIRE(bt_ctf_field_type_variant_get_field_by_index(variant_type, NULL, &ret_field_type, 0) ==
            0);
    bt_ctf_object_put_ref(ret_field_type);
    REQUIRE(bt_ctf_field_type_variant_get_field_by_index(variant_type, &ret_string, NULL, 0) == 0);
    REQUIRE(bt_ctf_field_type_variant_get_field_by_index(variant_type, &ret_string, &ret_field_type,
                                                         1) == 0);
    REQUIRE(strcmp("INT16_TYPE", ret_string) == 0);
    REQUIRE(ret_field_type == int_16_type);
    bt_ctf_object_put_ref(ret_field_type);

    REQUIRE(!bt_ctf_field_type_structure_add_field(complex_structure_type, enum_variant_type,
                                                   "variant_selector"));
    REQUIRE(!bt_ctf_field_type_structure_add_field(complex_structure_type, string_type, "string"));
    REQUIRE(!bt_ctf_field_type_structure_add_field(complex_structure_type, variant_type,
                                                   "variant_value"));
    REQUIRE(!bt_ctf_field_type_structure_add_field(complex_structure_type, inner_structure_type,
                                                   "inner_structure"));

    event_class = bt_ctf_event_class_create(complex_test_event_string);
    REQUIRE(event_class);
    REQUIRE(bt_ctf_event_class_add_field(event_class, uint_35_type, ""));
    REQUIRE(bt_ctf_event_class_add_field(event_class, NULL, "an_integer"));
    REQUIRE(bt_ctf_event_class_add_field(event_class, uint_35_type, "int"));
    REQUIRE(bt_ctf_event_class_add_field(event_class, uint_35_type, "uint_35") == 0);
    REQUIRE(bt_ctf_event_class_add_field(event_class, int_16_type, "int_16") == 0);
    REQUIRE(bt_ctf_event_class_add_field(event_class, complex_structure_type,
                                         "complex_structure") == 0);

    ret_string = bt_ctf_event_class_get_name(event_class);
    REQUIRE(strcmp(ret_string, complex_test_event_string) == 0);
    REQUIRE(bt_ctf_event_class_get_id(event_class) < 0);
    REQUIRE(bt_ctf_event_class_set_id(NULL, 42) < 0);
    REQUIRE(bt_ctf_event_class_set_id(event_class, 42) == 0);
    REQUIRE(bt_ctf_event_class_get_id(event_class) == 42);

    /* Test event class attributes */
    REQUIRE(bt_ctf_event_class_get_log_level(event_class) ==
            BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED);
    REQUIRE(!bt_ctf_event_class_get_emf_uri(event_class));
    REQUIRE(bt_ctf_event_class_set_log_level(NULL, BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO));
    REQUIRE(bt_ctf_event_class_set_log_level(event_class, BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN));
    REQUIRE(!bt_ctf_event_class_set_log_level(event_class, BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO));
    REQUIRE(bt_ctf_event_class_get_log_level(event_class) == BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO);
    REQUIRE(bt_ctf_event_class_set_emf_uri(NULL, "https://babeltrace.org/"));
    REQUIRE(!bt_ctf_event_class_set_emf_uri(event_class, "https://babeltrace.org/"));
    REQUIRE(strcmp(bt_ctf_event_class_get_emf_uri(event_class), "https://babeltrace.org/") == 0);
    REQUIRE(!bt_ctf_event_class_set_emf_uri(event_class, NULL));
    REQUIRE(!bt_ctf_event_class_get_emf_uri(event_class));

    /* Add event class to the stream class */
    REQUIRE(bt_ctf_stream_class_add_event_class(stream_class, NULL));
    REQUIRE(bt_ctf_stream_class_add_event_class(stream_class, event_class) == 0);

    ret_stream_class = bt_ctf_event_class_get_stream_class(event_class);
    REQUIRE(ret_stream_class == stream_class);
    bt_ctf_object_put_ref(ret_stream_class);

    REQUIRE(!bt_ctf_event_class_get_field_by_name(event_class, "truie"));
    ret_field_type = bt_ctf_event_class_get_field_by_name(event_class, "complex_structure");
    bt_ctf_object_put_ref(ret_field_type);

    event = bt_ctf_event_create(event_class);
    REQUIRE(event);

    ret_event_class = bt_ctf_event_get_class(event);
    REQUIRE(ret_event_class == event_class);
    bt_ctf_object_put_ref(ret_event_class);

    uint_35_field = bt_ctf_event_get_payload(event, "uint_35");
    REQUIRE(uint_35_field);
    bt_ctf_field_integer_unsigned_set_value(uint_35_field, 0x0DDF00D);
    REQUIRE(bt_ctf_field_integer_unsigned_get_value(uint_35_field, &ret_unsigned_int) == 0);
    REQUIRE(ret_unsigned_int == 0x0DDF00D);
    bt_ctf_object_put_ref(uint_35_field);

    int_16_field = bt_ctf_event_get_payload(event, "int_16");
    bt_ctf_field_integer_signed_set_value(int_16_field, -12345);
    REQUIRE(bt_ctf_field_integer_signed_get_value(int_16_field, &ret_signed_int) == 0);
    REQUIRE(ret_signed_int == -12345);
    bt_ctf_object_put_ref(int_16_field);

    complex_structure_field = bt_ctf_event_get_payload(event, "complex_structure");

    inner_structure_field = bt_ctf_field_structure_get_field_by_index(complex_structure_field, 3);
    ret_field_type = bt_ctf_field_get_type(inner_structure_field);
    bt_ctf_object_put_ref(inner_structure_field);
    bt_ctf_object_put_ref(ret_field_type);

    inner_structure_field =
        bt_ctf_field_structure_get_field_by_name(complex_structure_field, "inner_structure");
    a_string_field = bt_ctf_field_structure_get_field_by_name(complex_structure_field, "string");
    enum_variant_field =
        bt_ctf_field_structure_get_field_by_name(complex_structure_field, "variant_selector");
    variant_field =
        bt_ctf_field_structure_get_field_by_name(complex_structure_field, "variant_value");
    uint_35_field = bt_ctf_field_structure_get_field_by_name(inner_structure_field, "seq_len");
    a_sequence_field =
        bt_ctf_field_structure_get_field_by_name(inner_structure_field, "a_sequence");
    an_array_field = bt_ctf_field_structure_get_field_by_name(inner_structure_field, "an_array");

    enum_container_field = bt_ctf_field_enumeration_get_container(enum_variant_field);
    bt_ctf_field_integer_unsigned_set_value(enum_container_field, 1);
    int_16_field = bt_ctf_field_variant_get_field(variant_field, enum_variant_field);
    bt_ctf_field_integer_signed_set_value(int_16_field, -200);
    bt_ctf_object_put_ref(int_16_field);
    bt_ctf_field_string_set_value(a_string_field, test_string_1);
    REQUIRE(!bt_ctf_field_string_append(a_string_field, test_string_2));
    REQUIRE(!bt_ctf_field_string_append_len(a_string_field, test_string_3, 5));
    REQUIRE(!bt_ctf_field_string_append_len(a_string_field, &test_string_4[5], 3));
    REQUIRE(!bt_ctf_field_string_append_len(a_string_field, test_string_3, 0));

    ret_string = bt_ctf_field_string_get_value(a_string_field);
    REQUIRE(ret_string);
    REQUIRE((ret_string ? strcmp(ret_string, test_string_cat) == 0 : 0));
    bt_ctf_field_integer_unsigned_set_value(uint_35_field, SEQUENCE_TEST_LENGTH);

    ret_field_type =
        bt_ctf_field_type_variant_get_field_type_from_tag(variant_type, enum_variant_field);
    REQUIRE(ret_field_type == int_16_type);

    REQUIRE(bt_ctf_field_sequence_set_length(a_sequence_field, uint_35_field) == 0);

    for (i = 0; i < SEQUENCE_TEST_LENGTH; i++) {
        int_16_field = bt_ctf_field_sequence_get_field(a_sequence_field, i);
        bt_ctf_field_integer_signed_set_value(int_16_field, 4 - i);
        bt_ctf_object_put_ref(int_16_field);
    }

    for (i = 0; i < ARRAY_TEST_LENGTH; i++) {
        int_16_field = bt_ctf_field_array_get_field(an_array_field, i);
        bt_ctf_field_integer_signed_set_value(int_16_field, i);
        bt_ctf_object_put_ref(int_16_field);
    }

    stream_event_ctx_field = bt_ctf_event_get_stream_event_context(event);
    REQUIRE(stream_event_ctx_field);
    stream_event_ctx_int_field =
        bt_ctf_field_structure_get_field_by_name(stream_event_ctx_field, "common_event_context");
    BT_CTF_OBJECT_PUT_REF_AND_RESET(stream_event_ctx_field);
    bt_ctf_field_integer_unsigned_set_value(stream_event_ctx_int_field, 17);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(stream_event_ctx_int_field);

    bt_ctf_clock_set_time(clock, ++current_time);
    REQUIRE(bt_ctf_stream_append_event(stream, event) == 0);

    /*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
    packet_context = bt_ctf_stream_get_packet_context(stream);
    packet_context_field =
        bt_ctf_field_structure_get_field_by_name(packet_context, "custom_packet_context_field");
    bt_ctf_field_integer_unsigned_set_value(packet_context_field, 1);

    REQUIRE(bt_ctf_stream_flush(stream) == 0);

    bt_ctf_object_put_ref(uint_35_field);
    bt_ctf_object_put_ref(a_string_field);
    bt_ctf_object_put_ref(inner_structure_field);
    bt_ctf_object_put_ref(complex_structure_field);
    bt_ctf_object_put_ref(a_sequence_field);
    bt_ctf_object_put_ref(an_array_field);
    bt_ctf_object_put_ref(enum_variant_field);
    bt_ctf_object_put_ref(enum_container_field);
    bt_ctf_object_put_ref(variant_field);
    bt_ctf_object_put_ref(packet_context_field);
    bt_ctf_object_put_ref(packet_context);
    bt_ctf_object_put_ref(uint_35_type);
    bt_ctf_object_put_ref(int_16_type);
    bt_ctf_object_put_ref(string_type);
    bt_ctf_object_put_ref(sequence_type);
    bt_ctf_object_put_ref(array_type);
    bt_ctf_object_put_ref(inner_structure_type);
    bt_ctf_object_put_ref(complex_structure_type);
    bt_ctf_object_put_ref(uint_3_type);
    bt_ctf_object_put_ref(enum_variant_type);
    bt_ctf_object_put_ref(variant_type);
    bt_ctf_object_put_ref(ret_field_type);
    bt_ctf_object_put_ref(event_class);
    bt_ctf_object_put_ref(event);
}

TEST_CASE("CTF writer: field types")
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
    struct bt_ctf_field_type *uint_12_type = bt_ctf_field_type_integer_create(12);
    struct bt_ctf_field_type *enumeration_type;
    struct bt_ctf_field_type *returned_type;
    const char *ret_string;

    REQUIRE(uint_12_type);
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, BT_CTF_INTEGER_BASE_BINARY) == 0);
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, BT_CTF_INTEGER_BASE_DECIMAL) == 0);
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, BT_CTF_INTEGER_BASE_UNKNOWN));
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, BT_CTF_INTEGER_BASE_OCTAL) == 0);
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, BT_CTF_INTEGER_BASE_HEXADECIMAL) == 0);
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, (enum bt_ctf_integer_base) 457417));
    REQUIRE(bt_ctf_field_type_integer_set_signed(uint_12_type, 952835) == 0);
    REQUIRE(bt_ctf_field_type_integer_set_signed(uint_12_type, 0) == 0);
    REQUIRE(bt_ctf_field_type_integer_get_size(uint_12_type) == 12);
    REQUIRE(bt_ctf_field_type_integer_get_signed(uint_12_type) == 0);

    REQUIRE(bt_ctf_field_type_set_byte_order(NULL, BT_CTF_BYTE_ORDER_LITTLE_ENDIAN) < 0);
    REQUIRE(bt_ctf_field_type_set_byte_order(uint_12_type, (enum bt_ctf_byte_order) 42) < 0);
    REQUIRE(bt_ctf_field_type_set_byte_order(uint_12_type, BT_CTF_BYTE_ORDER_LITTLE_ENDIAN) == 0);
    REQUIRE(bt_ctf_field_type_set_byte_order(uint_12_type, BT_CTF_BYTE_ORDER_BIG_ENDIAN) == 0);
    REQUIRE(bt_ctf_field_type_get_byte_order(uint_12_type) == BT_CTF_BYTE_ORDER_BIG_ENDIAN);

    REQUIRE(bt_ctf_field_type_get_type_id(uint_12_type) == BT_CTF_FIELD_TYPE_ID_INTEGER);

    REQUIRE(bt_ctf_field_type_integer_get_base(uint_12_type) == BT_CTF_INTEGER_BASE_HEXADECIMAL);

    REQUIRE(bt_ctf_field_type_integer_set_encoding(NULL, BT_CTF_STRING_ENCODING_ASCII) < 0);
    REQUIRE(bt_ctf_field_type_integer_set_encoding(uint_12_type,
                                                   (enum bt_ctf_string_encoding) 123) < 0);
    REQUIRE(bt_ctf_field_type_integer_set_encoding(uint_12_type, BT_CTF_STRING_ENCODING_UTF8) == 0);
    REQUIRE(bt_ctf_field_type_integer_get_encoding(uint_12_type) == BT_CTF_STRING_ENCODING_UTF8);

    int_16_type = bt_ctf_field_type_integer_create(16);
    REQUIRE(int_16_type);
    REQUIRE(!bt_ctf_field_type_integer_set_signed(int_16_type, 1));
    REQUIRE(bt_ctf_field_type_integer_get_signed(int_16_type) == 1);
    uint_8_type = bt_ctf_field_type_integer_create(8);
    sequence_type = bt_ctf_field_type_sequence_create(int_16_type, "seq_len");
    REQUIRE(sequence_type);
    REQUIRE(bt_ctf_field_type_get_type_id(sequence_type) == BT_CTF_FIELD_TYPE_ID_SEQUENCE);

    ret_string = bt_ctf_field_type_sequence_get_length_field_name(sequence_type);
    REQUIRE(strcmp(ret_string, "seq_len") == 0);
    returned_type = bt_ctf_field_type_sequence_get_element_field_type(sequence_type);
    REQUIRE(returned_type == int_16_type);
    bt_ctf_object_put_ref(returned_type);

    string_type = bt_ctf_field_type_string_create();
    REQUIRE(string_type);
    REQUIRE(bt_ctf_field_type_string_set_encoding(string_type, BT_CTF_STRING_ENCODING_NONE));
    REQUIRE(bt_ctf_field_type_string_set_encoding(string_type, (enum bt_ctf_string_encoding) 42));
    REQUIRE(bt_ctf_field_type_string_set_encoding(string_type, BT_CTF_STRING_ENCODING_ASCII) == 0);

    REQUIRE(bt_ctf_field_type_string_get_encoding(string_type) == BT_CTF_STRING_ENCODING_ASCII);

    structure_seq_type = bt_ctf_field_type_structure_create();
    REQUIRE(bt_ctf_field_type_get_type_id(structure_seq_type) == BT_CTF_FIELD_TYPE_ID_STRUCT);
    REQUIRE(structure_seq_type);
    REQUIRE(bt_ctf_field_type_structure_add_field(structure_seq_type, uint_8_type, "seq_len") == 0);
    REQUIRE(bt_ctf_field_type_structure_add_field(structure_seq_type, sequence_type,
                                                  "a_sequence") == 0);

    REQUIRE(bt_ctf_field_type_structure_get_field_count(structure_seq_type) == 2);

    REQUIRE(bt_ctf_field_type_structure_get_field(structure_seq_type, NULL, &returned_type, 1) ==
            0);
    bt_ctf_object_put_ref(returned_type);
    REQUIRE(bt_ctf_field_type_structure_get_field(structure_seq_type, &ret_string, NULL, 1) == 0);
    REQUIRE(bt_ctf_field_type_structure_get_field(structure_seq_type, &ret_string, &returned_type,
                                                  1) == 0);
    REQUIRE(strcmp(ret_string, "a_sequence") == 0);
    REQUIRE(returned_type == sequence_type);
    bt_ctf_object_put_ref(returned_type);

    returned_type =
        bt_ctf_field_type_structure_get_field_type_by_name(structure_seq_type, "a_sequence");
    REQUIRE(returned_type == sequence_type);
    bt_ctf_object_put_ref(returned_type);

    composite_structure_type = bt_ctf_field_type_structure_create();
    REQUIRE(bt_ctf_field_type_structure_add_field(composite_structure_type, string_type,
                                                  "a_string") == 0);
    REQUIRE(bt_ctf_field_type_structure_add_field(composite_structure_type, structure_seq_type,
                                                  "inner_structure") == 0);

    returned_type =
        bt_ctf_field_type_structure_get_field_type_by_name(structure_seq_type, "a_sequence");
    REQUIRE(returned_type == sequence_type);
    bt_ctf_object_put_ref(returned_type);

    int_16 = bt_ctf_field_create(int_16_type);
    REQUIRE(int_16);
    uint_12 = bt_ctf_field_create(uint_12_type);
    REQUIRE(uint_12);
    returned_type = bt_ctf_field_get_type(int_16);
    REQUIRE(returned_type == int_16_type);

    /* Can't modify types after instantiating them */
    REQUIRE(bt_ctf_field_type_integer_set_base(uint_12_type, BT_CTF_INTEGER_BASE_DECIMAL));
    REQUIRE(bt_ctf_field_type_integer_set_signed(uint_12_type, 0));

    /* Check overflows are properly tested for */
    REQUIRE(bt_ctf_field_integer_signed_set_value(int_16, -32768) == 0);
    REQUIRE(bt_ctf_field_integer_signed_set_value(int_16, 32767) == 0);
    REQUIRE(bt_ctf_field_integer_signed_set_value(int_16, -42) == 0);

    REQUIRE(bt_ctf_field_integer_unsigned_set_value(uint_12, 4095) == 0);
    REQUIRE(bt_ctf_field_integer_unsigned_set_value(uint_12, 0) == 0);

    string = bt_ctf_field_create(string_type);
    REQUIRE(string);
    REQUIRE(bt_ctf_field_string_set_value(string, "A value") == 0);

    enumeration_type = bt_ctf_field_type_enumeration_create(uint_12_type);
    REQUIRE(enumeration_type);

    bt_ctf_object_put_ref(string);
    bt_ctf_object_put_ref(uint_12);
    bt_ctf_object_put_ref(int_16);
    bt_ctf_object_put_ref(composite_structure_type);
    bt_ctf_object_put_ref(structure_seq_type);
    bt_ctf_object_put_ref(string_type);
    bt_ctf_object_put_ref(sequence_type);
    bt_ctf_object_put_ref(uint_8_type);
    bt_ctf_object_put_ref(int_16_type);
    bt_ctf_object_put_ref(uint_12_type);
    bt_ctf_object_put_ref(enumeration_type);
    bt_ctf_object_put_ref(returned_type);
}

static void packet_resize_test(struct bt_ctf_stream_class *stream_class,
                               struct bt_ctf_stream *stream, struct bt_ctf_clock *clock)
{
    /*
	 * Append enough events to force the underlying packet to be resized.
	 * Also tests that a new event can be declared after a stream has been
	 * instantiated and used/flushed.
	 */
    int ret = 0;
    int i;
    struct bt_ctf_event_class *event_class = bt_ctf_event_class_create("Spammy_Event");
    struct bt_ctf_field_type *integer_type = bt_ctf_field_type_integer_create(17);
    struct bt_ctf_field_type *string_type = bt_ctf_field_type_string_create();
    struct bt_ctf_event *event = NULL;
    struct bt_ctf_field *ret_field = NULL;
    struct bt_ctf_field_type *ret_field_type = NULL;
    uint64_t ret_uint64;
    int events_appended = 0;
    struct bt_ctf_field *packet_context = NULL, *packet_context_field = NULL,
                        *stream_event_context = NULL;
    struct bt_ctf_field_type *ep_field_1_type = NULL;
    struct bt_ctf_field_type *ep_a_string_type = NULL;
    struct bt_ctf_field_type *ep_type = NULL;

    ret |= bt_ctf_event_class_add_field(event_class, integer_type, "field_1");
    ret |= bt_ctf_event_class_add_field(event_class, string_type, "a_string");
    ret |= bt_ctf_stream_class_add_event_class(stream_class, event_class);
    REQUIRE(ret == 0);
    if (ret) {
        goto end;
    }

    /*
	 * bt_ctf_stream_class_add_event_class() copies the field types
	 * of event_class, so we retrieve the new ones to create the
	 * appropriate fields.
	 */
    ep_type = bt_ctf_event_class_get_payload_field_type(event_class);
    REQUIRE(ep_type);
    ep_field_1_type = bt_ctf_field_type_structure_get_field_type_by_name(ep_type, "field_1");
    REQUIRE(ep_field_1_type);
    ep_a_string_type = bt_ctf_field_type_structure_get_field_type_by_name(ep_type, "a_string");
    REQUIRE(ep_a_string_type);

    event = bt_ctf_event_create(event_class);
    ret_field = bt_ctf_event_get_payload(event, 0);
    ret_field_type = bt_ctf_field_get_type(ret_field);
    bt_ctf_object_put_ref(ret_field_type);
    bt_ctf_object_put_ref(ret_field);
    bt_ctf_object_put_ref(event);

    for (i = 0; i < packet_resize_test_length; i++) {
        event = bt_ctf_event_create(event_class);
        struct bt_ctf_field *integer = bt_ctf_field_create(ep_field_1_type);
        struct bt_ctf_field *string = bt_ctf_field_create(ep_a_string_type);

        ret |= bt_ctf_clock_set_time(clock, ++current_time);
        ret |= bt_ctf_field_integer_unsigned_set_value(integer, i);
        ret |= bt_ctf_event_set_payload(event, "field_1", integer);
        bt_ctf_object_put_ref(integer);
        ret |= bt_ctf_field_string_set_value(string, "This is a test");
        ret |= bt_ctf_event_set_payload(event, "a_string", string);
        bt_ctf_object_put_ref(string);

        /* Populate stream event context */
        stream_event_context = bt_ctf_event_get_stream_event_context(event);
        integer =
            bt_ctf_field_structure_get_field_by_name(stream_event_context, "common_event_context");
        BT_CTF_OBJECT_PUT_REF_AND_RESET(stream_event_context);
        ret |= bt_ctf_field_integer_unsigned_set_value(integer, i % 42);
        bt_ctf_object_put_ref(integer);

        ret |= bt_ctf_stream_append_event(stream, event);
        bt_ctf_object_put_ref(event);

        if (ret) {
            break;
        }
    }

    events_appended = !!(i == packet_resize_test_length);
    ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
    REQUIRE((ret == 0 && ret_uint64 == 0));
    bt_ctf_stream_append_discarded_events(stream, 1000);
    ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
    REQUIRE((ret == 0 && ret_uint64 == 1000));

end:
    REQUIRE(events_appended);

    /*
	 * Populate the custom packet context field with a dummy value
	 * otherwise flush will fail.
	 */
    packet_context = bt_ctf_stream_get_packet_context(stream);
    packet_context_field =
        bt_ctf_field_structure_get_field_by_name(packet_context, "custom_packet_context_field");
    bt_ctf_field_integer_unsigned_set_value(packet_context_field, 2);

    REQUIRE(bt_ctf_stream_flush(stream) == 0);
    ret = bt_ctf_stream_get_discarded_events_count(stream, &ret_uint64);
    REQUIRE((ret == 0 && ret_uint64 == 1000));
    bt_ctf_object_put_ref(integer_type);
    bt_ctf_object_put_ref(string_type);
    bt_ctf_object_put_ref(packet_context);
    bt_ctf_object_put_ref(packet_context_field);
    bt_ctf_object_put_ref(stream_event_context);
    bt_ctf_object_put_ref(event_class);
    bt_ctf_object_put_ref(ep_field_1_type);
    bt_ctf_object_put_ref(ep_a_string_type);
    bt_ctf_object_put_ref(ep_type);
}

static void test_empty_stream(struct bt_ctf_writer *writer)
{
    int ret = 0;
    struct bt_ctf_trace *trace = NULL, *ret_trace = NULL;
    struct bt_ctf_stream_class *stream_class = NULL;
    struct bt_ctf_stream *stream = NULL;

    trace = bt_ctf_writer_get_trace(writer);
    if (!trace) {
        INFO("Failed to get trace from writer");
        ret = -1;
        goto end;
    }

    stream_class = bt_ctf_stream_class_create("empty_stream");
    if (!stream_class) {
        INFO("Failed to create stream class");
        ret = -1;
        goto end;
    }

    ret = bt_ctf_stream_class_set_packet_context_type(stream_class, NULL);
    REQUIRE(ret == 0);
    ret = bt_ctf_stream_class_set_event_header_type(stream_class, NULL);
    REQUIRE(ret == 0);

    REQUIRE(!bt_ctf_stream_class_get_trace(stream_class));

    stream = bt_ctf_writer_create_stream(writer, stream_class);
    if (!stream) {
        INFO("Failed to create writer stream");
        ret = -1;
        goto end;
    }

    ret_trace = bt_ctf_stream_class_get_trace(stream_class);
    REQUIRE(ret_trace == trace);
end:
    REQUIRE(ret == 0);
    bt_ctf_object_put_ref(trace);
    bt_ctf_object_put_ref(ret_trace);
    bt_ctf_object_put_ref(stream);
    bt_ctf_object_put_ref(stream_class);
}

static void test_custom_event_header_stream(struct bt_ctf_writer *writer,
                                            struct bt_ctf_clock *clock)
{
    int i, ret;
    struct bt_ctf_stream_class *stream_class = NULL;
    struct bt_ctf_stream *stream = NULL;
    struct bt_ctf_field_type *integer_type = NULL, *sequence_type = NULL, *event_header_type = NULL;
    struct bt_ctf_field *integer = NULL, *sequence = NULL, *event_header = NULL,
                        *packet_header = NULL;
    struct bt_ctf_event_class *event_class = NULL;
    struct bt_ctf_event *event = NULL;

    stream_class = bt_ctf_stream_class_create("custom_event_header_stream");
    if (!stream_class) {
        FAIL("Failed to create stream class");
        goto end;
    }

    ret = bt_ctf_stream_class_set_clock(stream_class, clock);
    if (ret) {
        FAIL("Failed to set stream class clock");
        goto end;
    }

    /*
	 * Customize event header to add an "seq_len" integer member
	 * which will be used as the length of a sequence in an event of this
	 * stream.
	 */
    event_header_type = bt_ctf_stream_class_get_event_header_type(stream_class);
    if (!event_header_type) {
        FAIL("Failed to get event header type");
        goto end;
    }

    integer_type = bt_ctf_field_type_integer_create(13);
    if (!integer_type) {
        FAIL("Failed to create length integer type");
        goto end;
    }

    ret = bt_ctf_field_type_structure_add_field(event_header_type, integer_type, "seq_len");
    if (ret) {
        FAIL("Failed to add a new field to stream event header");
        goto end;
    }

    event_class = bt_ctf_event_class_create("sequence_event");
    if (!event_class) {
        FAIL("Failed to create event class");
        goto end;
    }

    /*
	 * This event's payload will contain a sequence which references
	 * stream.event.header.seq_len as its length field.
	 */
    sequence_type = bt_ctf_field_type_sequence_create(integer_type, "stream.event.header.seq_len");
    if (!sequence_type) {
        FAIL("Failed to create a sequence");
        goto end;
    }

    ret = bt_ctf_event_class_add_field(event_class, sequence_type, "some_sequence");
    if (ret) {
        FAIL("Failed to add a sequence to an event class");
        goto end;
    }

    ret = bt_ctf_stream_class_add_event_class(stream_class, event_class);
    if (ret) {
        FAIL("Failed to add event class to stream class");
        goto end;
    }

    stream = bt_ctf_writer_create_stream(writer, stream_class);
    if (!stream) {
        FAIL("Failed to create stream");
        goto end;
    }

    /*
	 * We have defined a custom packet header field. We have to populate it
	 * explicitly.
	 */
    packet_header = bt_ctf_stream_get_packet_header(stream);
    if (!packet_header) {
        FAIL("Failed to get stream packet header");
        goto end;
    }

    integer =
        bt_ctf_field_structure_get_field_by_name(packet_header, "custom_trace_packet_header_field");
    if (!integer) {
        FAIL("Failed to retrieve custom_trace_packet_header_field");
        goto end;
    }

    ret = bt_ctf_field_integer_unsigned_set_value(integer, 3487);
    if (ret) {
        FAIL("Failed to set custom_trace_packet_header_field value");
        goto end;
    }
    bt_ctf_object_put_ref(integer);

    event = bt_ctf_event_create(event_class);
    if (!event) {
        FAIL("Failed to create event");
        goto end;
    }

    event_header = bt_ctf_event_get_header(event);
    if (!event_header) {
        FAIL("Failed to get event header");
        goto end;
    }

    integer = bt_ctf_field_structure_get_field_by_name(event_header, "seq_len");
    if (!integer) {
        FAIL("Failed to get seq_len field from event header");
        goto end;
    }

    ret = bt_ctf_field_integer_unsigned_set_value(integer, 2);
    if (ret) {
        FAIL("Failed to set seq_len value in event header");
        goto end;
    }

    /* Populate both sequence integer fields */
    sequence = bt_ctf_event_get_payload(event, "some_sequence");
    if (!sequence) {
        FAIL("Failed to retrieve sequence from event");
        goto end;
    }

    ret = bt_ctf_field_sequence_set_length(sequence, integer);
    if (ret) {
        FAIL("Failed to set sequence length");
        goto end;
    }
    bt_ctf_object_put_ref(integer);

    for (i = 0; i < 2; i++) {
        integer = bt_ctf_field_sequence_get_field(sequence, i);
        if (ret) {
            FAIL("Failed to retrieve sequence element");
            goto end;
        }

        ret = bt_ctf_field_integer_unsigned_set_value(integer, i);
        if (ret) {
            FAIL("Failed to set sequence element value");
            goto end;
        }

        bt_ctf_object_put_ref(integer);
        integer = NULL;
    }

    ret = bt_ctf_stream_append_event(stream, event);
    if (ret) {
        FAIL("Failed to append event to stream");
        goto end;
    }

    ret = bt_ctf_stream_flush(stream);
    if (ret) {
        FAIL("Failed to flush custom_event_header stream");
    }
end:
    bt_ctf_object_put_ref(stream);
    bt_ctf_object_put_ref(stream_class);
    bt_ctf_object_put_ref(event_class);
    bt_ctf_object_put_ref(event);
    bt_ctf_object_put_ref(integer);
    bt_ctf_object_put_ref(sequence);
    bt_ctf_object_put_ref(event_header);
    bt_ctf_object_put_ref(packet_header);
    bt_ctf_object_put_ref(sequence_type);
    bt_ctf_object_put_ref(integer_type);
    bt_ctf_object_put_ref(event_header_type);
}

static void test_instantiate_event_before_stream(struct bt_ctf_writer *writer,
                                                 struct bt_ctf_clock *clock)
{
    int ret = 0;
    struct bt_ctf_stream_class *stream_class = NULL;
    struct bt_ctf_stream *stream = NULL, *ret_stream = NULL;
    struct bt_ctf_event_class *event_class = NULL;
    struct bt_ctf_event *event = NULL;
    struct bt_ctf_field_type *integer_type = NULL;
    struct bt_ctf_field *payload_field = NULL;
    struct bt_ctf_field *integer = NULL;

    stream_class = bt_ctf_stream_class_create("event_before_stream_test");
    if (!stream_class) {
        INFO("Failed to create stream class");
        ret = -1;
        goto end;
    }

    ret = bt_ctf_stream_class_set_clock(stream_class, clock);
    if (ret) {
        INFO("Failed to set stream class clock");
        goto end;
    }

    event_class = bt_ctf_event_class_create("some_event_class_name");
    integer_type = bt_ctf_field_type_integer_create(32);
    if (!integer_type) {
        INFO("Failed to create integer field type");
        ret = -1;
        goto end;
    }

    ret = bt_ctf_event_class_add_field(event_class, integer_type, "integer_field");
    if (ret) {
        INFO("Failed to add field to event class");
        goto end;
    }

    ret = bt_ctf_stream_class_add_event_class(stream_class, event_class);
    if (ret) {
        INFO("Failed to add event class to stream class");
    }

    event = bt_ctf_event_create(event_class);
    if (!event) {
        INFO("Failed to create event");
        ret = -1;
        goto end;
    }

    payload_field = bt_ctf_event_get_payload_field(event);
    if (!payload_field) {
        INFO("Failed to get event's payload field");
        ret = -1;
        goto end;
    }

    integer = bt_ctf_field_structure_get_field_by_index(payload_field, 0);
    if (!integer) {
        INFO("Failed to get integer field payload from event");
        ret = -1;
        goto end;
    }

    ret = bt_ctf_field_integer_unsigned_set_value(integer, 1234);
    if (ret) {
        INFO("Failed to set integer field value");
        goto end;
    }

    stream = bt_ctf_writer_create_stream(writer, stream_class);
    if (!stream) {
        INFO("Failed to create writer stream");
        ret = -1;
        goto end;
    }

    ret = bt_ctf_stream_append_event(stream, event);
    if (ret) {
        INFO("Failed to append event to stream");
        goto end;
    }

    ret_stream = bt_ctf_event_get_stream(event);
    REQUIRE(ret_stream == stream);
end:
    REQUIRE(ret == 0);
    bt_ctf_object_put_ref(stream);
    bt_ctf_object_put_ref(ret_stream);
    bt_ctf_object_put_ref(stream_class);
    bt_ctf_object_put_ref(event_class);
    bt_ctf_object_put_ref(event);
    bt_ctf_object_put_ref(integer_type);
    bt_ctf_object_put_ref(integer);
    bt_ctf_object_put_ref(payload_field);
}

static void append_existing_event_class(struct bt_ctf_stream_class *stream_class)
{
    int ret;
    struct bt_ctf_event_class *event_class;

    event_class = bt_ctf_event_class_create("Simple Event");
    REQUIRE(event_class);
    REQUIRE(bt_ctf_stream_class_add_event_class(stream_class, event_class) == 0);
    bt_ctf_object_put_ref(event_class);

    event_class = bt_ctf_event_class_create("different name, ok");
    REQUIRE(event_class);
    ret = bt_ctf_event_class_set_id(event_class, 13);
    REQUIRE(ret == 0);
    REQUIRE(bt_ctf_stream_class_add_event_class(stream_class, event_class));
    bt_ctf_object_put_ref(event_class);
}

TEST_CASE("CTF writer: clock utils")
{
    int ret;
    struct bt_ctf_clock *clock = NULL;

    clock = bt_ctf_clock_create("water");
    REQUIRE(clock);
    ret = bt_ctf_clock_set_offset_s(clock, 1234);
    REQUIRE(!ret);
    ret = bt_ctf_clock_set_offset(clock, 1000);
    REQUIRE(!ret);
    ret = bt_ctf_clock_set_frequency(clock, 1000000000);
    REQUIRE(!ret);
    ret = bt_ctf_clock_set_frequency(clock, 1534);
    REQUIRE(!ret);

    BT_CTF_OBJECT_PUT_REF_AND_RESET(clock);
}

TEST_CASE("CTF writer: full")
{
    const char *env_resize_length;
    const char *env_trace_path;
    gchar *trace_path;
    bool free_trace_path;
    const char *clock_name = "test_clock";
    const char *clock_description = "This is a test clock";
    const char *returned_clock_name;
    const char *returned_clock_description;
    const uint64_t frequency = 1123456789;
    const int64_t offset_s = 13515309;
    const int64_t offset = 1234567;
    int64_t get_offset_s, get_offset;
    const uint64_t precision = 10;
    const int is_absolute = 0xFF;
    char *metadata_string;
    struct bt_ctf_writer *writer;
    struct bt_utsname name = {"GNU/Linux", "testhost", "4.4.0-87-generic",
                              "#110-Ubuntu SMP Tue Jul 18 12:55:35 UTC 2017", "x86_64"};
    struct bt_ctf_clock *clock, *ret_clock;
    struct bt_ctf_stream_class *stream_class, *ret_stream_class;
    struct bt_ctf_stream *stream1;
    struct bt_ctf_stream *stream;
    const char *ret_string;
    const uint8_t *ret_uuid;
    bt_uuid_t tmp_uuid = {0};
    struct bt_ctf_field_type *packet_context_type, *packet_context_field_type, *packet_header_type,
        *packet_header_field_type, *integer_type, *stream_event_context_type, *ret_field_type,
        *event_header_field_type;
    struct bt_ctf_field *packet_header, *packet_header_field;
    struct bt_ctf_trace *trace;
    int ret;

    env_resize_length = getenv("PACKET_RESIZE_TEST_LENGTH");
    if (env_resize_length) {
        packet_resize_test_length = (unsigned int) atoi(env_resize_length);
    }

    env_trace_path = getenv("TEST_CTF_WRITER_TRACE_DIR");
    if (env_trace_path) {
        trace_path = (gchar *) env_trace_path;
        free_trace_path = false;
    } else {
        trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);
        REQUIRE(g_mkdtemp(trace_path));
        free_trace_path = true;
    }

    writer = bt_ctf_writer_create(trace_path);
    REQUIRE(writer);

    REQUIRE(!bt_ctf_writer_get_trace(NULL));
    trace = bt_ctf_writer_get_trace(writer);
    REQUIRE(bt_ctf_trace_set_native_byte_order(trace, BT_CTF_BYTE_ORDER_NATIVE));
    REQUIRE(bt_ctf_trace_set_native_byte_order(trace, BT_CTF_BYTE_ORDER_UNSPECIFIED));
    REQUIRE(trace);
    REQUIRE(bt_ctf_trace_set_native_byte_order(trace, BT_CTF_BYTE_ORDER_BIG_ENDIAN) == 0);
    REQUIRE(bt_ctf_trace_get_native_byte_order(trace) == BT_CTF_BYTE_ORDER_BIG_ENDIAN);

    /* Add environment context to the trace */
    REQUIRE(bt_ctf_writer_add_environment_field(writer, "host", name.nodename) == 0);
    REQUIRE(bt_ctf_writer_add_environment_field(NULL, "test_field", "test_value"));
    REQUIRE(bt_ctf_writer_add_environment_field(writer, NULL, "test_value"));
    REQUIRE(bt_ctf_writer_add_environment_field(writer, "test_field", NULL));

    /* Test bt_ctf_trace_set_environment_field_integer */
    REQUIRE(bt_ctf_trace_set_environment_field_integer(NULL, "test_env_int", -194875));
    REQUIRE(bt_ctf_trace_set_environment_field_integer(trace, NULL, -194875));
    REQUIRE(!bt_ctf_trace_set_environment_field_integer(trace, "test_env_int", -164973));

    /* Test bt_ctf_trace_set_environment_field_string */
    REQUIRE(bt_ctf_trace_set_environment_field_string(NULL, "test_env_str", "yeah"));
    REQUIRE(bt_ctf_trace_set_environment_field_string(trace, NULL, "yeah"));
    REQUIRE(bt_ctf_trace_set_environment_field_string(trace, "test_env_str", NULL));
    REQUIRE(!bt_ctf_trace_set_environment_field_string(trace, "test_env_str", "oh yeah"));

    /* Test environment field replacement */
    REQUIRE(!bt_ctf_trace_set_environment_field_integer(trace, "test_env_int", 654321));

    REQUIRE(bt_ctf_writer_add_environment_field(writer, "sysname", name.sysname) == 0);
    REQUIRE(bt_ctf_writer_add_environment_field(writer, "nodename", name.nodename) == 0);
    REQUIRE(bt_ctf_writer_add_environment_field(writer, "release", name.release) == 0);
    REQUIRE(bt_ctf_writer_add_environment_field(writer, "version", name.version) == 0);
    REQUIRE(bt_ctf_writer_add_environment_field(writer, "machine", name.machine) == 0);

    /* Define a clock and add it to the trace */
    REQUIRE(!bt_ctf_clock_create("signed"));
    clock = bt_ctf_clock_create(clock_name);
    REQUIRE(clock);
    returned_clock_name = bt_ctf_clock_get_name(clock);
    REQUIRE(returned_clock_name);
    REQUIRE((returned_clock_name ? strcmp(returned_clock_name, clock_name) == 0 : 0));

    returned_clock_description = bt_ctf_clock_get_description(clock);
    REQUIRE(!returned_clock_description);
    REQUIRE(bt_ctf_clock_set_description(clock, clock_description) == 0);

    returned_clock_description = bt_ctf_clock_get_description(clock);
    REQUIRE(returned_clock_description);
    REQUIRE((returned_clock_description ?
                 strcmp(returned_clock_description, clock_description) == 0 :
                 0));

    REQUIRE(bt_ctf_clock_get_frequency(clock) == DEFAULT_CLOCK_FREQ);
    REQUIRE(bt_ctf_clock_set_frequency(clock, frequency) == 0);
    REQUIRE(bt_ctf_clock_get_frequency(clock) == frequency);

    REQUIRE(bt_ctf_clock_get_offset_s(clock, &get_offset_s) == 0);
    REQUIRE(get_offset_s == DEFAULT_CLOCK_OFFSET_S);
    REQUIRE(bt_ctf_clock_set_offset_s(clock, offset_s) == 0);
    REQUIRE(bt_ctf_clock_get_offset_s(clock, &get_offset_s) == 0);
    REQUIRE(get_offset_s == offset_s);

    REQUIRE(bt_ctf_clock_get_offset(clock, &get_offset) == 0);
    REQUIRE(get_offset == DEFAULT_CLOCK_OFFSET);
    REQUIRE(bt_ctf_clock_set_offset(clock, offset) == 0);
    REQUIRE(bt_ctf_clock_get_offset(clock, &get_offset) == 0);
    REQUIRE(get_offset == offset);

    REQUIRE(bt_ctf_clock_get_precision(clock) == DEFAULT_CLOCK_PRECISION);
    REQUIRE(bt_ctf_clock_set_precision(clock, precision) == 0);
    REQUIRE(bt_ctf_clock_get_precision(clock) == precision);

    REQUIRE(bt_ctf_clock_get_is_absolute(clock) == DEFAULT_CLOCK_IS_ABSOLUTE);
    REQUIRE(bt_ctf_clock_set_is_absolute(clock, is_absolute) == 0);
    REQUIRE(bt_ctf_clock_get_is_absolute(clock) == !!is_absolute);
    REQUIRE(bt_ctf_clock_set_time(clock, current_time) == 0);
    ret_uuid = bt_ctf_clock_get_uuid(clock);
    REQUIRE(ret_uuid);
    if (ret_uuid) {
        memcpy(tmp_uuid, ret_uuid, sizeof(tmp_uuid));
        /* Slightly modify UUID */
        tmp_uuid[sizeof(tmp_uuid) - 1]++;
    }

    REQUIRE(bt_ctf_clock_set_uuid(clock, tmp_uuid) == 0);
    ret_uuid = bt_ctf_clock_get_uuid(clock);
    REQUIRE(ret_uuid);
    REQUIRE(uuid_match(ret_uuid, tmp_uuid));

    /* Define a stream class */
    stream_class = bt_ctf_stream_class_create("test_stream");
    ret_string = bt_ctf_stream_class_get_name(stream_class);
    REQUIRE((ret_string && strcmp(ret_string, "test_stream") == 0));

    REQUIRE(!bt_ctf_stream_class_get_clock(stream_class));
    REQUIRE(!bt_ctf_stream_class_get_clock(NULL));

    REQUIRE(stream_class);
    REQUIRE(bt_ctf_stream_class_set_clock(stream_class, clock) == 0);
    ret_clock = bt_ctf_stream_class_get_clock(stream_class);
    REQUIRE(ret_clock == clock);
    bt_ctf_object_put_ref(ret_clock);

    REQUIRE(bt_ctf_stream_class_get_id(stream_class) < 0);
    REQUIRE(bt_ctf_stream_class_set_id(NULL, 123) < 0);
    REQUIRE(bt_ctf_stream_class_set_id(stream_class, 123) == 0);
    REQUIRE(bt_ctf_stream_class_get_id(stream_class) == 123);

    /* Validate default event header fields */
    ret_field_type = bt_ctf_stream_class_get_event_header_type(stream_class);
    REQUIRE(ret_field_type);
    REQUIRE(bt_ctf_field_type_get_type_id(ret_field_type) == BT_CTF_FIELD_TYPE_ID_STRUCT);
    event_header_field_type =
        bt_ctf_field_type_structure_get_field_type_by_name(ret_field_type, "id");
    REQUIRE(event_header_field_type);
    REQUIRE(bt_ctf_field_type_get_type_id(event_header_field_type) == BT_CTF_FIELD_TYPE_ID_INTEGER);
    bt_ctf_object_put_ref(event_header_field_type);
    event_header_field_type =
        bt_ctf_field_type_structure_get_field_type_by_name(ret_field_type, "timestamp");
    REQUIRE(event_header_field_type);
    REQUIRE(bt_ctf_field_type_get_type_id(event_header_field_type) == BT_CTF_FIELD_TYPE_ID_INTEGER);
    bt_ctf_object_put_ref(event_header_field_type);
    bt_ctf_object_put_ref(ret_field_type);

    /* Add a custom trace packet header field */
    packet_header_type = bt_ctf_trace_get_packet_header_field_type(trace);
    REQUIRE(packet_header_type);
    REQUIRE(bt_ctf_field_type_get_type_id(packet_header_type) == BT_CTF_FIELD_TYPE_ID_STRUCT);
    ret_field_type =
        bt_ctf_field_type_structure_get_field_type_by_name(packet_header_type, "magic");
    REQUIRE(ret_field_type);
    bt_ctf_object_put_ref(ret_field_type);
    ret_field_type = bt_ctf_field_type_structure_get_field_type_by_name(packet_header_type, "uuid");
    REQUIRE(ret_field_type);
    bt_ctf_object_put_ref(ret_field_type);
    ret_field_type =
        bt_ctf_field_type_structure_get_field_type_by_name(packet_header_type, "stream_id");
    REQUIRE(ret_field_type);
    bt_ctf_object_put_ref(ret_field_type);

    packet_header_field_type = bt_ctf_field_type_integer_create(22);
    REQUIRE(!bt_ctf_field_type_structure_add_field(packet_header_type, packet_header_field_type,
                                                   "custom_trace_packet_header_field"));

    REQUIRE(bt_ctf_trace_set_packet_header_field_type(NULL, packet_header_type) < 0);
    REQUIRE(!bt_ctf_trace_set_packet_header_field_type(trace, packet_header_type));

    /* Add a custom field to the stream class' packet context */
    packet_context_type = bt_ctf_stream_class_get_packet_context_type(stream_class);
    REQUIRE(packet_context_type);
    REQUIRE(bt_ctf_field_type_get_type_id(packet_context_type) == BT_CTF_FIELD_TYPE_ID_STRUCT);

    REQUIRE(bt_ctf_stream_class_set_packet_context_type(NULL, packet_context_type));

    integer_type = bt_ctf_field_type_integer_create(32);

    REQUIRE(bt_ctf_stream_class_set_packet_context_type(stream_class, integer_type) < 0);

    /* Create a "uint5_t" equivalent custom packet context field */
    packet_context_field_type = bt_ctf_field_type_integer_create(5);

    ret = bt_ctf_field_type_structure_add_field(packet_context_type, packet_context_field_type,
                                                "custom_packet_context_field");
    REQUIRE(ret == 0);

    /* Define a stream event context containing a my_integer field. */
    stream_event_context_type = bt_ctf_field_type_structure_create();
    bt_ctf_field_type_structure_add_field(stream_event_context_type, integer_type,
                                          "common_event_context");

    REQUIRE(bt_ctf_stream_class_set_event_context_type(NULL, stream_event_context_type) < 0);
    REQUIRE(bt_ctf_stream_class_set_event_context_type(stream_class, integer_type) < 0);

    REQUIRE(bt_ctf_stream_class_set_event_context_type(stream_class, stream_event_context_type) ==
            0);

    ret_field_type = bt_ctf_stream_class_get_event_context_type(stream_class);
    REQUIRE(ret_field_type == stream_event_context_type);
    bt_ctf_object_put_ref(ret_field_type);

    /* Instantiate a stream and append events */
    ret = bt_ctf_writer_add_clock(writer, clock);
    REQUIRE(ret == 0);

    REQUIRE(bt_ctf_trace_get_stream_count(trace) == 0);
    stream1 = bt_ctf_writer_create_stream(writer, stream_class);
    REQUIRE(stream1);
    REQUIRE(bt_ctf_trace_get_stream_count(trace) == 1);
    stream = bt_ctf_trace_get_stream_by_index(trace, 0);
    REQUIRE(stream == stream1);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(stream);

    /*
	 * Creating a stream through a writer adds the given stream
	 * class to the writer's trace, thus registering the stream
	 * class's clock to the trace.
	 */

    ret_stream_class = bt_ctf_stream_get_class(stream1);
    REQUIRE(ret_stream_class);
    REQUIRE(ret_stream_class == stream_class);

    /*
	 * Packet header, packet context, event header, and stream
	 * event context types were copied for the resolving
	 * process
	 */
    BT_CTF_OBJECT_PUT_REF_AND_RESET(packet_header_type);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(packet_context_type);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(stream_event_context_type);
    packet_header_type = bt_ctf_trace_get_packet_header_field_type(trace);
    REQUIRE(packet_header_type);
    packet_context_type = bt_ctf_stream_class_get_packet_context_type(stream_class);
    REQUIRE(packet_context_type);
    stream_event_context_type = bt_ctf_stream_class_get_event_context_type(stream_class);
    REQUIRE(stream_event_context_type);

    /*
	 * Try to modify the packet context type after a stream has been
	 * created.
	 */
    ret = bt_ctf_field_type_structure_add_field(packet_header_type, packet_header_field_type,
                                                "should_fail");
    REQUIRE(ret < 0);

    /*
	 * Try to modify the packet context type after a stream has been
	 * created.
	 */
    ret = bt_ctf_field_type_structure_add_field(packet_context_type, packet_context_field_type,
                                                "should_fail");
    REQUIRE(ret < 0);

    /*
	 * Try to modify the stream event context type after a stream has been
	 * created.
	 */
    ret = bt_ctf_field_type_structure_add_field(stream_event_context_type, integer_type,
                                                "should_fail");
    REQUIRE(ret < 0);

    /* Should fail after instantiating a stream (frozen) */
    REQUIRE(bt_ctf_stream_class_set_clock(stream_class, clock));

    /* Populate the custom packet header field only once for all tests */
    REQUIRE(!bt_ctf_stream_get_packet_header(NULL));
    packet_header = bt_ctf_stream_get_packet_header(stream1);
    REQUIRE(packet_header);
    ret_field_type = bt_ctf_field_get_type(packet_header);
    REQUIRE(ret_field_type == packet_header_type);
    bt_ctf_object_put_ref(ret_field_type);
    packet_header_field =
        bt_ctf_field_structure_get_field_by_name(packet_header, "custom_trace_packet_header_field");
    REQUIRE(packet_header_field);
    ret_field_type = bt_ctf_field_get_type(packet_header_field);
    REQUIRE(!bt_ctf_field_integer_unsigned_set_value(packet_header_field, 54321));
    REQUIRE(bt_ctf_stream_set_packet_header(stream1, NULL) < 0);
    REQUIRE(bt_ctf_stream_set_packet_header(NULL, packet_header) < 0);
    REQUIRE(bt_ctf_stream_set_packet_header(stream1, packet_header_field) < 0);
    REQUIRE(!bt_ctf_stream_set_packet_header(stream1, packet_header));

    REQUIRE(bt_ctf_writer_add_environment_field(writer, "new_field", "test") == 0);

    test_instantiate_event_before_stream(writer, clock);

    append_simple_event(stream_class, stream1, clock);

    packet_resize_test(stream_class, stream1, clock);

    append_complex_event(stream_class, stream1, clock);

    append_existing_event_class(stream_class);

    test_empty_stream(writer);

    test_custom_event_header_stream(writer, clock);

    metadata_string = bt_ctf_writer_get_metadata_string(writer);
    REQUIRE(metadata_string);

    bt_ctf_writer_flush_metadata(writer);

    bt_ctf_object_put_ref(clock);
    bt_ctf_object_put_ref(ret_stream_class);
    bt_ctf_object_put_ref(writer);
    bt_ctf_object_put_ref(stream1);
    bt_ctf_object_put_ref(packet_context_type);
    bt_ctf_object_put_ref(packet_context_field_type);
    bt_ctf_object_put_ref(integer_type);
    bt_ctf_object_put_ref(stream_event_context_type);
    bt_ctf_object_put_ref(ret_field_type);
    bt_ctf_object_put_ref(packet_header_type);
    bt_ctf_object_put_ref(packet_header_field_type);
    bt_ctf_object_put_ref(packet_header);
    bt_ctf_object_put_ref(packet_header_field);
    bt_ctf_object_put_ref(trace);
    free(metadata_string);
    bt_ctf_object_put_ref(stream_class);

    if (free_trace_path) {
        recursive_rmdir(trace_path);
        g_free(trace_path);
    }
}

/*
 * Copy a variant field type of which the choice ranges are already
 * computed.
 *
 * Creating a variant field computes the ranges of the choices of its
 * type. Adding the event class to a stream class of a trace then
 * validates it, copying its payload field type because it contains a
 * variant field type.
 */
TEST_CASE("CTF writer: variant field type copy with computed choice ranges")
{
    gchar *trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);

    REQUIRE(g_mkdtemp(trace_path));

    struct bt_ctf_writer *writer = bt_ctf_writer_create(trace_path);

    REQUIRE(writer);

    struct bt_ctf_field_type *uint_8_type = bt_ctf_field_type_integer_create(8);

    REQUIRE(uint_8_type);

    struct bt_ctf_field_type *tag_type = bt_ctf_field_type_enumeration_create(uint_8_type);

    REQUIRE(tag_type);

    /* Choice `a` has two ranges */
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(tag_type, "a", 0, 0) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(tag_type, "b", 1, 1) == 0);
    REQUIRE(bt_ctf_field_type_enumeration_unsigned_add_mapping(tag_type, "a", 2, 2) == 0);

    struct bt_ctf_field_type *variant_type = bt_ctf_field_type_variant_create(tag_type, "tag");

    REQUIRE(variant_type);
    REQUIRE(bt_ctf_field_type_variant_add_field(variant_type, uint_8_type, "a") == 0);
    REQUIRE(bt_ctf_field_type_variant_add_field(variant_type, uint_8_type, "b") == 0);

    /* Computes the choice ranges of `variant_type` */
    struct bt_ctf_field *variant_field = bt_ctf_field_create(variant_type);

    REQUIRE(variant_field);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(variant_field);

    struct bt_ctf_event_class *event_class = bt_ctf_event_class_create("variant_event");

    REQUIRE(event_class);
    REQUIRE(bt_ctf_event_class_add_field(event_class, tag_type, "tag") == 0);
    REQUIRE(bt_ctf_event_class_add_field(event_class, variant_type, "var") == 0);

    struct bt_ctf_clock *clock = bt_ctf_clock_create("variant_clock");

    REQUIRE(clock);
    REQUIRE(bt_ctf_writer_add_clock(writer, clock) == 0);

    struct bt_ctf_stream_class *stream_class = bt_ctf_stream_class_create("variant_stream");

    REQUIRE(stream_class);
    REQUIRE(bt_ctf_stream_class_set_clock(stream_class, clock) == 0);
    REQUIRE(bt_ctf_stream_class_add_event_class(stream_class, event_class) == 0);

    /* Adds the stream class to the trace, copying the payload field type */
    struct bt_ctf_stream *stream = bt_ctf_writer_create_stream(writer, stream_class);

    REQUIRE(stream);

    /* Select choice `a` through its second range, using the copy */
    struct bt_ctf_event *event = bt_ctf_event_create(event_class);

    REQUIRE(event);

    struct bt_ctf_field *tag_field = bt_ctf_event_get_payload(event, "tag");

    REQUIRE(tag_field);

    struct bt_ctf_field *tag_container_field = bt_ctf_field_enumeration_get_container(tag_field);

    REQUIRE(tag_container_field);
    REQUIRE(bt_ctf_field_integer_unsigned_set_value(tag_container_field, 2) == 0);

    struct bt_ctf_field *var_field = bt_ctf_event_get_payload(event, "var");

    REQUIRE(var_field);

    struct bt_ctf_field *choice_field = bt_ctf_field_variant_get_field(var_field, tag_field);

    REQUIRE(choice_field);

    struct bt_ctf_field *cur_choice_field = bt_ctf_field_variant_get_current_field(var_field);

    REQUIRE(cur_choice_field == choice_field);
    REQUIRE(bt_ctf_field_integer_unsigned_set_value(choice_field, 23) == 0);
    REQUIRE(bt_ctf_stream_append_event(stream, event) == 0);
    REQUIRE(bt_ctf_stream_flush(stream) == 0);

    bt_ctf_object_put_ref(cur_choice_field);
    bt_ctf_object_put_ref(choice_field);
    bt_ctf_object_put_ref(var_field);
    bt_ctf_object_put_ref(tag_container_field);
    bt_ctf_object_put_ref(tag_field);
    bt_ctf_object_put_ref(event);
    bt_ctf_object_put_ref(stream);
    bt_ctf_object_put_ref(stream_class);
    bt_ctf_object_put_ref(clock);
    bt_ctf_object_put_ref(event_class);
    bt_ctf_object_put_ref(variant_type);
    bt_ctf_object_put_ref(tag_type);
    bt_ctf_object_put_ref(uint_8_type);
    bt_ctf_object_put_ref(writer);
    recursive_rmdir(trace_path);
    g_free(trace_path);
}

/*
 * Append an event to a stream of which the event header field type has
 * a `timestamp` field, but no `id` field, and check that the library
 * automatically sets the `timestamp` field from the clock of the stream
 * class.
 */
TEST_CASE("CTF writer: event header with timestamp and no ID")
{
    gchar *trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);

    REQUIRE(g_mkdtemp(trace_path));

    struct bt_ctf_writer *writer = bt_ctf_writer_create(trace_path);

    REQUIRE(writer);

    struct bt_ctf_clock *clock = bt_ctf_clock_create("timestamp_only_clock");

    REQUIRE(clock);
    REQUIRE(bt_ctf_writer_add_clock(writer, clock) == 0);
    REQUIRE(bt_ctf_clock_set_time(clock, 1234) == 0);

    struct bt_ctf_stream_class *stream_class = bt_ctf_stream_class_create("timestamp_only_stream");

    REQUIRE(stream_class);
    REQUIRE(bt_ctf_stream_class_set_clock(stream_class, clock) == 0);

    struct bt_ctf_field_type *uint_64_type = bt_ctf_field_type_integer_create(64);

    REQUIRE(uint_64_type);

    struct bt_ctf_field_type *event_header_type = bt_ctf_field_type_structure_create();

    REQUIRE(event_header_type);
    REQUIRE(bt_ctf_field_type_structure_add_field(event_header_type, uint_64_type, "timestamp") ==
            0);
    REQUIRE(bt_ctf_stream_class_set_event_header_type(stream_class, event_header_type) == 0);

    struct bt_ctf_event_class *event_class = bt_ctf_event_class_create("timestamp_only_event");

    REQUIRE(event_class);
    REQUIRE(bt_ctf_event_class_add_field(event_class, uint_64_type, "value") == 0);
    REQUIRE(bt_ctf_stream_class_add_event_class(stream_class, event_class) == 0);

    struct bt_ctf_stream *stream = bt_ctf_writer_create_stream(writer, stream_class);

    REQUIRE(stream);

    struct bt_ctf_event *event = bt_ctf_event_create(event_class);

    REQUIRE(event);

    struct bt_ctf_field *value_field = bt_ctf_event_get_payload(event, "value");

    REQUIRE(value_field);
    REQUIRE(bt_ctf_field_integer_unsigned_set_value(value_field, 42) == 0);
    REQUIRE(bt_ctf_stream_append_event(stream, event) == 0);

    /* The library must have set the `timestamp` field */
    struct bt_ctf_field *event_header = bt_ctf_event_get_header(event);

    REQUIRE(event_header);

    struct bt_ctf_field *timestamp_field =
        bt_ctf_field_structure_get_field_by_name(event_header, "timestamp");

    REQUIRE(timestamp_field);

    uint64_t timestamp;

    REQUIRE(bt_ctf_field_integer_unsigned_get_value(timestamp_field, &timestamp) == 0);
    REQUIRE(timestamp == 1234);
    REQUIRE(bt_ctf_stream_flush(stream) == 0);

    bt_ctf_object_put_ref(timestamp_field);
    bt_ctf_object_put_ref(event_header);
    bt_ctf_object_put_ref(value_field);
    bt_ctf_object_put_ref(event);
    bt_ctf_object_put_ref(stream);
    bt_ctf_object_put_ref(event_class);
    bt_ctf_object_put_ref(event_header_type);
    bt_ctf_object_put_ref(uint_64_type);
    bt_ctf_object_put_ref(stream_class);
    bt_ctf_object_put_ref(clock);
    bt_ctf_object_put_ref(writer);
    recursive_rmdir(trace_path);
    g_free(trace_path);
}
