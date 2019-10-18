#ifndef BABELTRACE_TRACE_IR_EVENT_CLASS_INTERNAL_H
#define BABELTRACE_TRACE_IR_EVENT_CLASS_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include "lib/assert-pre.h"
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field.h>
#include "common/macros.h"
#include <babeltrace2/value.h>
#include <babeltrace2/trace-ir/stream-class.h>
#include <babeltrace2/trace-ir/stream.h>
#include <babeltrace2/trace-ir/event-class.h>
#include "lib/object.h"
#include "common/assert.h"
#include "lib/object-pool.h"
#include "lib/property.h"
#include <glib.h>
#include <stdbool.h>

#include "trace.h"

struct bt_event_class {
	struct bt_object base;
	struct bt_field_class *specific_context_fc;
	struct bt_field_class *payload_fc;

	/* Owned by this */
	struct bt_value *user_attributes;

	struct {
		GString *str;

		/* NULL or `str->str` above */
		const char *value;
	} name;

	uint64_t id;
	struct bt_property_uint log_level;

	struct {
		GString *str;

		/* NULL or `str->str` above */
		const char *value;
	} emf_uri;

	/* Pool of `struct bt_event *` */
	struct bt_object_pool event_pool;

	bool frozen;
};

BT_HIDDEN
void _bt_event_class_freeze(const struct bt_event_class *event_class);

#ifdef BT_DEV_MODE
# define bt_event_class_freeze		_bt_event_class_freeze
#else
# define bt_event_class_freeze(_ec)
#endif

static inline
struct bt_stream_class *bt_event_class_borrow_stream_class_inline(
		const struct bt_event_class *event_class)
{
	BT_ASSERT_DBG(event_class);
	return (void *) bt_object_borrow_parent(&event_class->base);
}

#endif /* BABELTRACE_TRACE_IR_EVENT_CLASS_INTERNAL_H */
