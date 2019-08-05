SUBDIRS = message

noinst_LTLIBRARIES = libgraph.la

# Graph library
libgraph_la_SOURCES = \
	component.c \
	component-class.c \
	component-class.h \
	component-filter.c \
	component-filter.h \
	component.h \
	component-sink.c \
	component-sink.h \
	component-source.c \
	component-source.h \
	connection.c \
	connection.h \
	graph.c \
	graph.h \
	interrupter.c \
	interrupter.h \
	iterator.c \
	port.c \
	port.h \
	query-executor.c \
	query-executor.h

libgraph_la_LIBADD = \
	message/libgraph-message.la
