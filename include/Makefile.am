# Core API
babeltraceincludedir = "$(includedir)/babeltrace"
babeltraceinclude_HEADERS = \
	babeltrace/babeltrace.h \
	babeltrace/logging.h \
	babeltrace/ref.h \
	babeltrace/types.h \
	babeltrace/values.h \
	babeltrace/version.h

# Legacy API (for CTF writer)
babeltracectfincludedir = "$(includedir)/babeltrace/ctf"
babeltracectfinclude_HEADERS = \
	babeltrace/ctf/events.h

# CTF writer API
babeltracectfwriterincludedir = "$(includedir)/babeltrace/ctf-writer"
babeltracectfwriterinclude_HEADERS = \
	babeltrace/ctf-writer/clock.h \
	babeltrace/ctf-writer/event-fields.h \
	babeltrace/ctf-writer/event.h \
	babeltrace/ctf-writer/event-types.h \
	babeltrace/ctf-writer/fields.h \
	babeltrace/ctf-writer/field-types.h \
	babeltrace/ctf-writer/stream-class.h \
	babeltrace/ctf-writer/stream.h \
	babeltrace/ctf-writer/trace.h \
	babeltrace/ctf-writer/utils.h \
	babeltrace/ctf-writer/visitor.h \
	babeltrace/ctf-writer/writer.h

# Legacy API (for CTF writer)
babeltracectfirincludedir = "$(includedir)/babeltrace/ctf-ir"
babeltracectfirinclude_HEADERS = \
	babeltrace/ctf-ir/clock.h \
	babeltrace/ctf-ir/event-class.h \
	babeltrace/ctf-ir/event-fields.h \
	babeltrace/ctf-ir/event-types.h \
	babeltrace/ctf-ir/event.h \
	babeltrace/ctf-ir/field-types.h \
	babeltrace/ctf-ir/fields.h \
	babeltrace/ctf-ir/stream-class.h \
	babeltrace/ctf-ir/stream.h \
	babeltrace/ctf-ir/trace.h \
	babeltrace/ctf-ir/utils.h


# Trace IR API
babeltracetraceirincludedir = "$(includedir)/babeltrace/trace-ir"
babeltracetraceirinclude_HEADERS = \
	babeltrace/trace-ir/clock-class.h \
	babeltrace/trace-ir/clock-value.h \
	babeltrace/trace-ir/event-class.h \
	babeltrace/trace-ir/event.h \
	babeltrace/trace-ir/event-header-field.h \
	babeltrace/trace-ir/field-path.h \
	babeltrace/trace-ir/field-types.h \
	babeltrace/trace-ir/fields.h \
	babeltrace/trace-ir/packet.h \
	babeltrace/trace-ir/packet-context-field.h \
	babeltrace/trace-ir/packet-header-field.h \
	babeltrace/trace-ir/stream-class.h \
	babeltrace/trace-ir/stream.h \
	babeltrace/trace-ir/trace.h

# Plugin and plugin development API
babeltracepluginincludedir = "$(includedir)/babeltrace/plugin"
babeltraceplugininclude_HEADERS = \
	babeltrace/plugin/plugin-dev.h \
	babeltrace/plugin/plugin.h

# Graph, component, and notification API
babeltracegraphincludedir = "$(includedir)/babeltrace/graph"
babeltracegraphinclude_HEADERS = \
	babeltrace/graph/component-class-filter.h \
	babeltrace/graph/component-class-sink.h \
	babeltrace/graph/component-class-source.h \
	babeltrace/graph/component-class.h \
	babeltrace/graph/component-filter.h \
	babeltrace/graph/component-sink.h \
	babeltrace/graph/component-source.h \
	babeltrace/graph/component-status.h \
	babeltrace/graph/component.h \
	babeltrace/graph/connection.h \
	babeltrace/graph/graph.h \
	babeltrace/graph/notification-event.h \
	babeltrace/graph/notification-inactivity.h \
	babeltrace/graph/notification-iterator.h \
	babeltrace/graph/notification-packet.h \
	babeltrace/graph/notification-stream.h \
	babeltrace/graph/notification.h \
	babeltrace/graph/output-port-notification-iterator.h \
	babeltrace/graph/port.h \
	babeltrace/graph/private-component-filter.h \
	babeltrace/graph/private-component-sink.h \
	babeltrace/graph/private-component-source.h \
	babeltrace/graph/private-component.h \
	babeltrace/graph/private-connection-notification-iterator.h \
	babeltrace/graph/private-connection-private-notification-iterator.h \
	babeltrace/graph/private-connection.h \
	babeltrace/graph/private-port.h \
	babeltrace/graph/query-executor.h

noinst_HEADERS = \
	babeltrace/compat/stdlib-internal.h \
	babeltrace/compat/fcntl-internal.h \
	babeltrace/compat/glib-internal.h \
	babeltrace/compat/uuid-internal.h \
	babeltrace/compat/unistd-internal.h \
	babeltrace/compat/stdio-internal.h \
	babeltrace/compat/time-internal.h \
	babeltrace/compat/utc-internal.h \
	babeltrace/compat/memstream-internal.h \
	babeltrace/compat/string-internal.h \
	babeltrace/compat/limits-internal.h \
	babeltrace/compat/mman-internal.h \
	babeltrace/compat/socket-internal.h \
	babeltrace/common-internal.h \
	babeltrace/bitfield-internal.h \
	babeltrace/object-internal.h \
	babeltrace/object-pool-internal.h \
	babeltrace/plugin/plugin-internal.h \
	babeltrace/plugin/plugin-so-internal.h \
	babeltrace/plugin/python-plugin-provider-internal.h \
	babeltrace/assert-internal.h \
	babeltrace/values-internal.h \
	babeltrace/ctf-writer/attributes-internal.h \
	babeltrace/ctf-writer/clock-class-internal.h \
	babeltrace/ctf-writer/clock-internal.h \
	babeltrace/ctf-writer/event-class-internal.h \
	babeltrace/ctf-writer/event-internal.h \
	babeltrace/ctf-writer/field-path-internal.h \
	babeltrace/ctf-writer/fields-internal.h \
	babeltrace/ctf-writer/field-types-internal.h \
	babeltrace/ctf-writer/field-wrapper-internal.h \
	babeltrace/ctf-writer/functor-internal.h \
	babeltrace/ctf-writer/resolve-internal.h \
	babeltrace/ctf-writer/serialize-internal.h \
	babeltrace/ctf-writer/stream-class-internal.h \
	babeltrace/ctf-writer/stream-internal.h \
	babeltrace/ctf-writer/trace-internal.h \
	babeltrace/ctf-writer/utils-internal.h \
	babeltrace/ctf-writer/validation-internal.h \
	babeltrace/ctf-writer/visitor-internal.h \
	babeltrace/ctf-writer/writer-internal.h \
	babeltrace/mmap-align-internal.h \
	babeltrace/align-internal.h \
	babeltrace/logging-internal.h \
	babeltrace/endian-internal.h \
	babeltrace/trace-ir/packet-internal.h \
	babeltrace/trace-ir/event-class-internal.h \
	babeltrace/trace-ir/utils-internal.h \
	babeltrace/trace-ir/fields-internal.h \
	babeltrace/trace-ir/stream-class-internal.h \
	babeltrace/trace-ir/event-internal.h \
	babeltrace/trace-ir/field-path-internal.h \
	babeltrace/trace-ir/field-wrapper-internal.h \
	babeltrace/trace-ir/trace-internal.h \
	babeltrace/trace-ir/clock-class-internal.h \
	babeltrace/trace-ir/field-types-internal.h \
	babeltrace/trace-ir/clock-value-internal.h \
	babeltrace/trace-ir/attributes-internal.h \
	babeltrace/trace-ir/stream-internal.h \
	babeltrace/trace-ir/resolve-field-path-internal.h \
	babeltrace/prio-heap-internal.h \
	babeltrace/lib-logging-internal.h \
	babeltrace/compiler-internal.h \
	babeltrace/babeltrace-internal.h \
	babeltrace/assert-pre-internal.h \
	babeltrace/graph/component-internal.h \
	babeltrace/graph/notification-stream-internal.h \
	babeltrace/graph/connection-internal.h \
	babeltrace/graph/notification-event-internal.h \
	babeltrace/graph/query-executor-internal.h \
	babeltrace/graph/graph-internal.h \
	babeltrace/graph/component-class-sink-colander-internal.h \
	babeltrace/graph/notification-inactivity-internal.h \
	babeltrace/graph/component-source-internal.h \
	babeltrace/graph/notification-packet-internal.h \
	babeltrace/graph/notification-iterator-internal.h \
	babeltrace/graph/notification-internal.h \
	babeltrace/graph/component-filter-internal.h \
	babeltrace/graph/component-class-internal.h \
	babeltrace/graph/component-sink-internal.h \
	babeltrace/graph/port-internal.h \
	babeltrace/list-internal.h
