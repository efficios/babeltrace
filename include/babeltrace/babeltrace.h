#ifndef _BABELTRACE_H
#define _BABELTRACE_H

/*
 * BabelTrace API
 *
 * Copyright 2010-2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <glib.h>
#include <stdint.h>

typedef GQuark bt_event_name;

/* Forward declarations */
struct babeltrace_iter;
struct trace_collection;
struct ctf_stream_event;
struct ctf_stream;
struct babeltrace_saved_pos;
struct bt_dependencies;

enum bt_cb_ret {
	BT_CB_OK		= 0,
	BT_CB_OK_STOP		= 1,
	BT_CB_ERROR_STOP	= 2,
	BT_CB_ERROR_CONTINUE	= 3,
};

struct trace_collection_pos {
	enum {
		BT_SEEK_TIME,		/* uses u.seek_time */
		BT_SEEK_RESTORE,	/* uses u.restore */
		BT_SEEK_CUR,
		BT_SEEK_BEGIN,
		BT_SEEK_END,
	} type;
	union {
		uint64_t seek_time;
		struct babeltrace_saved_pos *restore;
	} u;
};

struct bt_ctf_data {
	struct ctf_stream_event *event;
};

/*
 * babeltrace_iter_create - Allocate a trace collection iterator.
 *
 * begin_pos and end_pos are optional parameters to specify the position
 * at which the trace collection should be seeked upon iterator
 * creation, and the position at which iteration will start returning
 * "EOF".
 *
 * By default, if begin_pos is NULL, a BT_SEEK_CUR is performed at
 * creation. By default, if end_pos is NULL, a BT_SEEK_END (end of
 * trace) is the EOF criterion.
 */
struct babeltrace_iter *babeltrace_iter_create(struct trace_collection *tc,
		struct trace_collection_pos *begin_pos,
		struct trace_collection_pos *end_pos);

/*
 * babeltrace_iter_destroy - Free a trace collection iterator.
 */
void babeltrace_iter_destroy(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_next: Move trace collection position to the next event.
 *
 * Returns 0 on success, a negative value on error
 */
int babeltrace_iter_next(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_save_pos - Save the current trace collection position.
 *
 * The position returned by this function needs to be freed by
 * babeltrace_iter_free_pos after use.
 */
struct trace_collection_pos *
	babeltrace_iter_save_pos(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_free_pos - Free the position.
 */
void babeltrace_iter_free_pos(struct trace_collection_pos *pos);

/*
 * babeltrace_iter_seek: seek iterator to given position.
 *
 * Return EOF if position is after the last event of the trace collection.
 * Return other negative value for other errors.
 * Return 0 for success.
 */
int babeltrace_iter_seek(struct babeltrace_iter *iter,
		const struct trace_collection_pos *pos);

/*
 * babeltrace_iter_read_event: Read the iterator's current event data.
 *
 * @iter: trace collection iterator (input)
 * @stream: stream containing event at current position (output)
 * @event: current event (output)
 * Return 0 on success, negative error value on error.
 */
int babeltrace_iter_read_event(struct babeltrace_iter *iter,
		struct ctf_stream **stream,
		struct ctf_stream_event **event);

/*
 * Receives a variable number of strings as parameter, ended with NULL.
 */
struct bt_dependencies *babeltrace_dependencies_create(const char *first, ...);

/*
 * struct bt_dependencies must be destroyed explicitly if not passed as
 * parameter to a babeltrace_iter_add_callback().
 */
void babeltrace_dependencies_destroy(struct bt_dependencies *dep);

/*
 * babeltrace_iter_add_callback: Add a callback to iterator.
 *
 * @iter: trace collection iterator (input)
 * @event: event to target. 0 for all events.
 * @private_data: private data pointer to pass to the callback
 * @flags: specific flags controlling the behavior of this callback
 *         (or'd).
 *            
 * @callback: function pointer to call
 * @depends: struct bt_dependency detailing the required computation results.
 *           Ends with 0.
 * @weak_depends: struct bt_dependency detailing the optional computation
 *                results that can be optionally consumed by this
 *                callback.
 * @provides: struct bt_dependency detailing the computation results
 *            provided by this callback.
 *            Ends with 0.
 *
 * "depends", "weak_depends" and "provides" memory is handled by the
 * babeltrace library after this call succeeds or fails. These objects
 * can still be used by the caller until the babeltrace iterator is
 * destroyed, but they belong to the babeltrace library.
 *
 * (note to implementor: we need to keep a gptrarray of struct
 * bt_dependencies to "garbage collect" in struct babeltrace_iter, and
 * dependencies need to have a refcount to handle the case where they
 * would be passed to more than one iterator. Upon iterator detroy, we
 * iterate on all the gc ptrarray and decrement the refcounts, freeing
 * if we reach 0.)
 * (note to implementor: we calculate the dependency graph when
 * babeltrace_iter_read_event() is executed after a
 * babeltrace_iter_add_callback(). Beware that it is valid to create/add
 * callbacks/read/add more callbacks/read some more.)
 */
int babeltrace_iter_add_callback(struct babeltrace_iter *iter,
		bt_event_name event, void *private_data, int flags,
		enum bt_cb_ret (*callback)(struct bt_ctf_data *ctf_data,
					   void *caller_data),
		struct bt_dependencies *depends,
		struct bt_dependencies *weak_depends,
		struct bt_dependencies *provides);

/*
 * For flags parameter above.
 */
enum {
	BT_FLAGS_FREE_PRIVATE_DATA	= (1 << 0),
};

#endif /* _BABELTRACE_H */
