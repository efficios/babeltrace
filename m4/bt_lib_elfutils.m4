# bt_lib_elfutils.m4 -- Check elfutils version
#
# Copyright (C) 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions: The above copyright notice and
# this permission notice shall be included in all copies or substantial
# portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Check the currently installed version of elfutils by using the
# `_ELFUTILS_PREREQ` macro defined in <elfutils/version.h>.
#
# The cache variable for this test is `bt_cv_lib_elfutils`.
#
# BT_LIB_ELFUTILS(MAJOR_VERSION, MINOR_VERSION, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
# ---------------------------------------------------------------------------
AC_DEFUN([BT_LIB_ELFUTILS], [
	m4_pushdef([major_version], [$1])
	m4_pushdef([minor_version], [$2])
	m4_pushdef([true_action], m4_default([$3], [:]))
	m4_pushdef([false_action], m4_default(
		[$4], [AC_MSG_ERROR(elfutils >= major_version.minor_version is required)]
	))

	AC_CACHE_CHECK(
		[for elfutils version >= major_version.minor_version],
		[bt_cv_lib_elfutils], [
			AC_RUN_IFELSE([AC_LANG_SOURCE([
				#include <stdlib.h>
				#include <elfutils/version.h>

				int main(void) {
					return _ELFUTILS_PREREQ(major_version, minor_version) ? EXIT_SUCCESS : EXIT_FAILURE;
				}
			])], [bt_cv_lib_elfutils=yes], [bt_cv_lib_elfutils=no])
		]
	)

	AS_IF([test "x$bt_cv_lib_elfutils" = "xyes"], [true_action], [false_action])

	m4_popdef([false_action])
	m4_popdef([true_action])
	m4_popdef([minor_version])
	m4_popdef([major_version])
])
