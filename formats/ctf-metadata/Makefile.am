AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libbabeltrace-ctf-metadata.la

libbabeltrace_ctf_metadata_la_SOURCES = \
	ctf-metadata.c

# Request that the linker keeps all static libraries objects.
libbabeltrace_ctf_metadata_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED) -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_ctf_metadata_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
