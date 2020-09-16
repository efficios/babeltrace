# SPDX-License-Identifier: MIT
#
# Copyright (C) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
# Copyright (C) 2020 Michael Jeanson <mjeanson@efficios.com>
#
# ae_lib_elfutils.m4 -- Check elfutils version
#
# Check the currently installed version of elfutils by using the
# `_ELFUTILS_PREREQ` macro defined in <elfutils/version.h>.
#
# The cache variable for this test is `ae_cv_lib_elfutils`.
#
# AE_LIB_ELFUTILS(MAJOR_VERSION, MINOR_VERSION, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
# ---------------------------------------------------------------------------
AC_DEFUN([AE_LIB_ELFUTILS], [
m4_pushdef([major_version], [$1])
m4_pushdef([minor_version], [$2])
m4_pushdef([true_action], m4_default([$3], [:]))
m4_pushdef([false_action], m4_default(
	[$4], [AC_MSG_ERROR(elfutils >= major_version.minor_version is required)]
))

AC_CACHE_CHECK(
	[for elfutils version >= major_version.minor_version],
	[ae_cv_lib_elfutils], [
	AC_LANG_PUSH([C])
	AC_COMPILE_IFELSE(
		[AC_LANG_SOURCE([
		#include <elfutils/version.h>

		#if (!_ELFUTILS_PREREQ(][]major_version[][, ][]minor_version[][))
		#error "elfutils minimum required version not met."
		#endif

		void main(void) {
			return;
		}
		])],
		[ae_cv_lib_elfutils=yes],
		[ae_cv_lib_elfutils=no]
	)
	AC_LANG_POP([C])
])

AS_IF([test "x$ae_cv_lib_elfutils" = "xyes"], [true_action], [false_action])

m4_popdef([false_action])
m4_popdef([true_action])
m4_popdef([minor_version])
m4_popdef([major_version])
])
