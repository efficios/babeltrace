##
## This target generates an include file that contains the git version
## string of the current branch, it must be continuously updated when
## we build in the git repo and shipped in dist tarballs to reflect the
## status of the tree when it was generated. If the tree is clean and
## the current commit is tag a starting with "v", consider this a
## release version and set an empty git version.
##
## Here is what the inline script does:
##
## First, delete any stale "version.i.tmp" file.
##
## If "bootstrap" and ".git" exists in the top source directory and the git
## executable is available, get the current git version string in the form:
##
##  "latest_tag"(-"number_of_commits_on_top")(-g"latest_commit_hash")(-dirty)
##
## And store it in "version.i.tmp", if the current commit is tagged, the tag
## starts with "v" and the tree is clean, consider this a release version and
## overwrite the git version with an empty string in "version.i.tmp".
##
## If we don't have a "version.i.tmp" nor a "version.i", generate an empty
## string as a failover.
##
## If we don't have a "version.i" or we have both files and they are different,
## copy "version.i.tmp" over "version.i". This way the dependent targets are
## only rebuilt when the version string changes.
##

version_verbose = $(version_verbose_@AM_V@)
version_verbose_ = $(version_verbose_@AM_DEFAULT_V@)
version_verbose_0 = @echo "  GEN       " $@;

version.i:
	$(version_verbose)rm -f version.i.tmp; \
	if (test -r "$(top_srcdir)/bootstrap" && test -r "$(top_srcdir)/.git") && \
			test -x "`which git 2>&1;true`"; then \
		GIT_VERSION_STR="`cd "$(top_srcdir)" && git describe --tags --dirty`"; \
		GIT_CURRENT_TAG="`cd "$(top_srcdir)" && git describe --tags --exact-match --match="v[0-9]*" HEAD 2> /dev/null`"; \
		echo "#define GIT_VERSION \"$$GIT_VERSION_STR\"" > version.i.tmp; \
		if ! $(GREP) -- "-dirty" version.i.tmp > /dev/null && \
				test "x$$GIT_CURRENT_TAG" != "x"; then \
			echo "#define GIT_VERSION \"\"" > version.i.tmp; \
		fi; \
	fi; \
	if test ! -f version.i.tmp; then \
		if test ! -f version.i; then \
			echo '#define GIT_VERSION ""' > version.i; \
		fi; \
	elif test ! -f version.i || \
			test x"`cat version.i.tmp`" != x"`cat version.i`"; then \
		mv version.i.tmp version.i; \
	fi; \
	rm -f version.i.tmp; \
	true

##
## version.i is defined as a .PHONY target even if it's a real file,
## we want the target to be re-run on every make.
##
.PHONY: version.i

CLEANFILES = version.i.tmp

##
## Only clean "version.i" on dist-clean, we need to keep it on regular
## clean when it's part of a dist tarball.
##
DISTCLEANFILES = version.i

# Core API
babeltraceincludedir = "$(includedir)/babeltrace"
babeltraceinclude_HEADERS = \
	babeltrace/babeltrace.h \
	babeltrace/logging.h \
	babeltrace/property.h \
	babeltrace/types.h \
	babeltrace/util.h \
	babeltrace/value-const.h \
	babeltrace/value.h \
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
	babeltrace/trace-ir/clock-class-const.h \
	babeltrace/trace-ir/clock-class.h \
	babeltrace/trace-ir/clock-snapshot-const.h \
	babeltrace/trace-ir/event-class-const.h \
	babeltrace/trace-ir/event-class.h \
	babeltrace/trace-ir/event-const.h \
	babeltrace/trace-ir/event.h \
	babeltrace/trace-ir/field-class-const.h \
	babeltrace/trace-ir/field-class.h \
	babeltrace/trace-ir/field-path-const.h \
	babeltrace/trace-ir/field-const.h \
	babeltrace/trace-ir/field.h \
	babeltrace/trace-ir/packet-const.h \
	babeltrace/trace-ir/packet-context-field.h \
	babeltrace/trace-ir/packet.h \
	babeltrace/trace-ir/stream-class-const.h \
	babeltrace/trace-ir/stream-class.h \
	babeltrace/trace-ir/stream-const.h \
	babeltrace/trace-ir/stream.h \
	babeltrace/trace-ir/trace-class-const.h \
	babeltrace/trace-ir/trace-class.h \
	babeltrace/trace-ir/trace-const.h \
	babeltrace/trace-ir/trace.h

# Plugin and plugin development API
babeltracepluginincludedir = "$(includedir)/babeltrace/plugin"
babeltraceplugininclude_HEADERS = \
	babeltrace/plugin/plugin-dev.h \
	babeltrace/plugin/plugin-const.h \
	babeltrace/plugin/plugin-set-const.h

# Graph, component, and message API
babeltracegraphincludedir = "$(includedir)/babeltrace/graph"
babeltracegraphinclude_HEADERS = \
	babeltrace/graph/component-class-const.h \
	babeltrace/graph/component-class-filter-const.h \
	babeltrace/graph/component-class-filter.h \
	babeltrace/graph/component-class-sink-const.h \
	babeltrace/graph/component-class-sink.h \
	babeltrace/graph/component-class-source-const.h \
	babeltrace/graph/component-class-source.h \
	babeltrace/graph/component-class.h \
	babeltrace/graph/component-const.h \
	babeltrace/graph/component-filter-const.h \
	babeltrace/graph/component-sink-const.h \
	babeltrace/graph/component-source-const.h \
	babeltrace/graph/connection-const.h \
	babeltrace/graph/graph-const.h \
	babeltrace/graph/graph.h \
	babeltrace/graph/message-const.h \
	babeltrace/graph/message-discarded-events-const.h \
	babeltrace/graph/message-discarded-events.h \
	babeltrace/graph/message-event-const.h \
	babeltrace/graph/message-event.h \
	babeltrace/graph/message-inactivity-const.h \
	babeltrace/graph/message-inactivity.h \
	babeltrace/graph/message-iterator-const.h \
	babeltrace/graph/message-packet-beginning-const.h \
	babeltrace/graph/message-packet-beginning.h \
	babeltrace/graph/message-packet-end-const.h \
	babeltrace/graph/message-packet-end.h \
	babeltrace/graph/message-stream-activity-beginning.h \
	babeltrace/graph/message-stream-activity-const.h \
	babeltrace/graph/message-stream-activity-end.h \
	babeltrace/graph/message-stream-beginning-const.h \
	babeltrace/graph/message-stream-beginning.h \
	babeltrace/graph/message-stream-end-const.h \
	babeltrace/graph/message-stream-end.h \
	babeltrace/graph/port-const.h \
	babeltrace/graph/port-input-const.h \
	babeltrace/graph/port-output-const.h \
	babeltrace/graph/port-output-message-iterator.h \
	babeltrace/graph/query-executor-const.h \
	babeltrace/graph/query-executor.h \
	babeltrace/graph/self-component-class-filter.h \
	babeltrace/graph/self-component-class-sink.h \
	babeltrace/graph/self-component-class-source.h \
	babeltrace/graph/self-component-filter.h \
	babeltrace/graph/self-component-port-input-message-iterator.h \
	babeltrace/graph/self-component-port-input.h \
	babeltrace/graph/self-component-port-output.h \
	babeltrace/graph/self-component-port.h \
	babeltrace/graph/self-component-sink.h \
	babeltrace/graph/self-component-source.h \
	babeltrace/graph/self-component.h \
	babeltrace/graph/self-message-iterator.h

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
	babeltrace/value-internal.h \
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
	babeltrace/trace-ir/attributes-internal.h \
	babeltrace/trace-ir/clock-class-internal.h \
	babeltrace/trace-ir/clock-snapshot-internal.h \
	babeltrace/trace-ir/clock-snapshot-set-internal.h \
	babeltrace/trace-ir/event-class-internal.h \
	babeltrace/trace-ir/event-internal.h \
	babeltrace/trace-ir/field-class-internal.h \
	babeltrace/trace-ir/field-path-internal.h \
	babeltrace/trace-ir/field-internal.h \
	babeltrace/trace-ir/field-wrapper-internal.h \
	babeltrace/trace-ir/packet-internal.h \
	babeltrace/trace-ir/resolve-field-path-internal.h \
	babeltrace/trace-ir/stream-class-internal.h \
	babeltrace/trace-ir/stream-internal.h \
	babeltrace/trace-ir/trace-internal.h \
	babeltrace/trace-ir/utils-internal.h \
	babeltrace/prio-heap-internal.h \
	babeltrace/lib-logging-internal.h \
	babeltrace/compiler-internal.h \
	babeltrace/babeltrace-internal.h \
	babeltrace/assert-pre-internal.h \
	babeltrace/graph/component-class-internal.h \
	babeltrace/graph/component-class-sink-colander-internal.h \
	babeltrace/graph/component-filter-internal.h \
	babeltrace/graph/component-internal.h \
	babeltrace/graph/component-sink-internal.h \
	babeltrace/graph/component-source-internal.h \
	babeltrace/graph/connection-internal.h \
	babeltrace/graph/graph-internal.h \
	babeltrace/graph/message-discarded-items-internal.h \
	babeltrace/graph/message-event-internal.h \
	babeltrace/graph/message-inactivity-internal.h \
	babeltrace/graph/message-internal.h \
	babeltrace/graph/message-iterator-internal.h \
	babeltrace/graph/message-packet-internal.h \
	babeltrace/graph/message-stream-internal.h \
	babeltrace/graph/port-internal.h \
	babeltrace/graph/query-executor-internal.h \
	babeltrace/list-internal.h \
	version.h \
	version.i
