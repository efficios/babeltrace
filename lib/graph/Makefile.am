SUBDIRS = notification

noinst_LTLIBRARIES = libgraph.la

# Graph library
libgraph_la_SOURCES = \
	component.c \
	component-class.c \
	graph.c \
	connection.c \
	port.c \
	source.c \
	sink.c \
	filter.c \
	iterator.c \
	component-class-sink-colander.c \
	query-executor.c

libgraph_la_LIBADD = \
	notification/libgraph-notification.la
