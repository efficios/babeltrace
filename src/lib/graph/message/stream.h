#ifndef BABELTRACE_GRAPH_MESSAGE_STREAM_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_STREAM_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace2/graph/message.h>

#include "compat/compiler.h"
#include "lib/trace-ir/stream.h"
#include "lib/trace-ir/clock-snapshot.h"
#include "common/assert.h"

#include "message.h"

struct bt_message_stream {
	struct bt_message parent;
	struct bt_stream *stream;
	struct bt_clock_snapshot *default_cs;
	enum bt_message_stream_clock_snapshot_state default_cs_state;
};

static inline
const char *bt_message_stream_clock_snapshot_state_string(
		enum bt_message_stream_clock_snapshot_state state)
{
	switch (state) {
	case BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN:
		return "KNOWN";
	case BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN:
		return "UNKNOWN";
	default:
		return "(unknown)";
	}
}


#endif /* BABELTRACE_GRAPH_MESSAGE_STREAM_INTERNAL_H */
