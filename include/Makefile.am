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
babeltrace2includedir = "$(includedir)/babeltrace2"
babeltrace2include_HEADERS = \
	babeltrace2/babeltrace.h \
	babeltrace2/logging.h \
	babeltrace2/property.h \
	babeltrace2/types.h \
	babeltrace2/util.h \
	babeltrace2/value-const.h \
	babeltrace2/value.h \
	babeltrace2/version.h

# Legacy API (for CTF writer)
babeltrace2ctfincludedir = "$(includedir)/babeltrace2/ctf"
babeltrace2ctfinclude_HEADERS = \
	babeltrace2/ctf/events.h

# CTF writer API
babeltrace2ctfwriterincludedir = "$(includedir)/babeltrace2/ctf-writer"
babeltrace2ctfwriterinclude_HEADERS = \
	babeltrace2/ctf-writer/clock-class.h \
	babeltrace2/ctf-writer/clock.h \
	babeltrace2/ctf-writer/event-fields.h \
	babeltrace2/ctf-writer/event-types.h \
	babeltrace2/ctf-writer/event.h \
	babeltrace2/ctf-writer/field-types.h \
	babeltrace2/ctf-writer/fields.h \
	babeltrace2/ctf-writer/object.h \
	babeltrace2/ctf-writer/stream-class.h \
	babeltrace2/ctf-writer/stream.h \
	babeltrace2/ctf-writer/trace.h \
	babeltrace2/ctf-writer/utils.h \
	babeltrace2/ctf-writer/visitor.h \
	babeltrace2/ctf-writer/writer.h

# Legacy API (for CTF writer)
babeltrace2ctfirincludedir = "$(includedir)/babeltrace2/ctf-ir"
babeltrace2ctfirinclude_HEADERS = \
	babeltrace2/ctf-ir/clock.h \
	babeltrace2/ctf-ir/event-fields.h \
	babeltrace2/ctf-ir/event-types.h \
	babeltrace2/ctf-ir/event.h \
	babeltrace2/ctf-ir/field-types.h \
	babeltrace2/ctf-ir/fields.h \
	babeltrace2/ctf-ir/stream-class.h \
	babeltrace2/ctf-ir/stream.h \
	babeltrace2/ctf-ir/trace.h \
	babeltrace2/ctf-ir/utils.h

# Trace IR API
babeltrace2traceirincludedir = "$(includedir)/babeltrace2/trace-ir"
babeltrace2traceirinclude_HEADERS = \
	babeltrace2/trace-ir/clock-class-const.h \
	babeltrace2/trace-ir/clock-class.h \
	babeltrace2/trace-ir/clock-snapshot-const.h \
	babeltrace2/trace-ir/event-class-const.h \
	babeltrace2/trace-ir/event-class.h \
	babeltrace2/trace-ir/event-const.h \
	babeltrace2/trace-ir/event.h \
	babeltrace2/trace-ir/field-class-const.h \
	babeltrace2/trace-ir/field-class.h \
	babeltrace2/trace-ir/field-path-const.h \
	babeltrace2/trace-ir/field-const.h \
	babeltrace2/trace-ir/field.h \
	babeltrace2/trace-ir/packet-const.h \
	babeltrace2/trace-ir/packet-context-field.h \
	babeltrace2/trace-ir/packet.h \
	babeltrace2/trace-ir/stream-class-const.h \
	babeltrace2/trace-ir/stream-class.h \
	babeltrace2/trace-ir/stream-const.h \
	babeltrace2/trace-ir/stream.h \
	babeltrace2/trace-ir/trace-class-const.h \
	babeltrace2/trace-ir/trace-class.h \
	babeltrace2/trace-ir/trace-const.h \
	babeltrace2/trace-ir/trace.h

# Plugin and plugin development API
babeltrace2pluginincludedir = "$(includedir)/babeltrace2/plugin"
babeltrace2plugininclude_HEADERS = \
	babeltrace2/plugin/plugin-dev.h \
	babeltrace2/plugin/plugin-const.h \
	babeltrace2/plugin/plugin-set-const.h

# Graph, component, and message API
babeltrace2graphincludedir = "$(includedir)/babeltrace2/graph"
babeltrace2graphinclude_HEADERS = \
	babeltrace2/graph/component-class-const.h \
	babeltrace2/graph/component-class-filter-const.h \
	babeltrace2/graph/component-class-filter.h \
	babeltrace2/graph/component-class-sink-const.h \
	babeltrace2/graph/component-class-sink.h \
	babeltrace2/graph/component-class-source-const.h \
	babeltrace2/graph/component-class-source.h \
	babeltrace2/graph/component-class.h \
	babeltrace2/graph/component-const.h \
	babeltrace2/graph/component-filter-const.h \
	babeltrace2/graph/component-sink-const.h \
	babeltrace2/graph/component-source-const.h \
	babeltrace2/graph/connection-const.h \
	babeltrace2/graph/graph-const.h \
	babeltrace2/graph/graph.h \
	babeltrace2/graph/message-const.h \
	babeltrace2/graph/message-discarded-events-const.h \
	babeltrace2/graph/message-discarded-events.h \
	babeltrace2/graph/message-discarded-packets-const.h \
	babeltrace2/graph/message-discarded-packets.h \
	babeltrace2/graph/message-event-const.h \
	babeltrace2/graph/message-event.h \
	babeltrace2/graph/message-message-iterator-inactivity-const.h \
	babeltrace2/graph/message-message-iterator-inactivity.h \
	babeltrace2/graph/message-iterator-const.h \
	babeltrace2/graph/message-packet-beginning-const.h \
	babeltrace2/graph/message-packet-beginning.h \
	babeltrace2/graph/message-packet-end-const.h \
	babeltrace2/graph/message-packet-end.h \
	babeltrace2/graph/message-stream-activity-beginning-const.h \
	babeltrace2/graph/message-stream-activity-beginning.h \
	babeltrace2/graph/message-stream-activity-const.h \
	babeltrace2/graph/message-stream-activity-end-const.h \
	babeltrace2/graph/message-stream-activity-end.h \
	babeltrace2/graph/message-stream-beginning-const.h \
	babeltrace2/graph/message-stream-beginning.h \
	babeltrace2/graph/message-stream-end-const.h \
	babeltrace2/graph/message-stream-end.h \
	babeltrace2/graph/port-const.h \
	babeltrace2/graph/port-input-const.h \
	babeltrace2/graph/port-output-const.h \
	babeltrace2/graph/port-output-message-iterator.h \
	babeltrace2/graph/query-executor-const.h \
	babeltrace2/graph/query-executor.h \
	babeltrace2/graph/self-component-class-filter.h \
	babeltrace2/graph/self-component-class-sink.h \
	babeltrace2/graph/self-component-class-source.h \
	babeltrace2/graph/self-component-filter.h \
	babeltrace2/graph/self-component-port-input-message-iterator.h \
	babeltrace2/graph/self-component-port-input.h \
	babeltrace2/graph/self-component-port-output.h \
	babeltrace2/graph/self-component-port.h \
	babeltrace2/graph/self-component-sink.h \
	babeltrace2/graph/self-component-source.h \
	babeltrace2/graph/self-component.h \
	babeltrace2/graph/self-message-iterator.h

noinst_HEADERS = \
	babeltrace2/compat/stdlib-internal.h \
	babeltrace2/compat/fcntl-internal.h \
	babeltrace2/compat/glib-internal.h \
	babeltrace2/compat/uuid-internal.h \
	babeltrace2/compat/unistd-internal.h \
	babeltrace2/compat/stdio-internal.h \
	babeltrace2/compat/time-internal.h \
	babeltrace2/compat/utc-internal.h \
	babeltrace2/compat/memstream-internal.h \
	babeltrace2/compat/string-internal.h \
	babeltrace2/compat/limits-internal.h \
	babeltrace2/compat/mman-internal.h \
	babeltrace2/compat/socket-internal.h \
	babeltrace2/common-internal.h \
	babeltrace2/ctfser-internal.h \
	babeltrace2/bitfield-internal.h \
	babeltrace2/object-internal.h \
	babeltrace2/object-pool-internal.h \
	babeltrace2/plugin/plugin-internal.h \
	babeltrace2/plugin/plugin-so-internal.h \
	babeltrace2/plugin/python-plugin-provider-internal.h \
	babeltrace2/assert-internal.h \
	babeltrace2/value-internal.h \
	babeltrace2/ctf-writer/attributes-internal.h \
	babeltrace2/ctf-writer/clock-class-internal.h \
	babeltrace2/ctf-writer/clock-internal.h \
	babeltrace2/ctf-writer/event-class-internal.h \
	babeltrace2/ctf-writer/event-internal.h \
	babeltrace2/ctf-writer/field-path-internal.h \
	babeltrace2/ctf-writer/fields-internal.h \
	babeltrace2/ctf-writer/field-types-internal.h \
	babeltrace2/ctf-writer/field-wrapper-internal.h \
	babeltrace2/ctf-writer/functor-internal.h \
	babeltrace2/ctf-writer/object-internal.h \
	babeltrace2/ctf-writer/object-pool-internal.h \
	babeltrace2/ctf-writer/resolve-internal.h \
	babeltrace2/ctf-writer/stream-class-internal.h \
	babeltrace2/ctf-writer/stream-internal.h \
	babeltrace2/ctf-writer/trace-internal.h \
	babeltrace2/ctf-writer/utils-internal.h \
	babeltrace2/ctf-writer/validation-internal.h \
	babeltrace2/ctf-writer/values-internal.h \
	babeltrace2/ctf-writer/visitor-internal.h \
	babeltrace2/ctf-writer/writer-internal.h \
	babeltrace2/mmap-align-internal.h \
	babeltrace2/align-internal.h \
	babeltrace2/logging-internal.h \
	babeltrace2/endian-internal.h \
	babeltrace2/trace-ir/attributes-internal.h \
	babeltrace2/trace-ir/clock-class-internal.h \
	babeltrace2/trace-ir/clock-snapshot-internal.h \
	babeltrace2/trace-ir/clock-snapshot-set-internal.h \
	babeltrace2/trace-ir/event-class-internal.h \
	babeltrace2/trace-ir/event-internal.h \
	babeltrace2/trace-ir/field-class-internal.h \
	babeltrace2/trace-ir/field-path-internal.h \
	babeltrace2/trace-ir/field-internal.h \
	babeltrace2/trace-ir/field-wrapper-internal.h \
	babeltrace2/trace-ir/packet-internal.h \
	babeltrace2/trace-ir/resolve-field-path-internal.h \
	babeltrace2/trace-ir/stream-class-internal.h \
	babeltrace2/trace-ir/stream-internal.h \
	babeltrace2/trace-ir/trace-class-internal.h \
	babeltrace2/trace-ir/trace-internal.h \
	babeltrace2/trace-ir/utils-internal.h \
	babeltrace2/prio-heap-internal.h \
	babeltrace2/lib-logging-internal.h \
	babeltrace2/compiler-internal.h \
	babeltrace2/babeltrace-internal.h \
	babeltrace2/assert-pre-internal.h \
	babeltrace2/graph/component-class-internal.h \
	babeltrace2/graph/component-class-sink-colander-internal.h \
	babeltrace2/graph/component-filter-internal.h \
	babeltrace2/graph/component-internal.h \
	babeltrace2/graph/component-sink-internal.h \
	babeltrace2/graph/component-source-internal.h \
	babeltrace2/graph/connection-internal.h \
	babeltrace2/graph/graph-internal.h \
	babeltrace2/graph/message-discarded-items-internal.h \
	babeltrace2/graph/message-event-internal.h \
	babeltrace2/graph/message-message-iterator-inactivity-internal.h \
	babeltrace2/graph/message-internal.h \
	babeltrace2/graph/message-iterator-internal.h \
	babeltrace2/graph/message-packet-internal.h \
	babeltrace2/graph/message-stream-activity-internal.h \
	babeltrace2/graph/message-stream-internal.h \
	babeltrace2/graph/port-internal.h \
	babeltrace2/graph/query-executor-internal.h \
	babeltrace2/list-internal.h \
	babeltrace2/fd-cache-internal.h \
	babeltrace2/property-internal.h \
	version.h \
	version.i
