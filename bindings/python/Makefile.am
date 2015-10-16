NATIVEBT_PY = nativebt.py
NATIVEBT_WRAP_C = nativebt_wrap.c
INIT_PY = __init__.py
INIT_PY_IN = $(srcdir)/$(INIT_PY).in
NATIVEBT_I = $(srcdir)/nativebt.i

all-local: $(INIT_PY)

$(INIT_PY): $(INIT_PY_IN)
	sed "s/BABELTRACE_VERSION_STR/$(PACKAGE_VERSION)/g" < $< > $@

AM_CFLAGS = $(PYTHON_INCLUDE) -I$(top_srcdir)/include/ -I$(top_srcdir)/bindings/python

EXTRA_DIST = common.py reader.py writer.py $(INIT_PY_IN) $(NATIVEBT_I)
nodist_btpackage_PYTHON = $(INIT_PY) $(NATIVEBT_PY) common.py reader.py writer.py
nativebtlib_LTLIBRARIES = _nativebt.la

btpackagedir = $(pythondir)/babeltrace
nativebtlibdir = $(btpackagedir)

MAINTAINERCLEANFILES = $(NATIVEBT_PY) $(NATIVEBT_WRAP_C)

nodist__nativebt_la_SOURCES = $(NATIVEBT_WRAP_C)
_nativebt_la_SOURCES = python-complements.h python-complements.c
_nativebt_la_LDFLAGS = -module
_nativebt_la_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
_nativebt_la_LIBS = $(GLIB_LIBS)
_nativebt_la_LIBADD = $(top_builddir)/formats/ctf/libbabeltrace-ctf.la

# SWIG 'warning md variable unused' fixed after SWIG build:
$(NATIVEBT_WRAP_C): $(NATIVEBT_I)
	$(SWIG) -python -Wall -I$(srcdir) -I$(top_srcdir)/include -outcurrentdir $<

CLEANFILES = $(NATIVEBT_PY) $(NATIVEBT_WRAP_C) $(INIT_PY)
