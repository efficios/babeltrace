/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_CTF_WRITER_STREAM_CLASS_H
#define BABELTRACE2_CTF_WRITER_STREAM_CLASS_H

#include <babeltrace2-ctf-writer/object.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_stream_class;
struct bt_ctf_trace;
struct bt_ctf_event_class;
struct bt_ctf_field_type;
struct bt_ctf_clock;

extern struct bt_ctf_stream_class *bt_ctf_stream_class_create(
		const char *name);

extern struct bt_ctf_trace *bt_ctf_stream_class_get_trace(
		struct bt_ctf_stream_class *stream_class);

extern const char *bt_ctf_stream_class_get_name(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_name(
		struct bt_ctf_stream_class *stream_class, const char *name);

extern int64_t bt_ctf_stream_class_get_id(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id);

extern struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_packet_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type);

extern struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_header_type(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_event_header_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_header_type);

extern struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_context_type(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_event_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_context_type);

extern int64_t bt_ctf_stream_class_get_event_class_count(
		struct bt_ctf_stream_class *stream_class);

extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_index(
		struct bt_ctf_stream_class *stream_class, uint64_t index);

extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id);

extern int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_stream_class_set_clock(
		struct bt_ctf_stream_class *ctf_stream_class,
		struct bt_ctf_clock *clock);

extern struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
        struct bt_ctf_stream_class *stream_class);

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class)
{
	bt_ctf_object_get_ref(stream_class);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class)
{
	bt_ctf_object_put_ref(stream_class);
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_CTF_WRITER_STREAM_CLASS_H */
