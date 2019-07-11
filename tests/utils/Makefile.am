SUBDIRS = tap

noinst_LTLIBRARIES = libtestcommon.la
libtestcommon_la_SOURCES = common.c common.h

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = python

dist_check_SCRIPTS = \
	run_python_bt2 \
	tap-driver.sh \
	utils.sh
