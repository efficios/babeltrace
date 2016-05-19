AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/tests/utils

noinst_LIBRARIES = libtestcommon.a

LIBTAP=$(top_builddir)/tests/utils/tap/libtap.la

libtestcommon_a_SOURCES = common.c common.h

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
test_seek_LDFLAGS = $(LD_NO_AS_NEEDED)
test_seek_LDADD = $(LIBTAP) libtestcommon.a \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

test_bitfield_LDADD = $(LIBTAP) libtestcommon.a

test_ctf_writer_LDADD = $(LIBTAP) \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

noinst_PROGRAMS = test_seek test_bitfield test_ctf_writer

test_seek_SOURCES = test_seek.c
test_bitfield_SOURCES = test_bitfield.c
test_ctf_writer_SOURCES = test_ctf_writer.c

check_SCRIPTS = test_seek_big_trace \
		test_seek_empty_packet \
		test_ctf_writer_complete

if ENABLE_DEBUG_INFO
test_dwarf_LDFLAGS = -static
test_dwarf_LDADD = $(LIBTAP) \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/lib/libdebug-info.la
test_dwarf_SOURCES = test_dwarf.c

test_bin_info_LDFLAGS = -static
test_bin_info_LDADD = $(LIBTAP) \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/lib/libdebug-info.la
test_bin_info_SOURCES = test_bin_info.c

noinst_PROGRAMS += test_dwarf test_bin_info
check_SCRIPTS += test_dwarf_complete test_bin_info_complete
endif
