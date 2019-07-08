AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

test_bitfield_SOURCES = test_bitfield.c
test_bitfield_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/tests/utils/libtestcommon.la

noinst_PROGRAMS = test_bitfield
