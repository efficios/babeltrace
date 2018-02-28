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
	babeltrace/ctf-writer/event-types.h \
	babeltrace/ctf-writer/event.h \
	babeltrace/ctf-writer/stream-class.h \
	babeltrace/ctf-writer/stream.h \
	babeltrace/ctf-writer/writer.h

# CTF IR API
babeltracectfirincludedir = "$(includedir)/babeltrace/ctf-ir"
babeltracectfirinclude_HEADERS = \
	babeltrace/ctf-ir/clock-class.h \
	babeltrace/ctf-ir/clock-value.h \
	babeltrace/ctf-ir/clock.h \
	babeltrace/ctf-ir/event-class.h \
	babeltrace/ctf-ir/event.h \
	babeltrace/ctf-ir/field-path.h \
	babeltrace/ctf-ir/field-types.h \
	babeltrace/ctf-ir/fields.h \
	babeltrace/ctf-ir/packet.h \
	babeltrace/ctf-ir/stream-class.h \
	babeltrace/ctf-ir/stream.h \
	babeltrace/ctf-ir/trace.h \
	babeltrace/ctf-ir/utils.h \
	babeltrace/ctf-ir/visitor.h

# Plugin and plugin development API
babeltracepluginincludedir = "$(includedir)/babeltrace/plugin"
babeltraceplugininclude_HEADERS = \
	babeltrace/plugin/plugin-dev.h \
	babeltrace/plugin/plugin.h

# Graph, component, and notification API
babeltracegraphincludedir = "$(includedir)/babeltrace/graph"
babeltracegraphinclude_HEADERS = \
	babeltrace/graph/clock-class-priority-map.h \
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
	babeltrace/graph/notification-discarded-events.h \
	babeltrace/graph/notification-discarded-packets.h \
	babeltrace/graph/notification-event.h \
	babeltrace/graph/notification-heap.h \
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
	babeltrace/align-internal.h \
	babeltrace/assert-internal.h \
	babeltrace/assert-pre-internal.h \
	babeltrace/babeltrace-internal.h \
	babeltrace/bitfield-internal.h \
	babeltrace/common-internal.h \
	babeltrace/compat/fcntl-internal.h \
	babeltrace/compat/glib-internal.h \
	babeltrace/compat/limits-internal.h \
	babeltrace/compat/memstream-internal.h \
	babeltrace/compat/mman-internal.h \
	babeltrace/compat/socket-internal.h \
	babeltrace/compat/stdio-internal.h \
	babeltrace/compat/stdlib-internal.h \
	babeltrace/compat/string-internal.h \
	babeltrace/compat/time-internal.h \
	babeltrace/compat/unistd-internal.h \
	babeltrace/compat/utc-internal.h \
	babeltrace/compat/uuid-internal.h \
	babeltrace/compiler-internal.h \
	babeltrace/ctf-ir/attributes-internal.h \
	babeltrace/ctf-ir/clock-class-internal.h \
	babeltrace/ctf-ir/clock-value-internal.h \
	babeltrace/ctf-ir/event-class-internal.h \
	babeltrace/ctf-ir/event-internal.h \
	babeltrace/ctf-ir/field-path-internal.h \
	babeltrace/ctf-ir/field-types-internal.h \
	babeltrace/ctf-ir/fields-internal.h \
	babeltrace/ctf-ir/packet-internal.h \
	babeltrace/ctf-ir/resolve-internal.h \
	babeltrace/ctf-ir/stream-class-internal.h \
	babeltrace/ctf-ir/stream-internal.h \
	babeltrace/ctf-ir/trace-internal.h \
	babeltrace/ctf-ir/validation-internal.h \
	babeltrace/ctf-ir/visitor-internal.h \
	babeltrace/ctf-ir/utils-internal.h \
	babeltrace/ctf-writer/clock-internal.h \
	babeltrace/ctf-writer/functor-internal.h \
	babeltrace/ctf-writer/serialize-internal.h \
	babeltrace/ctf-writer/writer-internal.h \
	babeltrace/endian-internal.h \
	babeltrace/graph/clock-class-priority-map-internal.h \
	babeltrace/graph/component-class-internal.h \
	babeltrace/graph/component-class-sink-colander-internal.h \
	babeltrace/graph/component-filter-internal.h \
	babeltrace/graph/component-internal.h \
	babeltrace/graph/component-sink-internal.h \
	babeltrace/graph/component-source-internal.h \
	babeltrace/graph/connection-internal.h \
	babeltrace/graph/graph-internal.h \
	babeltrace/graph/notification-discarded-elements-internal.h \
	babeltrace/graph/notification-event-internal.h \
	babeltrace/graph/notification-heap-internal.h \
	babeltrace/graph/notification-inactivity-internal.h \
	babeltrace/graph/notification-internal.h \
	babeltrace/graph/notification-iterator-internal.h \
	babeltrace/graph/notification-packet-internal.h \
	babeltrace/graph/notification-stream-internal.h \
	babeltrace/graph/port-internal.h \
	babeltrace/graph/query-executor-internal.h \
	babeltrace/lib-logging-internal.h \
	babeltrace/list-internal.h \
	babeltrace/logging-internal.h \
	babeltrace/mmap-align-internal.h \
	babeltrace/object-internal.h \
	babeltrace/plugin/plugin-internal.h \
	babeltrace/plugin/plugin-so-internal.h \
	babeltrace/prio-heap-internal.h \
	babeltrace/ref-internal.h \
	babeltrace/values-internal.h \
	version.h \
	version.i
