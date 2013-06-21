babeltrace.i: babeltrace.i.in
	sed "s/BABELTRACE_VERSION_STR/Babeltrace $(PACKAGE_VERSION)/g" <babeltrace.i.in >babeltrace.i

AM_CFLAGS = $(PYTHON_INCLUDE) -I$(top_srcdir)/include/

EXTRA_DIST = babeltrace.i
python_PYTHON = babeltrace.py
pyexec_LTLIBRARIES = _babeltrace.la

MAINTAINERCLEANFILES = babeltrace_wrap.c babeltrace.py

_babeltrace_la_SOURCES = babeltrace_wrap.c python-complements.c

_babeltrace_la_LDFLAGS = -module

_babeltrace_la_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)

_babeltrace_la_LIBS = $(GLIB_LIBS)

_babeltrace_la_LIBADD = $(top_srcdir)/formats/ctf/libbabeltrace-ctf.la	\
			$(top_srcdir)/formats/ctf-text/libbabeltrace-ctf-text.la

# SWIG 'warning md variable unused' fixed after SWIG build:
babeltrace_wrap.c: babeltrace.i
	$(SWIG) -python -Wall -I. -I$(top_srcdir)/include babeltrace.i

CLEANFILES = babeltrace.i babeltrace.py babeltrace_wrap.c
