/*
 * nativebt.i.in
 *
 * Babeltrace native interface Python module
 *
 * Copyright 2012 EfficiOS Inc.
 *
 * Author: Danny Serres <danny.serres@efficios.com>
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
%module nativebt


%include "stdint.i"
%include "typemaps.i"
%{
#define SWIG_FILE_WITH_INIT
#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/trace-handle.h>
#include <babeltrace/trace-handle-internal.h>
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/iterator.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/format.h>
#include <babeltrace/list.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/iterator.h>
#include "python-complements.h"
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/writer.h>
%}


typedef int bt_intern_str;
typedef int64_t ssize_t;


/* python-complements.h */
struct bt_definition **_bt_python_field_listcaller(
		const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,
		unsigned int *OUTPUT);
struct bt_definition *_bt_python_field_one_from_list(
		struct bt_definition **list, int index);
struct bt_ctf_event_decl **_bt_python_event_decl_listcaller(
		int handle_id,
		struct bt_context *ctx,
		unsigned int *OUTPUT);
struct bt_ctf_event_decl *_bt_python_decl_one_from_list(
		struct bt_ctf_event_decl **list, int index);
struct bt_ctf_field_decl **_by_python_field_decl_listcaller(
		struct bt_ctf_event_decl *event_decl,
		enum bt_ctf_scope scope,
		unsigned int *OUTPUT);
struct bt_ctf_field_decl *_bt_python_field_decl_one_from_list(
		struct bt_ctf_field_decl **list, int index);
struct definition_array *_bt_python_get_array_from_def(
		struct bt_definition *field);
struct definition_sequence *_bt_python_get_sequence_from_def(
		struct bt_definition *field);
struct bt_declaration *_bt_python_get_array_element_declaration(
		struct bt_declaration *field);
struct bt_declaration *_bt_python_get_sequence_element_declaration(
		struct bt_declaration *field);
const char *_bt_python_get_array_string(struct bt_definition *field);
const char *_bt_python_get_sequence_string(struct bt_definition *field);
int _bt_python_field_integer_get_signedness(const struct bt_ctf_field *field);
enum ctf_type_id _bt_python_get_field_type(const struct bt_ctf_field *field);
const char *_bt_python_ctf_field_type_enumeration_get_mapping(
		struct bt_ctf_field_type *enumeration, size_t index,
		int64_t *OUTPUT, int64_t *OUTPUT);
const char *_bt_python_ctf_field_type_enumeration_get_mapping_unsigned(
		struct bt_ctf_field_type *enumeration, size_t index,
		uint64_t *OUTPUT, uint64_t *OUTPUT);
const char *_bt_python_ctf_field_type_structure_get_field_name(
		struct bt_ctf_field_type *structure, size_t index);
struct bt_ctf_field_type *_bt_python_ctf_field_type_structure_get_field_type(
		struct bt_ctf_field_type *structure, size_t index);
const char *_bt_python_ctf_field_type_variant_get_field_name(
		struct bt_ctf_field_type *variant, size_t index);
struct bt_ctf_field_type *_bt_python_ctf_field_type_variant_get_field_type(
		struct bt_ctf_field_type *variant, size_t index);
const char *_bt_python_ctf_event_class_get_field_name(
		struct bt_ctf_event_class *event_class, size_t index);
struct bt_ctf_field_type *_bt_python_ctf_event_class_get_field_type(
		struct bt_ctf_event_class *event_class, size_t index);
int _bt_python_ctf_clock_get_uuid_index(struct bt_ctf_clock *clock,
		size_t index, unsigned char *OUTPUT);
int _bt_python_ctf_clock_set_uuid_index(struct bt_ctf_clock *clock,
		size_t index, unsigned char value);
struct bt_iter_pos *_bt_python_create_iter_pos(void);

/* context.h, context-internal.h */
%rename("_bt_context_create") bt_context_create(void);
%rename("_bt_context_add_trace") bt_context_add_trace(
		struct bt_context *ctx, const char *path, const char *format,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index, int whence),
		struct bt_mmap_stream_list *stream_list, FILE *metadata);
%rename("_bt_context_remove_trace") bt_context_remove_trace(
		struct bt_context *ctx, int trace_id);
%rename("_bt_context_get") bt_context_get(struct bt_context *ctx);
%rename("_bt_context_put") bt_context_put(struct bt_context *ctx);
%rename("_bt_ctf_event_get_context") bt_ctf_event_get_context(
		const struct bt_ctf_event *event);

struct bt_context *bt_context_create(void);
int bt_context_add_trace(struct bt_context *ctx, const char *path, const char *format,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index, int whence),
		struct bt_mmap_stream_list *stream_list, FILE *metadata);
void bt_context_remove_trace(struct bt_context *ctx, int trace_id);
void bt_context_get(struct bt_context *ctx);
void bt_context_put(struct bt_context *ctx);
struct bt_context *bt_ctf_event_get_context(const struct bt_ctf_event *event);


/* format.h */
%rename("lookup_format") bt_lookup_format(bt_intern_str qname);
%rename("_bt_print_format_list") bt_fprintf_format_list(FILE *fp);
%rename("register_format") bt_register_format(struct format *format);
%rename("unregister_format") bt_unregister_format(struct bt_format *format);

extern struct format *bt_lookup_format(bt_intern_str qname);
extern void bt_fprintf_format_list(FILE *fp);
extern int bt_register_format(struct bt_format *format);
extern void bt_unregister_format(struct bt_format *format);


/* iterator.h, iterator-internal.h */
%rename("_bt_iter_create") bt_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos, const struct bt_iter_pos *end_pos);
%rename("_bt_iter_destroy") bt_iter_destroy(struct bt_iter *iter);
%rename("_bt_iter_next") bt_iter_next(struct bt_iter *iter);
%rename("_bt_iter_get_pos") bt_iter_get_pos(struct bt_iter *iter);
%rename("_bt_iter_free_pos") bt_iter_free_pos(struct bt_iter_pos *pos);
%rename("_bt_iter_set_pos") bt_iter_set_pos(struct bt_iter *iter,
		const struct bt_iter_pos *pos);
%rename("_bt_iter_create_time_pos") bt_iter_create_time_pos(struct bt_iter *iter,
		uint64_t timestamp);

struct bt_iter *bt_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos, const struct bt_iter_pos *end_pos);
void bt_iter_destroy(struct bt_iter *iter);
int bt_iter_next(struct bt_iter *iter);
struct bt_iter_pos *bt_iter_get_pos(struct bt_iter *iter);
void bt_iter_free_pos(struct bt_iter_pos *pos);
int bt_iter_set_pos(struct bt_iter *iter, const struct bt_iter_pos *pos);
struct bt_iter_pos *bt_iter_create_time_pos(struct bt_iter *iter, uint64_t timestamp);

%rename("_bt_iter_pos") bt_iter_pos;
%rename("SEEK_TIME") BT_SEEK_TIME;
%rename("SEEK_RESTORE") BT_SEEK_RESTORE;
%rename("SEEK_CUR") BT_SEEK_CUR;
%rename("SEEK_BEGIN") BT_SEEK_BEGIN;
%rename("SEEK_LAST") BT_SEEK_LAST;


/*
 * This struct is taken from iterator.h
 * All changes to the struct must also be made here.
 */
struct bt_iter_pos {
	enum {
		BT_SEEK_TIME,		/* uses u.seek_time */
		BT_SEEK_RESTORE,	/* uses u.restore */
		BT_SEEK_CUR,
		BT_SEEK_BEGIN,
		BT_SEEK_LAST
	} type;
	union {
		uint64_t seek_time;
		struct bt_saved_pos *restore;
	} u;
};


/* trace-handle.h, trace-handle-internal.h */
%rename("_bt_trace_handle_create") bt_trace_handle_create(struct bt_context *ctx);
%rename("_bt_trace_handle_destroy") bt_trace_handle_destroy(struct bt_trace_handle *bt);
struct bt_trace_handle *bt_trace_handle_create(struct bt_context *ctx);
void bt_trace_handle_destroy(struct bt_trace_handle *bt);

%rename("_bt_trace_handle_get_path") bt_trace_handle_get_path(struct bt_context *ctx,
		int handle_id);
%rename("_bt_trace_handle_get_timestamp_begin") bt_trace_handle_get_timestamp_begin(
		struct bt_context *ctx, int handle_id, enum bt_clock_type type);
%rename("_bt_trace_handle_get_timestamp_end") bt_trace_handle_get_timestamp_end(
		struct bt_context *ctx, int handle_id, enum bt_clock_type type);
const char *bt_trace_handle_get_path(struct bt_context *ctx, int handle_id);
uint64_t bt_trace_handle_get_timestamp_begin(struct bt_context *ctx, int handle_id,
		enum bt_clock_type type);
uint64_t bt_trace_handle_get_timestamp_end(struct bt_context *ctx, int handle_id,
		enum bt_clock_type type);

%rename("_bt_ctf_event_get_handle_id") bt_ctf_event_get_handle_id(
		const struct bt_ctf_event *event);
int bt_ctf_event_get_handle_id(const struct bt_ctf_event *event);


/* iterator.h, events.h */
%rename("_bt_ctf_iter_create") bt_ctf_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos);
%rename("_bt_ctf_get_iter") bt_ctf_get_iter(struct bt_ctf_iter *iter);
%rename("_bt_ctf_iter_destroy") bt_ctf_iter_destroy(struct bt_ctf_iter *iter);
%rename("_bt_ctf_iter_read_event") bt_ctf_iter_read_event(struct bt_ctf_iter *iter);

struct bt_ctf_iter *bt_ctf_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos);
struct bt_iter *bt_ctf_get_iter(struct bt_ctf_iter *iter);
void bt_ctf_iter_destroy(struct bt_ctf_iter *iter);
struct bt_ctf_event *bt_ctf_iter_read_event(struct bt_ctf_iter *iter);


/* events.h */
%rename("_bt_ctf_get_top_level_scope") bt_ctf_get_top_level_scope(const struct
		bt_ctf_event *event, enum bt_ctf_scope scope);
%rename("_bt_ctf_event_name") bt_ctf_event_name(const struct bt_ctf_event *ctf_event);
%rename("_bt_ctf_get_timestamp") bt_ctf_get_timestamp(
		const struct bt_ctf_event *ctf_event);
%rename("_bt_ctf_get_cycles") bt_ctf_get_cycles(
		const struct bt_ctf_event *ctf_event);

%rename("_bt_ctf_get_field") bt_ctf_get_field(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,	const char *field);
%rename("_bt_ctf_get_index") bt_ctf_get_index(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *field,	unsigned int index);
%rename("_bt_ctf_field_name") bt_ctf_field_name(const struct bt_definition *field);
%rename("_bt_ctf_field_type") bt_ctf_field_type(const struct bt_declaration *field);
%rename("_bt_ctf_get_int_signedness") bt_ctf_get_int_signedness(
		const struct bt_declaration *field);
%rename("_bt_ctf_get_int_base") bt_ctf_get_int_base(const struct bt_declaration *field);
%rename("_bt_ctf_get_int_byte_order") bt_ctf_get_int_byte_order(
		const struct bt_declaration *field);
%rename("_bt_ctf_get_int_len") bt_ctf_get_int_len(const struct bt_declaration *field);
%rename("_bt_ctf_get_enum_int") bt_ctf_get_enum_int(const struct bt_definition *field);
%rename("_bt_ctf_get_enum_str") bt_ctf_get_enum_str(const struct bt_definition *field);
%rename("_bt_ctf_get_encoding") bt_ctf_get_encoding(const struct bt_declaration *field);
%rename("_bt_ctf_get_array_len") bt_ctf_get_array_len(const struct bt_declaration *field);
%rename("_bt_ctf_get_uint64") bt_ctf_get_uint64(const struct bt_definition *field);
%rename("_bt_ctf_get_int64") bt_ctf_get_int64(const struct bt_definition *field);
%rename("_bt_ctf_get_char_array") bt_ctf_get_char_array(const struct bt_definition *field);
%rename("_bt_ctf_get_string") bt_ctf_get_string(const struct bt_definition *field);
%rename("_bt_ctf_get_float") bt_ctf_get_float(const struct bt_definition *field);
%rename("_bt_ctf_get_variant") bt_ctf_get_variant(const struct bt_definition *field);
%rename("_bt_ctf_field_get_error") bt_ctf_field_get_error(void);
%rename("_bt_ctf_get_decl_event_name") bt_ctf_get_decl_event_name(const struct
		bt_ctf_event_decl *event);
%rename("_bt_ctf_get_decl_event_id") bt_ctf_get_decl_event_id(const struct
		bt_ctf_event_decl *event);
%rename("_bt_ctf_get_decl_field_name") bt_ctf_get_decl_field_name(
		const struct bt_ctf_field_decl *field);
%rename("_bt_ctf_get_decl_from_def") bt_ctf_get_decl_from_def(
		const struct bt_definition *field);
%rename("_bt_ctf_get_decl_from_field_decl") bt_ctf_get_decl_from_field_decl(
		const struct bt_ctf_field_decl *field);
%rename("_bt_array_index") bt_array_index(struct definition_array *array, uint64_t i);
%rename("_bt_sequence_len")  bt_sequence_len(struct definition_sequence *sequence);
%rename("_bt_sequence_index") bt_sequence_index(struct definition_sequence *sequence, uint64_t i);
%rename("_bt_ctf_get_struct_field_count") bt_ctf_get_struct_field_count(const struct bt_definition *structure);
%rename("_bt_ctf_get_struct_field_index") bt_ctf_get_struct_field_index(const struct bt_definition *structure, uint64_t i);

const struct bt_definition *bt_ctf_get_top_level_scope(const struct bt_ctf_event *ctf_event,
		enum bt_ctf_scope scope);
const char *bt_ctf_event_name(const struct bt_ctf_event *ctf_event);
uint64_t bt_ctf_get_timestamp(const struct bt_ctf_event *ctf_event);
uint64_t bt_ctf_get_cycles(const struct bt_ctf_event *ctf_event);
const struct bt_definition *bt_ctf_get_field(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,
		const char *field);
const struct bt_definition *bt_ctf_get_index(const struct bt_ctf_event *ctf_event,
		const struct bt_definition *field,
		unsigned int index);
const char *bt_ctf_field_name(const struct bt_definition *field);
enum ctf_type_id bt_ctf_field_type(const struct bt_declaration *field);
int bt_ctf_get_int_signedness(const struct bt_declaration *field);
int bt_ctf_get_int_base(const struct bt_declaration *field);
int bt_ctf_get_int_byte_order(const struct bt_declaration *field);
ssize_t bt_ctf_get_int_len(const struct bt_declaration *field);
const struct bt_definition *bt_ctf_get_enum_int(const struct bt_definition *field);
const char *bt_ctf_get_enum_str(const struct bt_definition *field);
enum ctf_string_encoding bt_ctf_get_encoding(const struct bt_declaration *field);
int bt_ctf_get_array_len(const struct bt_declaration *field);
struct bt_definition *bt_array_index(struct definition_array *array, uint64_t i);
uint64_t bt_ctf_get_uint64(const struct bt_definition *field);
int64_t bt_ctf_get_int64(const struct bt_definition *field);
char *bt_ctf_get_char_array(const struct bt_definition *field);
char *bt_ctf_get_string(const struct bt_definition *field);
double bt_ctf_get_float(const struct bt_definition *field);
const struct bt_definition *bt_ctf_get_variant(const struct bt_definition *field);
int bt_ctf_field_get_error(void);
const char *bt_ctf_get_decl_event_name(const struct bt_ctf_event_decl *event);
uint64_t bt_ctf_get_decl_event_id(const struct bt_ctf_event_decl *event);
const char *bt_ctf_get_decl_field_name(const struct bt_ctf_field_decl *field);
const struct bt_declaration *bt_ctf_get_decl_from_def(const struct bt_definition *field);
const struct bt_declaration *bt_ctf_get_decl_from_field_decl(const struct bt_ctf_field_decl *field);
uint64_t bt_sequence_len(struct definition_sequence *sequence);
struct bt_definition *bt_sequence_index(struct definition_sequence *sequence, uint64_t i);
uint64_t bt_ctf_get_struct_field_count(const struct bt_definition *structure);
const struct bt_definition *bt_ctf_get_struct_field_index(const struct bt_definition *structure, uint64_t i);


/* CTF Writer */

/* clock.h */
%rename("_bt_ctf_clock_create") bt_ctf_clock_create(const char *name);
%rename("_bt_ctf_clock_get_name") bt_ctf_clock_get_name(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_get_description") bt_ctf_clock_get_description(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_description") bt_ctf_clock_set_description(struct bt_ctf_clock *clock, const char *desc);
%rename("_bt_ctf_clock_get_frequency") bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_frequency") bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock, uint64_t freq);
%rename("_bt_ctf_clock_get_precision") bt_ctf_clock_get_precision(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_precision") bt_ctf_clock_set_precision(struct bt_ctf_clock *clock, uint64_t precision);
%rename("_bt_ctf_clock_get_offset_s") bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_offset_s") bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock, uint64_t offset_s);
%rename("_bt_ctf_clock_get_offset") bt_ctf_clock_get_offset(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_offset") bt_ctf_clock_set_offset(struct bt_ctf_clock *clock, uint64_t offset);
%rename("_bt_ctf_clock_get_is_absolute") bt_ctf_clock_get_is_absolute(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_is_absolute") bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock, int is_absolute);
%rename("_bt_ctf_clock_get_time") bt_ctf_clock_get_time(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_set_time") bt_ctf_clock_set_time(struct bt_ctf_clock *clock, uint64_t time);
%rename("_bt_ctf_clock_get") bt_ctf_clock_get(struct bt_ctf_clock *clock);
%rename("_bt_ctf_clock_put") bt_ctf_clock_put(struct bt_ctf_clock *clock);

struct bt_ctf_clock *bt_ctf_clock_create(const char *name);
const char *bt_ctf_clock_get_name(struct bt_ctf_clock *clock);
const char *bt_ctf_clock_get_description(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_description(struct bt_ctf_clock *clock, const char *desc);
uint64_t bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock, uint64_t freq);
uint64_t bt_ctf_clock_get_precision(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_precision(struct bt_ctf_clock *clock, uint64_t precision);
uint64_t bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock, uint64_t offset_s);
uint64_t bt_ctf_clock_get_offset(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_offset(struct bt_ctf_clock *clock, uint64_t offset);
int bt_ctf_clock_get_is_absolute(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock, int is_absolute);
uint64_t bt_ctf_clock_get_time(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_time(struct bt_ctf_clock *clock, uint64_t time);
void bt_ctf_clock_get(struct bt_ctf_clock *clock);
void bt_ctf_clock_put(struct bt_ctf_clock *clock);


/* event-types.h */
%rename("_bt_ctf_field_type_integer_create") bt_ctf_field_type_integer_create(unsigned int size);
%rename("_bt_ctf_field_type_integer_get_size") bt_ctf_field_type_integer_get_size(struct bt_ctf_field_type *integer);
%rename("_bt_ctf_field_type_integer_get_signed") bt_ctf_field_type_integer_get_signed(struct bt_ctf_field_type *integer);
%rename("_bt_ctf_field_type_integer_set_signed") bt_ctf_field_type_integer_set_signed(struct bt_ctf_field_type *integer, int is_signed);
%rename("_bt_ctf_field_type_integer_get_base") bt_ctf_field_type_integer_get_base(struct bt_ctf_field_type *integer);
%rename("_bt_ctf_field_type_integer_set_base") bt_ctf_field_type_integer_set_base(struct bt_ctf_field_type *integer, enum bt_ctf_integer_base base);
%rename("_bt_ctf_field_type_integer_get_encoding") bt_ctf_field_type_integer_get_encoding(struct bt_ctf_field_type *integer);
%rename("_bt_ctf_field_type_integer_set_encoding") bt_ctf_field_type_integer_set_encoding(struct bt_ctf_field_type *integer, enum ctf_string_encoding encoding);
%rename("_bt_ctf_field_type_enumeration_create") bt_ctf_field_type_enumeration_create(struct bt_ctf_field_type *integer_container_type);
%rename("_bt_ctf_field_type_enumeration_get_container_type") bt_ctf_field_type_enumeration_get_container_type(struct bt_ctf_field_type *enumeration);
%rename("_bt_ctf_field_type_enumeration_add_mapping") bt_ctf_field_type_enumeration_add_mapping(struct bt_ctf_field_type *enumeration, const char *name, int64_t range_start, int64_t range_end);
%rename("_bt_ctf_field_type_enumeration_add_mapping_unsigned") bt_ctf_field_type_enumeration_add_mapping_unsigned(struct bt_ctf_field_type *enumeration, const char *name, uint64_t range_start, uint64_t range_end);
%rename("_bt_ctf_field_type_enumeration_get_mapping_count") bt_ctf_field_type_enumeration_get_mapping_count(struct bt_ctf_field_type *enumeration);
%rename("_bt_ctf_field_type_enumeration_get_mapping") bt_ctf_field_type_enumeration_get_mapping(struct bt_ctf_field_type *enumeration, int index, const char **name, int64_t *range_start, int64_t *range_end);
%rename("_bt_ctf_field_type_enumeration_get_mapping_unsigned") bt_ctf_field_type_enumeration_get_mapping_unsigned(struct bt_ctf_field_type *enumeration, int index, const char **name, uint64_t *range_start, uint64_t *range_end);
%rename("_bt_ctf_field_type_enumeration_get_mapping_index_by_name") bt_ctf_field_type_enumeration_get_mapping_index_by_name(struct bt_ctf_field_type *enumeration, const char *name);
%rename("_bt_ctf_field_type_enumeration_get_mapping_index_by_value") bt_ctf_field_type_enumeration_get_mapping_index_by_value(struct bt_ctf_field_type *enumeration, int64_t value);
%rename("_bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value") bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(struct bt_ctf_field_type *enumeration, uint64_t value);
%rename("_bt_ctf_field_type_floating_point_create") bt_ctf_field_type_floating_point_create(void);
%rename("_bt_ctf_field_type_floating_point_get_exponent_digits") bt_ctf_field_type_floating_point_get_exponent_digits(struct bt_ctf_field_type *floating_point);
%rename("_bt_ctf_field_type_floating_point_set_exponent_digits") bt_ctf_field_type_floating_point_set_exponent_digits(struct bt_ctf_field_type *floating_point, unsigned int exponent_digits);
%rename("_bt_ctf_field_type_floating_point_get_mantissa_digits") bt_ctf_field_type_floating_point_get_mantissa_digits(struct bt_ctf_field_type *floating_point);
%rename("_bt_ctf_field_type_floating_point_set_mantissa_digits") bt_ctf_field_type_floating_point_set_mantissa_digits(struct bt_ctf_field_type *floating_point, unsigned int mantissa_digits);
%rename("_bt_ctf_field_type_structure_create") bt_ctf_field_type_structure_create(void);
%rename("_bt_ctf_field_type_structure_add_field") bt_ctf_field_type_structure_add_field(struct bt_ctf_field_type *structure, struct bt_ctf_field_type *field_type, const char *field_name);
%rename("_bt_ctf_field_type_structure_get_field_count") bt_ctf_field_type_structure_get_field_count(struct bt_ctf_field_type *structure);
%rename("_bt_ctf_field_type_structure_get_field") bt_ctf_field_type_structure_get_field(struct bt_ctf_field_type *structure, const char **field_name, struct bt_ctf_field_type **field_type, int index);
%rename("_bt_ctf_field_type_structure_get_field_type_by_name") bt_ctf_field_type_structure_get_field_type_by_name(struct bt_ctf_field_type *structure, const char *field_name);
%rename("_bt_ctf_field_type_variant_create") bt_ctf_field_type_variant_create(struct bt_ctf_field_type *enum_tag, const char *tag_name);
%rename("_bt_ctf_field_type_variant_get_tag_type") bt_ctf_field_type_variant_get_tag_type(struct bt_ctf_field_type *variant);
%rename("_bt_ctf_field_type_variant_get_tag_name") bt_ctf_field_type_variant_get_tag_name(struct bt_ctf_field_type *variant);
%rename("_bt_ctf_field_type_variant_add_field") bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *variant, struct bt_ctf_field_type *field_type, const char *field_name);
%rename("_bt_ctf_field_type_variant_get_field_type_by_name") bt_ctf_field_type_variant_get_field_type_by_name(struct bt_ctf_field_type *variant, const char *field_name);
%rename("_bt_ctf_field_type_variant_get_field_type_from_tag") bt_ctf_field_type_variant_get_field_type_from_tag(struct bt_ctf_field_type *variant, struct bt_ctf_field *tag);
%rename("_bt_ctf_field_type_variant_get_field_count") bt_ctf_field_type_variant_get_field_count(struct bt_ctf_field_type *variant);
%rename("_bt_ctf_field_type_variant_get_field") bt_ctf_field_type_variant_get_field(struct bt_ctf_field_type *variant, const char **field_name, struct bt_ctf_field_type **field_type, int index);
%rename("_bt_ctf_field_type_array_create") bt_ctf_field_type_array_create(struct bt_ctf_field_type *element_type, unsigned int length);
%rename("_bt_ctf_field_type_array_get_element_type") bt_ctf_field_type_array_get_element_type(struct bt_ctf_field_type *array);
%rename("_bt_ctf_field_type_array_get_length") bt_ctf_field_type_array_get_length(struct bt_ctf_field_type *array);
%rename("_bt_ctf_field_type_sequence_create") bt_ctf_field_type_sequence_create(struct bt_ctf_field_type *element_type, const char *length_field_name);
%rename("_bt_ctf_field_type_sequence_get_element_type") bt_ctf_field_type_sequence_get_element_type(struct bt_ctf_field_type *sequence);
%rename("_bt_ctf_field_type_sequence_get_length_field_name") bt_ctf_field_type_sequence_get_length_field_name(struct bt_ctf_field_type *sequence);
%rename("_bt_ctf_field_type_string_create") bt_ctf_field_type_string_create(void);
%rename("_bt_ctf_field_type_string_get_encoding") bt_ctf_field_type_string_get_encoding(struct bt_ctf_field_type *string_type);
%rename("_bt_ctf_field_type_string_set_encoding") bt_ctf_field_type_string_set_encoding(struct bt_ctf_field_type *string_type, enum ctf_string_encoding encoding);
%rename("_bt_ctf_field_type_get_alignment") bt_ctf_field_type_get_alignment(struct bt_ctf_field_type *type);
%rename("_bt_ctf_field_type_set_alignment") bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type, unsigned int alignment);
%rename("_bt_ctf_field_type_get_byte_order") bt_ctf_field_type_get_byte_order(struct bt_ctf_field_type *type);
%rename("_bt_ctf_field_type_set_byte_order") bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *type, enum bt_ctf_byte_order byte_order);
%rename("_bt_ctf_field_type_get_type_id") bt_ctf_field_type_get_type_id(struct bt_ctf_field_type *type);
%rename("_bt_ctf_field_type_get") bt_ctf_field_type_get(struct bt_ctf_field_type *type);
%rename("_bt_ctf_field_type_put") bt_ctf_field_type_put(struct bt_ctf_field_type *type);

struct bt_ctf_field_type *bt_ctf_field_type_integer_create(unsigned int size);
int bt_ctf_field_type_integer_get_size(struct bt_ctf_field_type *integer);
int bt_ctf_field_type_integer_get_signed(struct bt_ctf_field_type *integer);
int bt_ctf_field_type_integer_set_signed(struct bt_ctf_field_type *integer, int is_signed);
enum bt_ctf_integer_base bt_ctf_field_type_integer_get_base(struct bt_ctf_field_type *integer);
int bt_ctf_field_type_integer_set_base(struct bt_ctf_field_type *integer, enum bt_ctf_integer_base base);
enum ctf_string_encoding bt_ctf_field_type_integer_get_encoding(struct bt_ctf_field_type *integer);
int bt_ctf_field_type_integer_set_encoding(struct bt_ctf_field_type *integer, enum ctf_string_encoding encoding);
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(struct bt_ctf_field_type *integer_container_type);
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_get_container_type(struct bt_ctf_field_type *enumeration);
int bt_ctf_field_type_enumeration_add_mapping(struct bt_ctf_field_type *enumeration, const char *name, int64_t range_start, int64_t range_end);
int bt_ctf_field_type_enumeration_add_mapping_unsigned(struct bt_ctf_field_type *enumeration, const char *name, uint64_t range_start, uint64_t range_end);
int bt_ctf_field_type_enumeration_get_mapping_count(struct bt_ctf_field_type *enumeration);
int bt_ctf_field_type_enumeration_get_mapping(struct bt_ctf_field_type *enumeration, int index, const char **OUTPUT, int64_t *OUTPUT, int64_t *OUTPUT);
int bt_ctf_field_type_enumeration_get_mapping_unsigned(struct bt_ctf_field_type *enumeration, int index, const char **OUTPUT, uint64_t *OUTPUT, uint64_t *OUTPUT);
int bt_ctf_field_type_enumeration_get_mapping_index_by_name(struct bt_ctf_field_type *enumeration, const char *name);
int bt_ctf_field_type_enumeration_get_mapping_index_by_value(struct bt_ctf_field_type *enumeration, int64_t value);
int bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(struct bt_ctf_field_type *enumeration, uint64_t value);
struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void);
int bt_ctf_field_type_floating_point_get_exponent_digits(struct bt_ctf_field_type *floating_point);
int bt_ctf_field_type_floating_point_set_exponent_digits(struct bt_ctf_field_type *floating_point, unsigned int exponent_digits);
int bt_ctf_field_type_floating_point_get_mantissa_digits(struct bt_ctf_field_type *floating_point);
int bt_ctf_field_type_floating_point_set_mantissa_digits(struct bt_ctf_field_type *floating_point, unsigned int mantissa_digits);
struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void);
int bt_ctf_field_type_structure_add_field(struct bt_ctf_field_type *structure, struct bt_ctf_field_type *field_type, const char *field_name);
int bt_ctf_field_type_structure_get_field_count(struct bt_ctf_field_type *structure);
int bt_ctf_field_type_structure_get_field(struct bt_ctf_field_type *structure, const char **OUTPUT, struct bt_ctf_field_type **OUTPUT, int index);
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_field_type_by_name(struct bt_ctf_field_type *structure, const char *field_name);
struct bt_ctf_field_type *bt_ctf_field_type_variant_create(struct bt_ctf_field_type *enum_tag, const char *tag_name);
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_tag_type(struct bt_ctf_field_type *variant);
const char *bt_ctf_field_type_variant_get_tag_name(struct bt_ctf_field_type *variant);
int bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *variant, struct bt_ctf_field_type *field_type, const char *field_name);
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_by_name(struct bt_ctf_field_type *variant, const char *field_name);
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_from_tag(struct bt_ctf_field_type *variant, struct bt_ctf_field *tag);
int bt_ctf_field_type_variant_get_field_count(struct bt_ctf_field_type *variant);
int bt_ctf_field_type_variant_get_field(struct bt_ctf_field_type *variant, const char **OUTPUT, struct bt_ctf_field_type **OUTPUT, int index);
struct bt_ctf_field_type *bt_ctf_field_type_array_create(struct bt_ctf_field_type *element_type, unsigned int length);
struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(struct bt_ctf_field_type *array);
int64_t bt_ctf_field_type_array_get_length(struct bt_ctf_field_type *array);
struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(struct bt_ctf_field_type *element_type, const char *length_field_name);
struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(struct bt_ctf_field_type *sequence);
const char *bt_ctf_field_type_sequence_get_length_field_name(struct bt_ctf_field_type *sequence);
struct bt_ctf_field_type *bt_ctf_field_type_string_create(void);
enum ctf_string_encoding bt_ctf_field_type_string_get_encoding(struct bt_ctf_field_type *string_type);
int bt_ctf_field_type_string_set_encoding(struct bt_ctf_field_type *string_type, enum ctf_string_encoding encoding);
int bt_ctf_field_type_get_alignment(struct bt_ctf_field_type *type);
int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type, unsigned int alignment);
enum bt_ctf_byte_order bt_ctf_field_type_get_byte_order(struct bt_ctf_field_type *type);
int bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *type, enum bt_ctf_byte_order byte_order);
enum ctf_type_id bt_ctf_field_type_get_type_id(struct bt_ctf_field_type *type);
void bt_ctf_field_type_get(struct bt_ctf_field_type *type);
void bt_ctf_field_type_put(struct bt_ctf_field_type *type);


/* event-fields.h */
%rename("_bt_ctf_field_create") bt_ctf_field_create(struct bt_ctf_field_type *type);
%rename("_bt_ctf_field_structure_get_field") bt_ctf_field_structure_get_field(struct bt_ctf_field *structure, const char *name);
%rename("_bt_ctf_field_array_get_field") bt_ctf_field_array_get_field(struct bt_ctf_field *array, uint64_t index);
%rename("_bt_ctf_field_sequence_get_length") bt_ctf_field_sequence_get_length(struct bt_ctf_field *sequence);
%rename("_bt_ctf_field_sequence_set_length") bt_ctf_field_sequence_set_length(struct bt_ctf_field *sequence, struct bt_ctf_field *length_field);
%rename("_bt_ctf_field_sequence_get_field") bt_ctf_field_sequence_get_field(struct bt_ctf_field *sequence, uint64_t index);
%rename("_bt_ctf_field_variant_get_field") bt_ctf_field_variant_get_field(struct bt_ctf_field *variant, struct bt_ctf_field *tag);
%rename("_bt_ctf_field_enumeration_get_container") bt_ctf_field_enumeration_get_container(struct bt_ctf_field *enumeration);
%rename("_bt_ctf_field_enumeration_get_mapping_name") bt_ctf_field_enumeration_get_mapping_name(struct bt_ctf_field *enumeration);
%rename("_bt_ctf_field_signed_integer_get_value") bt_ctf_field_signed_integer_get_value(struct bt_ctf_field *integer, int64_t *value);
%rename("_bt_ctf_field_signed_integer_set_value") bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *integer, int64_t value);
%rename("_bt_ctf_field_unsigned_integer_get_value") bt_ctf_field_unsigned_integer_get_value(struct bt_ctf_field *integer, uint64_t *value);
%rename("_bt_ctf_field_unsigned_integer_set_value") bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *integer, uint64_t value);
%rename("_bt_ctf_field_floating_point_get_value") bt_ctf_field_floating_point_get_value(struct bt_ctf_field *floating_point, double *value);
%rename("_bt_ctf_field_floating_point_set_value") bt_ctf_field_floating_point_set_value(struct bt_ctf_field *floating_point, double value);
%rename("_bt_ctf_field_string_get_value") bt_ctf_field_string_get_value(struct bt_ctf_field *string_field);
%rename("_bt_ctf_field_string_set_value") bt_ctf_field_string_set_value(struct bt_ctf_field *string_field, const char *value);
%rename("_bt_ctf_field_get_type") bt_ctf_field_get_type(struct bt_ctf_field *field);
%rename("_bt_ctf_field_get") bt_ctf_field_get(struct bt_ctf_field *field);
%rename("_bt_ctf_field_put") bt_ctf_field_put(struct bt_ctf_field *field);

struct bt_ctf_field *bt_ctf_field_create(struct bt_ctf_field_type *type);
struct bt_ctf_field *bt_ctf_field_structure_get_field(struct bt_ctf_field *structure, const char *name);
struct bt_ctf_field *bt_ctf_field_array_get_field(struct bt_ctf_field *array, uint64_t index);
struct bt_ctf_field * bt_ctf_field_sequence_get_length(struct bt_ctf_field *sequence);
int bt_ctf_field_sequence_set_length(struct bt_ctf_field *sequence, struct bt_ctf_field *length_field);
struct bt_ctf_field *bt_ctf_field_sequence_get_field(struct bt_ctf_field *sequence, uint64_t index);
struct bt_ctf_field *bt_ctf_field_variant_get_field(struct bt_ctf_field *variant, struct bt_ctf_field *tag);
struct bt_ctf_field *bt_ctf_field_enumeration_get_container(struct bt_ctf_field *enumeration);
const char *bt_ctf_field_enumeration_get_mapping_name(struct bt_ctf_field *enumeration);
int bt_ctf_field_signed_integer_get_value(struct bt_ctf_field *integer, int64_t *OUTPUT);
int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *integer, int64_t value);
int bt_ctf_field_unsigned_integer_get_value(struct bt_ctf_field *integer, uint64_t *OUTPUT);
int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *integer, uint64_t value);
int bt_ctf_field_floating_point_get_value(struct bt_ctf_field *floating_point, double *OUTPUT);
int bt_ctf_field_floating_point_set_value(struct bt_ctf_field *floating_point, double value);
const char *bt_ctf_field_string_get_value(struct bt_ctf_field *string_field);
int bt_ctf_field_string_set_value(struct bt_ctf_field *string_field, const char *value);
struct bt_ctf_field_type *bt_ctf_field_get_type(struct bt_ctf_field *field);
void bt_ctf_field_get(struct bt_ctf_field *field);
void bt_ctf_field_put(struct bt_ctf_field *field);


/* event-class.h */
%rename("_bt_ctf_event_class_create") bt_ctf_event_class_create(const char *name);
%rename("_bt_ctf_event_class_get_name") bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_event_class_get_id") bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_event_class_set_id") bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class, uint32_t id);
%rename("_bt_ctf_event_class_get_stream_class") bt_ctf_event_class_get_stream_class(struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_event_class_add_field") bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class, struct bt_ctf_field_type *type, const char *name);
%rename("_bt_ctf_event_class_get_field_count") bt_ctf_event_class_get_field_count(struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_event_class_get_field") bt_ctf_event_class_get_field(struct bt_ctf_event_class *event_class, const char **field_name, struct bt_ctf_field_type **field_type, int index);
%rename("_bt_ctf_event_class_get_field_by_name") bt_ctf_event_class_get_field_by_name(struct bt_ctf_event_class *event_class, const char *name);
%rename("_bt_ctf_event_class_get") bt_ctf_event_class_get(struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_event_class_put") bt_ctf_event_class_put(struct bt_ctf_event_class *event_class);

struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);
const char *bt_ctf_event_class_get_name(struct bt_ctf_event_class *event_class);
int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_id(struct bt_ctf_event_class *event_class, uint32_t id);
struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class, struct bt_ctf_field_type *type, const char *name);
int bt_ctf_event_class_get_field_count(struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_get_field(struct bt_ctf_event_class *event_class, const char **field_name, struct bt_ctf_field_type **field_type, int index);
struct bt_ctf_field_type *bt_ctf_event_class_get_field_by_name(struct bt_ctf_event_class *event_class, const char *name);
void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class);
void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class);


/* event.h */
%rename("_bt_ctf_event_create") bt_ctf_event_create(struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_event_get_class") bt_ctf_event_get_class(struct bt_ctf_event *event);
%rename("_bt_ctf_event_get_clock") bt_ctf_event_get_clock(struct bt_ctf_event *event);
%rename("_bt_ctf_event_get_payload") bt_ctf_event_get_payload(struct bt_ctf_event *event, const char *name);
%rename("_bt_ctf_event_set_payload") bt_ctf_event_set_payload(struct bt_ctf_event *event, const char *name, struct bt_ctf_field *value);
%rename("_bt_ctf_event_get_payload_by_index") bt_ctf_event_get_payload_by_index(struct bt_ctf_event *event, int index);
%rename("_bt_ctf_event_get") bt_ctf_event_get(struct bt_ctf_event *event);
%rename("_bt_ctf_event_put") bt_ctf_event_put(struct bt_ctf_event *event);

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class);
struct bt_ctf_event_class *bt_ctf_event_get_class(struct bt_ctf_event *event);
struct bt_ctf_clock *bt_ctf_event_get_clock(struct bt_ctf_event *event);
struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event, const char *name);
int bt_ctf_event_set_payload(struct bt_ctf_event *event, const char *name, struct bt_ctf_field *value);
struct bt_ctf_field *bt_ctf_event_get_payload_by_index(struct bt_ctf_event *event, int index);
void bt_ctf_event_get(struct bt_ctf_event *event);
void bt_ctf_event_put(struct bt_ctf_event *event);


/* stream-class.h */
%rename("_bt_ctf_stream_class_create") bt_ctf_stream_class_create(const char *name);
%rename("_bt_ctf_stream_class_get_name") bt_ctf_stream_class_get_name(struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_stream_class_get_clock") bt_ctf_stream_class_get_clock(struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_stream_class_set_clock") bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class, struct bt_ctf_clock *clock);
%rename("_bt_ctf_stream_class_get_id") bt_ctf_stream_class_get_id(struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_stream_class_set_id") bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class, uint32_t id);
%rename("_bt_ctf_stream_class_add_event_class") bt_ctf_stream_class_add_event_class(struct bt_ctf_stream_class *stream_class, struct bt_ctf_event_class *event_class);
%rename("_bt_ctf_stream_class_get_event_class_count") bt_ctf_stream_class_get_event_class_count(struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_stream_class_get_event_class") bt_ctf_stream_class_get_event_class(struct bt_ctf_stream_class *stream_class, int index);
%rename("_bt_ctf_stream_class_get_event_class_by_name") bt_ctf_stream_class_get_event_class_by_name(struct bt_ctf_stream_class *stream_class, const char *name);
%rename("_bt_ctf_stream_class_get_packet_context_type") bt_ctf_stream_class_get_packet_context_type(struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_stream_class_set_packet_context_type") bt_ctf_stream_class_set_packet_context_type(struct bt_ctf_stream_class *stream_class, struct bt_ctf_field_type *packet_context_type);
%rename("_bt_ctf_stream_class_get") bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_stream_class_put") bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class);

struct bt_ctf_stream_class *bt_ctf_stream_class_create(const char *name);
const char *bt_ctf_stream_class_get_name(struct bt_ctf_stream_class *stream_class);
struct bt_ctf_clock *bt_ctf_stream_class_get_clock(struct bt_ctf_stream_class *stream_class);
int bt_ctf_stream_class_set_clock(struct bt_ctf_stream_class *stream_class, struct bt_ctf_clock *clock);
int64_t bt_ctf_stream_class_get_id(struct bt_ctf_stream_class *stream_class);
int bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class, uint32_t id);
int bt_ctf_stream_class_add_event_class(struct bt_ctf_stream_class *stream_class, struct bt_ctf_event_class *event_class);
int bt_ctf_stream_class_get_event_class_count(struct bt_ctf_stream_class *stream_class);
struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class(struct bt_ctf_stream_class *stream_class, int index);
struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_name(struct bt_ctf_stream_class *stream_class, const char *name);
struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(struct bt_ctf_stream_class *stream_class);
int bt_ctf_stream_class_set_packet_context_type(struct bt_ctf_stream_class *stream_class, struct bt_ctf_field_type *packet_context_type);
void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class);
void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class);


/* stream.h */
%rename("_bt_ctf_stream_get_discarded_events_count") bt_ctf_stream_get_discarded_events_count(struct bt_ctf_stream *stream, uint64_t *count);
%rename("_bt_ctf_stream_append_discarded_events") bt_ctf_stream_append_discarded_events(struct bt_ctf_stream *stream, uint64_t event_count);
%rename("_bt_ctf_stream_append_event") bt_ctf_stream_append_event(struct bt_ctf_stream *stream, struct bt_ctf_event *event);
%rename("_bt_ctf_stream_get_packet_context") bt_ctf_stream_get_packet_context(struct bt_ctf_stream *stream);
%rename("_bt_ctf_stream_set_packet_context") bt_ctf_stream_set_packet_context(struct bt_ctf_stream *stream, struct bt_ctf_field *packet_context);
%rename("_bt_ctf_stream_flush") bt_ctf_stream_flush(struct bt_ctf_stream *stream);
%rename("_bt_ctf_stream_get") bt_ctf_stream_get(struct bt_ctf_stream *stream);
%rename("_bt_ctf_stream_put") bt_ctf_stream_put(struct bt_ctf_stream *stream);

int bt_ctf_stream_get_discarded_events_count(struct bt_ctf_stream *stream, uint64_t *OUTPUT);
void bt_ctf_stream_append_discarded_events(struct bt_ctf_stream *stream, uint64_t event_count);
int bt_ctf_stream_append_event(struct bt_ctf_stream *stream, struct bt_ctf_event *event);
struct bt_ctf_field *bt_ctf_stream_get_packet_context(struct bt_ctf_stream *stream);
int bt_ctf_stream_set_packet_context(struct bt_ctf_stream *stream, struct bt_ctf_field *packet_context);
int bt_ctf_stream_flush(struct bt_ctf_stream *stream);
void bt_ctf_stream_get(struct bt_ctf_stream *stream);
void bt_ctf_stream_put(struct bt_ctf_stream *stream);


/* writer.h */
%rename("_bt_ctf_writer_create") bt_ctf_writer_create(const char *path);
%rename("_bt_ctf_writer_create_stream") bt_ctf_writer_create_stream(struct bt_ctf_writer *writer, struct bt_ctf_stream_class *stream_class);
%rename("_bt_ctf_writer_add_environment_field") bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer, const char *name, const char *value);
%rename("_bt_ctf_writer_add_clock") bt_ctf_writer_add_clock(struct bt_ctf_writer *writer, struct bt_ctf_clock *clock);
%newobject bt_ctf_writer_get_metadata_string;
%rename("_bt_ctf_writer_get_metadata_string") bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer);
%rename("_bt_ctf_writer_flush_metadata") bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer);
%rename("_bt_ctf_writer_set_byte_order") bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer, enum bt_ctf_byte_order byte_order);
%rename("_bt_ctf_writer_get") bt_ctf_writer_get(struct bt_ctf_writer *writer);
%rename("_bt_ctf_writer_put") bt_ctf_writer_put(struct bt_ctf_writer *writer);

struct bt_ctf_writer *bt_ctf_writer_create(const char *path);
struct bt_ctf_stream *bt_ctf_writer_create_stream(struct bt_ctf_writer *writer, struct bt_ctf_stream_class *stream_class);
int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer, const char *name, const char *value);
int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer, struct bt_ctf_clock *clock);
char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer);
void bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer);
int bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer, enum bt_ctf_byte_order byte_order);
void bt_ctf_writer_get(struct bt_ctf_writer *writer);
void bt_ctf_writer_put(struct bt_ctf_writer *writer);
