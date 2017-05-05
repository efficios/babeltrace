check_SCRIPTS = test_ctf_writer_no_packet_context.py \
	test_ctf_writer_empty_packet.py

all-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(check_SCRIPTS); do \
			cp -f $(srcdir)/$$script $(builddir); \
		done; \
	fi

clean-local:
	@if [ x"$(srcdir)" != x"$(builddir)" ]; then \
		for script in $(check_SCRIPTS); do \
			rm -f $(builddir)/$$script; \
		done; \
	fi
