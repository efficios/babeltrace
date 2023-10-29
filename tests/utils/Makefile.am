# SPDX-License-Identifier: MIT

SUBDIRS = tap

noinst_LTLIBRARIES = libtestcommon.la
libtestcommon_la_SOURCES = common.c common.h

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = python

dist_check_SCRIPTS = \
	run-python-bt2.sh \
	tap-driver.sh \
	utils.sh
