AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

noinst_PROGRAMS = ctf_writer

ctf_writer_SOURCES = ctf_writer.c
ctf_writer_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/tests/utils/libtestcommon.la \
	$(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la

dist_check_SCRIPTS = test_ctf_writer
