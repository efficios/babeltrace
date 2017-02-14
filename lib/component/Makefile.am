AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = notification

noinst_LTLIBRARIES = libcomponent.la

# Plug-in system library
libcomponent_la_SOURCES = \
	component.c \
	component-class.c \
	component-graph.c \
	component-connection.c \
	component-port.c \
	source.c \
	sink.c \
	filter.c \
	iterator.c

libcomponent_la_LIBADD = \
	notification/libcomponent-notification.la
