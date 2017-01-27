check_SCRIPTS = test_ctf_writer_empty_packet.py

dist_noinst_SCRIPTS = test_ctf_writer_empty_packet.py
EXTRA_DIST = test_ctf_writer_empty_packet.py

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
