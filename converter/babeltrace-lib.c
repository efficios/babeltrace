/*
 * babeltrace-lib.c
 *
 * Babeltrace Trace Converter Library
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

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/prio_heap.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <stdarg.h>

struct stream_saved_pos {
	/*
	 * Use file_stream pointer to check if the trace collection we
	 * restore to match the one we saved from, for each stream.
	 */
	struct ctf_file_stream *file_stream;
	size_t cur_index;	/* current index in packet index */
	ssize_t offset;		/* offset from base, in bits. EOF for end of file. */
};

struct babeltrace_saved_pos {
	struct trace_collection *tc;
	GArray *stream_saved_pos;	/* Contains struct stream_saved_pos */
};

struct bt_callback {
	int prio;		/* Callback order priority. Lower first. Dynamically assigned from dependency graph. */
	void *private_data;
	int flags;
	struct bt_dependencies *depends;
	struct bt_dependencies *weak_depends;
	struct bt_dependencies *provides;
	enum bt_cb_ret (*callback)(struct bt_ctf_data *ctf_data,
				   void *private_data);
};

struct bt_callback_chain {
	GArray *callback;	/* Array of struct bt_callback, ordered by priority */
};

/*
 * per id callbacks need to be per stream class because event ID vs
 * event name mapping can vary from stream to stream.
 */
struct bt_stream_callbacks {
	GArray *per_id_callbacks;	/* Array of struct bt_callback_chain */
};

/*
 * struct babeltrace_iter: data structure representing an iterator on a trace
 * collection.
 */
struct babeltrace_iter {
	struct ptr_heap *stream_heap;
	struct trace_collection *tc;
	struct trace_collection_pos *end_pos;
	GArray *callbacks;				/* Array of struct bt_stream_callbacks */
	struct bt_callback_chain main_callbacks;	/* For all events */
	/*
	 * Flag indicating if dependency graph needs to be recalculated.
	 * Set by babeltrace_iter_add_callback(), and checked (and
	 * cleared) by upon entry into babeltrace_iter_read_event().
	 * babeltrace_iter_read_event() is responsible for calling dep
	 * graph calculation if it sees this flag set.
	 */
	int recalculate_dep_graph;
	/*
	 * Array of pointers to struct bt_dependencies, for garbage
	 * collection. We're not using a linked list here because each
	 * struct bt_dependencies can belong to more than one
	 * babeltrace_iter.
	 */
	GPtrArray *dep_gc;
};

struct bt_dependencies {
	GArray *deps;			/* Array of GQuarks */
	int refcount;			/* free when decremented to 0 */
};

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
 * babeltrace_iter_add_callback: Add a callback to iterator.
 */
int babeltrace_iter_add_callback(struct babeltrace_iter *iter,
		bt_event_name event, void *private_data, int flags,
		enum bt_cb_ret (*callback)(struct bt_ctf_data *ctf_data,
					   void *private_data),
		struct bt_dependencies *depends,
		struct bt_dependencies *weak_depends,
		struct bt_dependencies *provides)
{
	int i, stream_id;
	gpointer *event_id_ptr;
	unsigned long event_id;
	struct trace_collection *tc = iter->tc;

	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		tin = container_of(td_read, struct ctf_trace, parent);

		for (stream_id = 0; stream_id < tin->streams->len; stream_id++) {
			struct ctf_stream_class *stream;
			struct bt_stream_callbacks *bt_stream_cb = NULL;
			struct bt_callback_chain *bt_chain = NULL;
			struct bt_callback new_callback;

			stream = g_ptr_array_index(tin->streams, stream_id);

			/* find or create the bt_stream_callbacks for this stream */
			if (iter->callbacks->len >= stream_id) {
				bt_stream_cb = &g_array_index(iter->callbacks,
						struct bt_stream_callbacks, stream->stream_id);
			} else {
				g_array_set_size(iter->callbacks, stream->stream_id);
			}
			if (!bt_stream_cb || !bt_stream_cb->per_id_callbacks) {
				struct bt_stream_callbacks new_stream_cb;
				new_stream_cb.per_id_callbacks = g_array_new(1, 1,
						sizeof(struct bt_callback_chain));
				g_array_insert_val(iter->callbacks, stream->stream_id, new_stream_cb);
				bt_stream_cb = &g_array_index(iter->callbacks,
						struct bt_stream_callbacks, stream->stream_id);
			}

			if (event) {
				/* find the event id */
				event_id_ptr = g_hash_table_lookup(stream->event_quark_to_id,
						(gconstpointer) (unsigned long) event);
				/* event not found in this stream class */
				if (!event_id_ptr) {
					printf("event not found\n");
					continue;
				}
				event_id = (uint64_t)(unsigned long)*event_id_ptr;

				/* find or create the bt_callback_chain for this event */
				if (bt_stream_cb->per_id_callbacks->len >= event_id) {
					bt_chain = &g_array_index(bt_stream_cb->per_id_callbacks,
							struct bt_callback_chain, event_id);
				} else {
					g_array_set_size(bt_stream_cb->per_id_callbacks, event_id);
				}
				if (!bt_chain || !bt_chain->callback) {
					struct bt_callback_chain new_chain;
					new_chain.callback = g_array_new(1, 1, sizeof(struct bt_callback));
					g_array_insert_val(bt_stream_cb->per_id_callbacks, event_id,
							new_chain);
					bt_chain = &g_array_index(bt_stream_cb->per_id_callbacks,
							struct bt_callback_chain, event_id);
				}
			} else {
				/* callback for all events */
				if (!iter->main_callbacks.callback) {
					iter->main_callbacks.callback = g_array_new(1, 1,
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

static int stream_read_event(struct ctf_file_stream *sin)
{
	int ret;

	ret = sin->pos.parent.event_cb(&sin->pos.parent, &sin->parent);
	if (ret == EOF)
		return EOF;
	else if (ret) {
		fprintf(stdout, "[error] Reading event failed.\n");
		return ret;
	}
	return 0;
}

/*
 * returns true if a < b, false otherwise.
 */
int stream_compare(void *a, void *b)
{
	struct ctf_file_stream *s_a = a, *s_b = b;

	if (s_a->parent.timestamp < s_b->parent.timestamp)
		return 1;
	else
		return 0;
}

/*
 * babeltrace_filestream_seek: seek a filestream to given position.
 *
 * The stream_id parameter is only useful for BT_SEEK_RESTORE.
 */
static int babeltrace_filestream_seek(struct ctf_file_stream *file_stream,
		const struct trace_collection_pos *begin_pos,
		unsigned long stream_id)
{
	int ret = 0;

	switch (begin_pos->type) {
	case BT_SEEK_CUR:
		/*
		 * just insert into the heap we should already know
		 * the timestamps
		 */
		break;
	case BT_SEEK_BEGIN:
		ctf_move_pos_slow(&file_stream->pos, 0, SEEK_SET);
		ret = stream_read_event(file_stream);
		break;
	case BT_SEEK_TIME:
	case BT_SEEK_RESTORE:
	case BT_SEEK_END:
	default:
		assert(0); /* Not yet defined */
	}

	return ret;
}

/*
 * babeltrace_iter_seek: seek iterator to given position.
 */
int babeltrace_iter_seek(struct babeltrace_iter *iter,
		const struct trace_collection_pos *begin_pos)
{
    int i, stream_id;
	int ret = 0;
	struct trace_collection *tc = iter->tc;

	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		tin = container_of(td_read, struct ctf_trace, parent);

		/* Populate heap with each stream */
		for (stream_id = 0; stream_id < tin->streams->len;
				stream_id++) {
			struct ctf_stream_class *stream;
			int filenr;

			stream = g_ptr_array_index(tin->streams, stream_id);
			for (filenr = 0; filenr < stream->streams->len;
					filenr++) {
				struct ctf_file_stream *file_stream;

				file_stream = g_ptr_array_index(stream->streams,
						filenr);
				ret = babeltrace_filestream_seek(file_stream, begin_pos,
						stream_id);
				if (ret < 0)
					goto end;
			}
		}
	}
end:
	return ret;
}

struct babeltrace_iter *babeltrace_iter_create(struct trace_collection *tc,
		struct trace_collection_pos *begin_pos,
		struct trace_collection_pos *end_pos)
{
	int i, stream_id;
	int ret = 0;
	struct babeltrace_iter *iter;

	iter = malloc(sizeof(struct babeltrace_iter));
	if (!iter)
		goto error_malloc;
	iter->stream_heap = g_new(struct ptr_heap, 1);
	iter->tc = tc;
	iter->end_pos = end_pos;
	iter->callbacks = g_array_new(0, 1, sizeof(struct bt_stream_callbacks));
	iter->recalculate_dep_graph = 0;
	iter->main_callbacks.callback = NULL;
	iter->dep_gc = g_ptr_array_new();

	ret = heap_init(iter->stream_heap, 0, stream_compare);
	if (ret < 0)
		goto error_heap_init;

	for (i = 0; i < tc->array->len; i++) {
		struct ctf_trace *tin;
		struct trace_descriptor *td_read;

		td_read = g_ptr_array_index(tc->array, i);
		tin = container_of(td_read, struct ctf_trace, parent);

		/* Populate heap with each stream */
		for (stream_id = 0; stream_id < tin->streams->len;
				stream_id++) {
			struct ctf_stream_class *stream;
			int filenr;

			stream = g_ptr_array_index(tin->streams, stream_id);
			if (!stream)
				continue;
			for (filenr = 0; filenr < stream->streams->len;
					filenr++) {
				struct ctf_file_stream *file_stream;

				file_stream = g_ptr_array_index(stream->streams,
						filenr);

				if (begin_pos) {
				    ret = babeltrace_filestream_seek(file_stream, begin_pos,
							stream_id);
				    if (ret == EOF) {
					   ret = 0;
					   continue;
				    } else if (ret) {
					   goto error;
				    }
				}
				/* Add to heap */
				ret = heap_insert(iter->stream_heap, file_stream);
				if (ret)
					goto error;
			}
		}
	}

	return iter;

error:
	heap_free(iter->stream_heap);
error_heap_init:
	g_free(iter->stream_heap);
	free(iter);
error_malloc:
	return NULL;
}

void babeltrace_iter_destroy(struct babeltrace_iter *iter)
{
	struct bt_stream_callbacks *bt_stream_cb;
	struct bt_callback_chain *bt_chain;
	int i, j;

	heap_free(iter->stream_heap);
	g_free(iter->stream_heap);

	/* free all events callbacks */
	if (iter->main_callbacks.callback)
		g_array_free(iter->main_callbacks.callback, TRUE);

	/* free per-event callbacks */
	for (i = 0; i < iter->callbacks->len; i++) {
		bt_stream_cb = &g_array_index(iter->callbacks,
				struct bt_stream_callbacks, i);
		if (!bt_stream_cb || !bt_stream_cb->per_id_callbacks)
			continue;
		for (j = 0; j < bt_stream_cb->per_id_callbacks->len; j++) {
			bt_chain = &g_array_index(bt_stream_cb->per_id_callbacks,
					struct bt_callback_chain, j);
			if (bt_chain->callback) {
				g_array_free(bt_chain->callback, TRUE);
			}
		}
		g_array_free(bt_stream_cb->per_id_callbacks, TRUE);
	}

	free(iter);
}

int babeltrace_iter_next(struct babeltrace_iter *iter)
{
	struct ctf_file_stream *file_stream, *removed;
	int ret;

	file_stream = heap_maximum(iter->stream_heap);
	if (!file_stream) {
		/* end of file for all streams */
		ret = 0;
		goto end;
	}

	ret = stream_read_event(file_stream);
	if (ret == EOF) {
		removed = heap_remove(iter->stream_heap);
		assert(removed == file_stream);
		ret = 0;
		goto end;
	} else if (ret) {
		goto end;
	}
	/* Reinsert the file stream into the heap, and rebalance. */
	removed = heap_replace_max(iter->stream_heap, file_stream);
	assert(removed == file_stream);

end:
	return ret;
}

static
struct ctf_stream_event *extract_ctf_stream_event(struct ctf_stream *stream)
{
	struct ctf_stream_class *stream_class = stream->stream_class;
	struct ctf_event *event_class;
	struct ctf_stream_event *event;
	uint64_t id = stream->event_id;

	if (id >= stream_class->events_by_id->len) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return NULL;
	}
	event = g_ptr_array_index(stream->events_by_id, id);
	if (!event) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return NULL;
	}
	event_class = g_ptr_array_index(stream_class->events_by_id, id);
	if (!event_class) {
		fprintf(stdout, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return NULL;
	}

	return event;
}

static
void process_callbacks(struct babeltrace_iter *iter,
		       struct ctf_stream *stream)
{
	struct bt_stream_callbacks *bt_stream_cb;
	struct bt_callback_chain *bt_chain;
	struct bt_callback *cb;
	int i;
	enum bt_cb_ret ret;
	struct bt_ctf_data ctf_data;

	ctf_data.event = extract_ctf_stream_event(stream);

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

	if (stream->event_id > bt_stream_cb->per_id_callbacks->len)
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

int babeltrace_iter_read_event(struct babeltrace_iter *iter,
		struct ctf_stream **stream,
		struct ctf_stream_event **event)
{
	struct ctf_file_stream *file_stream;
	int ret = 0;

	file_stream = heap_maximum(iter->stream_heap);
	if (!file_stream) {
		/* end of file for all streams */
		ret = EOF;
		goto end;
	}
	*stream = &file_stream->parent;
	*event = g_ptr_array_index((*stream)->events_by_id, (*stream)->event_id);

	if ((*stream)->stream_id > iter->callbacks->len)
		goto end;

	process_callbacks(iter, *stream);

end:
	return ret;
}

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_collection *trace_collection_read)
{
	struct babeltrace_iter *iter;
	struct ctf_stream *stream;
	struct ctf_stream_event *event;
	struct ctf_text_stream_pos *sout;
	struct trace_collection_pos begin_pos;
	int ret = 0;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	begin_pos.type = BT_SEEK_BEGIN;
	iter = babeltrace_iter_create(trace_collection_read, &begin_pos, NULL);
	while (babeltrace_iter_read_event(iter, &stream, &event) == 0) {
		ret = sout->parent.event_cb(&sout->parent, stream);
		if (ret) {
			fprintf(stdout, "[error] Writing event failed.\n");
			goto end;
		}
		ret = babeltrace_iter_next(iter);
		if (ret < 0)
			goto end;
	}
end:
	babeltrace_iter_destroy(iter);
	return ret;
}
