AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libcompat.la

libcompat_la_SOURCES = dummy.c

libcompat_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED)

if BABELTRACE_BUILD_WITH_MINGW
libcompat_la_SOURCES += compat_uuid.c
endif
