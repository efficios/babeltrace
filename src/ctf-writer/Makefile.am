lib_LTLIBRARIES = libbabeltrace2-ctf-writer.la

libbabeltrace2_ctf_writer_la_SOURCES = \
	assert-pre.h \
	attributes.c \
	attributes.h \
	clock.c \
	clock-class.c \
	clock-class.h \
	clock.h \
	event.c \
	event-class.c \
	event-class.h \
	event.h \
	field-path.c \
	field-path.h \
	fields.c \
	fields.h \
	field-types.c \
	field-types.h \
	field-wrapper.c \
	field-wrapper.h \
	functor.c \
	functor.h \
	logging.c \
	logging.h \
	object.c \
	object.h \
	object-pool.c \
	object-pool.h \
	resolve.c \
	resolve.h \
	stream.c \
	stream-class.c \
	stream-class.h \
	stream.h \
	trace.c \
	trace.h \
	utils.c \
	utils.h \
	validation.c \
	validation.h \
	values.c \
	values.h \
	visitor.c \
	visitor.h \
	writer.c \
	writer.h

libbabeltrace2_ctf_writer_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace2_ctf_writer_la_LIBADD = \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/ctfser/libbabeltrace2-ctfser.la \
	$(top_builddir)/src/compat/libcompat.la
