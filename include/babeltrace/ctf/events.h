#ifndef _BABELTRACE_CTF_EVENTS_H
#define _BABELTRACE_CTF_EVENTS_H

/*
 * BabelTrace
 *
 * CTF events API
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <babeltrace/context.h>
#include <babeltrace/clock-types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct definition;
struct declaration;
struct bt_ctf_event;
struct bt_ctf_event_decl;
struct bt_ctf_field_decl;

/*
 * the top-level scopes in CTF
 */
enum bt_ctf_scope {
	BT_TRACE_PACKET_HEADER          = 0,
	BT_STREAM_PACKET_CONTEXT        = 1,
	BT_STREAM_EVENT_HEADER          = 2,
	BT_STREAM_EVENT_CONTEXT         = 3,
	BT_EVENT_CONTEXT                = 4,
	BT_EVENT_FIELDS                 = 5,
};

/*
 * the supported CTF types
 */
enum ctf_type_id {
	CTF_TYPE_UNKNOWN = 0,
	CTF_TYPE_INTEGER,
	CTF_TYPE_FLOAT,
	CTF_TYPE_ENUM,
	CTF_TYPE_STRING,
	CTF_TYPE_STRUCT,
	CTF_TYPE_UNTAGGED_VARIANT,
	CTF_TYPE_VARIANT,
	CTF_TYPE_ARRAY,
	CTF_TYPE_SEQUENCE,
	NR_CTF_TYPES,
};

/*
 * the supported CTF string encodings
 */
enum ctf_string_encoding {
	CTF_STRING_NONE = 0,
	CTF_STRING_UTF8,
	CTF_STRING_ASCII,
	CTF_STRING_UNKNOWN,
};

/*
 * bt_ctf_get_top_level_scope: return a definition of the top-level scope
 *
 * Top-level scopes are defined in the bt_ctf_scope enum.
 * In order to get a field or a field list, the user needs to pass a
 * scope as argument, this scope can be a top-level scope or a scope
 * relative to an arbitrary field. This function provides the mapping
 * between the enum and the actual definition of top-level scopes.
 * On error return NULL.
 */
const struct definition *bt_ctf_get_top_level_scope(const struct bt_ctf_event *event,
		enum bt_ctf_scope scope);

/*
 * bt_ctf_event_get_name: returns the name of the event or NULL on error
 */
const char *bt_ctf_event_name(const struct bt_ctf_event *event);

/*
 * bt_ctf_get_cycles: returns the timestamp of the event as written
 * in the packet (in cycles) or -1ULL on error.
 */
uint64_t bt_ctf_get_cycles(const struct bt_ctf_event *event);

/*
 * bt_ctf_get_timestamp: returns the timestamp of the event offsetted
 * with the system clock source (in ns) or -1ULL on error
 */
uint64_t bt_ctf_get_timestamp(const struct bt_ctf_event *event);

/*
 * bt_ctf_get_field_list: obtain the list of fields for compound type
 *
 * This function can be used to obtain the list of fields contained
 * within a top-level scope of an event or a compound type: array,
 * sequence, structure, or variant.

 * This function sets the "list" pointer to an array of definition
 * pointers and set count to the number of elements in the array.
 * Return 0 on success and a negative value on error.
 *
 * The content pointed to by "list" should *not* be freed. It stays
 * valid as long as the event is unchanged (as long as the iterator
 * from which the event is extracted is unchanged).
 */
int bt_ctf_get_field_list(const struct bt_ctf_event *event,
		const struct definition *scope,
		struct definition const * const **list,
		unsigned int *count);

/*
 * bt_ctf_get_field: returns the definition of a specific field
 */
const struct definition *bt_ctf_get_field(const struct bt_ctf_event *event,
		const struct definition *scope,
		const char *field);

/*
 * bt_ctf_get_index: if the field is an array or a sequence, return the element
 * at position index, otherwise return NULL;
 */
const struct definition *bt_ctf_get_index(const struct bt_ctf_event *event,
		const struct definition *field,
		unsigned int index);

/*
 * bt_ctf_field_name: returns the name of a field or NULL on error
 */
const char *bt_ctf_field_name(const struct definition *def);

/*
 * bt_ctf_get_decl_from_def: return the declaration of a field from
 * its definition or NULL on error
 */
const struct declaration *bt_ctf_get_decl_from_def(const struct definition *def);

/*
 * bt_ctf_get_decl_from_field_decl: return the declaration of a field from
 * a field_decl or NULL on error
 */
const struct declaration *bt_ctf_get_decl_from_field_decl(
		const struct bt_ctf_field_decl *field);

/*
 * bt_ctf_field_type: returns the type of a field or -1 if unknown
 */
enum ctf_type_id bt_ctf_field_type(const struct declaration *decl);

/*
 * bt_ctf_get_int_signedness: return the signedness of an integer
 *
 * return 0 if unsigned
 * return 1 if signed
 * return -1 on error
 */
int bt_ctf_get_int_signedness(const struct declaration *decl);

/*
 * bt_ctf_get_int_base: return the base of an int or a negative value on error
 */
int bt_ctf_get_int_base(const struct declaration *decl);

/*
 * bt_ctf_get_int_byte_order: return the byte order of an int or a negative
 * value on error
 */
int bt_ctf_get_int_byte_order(const struct declaration *decl);

/*
 * bt_ctf_get_int_len: return the size, in bits, of an int or a negative
 * value on error
 */
ssize_t bt_ctf_get_int_len(const struct declaration *decl);

/*
 * bt_ctf_get_encoding: return the encoding of an int, a string, or of
 * the integer contained in a char array or a sequence.
 * return a negative value on error
 */
enum ctf_string_encoding bt_ctf_get_encoding(const struct declaration *decl);

/*
 * bt_ctf_get_array_len: return the len of an array or a negative
 * value on error
 */
int bt_ctf_get_array_len(const struct declaration *decl);

/*
 * Field access functions
 *
 * These functions return the value associated with the field passed in
 * parameter.
 *
 * If the field does not exist or is not of the type requested, the value
 * returned is undefined. To check if an error occured, use the
 * bt_ctf_field_get_error() function after accessing a field.
 *
 * bt_ctf_get_enum_int gets the integer field of an enumeration.
 * bt_ctf_get_enum_str gets the string matching the current enumeration
 * value, or NULL if the current value does not match any string.
 */
uint64_t bt_ctf_get_uint64(const struct definition *field);
int64_t bt_ctf_get_int64(const struct definition *field);
const struct definition *bt_ctf_get_enum_int(const struct definition *field);
const char *bt_ctf_get_enum_str(const struct definition *field);
char *bt_ctf_get_char_array(const struct definition *field);
char *bt_ctf_get_string(const struct definition *field);

/*
 * bt_ctf_field_get_error: returns the last error code encountered while
 * accessing a field and reset the error flag.
 * Return 0 if no error, a negative value otherwise.
 */
int bt_ctf_field_get_error(void);

/*
 * bt_ctf_get_event_decl_list: get a list of all the event declarations in
 * a trace.
 *
 * The list array is pointed to the array of event declarations.
 * The number of events in the array is written in count.
 *
 * Return 0 on success and a negative value on error.
 *
 * The content pointed to by "list" should *not* be freed. It stays
 * valid as long as the trace is opened.
 */
int bt_ctf_get_event_decl_list(int handle_id, struct bt_context *ctx,
		struct bt_ctf_event_decl * const **list,
		unsigned int *count);

/*
 * bt_ctf_get_decl_event_name: return the name of the event or NULL on error
 */
const char *bt_ctf_get_decl_event_name(const struct bt_ctf_event_decl *event);

/*
 * bt_ctf_get_decl_fields: get all field declarations in a scope of an event
 *
 * The list array is pointed to the array of field declaration.
 * The number of field declaration in the array is written in count.
 *
 * Returns 0 on success and a negative value on error
 *
 * The content pointed to by "list" should *not* be freed. It stays
 * valid as long as the trace is opened.
 */
int bt_ctf_get_decl_fields(struct bt_ctf_event_decl *event_decl,
		enum bt_ctf_scope scope,
		struct bt_ctf_field_decl const * const **list,
		unsigned int *count);

/*
 * bt_ctf_get_decl_field_name: return the name of a field decl or NULL on error
 */
const char *bt_ctf_get_decl_field_name(const struct bt_ctf_field_decl *field);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_EVENTS_H */
