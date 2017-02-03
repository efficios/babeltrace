TESTS = test_ctf_writer_no_packet_context.py \
		test_ctf_writer_empty_packet.py

check_SCRIPTS = $(TESTS)

dist_noinst_SCRIPTS = $(TESTS)
EXTRA_DIST = $(TESTS)

all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(EXTRA_DIST); do \
			cp -f $(srcdir)/$$script $(builddir); \
		done; \
	fi

clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(EXTRA_DIST); do \
			rm -f $(builddir)/$$script; \
		done; \
	fi
