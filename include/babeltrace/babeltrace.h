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
#include <babeltrace/logging.h>
#include <babeltrace/property.h>
#include <babeltrace/types.h>
#include <babeltrace/util.h>
#include <babeltrace/value-const.h>
#include <babeltrace/value.h>
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
#include <babeltrace/trace-ir/clock-class-const.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-snapshot-const.h>
#include <babeltrace/trace-ir/event-class-const.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/event-const.h>
#include <babeltrace/trace-ir/event-header-field.h>
#include <babeltrace/trace-ir/event.h>
#include <babeltrace/trace-ir/field-class-const.h>
#include <babeltrace/trace-ir/field-class.h>
#include <babeltrace/trace-ir/field-const.h>
#include <babeltrace/trace-ir/field-path-const.h>
#include <babeltrace/trace-ir/field.h>
#include <babeltrace/trace-ir/packet-const.h>
#include <babeltrace/trace-ir/packet-context-field.h>
#include <babeltrace/trace-ir/packet-header-field.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/stream-class-const.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream-const.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/trace-class-const.h>
#include <babeltrace/trace-ir/trace-class.h>
#include <babeltrace/trace-ir/trace-const.h>
#include <babeltrace/trace-ir/trace.h>

/* Plugin and plugin development API */
#include <babeltrace/plugin/plugin-const.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/plugin/plugin-set-const.h>

/* Graph, component, and message API */
#include <babeltrace/graph/component-class-const.h>
#include <babeltrace/graph/component-class-filter-const.h>
#include <babeltrace/graph/component-class-filter.h>
#include <babeltrace/graph/component-class-sink-const.h>
#include <babeltrace/graph/component-class-sink.h>
#include <babeltrace/graph/component-class-source-const.h>
#include <babeltrace/graph/component-class-source.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-const.h>
#include <babeltrace/graph/component-filter-const.h>
#include <babeltrace/graph/component-sink-const.h>
#include <babeltrace/graph/component-source-const.h>
#include <babeltrace/graph/connection-const.h>
#include <babeltrace/graph/graph-const.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/message-const.h>
#include <babeltrace/graph/message-event-const.h>
#include <babeltrace/graph/message-event.h>
#include <babeltrace/graph/message-inactivity-const.h>
#include <babeltrace/graph/message-inactivity.h>
#include <babeltrace/graph/message-packet-const.h>
#include <babeltrace/graph/message-packet.h>
#include <babeltrace/graph/message-stream-const.h>
#include <babeltrace/graph/message-stream.h>
#include <babeltrace/graph/port-const.h>
#include <babeltrace/graph/port-input-const.h>
#include <babeltrace/graph/port-output-const.h>
#include <babeltrace/graph/port-output-message-iterator.h>
#include <babeltrace/graph/query-executor-const.h>
#include <babeltrace/graph/query-executor.h>
#include <babeltrace/graph/self-component-class-filter.h>
#include <babeltrace/graph/self-component-class-sink.h>
#include <babeltrace/graph/self-component-class-source.h>
#include <babeltrace/graph/self-component-filter.h>
#include <babeltrace/graph/self-component-port-input-message-iterator.h>
#include <babeltrace/graph/self-component-port-input.h>
#include <babeltrace/graph/self-component-port-output.h>
#include <babeltrace/graph/self-component-port.h>
#include <babeltrace/graph/self-component-sink.h>
#include <babeltrace/graph/self-component-source.h>
#include <babeltrace/graph/self-component.h>
#include <babeltrace/graph/self-message-iterator.h>

#endif /* BABELTRACE_BABELTRACE_H */
