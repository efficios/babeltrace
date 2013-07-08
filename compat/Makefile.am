AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libcompat.la

libcompat_la_SOURCES =

libcompat_la_LDFLAGS = \
	-Wl,--no-as-needed

if BABELTRACE_BUILD_WITH_MINGW
libcompat_la_SOURCES += compat_uuid.c
endif
