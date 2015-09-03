noinst_SCRIPTS = test_trace_read
CLEANFILES = $(noinst_SCRIPTS)
EXTRA_DIST = test_trace_read.in

$(noinst_SCRIPTS): %: %.in
	sed "s#@TOPSRCDIR@#$(top_srcdir)#g" < $< > $@
	chmod +x $@
