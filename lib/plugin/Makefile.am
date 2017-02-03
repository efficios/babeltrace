AM_CFLAGS = $(PYTHON_INCLUDE) $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

noinst_LTLIBRARIES = libplugin.la

# Plug-in system library
libplugin_la_SOURCES = \
	plugin.c \
	plugin-so.c

if WITH_PYTHON_PLUGINS
libplugin_la_SOURCES += plugin-python.c
libplugin_la_LDFLAGS = $(PYTHON_LIBS)
endif
