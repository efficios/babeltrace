lib_LTLIBRARIES = libbabeltrace2-ctf-writer.la

libbabeltrace2_ctf_writer_la_SOURCES = \
	attributes.c \
	clock.c \
	clock-class.c \
	event.c \
	event-class.c \
	field-path.c \
	fields.c \
	field-types.c \
	field-wrapper.c \
	functor.c \
	logging.c \
	logging.h \
	object.c \
	object-pool.c \
	resolve.c \
	stream.c \
	stream-class.c \
	trace.c \
	utils.c \
	validation.c \
	values.c \
	visitor.c \
	writer.c

libbabeltrace2_ctf_writer_la_LDFLAGS = $(LT_NO_UNDEFINED) \
			-version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace2_ctf_writer_la_LIBADD = \
	$(top_builddir)/logging/libbabeltrace2-logging.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/ctfser/libbabeltrace2-ctfser.la \
	$(top_builddir)/compat/libcompat.la \
	$(UUID_LIBS)
