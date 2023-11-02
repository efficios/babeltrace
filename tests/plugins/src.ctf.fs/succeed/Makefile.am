# SPDX-License-Identifier: MIT

dist_check_SCRIPTS = test-succeed.sh

# CTF trace generators
GEN_TRACE_LDADD = \
	$(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/logging/liblogging.la

gen_trace_simple_SOURCES = gen-trace-simple.c
gen_trace_simple_LDADD = $(GEN_TRACE_LDADD)

noinst_PROGRAMS = \
	gen-trace-simple
