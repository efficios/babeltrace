SUBDIRS = message

noinst_LTLIBRARIES = libgraph.la

# Graph library
libgraph_la_SOURCES = \
	component-class-sink-colander.c \
	component-class.c \
	component-filter.c \
	component-sink.c \
	component-source.c \
	component.c \
	connection.c \
	graph.c \
	iterator.c \
	port.c \
	query-executor.c

libgraph_la_LIBADD = \
	message/libgraph-message.la
