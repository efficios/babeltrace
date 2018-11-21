#ifndef BABELTRACE_BABELTRACE_H
#define BABELTRACE_BABELTRACE_H

/*
 * Babeltrace API
 *
 * Copyright 2010-2017 EfficiOS Inc. <http://www.efficios.com/>
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

/* Core API */
#include <babeltrace/logging.h>
#include <babeltrace/object.h>
#include <babeltrace/private-values.h>
#include <babeltrace/types.h>
#include <babeltrace/values.h>
#include <babeltrace/version.h>

/* Legacy API (for CTF writer) */
#include <babeltrace/ctf/events.h>

/* CTF writer API */
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/ctf-writer/utils.h>
#include <babeltrace/ctf-writer/visitor.h>
#include <babeltrace/ctf-writer/writer.h>

/* Legacy API (for CTF writer) */
#include <babeltrace/ctf-ir/clock.h>
#include <babeltrace/ctf-ir/event-fields.h>
#include <babeltrace/ctf-ir/event-types.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/utils.h>

/* Trace IR API */
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-value.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/event.h>
#include <babeltrace/trace-ir/field-classes.h>
#include <babeltrace/trace-ir/field-path.h>
#include <babeltrace/trace-ir/fields.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/private-clock-class.h>
#include <babeltrace/trace-ir/private-event-class.h>
#include <babeltrace/trace-ir/private-event-header-field.h>
#include <babeltrace/trace-ir/private-event.h>
#include <babeltrace/trace-ir/private-field-classes.h>
#include <babeltrace/trace-ir/private-fields.h>
#include <babeltrace/trace-ir/private-packet-context-field.h>
#include <babeltrace/trace-ir/private-packet-header-field.h>
#include <babeltrace/trace-ir/private-packet.h>
#include <babeltrace/trace-ir/private-stream-class.h>
#include <babeltrace/trace-ir/private-stream.h>
#include <babeltrace/trace-ir/private-trace.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/trace.h>

/* Plugin and plugin development API */
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/plugin/plugin.h>

/* Graph, component, and notification API */
#include <babeltrace/graph/component-class-filter.h>
#include <babeltrace/graph/component-class-sink.h>
#include <babeltrace/graph/component-class-source.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/component-source.h>
#include <babeltrace/graph/component-status.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-inactivity.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/output-port-notification-iterator.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/private-component-filter.h>
#include <babeltrace/graph/private-component-sink.h>
#include <babeltrace/graph/private-component-source.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-connection-notification-iterator.h>
#include <babeltrace/graph/private-connection-private-notification-iterator.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/private-notification-event.h>
#include <babeltrace/graph/private-notification-inactivity.h>
#include <babeltrace/graph/private-notification-packet.h>
#include <babeltrace/graph/private-notification-stream.h>
#include <babeltrace/graph/private-notification.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/query-executor.h>

#endif /* BABELTRACE_BABELTRACE_H */
