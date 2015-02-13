all-local: __init__.py

__init__.py: __init__.py.in
	sed "s/BABELTRACE_VERSION_STR/$(PACKAGE_VERSION)/g" < $< > $@

AM_CFLAGS = $(PYTHON_INCLUDE) -I$(top_srcdir)/include/

EXTRA_DIST = __init__.py.in nativebt.i common.py reader.py writer.py
nodist_btpackage_PYTHON = __init__.py nativebt.py common.py reader.py writer.py
nativebtlib_LTLIBRARIES = _nativebt.la

btpackagedir = $(pythondir)/babeltrace
nativebtlibdir = $(btpackagedir)

MAINTAINERCLEANFILES = nativebt_wrap.c nativebt.py

nodist__nativebt_la_SOURCES = nativebt_wrap.c
_nativebt_la_SOURCES = python-complements.h python-complements.c
_nativebt_la_LDFLAGS = -module

_nativebt_la_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)

_nativebt_la_LIBS = $(GLIB_LIBS)

_nativebt_la_LIBADD = $(top_builddir)/formats/ctf/libbabeltrace-ctf.la

# SWIG 'warning md variable unused' fixed after SWIG build:
nativebt_wrap.c: nativebt.i
	$(SWIG) -python -Wall -I. -I$(top_srcdir)/include $<

CLEANFILES = nativebt.py nativebt_wrap.c __init__.py
