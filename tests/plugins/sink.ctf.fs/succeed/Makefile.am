dist_check_SCRIPTS = test_succeed

# CTF trace generators
GEN_TRACE_LDADD = \
	$(top_builddir)/src/ctf-writer/libbabeltrace2-ctf-writer.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la

gen_trace_float_SOURCES = gen-trace-float.c
gen_trace_float_LDADD = $(GEN_TRACE_LDADD)
gen_trace_double_SOURCES = gen-trace-double.c
gen_trace_double_LDADD = $(GEN_TRACE_LDADD)

noinst_PROGRAMS = \
	gen-trace-float \
	gen-trace-double
