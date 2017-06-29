# native module name (without `.i` extension)
NATIVE_MODULE = native_bt

# interface dependencies (without `native_bt` prefix and `.i` extension)
NATIVE_MODULE_DEPS =	\
	clockclass	\
	eventclass	\
	event		\
	fields		\
	ft		\
	packet		\
	ref		\
	streamclass	\
	stream		\
	trace		\
	values		\
	ctfwriter	\
	componentclass	\
	component	\
	notification	\
	notifiter	\
	plugin

# Python modules (without `.py` extension)
EXTRA_MODULES = 		\
	clock_class		\
	event_class		\
	event			\
	fields			\
	field_types		\
	object			\
	packet			\
	stream_class		\
	stream			\
	trace			\
	utils			\
	values			\
	ctf_writer		\
	component		\
	notification		\
	notification_iterator	\
	plugin			\
	py_plugin

# automatically generated file lists
EXTRA_MODULES_PY = $(addprefix $(srcdir)/,$(addsuffix .py,$(EXTRA_MODULES)))
NATIVE_MODULE_I = $(srcdir)/$(NATIVE_MODULE).i
NATIVE_MODULE_PY = $(NATIVE_MODULE).py
NATIVE_MODULE_C = $(NATIVE_MODULE)_wrap.c
NATIVE_MODULE_DEPS_I = $(addprefix $(srcdir)/native_bt,$(addsuffix .i,$(NATIVE_MODULE_DEPS)))

# installed Python package
nodist_bt2package_PYTHON = __init__.py $(EXTRA_MODULES_PY) $(NATIVE_MODULE_PY)

# native libraries to build
nativelibs_LTLIBRARIES = _native_bt.la

# installation directory for the `bt2` package
bt2packagedir = $(pythondir)/bt2
nativelibsdir = $(bt2packagedir)

# SWIG to C wrapper (and Python file)
$(NATIVE_MODULE_C): $(NATIVE_MODULE_I) $(NATIVE_MODULE_DEPS_I)
	$(SWIG) -python -Wall -I$(srcdir) -I$(top_srcdir)/include -module $(NATIVE_MODULE) -outcurrentdir $(NATIVE_MODULE_I)

# native_bt module
_native_bt_la_SOURCES = native_bt_wrap.c
_native_bt_la_LDFLAGS = -module
_native_bt_la_CFLAGS = $(GLIB_CFLAGS) $(PYTHON_INCLUDE) -I$(srcdir) $(AM_CFLAGS)
_native_bt_la_LIBS = $(GLIB_LIBS)
_native_bt_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

# extra module sources -> build directory
all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then		\
		for file in $(EXTRA_MODULES_PY); do		\
			cp -f $(srcdir)/$$file $(builddir);	\
		done;						\
	fi

# clean extra module sources in build directory
clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then		\
		for file in $(EXTRA_MODULES_PY); do		\
			rm -f $(srcdir)/$$file $(builddir);	\
		done;						\
	fi

# distribute: extra Python modules and SWIG interface files
EXTRA_DIST = __init__.py.in $(EXTRA_MODULES_PY) $(NATIVE_MODULE_I) $(NATIVE_MODULE_DEPS_I)

# clean: generated C and Python files (by SWIG)
CLEANFILES = $(NATIVE_MODULE_PY) $(NATIVE_MODULE_C)
