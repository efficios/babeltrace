# python_modules.m4 -- Get the Python modules install path
#
# Copyright (C) 2014 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# While extra Python modules are generaly installed in the Python
# interpreter's "site-packages" directory, Debian prefers using the
# "dist-packages" nomenclature. This macro uses the interpreter
# designated by the PYTHON variable to check the interpreter's PATH
# and sets the PYTHON_MODULES_PATH by taking the prefix into account.

# AM_PATH_PYTHON_MODULES(PYTHON)
# ---------------------------------------------------------------------------
AC_DEFUN([AM_PATH_PYTHON_MODULES],
 [prog="import sys
for path in sys.path:
    if path.endswith(\"-packages\"):
       print(path[[path.find(\"/lib\"):]])
       break"
  PYTHON_MODULES_PATH=`${$1} -c "$prog"`])
