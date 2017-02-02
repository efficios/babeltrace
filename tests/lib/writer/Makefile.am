check_SCRIPTS = test_ctf_writer_no_packet_context.py

dist_noinst_SCRIPTS = test_ctf_writer_no_packet_context.py
EXTRA_DIST = test_ctf_writer_no_packet_context.py

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
