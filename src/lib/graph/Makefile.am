SUBDIRS = message

noinst_LTLIBRARIES = libgraph.la

# Graph library
libgraph_la_SOURCES = \
	component-class-sink-simple.c \
	component-class-sink-simple.h \
	component-class.c \
	component-class.h \
	component-descriptor-set.c \
	component-descriptor-set.h \
	component-filter.c \
	component-filter.h \
	component-sink.c \
	component-sink.h \
	component-source.c \
	component-source.h \
	component.c \
	component.h \
	connection.c \
	connection.h \
	graph.c \
	graph.h \
	interrupter.c \
	interrupter.h \
	iterator.c \
	message-iterator-class.c \
	message-iterator-class.h \
	mip.c \
	port.c \
	port.h \
	query-executor.c \
	query-executor.h

libgraph_la_LIBADD = \
	message/libgraph-message.la
