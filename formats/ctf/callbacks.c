/*
 * callbacks.c
 *
 * Babeltrace Library
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf/callbacks-internal.h>
#include <inttypes.h>

static
struct bt_dependencies *_babeltrace_dependencies_create(const char *first,
							va_list ap)
{
	const char *iter;
	struct bt_dependencies *dep;

	dep = g_new0(struct bt_dependencies, 1);
	dep->refcount = 1;
	dep->deps = g_array_new(FALSE, TRUE, sizeof(GQuark));
	iter = first;
	while (iter) {
		GQuark q = g_quark_from_string(iter);
		g_array_append_val(dep->deps, q);
		iter = va_arg(ap, const char *);
	}
	return dep;
}

struct bt_dependencies *babeltrace_dependencies_create(const char *first, ...)
{
	va_list ap;
	struct bt_dependencies *deps;

	va_start(ap, first);
	deps = _babeltrace_dependencies_create(first, ap);
	va_end(ap);
	return deps;
}

/*
 * bt_ctf_iter_add_callback: Add a callback to CTF iterator.
 */
int bt_ctf_iter_add_callback(struct bt_ctf_iter *iter,
		bt_intern_str event, void *private_data, int flags,
		enum bt_cb_ret (*callback)(struct bt_ctf_event *ctf_data,
					   void *private_data),
		struct bt_dependencies *depends,
		struct bt_dependencies *weak_depends,
		struct bt_dependencies *provides)
{
	int i, stream_id;
	gpointer *event_id_ptr;
	unsigned long event_id;
	struct trace_collection *tc = iter->parent.ctx->tc;

	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		tin = container_of(td_read, struct ctf_trace, parent);

		for (stream_id = 0; stream_id < tin->streams->len; stream_id++) {
			struct ctf_stream_declaration *stream;
			struct bt_stream_callbacks *bt_stream_cb = NULL;
			struct bt_callback_chain *bt_chain = NULL;
			struct bt_callback new_callback;

			stream = g_ptr_array_index(tin->streams, stream_id);

			if (stream_id >= iter->callbacks->len) {
				g_array_set_size(iter->callbacks, stream->stream_id + 1);
			}
			bt_stream_cb = &g_array_index(iter->callbacks,
					struct bt_stream_callbacks, stream->stream_id);
			if (!bt_stream_cb->per_id_callbacks) {
				bt_stream_cb->per_id_callbacks = g_array_new(FALSE, TRUE,
						sizeof(struct bt_callback_chain));
			}

			if (event) {
				/* find the event id */
				event_id_ptr = g_hash_table_lookup(stream->event_quark_to_id,
						(gconstpointer) (unsigned long) event);
				/* event not found in this stream class */
				if (!event_id_ptr) {
					fprintf(stderr, "[error] Event ID not found in stream class\n");
					continue;
				}
				event_id = (uint64_t)(unsigned long) *event_id_ptr;

				/* find or create the bt_callback_chain for this event */
				if (event_id >= bt_stream_cb->per_id_callbacks->len) {
					g_array_set_size(bt_stream_cb->per_id_callbacks, event_id + 1);
				}
				bt_chain = &g_array_index(bt_stream_cb->per_id_callbacks,
						struct bt_callback_chain, event_id);
				if (!bt_chain->callback) {
					bt_chain->callback = g_array_new(FALSE, TRUE,
						sizeof(struct bt_callback));
				}
			} else {
				/* callback for all events */
				if (!iter->main_callbacks.callback) {
					iter->main_callbacks.callback = g_array_new(FALSE, TRUE,
							sizeof(struct bt_callback));
				}
				bt_chain = &iter->main_callbacks;
			}

			new_callback.private_data = private_data;
			new_callback.flags = flags;
			new_callback.callback = callback;
			new_callback.depends = depends;
			new_callback.weak_depends = weak_depends;
			new_callback.provides = provides;

			/* TODO : take care of priority, for now just FIFO */
			g_array_append_val(bt_chain->callback, new_callback);
		}
	}

	return 0;
}

static
int extract_ctf_stream_event(struct ctf_stream_definition *stream,
		struct bt_ctf_event *event)
{
	struct ctf_stream_declaration *stream_class = stream->stream_class;
	struct ctf_event_declaration *event_class;
	uint64_t id = stream->event_id;

	if (id >= stream_class->events_by_id->len) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -1;
	}
	event->parent = g_ptr_array_index(stream->events_by_id, id);
	if (!event->parent) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -1;
	}
	event_class = g_ptr_array_index(stream_class->events_by_id, id);
	if (!event_class) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -1;
	}

	return 0;
}

void process_callbacks(struct bt_ctf_iter *iter,
		       struct ctf_stream_definition *stream)
{
	struct bt_stream_callbacks *bt_stream_cb;
	struct bt_callback_chain *bt_chain;
	struct bt_callback *cb;
	int i;
	enum bt_cb_ret ret;
	struct bt_ctf_event ctf_data;

	ret = extract_ctf_stream_event(stream, &ctf_data);

	/* process all events callback first */
	if (iter->main_callbacks.callback) {
		for (i = 0; i < iter->main_callbacks.callback->len; i++) {
			cb = &g_array_index(iter->main_callbacks.callback, struct bt_callback, i);
			if (!cb)
				goto end;
			ret = cb->callback(&ctf_data, cb->private_data);
			switch (ret) {
			case BT_CB_OK_STOP:
			case BT_CB_ERROR_STOP:
				goto end;
			default:
				break;
			}
		}
	}

	/* process per event callbacks */
	bt_stream_cb = &g_array_index(iter->callbacks,
			struct bt_stream_callbacks, stream->stream_id);
	if (!bt_stream_cb || !bt_stream_cb->per_id_callbacks)
		goto end;

	if (stream->event_id >= bt_stream_cb->per_id_callbacks->len)
		goto end;
	bt_chain = &g_array_index(bt_stream_cb->per_id_callbacks,
			struct bt_callback_chain, stream->event_id);
	if (!bt_chain || !bt_chain->callback)
		goto end;

	for (i = 0; i < bt_chain->callback->len; i++) {
		cb = &g_array_index(bt_chain->callback, struct bt_callback, i);
		if (!cb)
			goto end;
		ret = cb->callback(&ctf_data, cb->private_data);
		switch (ret) {
		case BT_CB_OK_STOP:
		case BT_CB_ERROR_STOP:
			goto end;
		default:
			break;
		}
	}

end:
	return;
}
