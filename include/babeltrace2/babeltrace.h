#ifndef BABELTRACE_BABELTRACE_H
#define BABELTRACE_BABELTRACE_H

/*
 * Babeltrace API
 *
 * Copyright 2010-2018 EfficiOS Inc. <http://www.efficios.com/>
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
#include <babeltrace2/logging.h>
#include <babeltrace2/property.h>
#include <babeltrace2/types.h>
#include <babeltrace2/util.h>
#include <babeltrace2/value-const.h>
#include <babeltrace2/value.h>
#include <babeltrace2/version.h>

/* Legacy API (for CTF writer) */
#include <babeltrace2/ctf/events.h>

/* CTF writer API */
#include <babeltrace2/ctf-writer/clock-class.h>
#include <babeltrace2/ctf-writer/clock.h>
#include <babeltrace2/ctf-writer/event-fields.h>
#include <babeltrace2/ctf-writer/event-types.h>
#include <babeltrace2/ctf-writer/event.h>
#include <babeltrace2/ctf-writer/field-types.h>
#include <babeltrace2/ctf-writer/fields.h>
#include <babeltrace2/ctf-writer/object.h>
#include <babeltrace2/ctf-writer/stream-class.h>
#include <babeltrace2/ctf-writer/stream.h>
#include <babeltrace2/ctf-writer/trace.h>
#include <babeltrace2/ctf-writer/utils.h>
#include <babeltrace2/ctf-writer/visitor.h>
#include <babeltrace2/ctf-writer/writer.h>

/* Legacy API (for CTF writer) */
#include <babeltrace2/ctf-ir/clock.h>
#include <babeltrace2/ctf-ir/event-fields.h>
#include <babeltrace2/ctf-ir/event-types.h>
#include <babeltrace2/ctf-ir/event.h>
#include <babeltrace2/ctf-ir/field-types.h>
#include <babeltrace2/ctf-ir/fields.h>
#include <babeltrace2/ctf-ir/stream-class.h>
#include <babeltrace2/ctf-ir/stream.h>
#include <babeltrace2/ctf-ir/trace.h>
#include <babeltrace2/ctf-ir/utils.h>

/* Trace IR API */
#include <babeltrace2/trace-ir/clock-class-const.h>
#include <babeltrace2/trace-ir/clock-class.h>
#include <babeltrace2/trace-ir/clock-snapshot-const.h>
#include <babeltrace2/trace-ir/event-class-const.h>
#include <babeltrace2/trace-ir/event-class.h>
#include <babeltrace2/trace-ir/event-const.h>
#include <babeltrace2/trace-ir/event.h>
#include <babeltrace2/trace-ir/field-class-const.h>
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field-const.h>
#include <babeltrace2/trace-ir/field-path-const.h>
#include <babeltrace2/trace-ir/field.h>
#include <babeltrace2/trace-ir/packet-const.h>
#include <babeltrace2/trace-ir/packet-context-field.h>
#include <babeltrace2/trace-ir/packet.h>
#include <babeltrace2/trace-ir/stream-class-const.h>
#include <babeltrace2/trace-ir/stream-class.h>
#include <babeltrace2/trace-ir/stream-const.h>
#include <babeltrace2/trace-ir/stream.h>
#include <babeltrace2/trace-ir/trace-class-const.h>
#include <babeltrace2/trace-ir/trace-class.h>
#include <babeltrace2/trace-ir/trace-const.h>
#include <babeltrace2/trace-ir/trace.h>

/* Plugin and plugin development API */
#include <babeltrace2/plugin/plugin-const.h>
#include <babeltrace2/plugin/plugin-dev.h>
#include <babeltrace2/plugin/plugin-set-const.h>

/* Graph, component, and message API */
#include <babeltrace2/graph/component-class-const.h>
#include <babeltrace2/graph/component-class-filter-const.h>
#include <babeltrace2/graph/component-class-filter.h>
#include <babeltrace2/graph/component-class-sink-const.h>
#include <babeltrace2/graph/component-class-sink.h>
#include <babeltrace2/graph/component-class-source-const.h>
#include <babeltrace2/graph/component-class-source.h>
#include <babeltrace2/graph/component-class.h>
#include <babeltrace2/graph/component-const.h>
#include <babeltrace2/graph/component-filter-const.h>
#include <babeltrace2/graph/component-sink-const.h>
#include <babeltrace2/graph/component-source-const.h>
#include <babeltrace2/graph/connection-const.h>
#include <babeltrace2/graph/graph-const.h>
#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/message-const.h>
#include <babeltrace2/graph/message-discarded-events-const.h>
#include <babeltrace2/graph/message-discarded-events.h>
#include <babeltrace2/graph/message-discarded-packets-const.h>
#include <babeltrace2/graph/message-discarded-packets.h>
#include <babeltrace2/graph/message-event-const.h>
#include <babeltrace2/graph/message-event.h>
#include <babeltrace2/graph/message-iterator-const.h>
#include <babeltrace2/graph/message-message-iterator-inactivity-const.h>
#include <babeltrace2/graph/message-message-iterator-inactivity.h>
#include <babeltrace2/graph/message-packet-beginning-const.h>
#include <babeltrace2/graph/message-packet-beginning.h>
#include <babeltrace2/graph/message-packet-end-const.h>
#include <babeltrace2/graph/message-packet-end.h>
#include <babeltrace2/graph/message-stream-activity-beginning-const.h>
#include <babeltrace2/graph/message-stream-activity-beginning.h>
#include <babeltrace2/graph/message-stream-activity-const.h>
#include <babeltrace2/graph/message-stream-activity-end-const.h>
#include <babeltrace2/graph/message-stream-activity-end.h>
#include <babeltrace2/graph/message-stream-beginning-const.h>
#include <babeltrace2/graph/message-stream-beginning.h>
#include <babeltrace2/graph/message-stream-end-const.h>
#include <babeltrace2/graph/message-stream-end.h>
#include <babeltrace2/graph/port-const.h>
#include <babeltrace2/graph/port-input-const.h>
#include <babeltrace2/graph/port-output-const.h>
#include <babeltrace2/graph/port-output-message-iterator.h>
#include <babeltrace2/graph/query-executor-const.h>
#include <babeltrace2/graph/query-executor.h>
#include <babeltrace2/graph/self-component-class-filter.h>
#include <babeltrace2/graph/self-component-class-sink.h>
#include <babeltrace2/graph/self-component-class-source.h>
#include <babeltrace2/graph/self-component-filter.h>
#include <babeltrace2/graph/self-component-port-input-message-iterator.h>
#include <babeltrace2/graph/self-component-port-input.h>
#include <babeltrace2/graph/self-component-port-output.h>
#include <babeltrace2/graph/self-component-port.h>
#include <babeltrace2/graph/self-component-sink.h>
#include <babeltrace2/graph/self-component-source.h>
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/self-message-iterator.h>

#endif /* BABELTRACE_BABELTRACE_H */
