# native module name (without `.i` extension)
NATIVE_MODULE = native_bt

# interface dependencies (without `native_bt` prefix and `.i` extension)
NATIVE_MODULE_DEPS =	\
	ccpriomap	\
	clockclass	\
	component	\
	componentclass	\
	connection	\
	ctfwriter	\
	event		\
	eventclass	\
	fields		\
	ft		\
	graph		\
	logging		\
	notification	\
	notifiter	\
	packet		\
	plugin		\
	port		\
	ref		\
	stream		\
	streamclass	\
	trace		\
	values		\
	version

# Python modules (without `.py` extension)
EXTRA_MODULES = 			\
	clock_class			\
	clock_class_priority_map	\
	component			\
	connection			\
	ctf_writer			\
	event				\
	event_class			\
	field_types			\
	fields				\
	graph				\
	logging				\
	notification			\
	notification_iterator		\
	object				\
	packet				\
	plugin				\
	port				\
	py_plugin			\
	stream				\
	stream_class			\
	trace				\
	utils				\
	values

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
_native_bt_la_SOURCES = logging.h logging.c
nodist__native_bt_la_SOURCES = native_bt_wrap.c
_native_bt_la_LDFLAGS = -module
_native_bt_la_CFLAGS = $(PYTHON_INCLUDE) -I$(srcdir) $(AM_CFLAGS)
_native_bt_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la

# extra module sources -> build directory
all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then		\
		for file in $(EXTRA_MODULES_PY); do		\
			cp -f "$(srcdir)/$$file" "$(builddir)";	\
		done;						\
	fi

# clean extra module sources in build directory
clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then		\
		for file in $(EXTRA_MODULES_PY); do		\
			rm -f "$(srcdir)/$$file" "$(builddir)";	\
		done;						\
	fi

# distribute: extra Python modules and SWIG interface files
EXTRA_DIST = __init__.py.in $(EXTRA_MODULES_PY) $(NATIVE_MODULE_I) $(NATIVE_MODULE_DEPS_I)

# clean: generated C and Python files (by SWIG)
CLEANFILES = $(NATIVE_MODULE_PY) $(NATIVE_MODULE_C)
