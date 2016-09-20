AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = notification

noinst_LTLIBRARIES = libplugin-system.la

# Plug-in system library
libplugin_system_la_SOURCES = \
	component.c \
	component-class.c \
	component-factory.c \
	plugin.c \
	source.c \
	sink.c \
	filter.c \
	iterator.c \
	input.c

libplugin_system_la_LIBADD = \
	notification/libplugin-system-notification.la
