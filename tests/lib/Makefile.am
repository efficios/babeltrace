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

SCRIPT_LIST = test_seek_big_trace \
	test_seek_empty_packet \
	test_ctf_writer_complete

dist_noinst_SCRIPTS = $(SCRIPT_LIST)

all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(SCRIPT_LIST); do \
			cp -f $(srcdir)/$$script $(builddir); \
		done; \
	fi

clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(SCRIPT_LIST); do \
			rm -f $(builddir)/$$script; \
		done; \
	fi
