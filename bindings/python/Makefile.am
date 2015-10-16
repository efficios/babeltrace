babeltrace.i: babeltrace.i.in
	sed "s/BABELTRACE_VERSION_STR/Babeltrace $(PACKAGE_VERSION)/g" < \
		$(top_srcdir)/bindings/python/babeltrace.i.in > \
		$(top_builddir)/bindings/python/babeltrace.i

AM_CFLAGS = $(PYTHON_INCLUDE) -I$(top_srcdir)/include/ \
		-I$(top_srcdir)/bindings/python

EXTRA_DIST = babeltrace.i.in
nodist_python_PYTHON = babeltrace.py
pyexec_LTLIBRARIES = _babeltrace.la

MAINTAINERCLEANFILES = babeltrace_wrap.c babeltrace.py

nodist__babeltrace_la_SOURCES = babeltrace_wrap.c
_babeltrace_la_SOURCES = python-complements.h python-complements.c
_babeltrace_la_LDFLAGS = -module

_babeltrace_la_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)

_babeltrace_la_LIBS = $(GLIB_LIBS)

_babeltrace_la_LIBADD = $(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/formats/ctf-text/libbabeltrace-ctf-text.la

# SWIG 'warning md variable unused' fixed after SWIG build:
babeltrace_wrap.c: babeltrace.i
	$(SWIG) -python -Wall -I. -I$(top_srcdir)/include \
		$(top_builddir)/bindings/python/babeltrace.i

CLEANFILES = babeltrace.i babeltrace.py babeltrace_wrap.c
