# SPDX-FileCopyrightText: 2013-2023 EfficiOS, Inc.
# SPDX-License-Identifier: MIT

SUBDIRS = tap

noinst_LTLIBRARIES = libtestcommon.la
libtestcommon_la_SOURCES = common.c common.h

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = python

dist_check_SCRIPTS = \
	run-in-py-env.sh \
	tap-driver.sh \
	utils.sh
