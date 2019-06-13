dist_check_SCRIPTS = test_succeed

# CTF trace generators
GEN_TRACE_LDADD = $(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la

gen_trace_simple_SOURCES = gen-trace-simple.c
gen_trace_simple_LDADD = $(GEN_TRACE_LDADD)

noinst_PROGRAMS = \
	gen-trace-simple
