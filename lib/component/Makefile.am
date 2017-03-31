AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = notification

noinst_LTLIBRARIES = libcomponent.la

# Plug-in system library
libcomponent_la_SOURCES = \
	clock-class-priority-map.c \
	component.c \
	component-class.c \
	graph.c \
	connection.c \
	port.c \
	source.c \
	sink.c \
	filter.c \
	iterator.c

libcomponent_la_LIBADD = \
	notification/libcomponent-notification.la
