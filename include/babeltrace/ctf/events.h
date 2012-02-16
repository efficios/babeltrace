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
 */

#include <stdint.h>

struct ctf_stream;
struct ctf_stream_event;
struct definition;

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
 * the structure to manipulate events
 */
struct bt_ctf_event {
	struct ctf_stream *stream;
	struct ctf_stream_event *event;
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
struct definition *bt_ctf_get_top_level_scope(struct bt_ctf_event *event,
		enum bt_ctf_scope scope);

/*
 * bt_ctf_event_get_name: returns the name of the event or NULL on error
 */
const char *bt_ctf_event_name(struct bt_ctf_event *event);

/*
 * bt_ctf_get_timestamp: returns the timestamp of the event or -1ULL on error
 */
uint64_t bt_ctf_get_timestamp(struct bt_ctf_event *event);

/*
 * bt_ctf_get_field_list: returns an array of *def pointing to each field of
 * the event. The array is NULL terminated.
 * On error : return NULL.
 */
int bt_ctf_get_field_list(struct bt_ctf_event *event,
		struct definition *scope,
		struct definition const * const **list,
		unsigned int *count);

/*
 * bt_ctf_get_field: returns the definition of a specific field
 */
struct definition *bt_ctf_get_field(struct bt_ctf_event *event,
		struct definition *scope,
		const char *field);

/*
 * bt_ctf_get_index: if the field is an array or a sequence, return the element
 * at position index, otherwise return NULL;
 */
struct definition *bt_ctf_get_index(struct bt_ctf_event *event,
		struct definition *field,
		unsigned int index);

/*
 * bt_ctf_field_name: returns the name of a field or NULL on error
 */
const char *bt_ctf_field_name(const struct definition *def);

/*
 * bt_ctf_field_type: returns the type of a field or -1 if unknown
 */
enum ctf_type_id bt_ctf_field_type(struct definition *def);

/*
 * Field access functions
 *
 * These functions return the value associated with the field passed in
 * parameter.
 *
 * If the field does not exist or is not of the type requested, the value
 * returned is undefined. To check if an error occured, use the
 * bt_ctf_field_error() function after accessing a field.
 */
uint64_t bt_ctf_get_uint64(struct definition *field);
int64_t bt_ctf_get_int64(struct definition *field);
char *bt_ctf_get_char_array(struct definition *field);
char *bt_ctf_get_string(struct definition *field);

/*
 * bt_ctf_field_error: returns the last error code encountered while
 * accessing a field and reset the error flag.
 * Return 0 if no error, a negative value otherwise.
 */
int bt_ctf_field_get_error(void);

#endif /* _BABELTRACE_CTF_EVENTS_H */
