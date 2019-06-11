dist_check_SCRIPTS = test_succeed

# Expectation files
EXTRA_DIST = \
	trace-smalltrace.expect \
	trace-simple.expect

# CTF trace generators
GEN_TRACE_LDADD = $(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la

gen_trace_simple_SOURCES = gen-trace-simple.c
gen_trace_simple_LDADD = $(GEN_TRACE_LDADD)

noinst_PROGRAMS = \
	gen-trace-simple
