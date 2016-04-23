# Pretty printing macros.
#
# Author: Philippe Proulx <pproulx@efficios.com>

# PPRINT_INIT(): initializes the pretty printing system.
#
# Use this macro before using any other PPRINT_* macro.
AC_DEFUN([PPRINT_INIT], [
  m4_define([PPRINT_CONFIG_TS], 50)
  m4_define([PPRINT_CONFIG_INDENT], 2)
  PPRINT_YES_MSG=yes
  PPRINT_NO_MSG=no

  # find tput, which tells us if colors are supported and gives us color codes
  AC_PATH_PROG(pprint_tput, tput)

  AS_IF([test -n "$pprint_tput"], [
    AS_IF([test -n "$PS1" && test `"$pprint_tput" colors` -ge 8 && test -t 1], [
      # interactive shell and colors supported and standard output
      # file descriptor is opened on a terminal
      PPRINT_COLOR_TXTBLK=`"$pprint_tput" setf 0`
      PPRINT_COLOR_TXTBLU=`"$pprint_tput" setf 1`
      PPRINT_COLOR_TXTGRN=`"$pprint_tput" setf 2`
      PPRINT_COLOR_TXTCYN=`"$pprint_tput" setf 3`
      PPRINT_COLOR_TXTRED=`"$pprint_tput" setf 4`
      PPRINT_COLOR_TXTPUR=`"$pprint_tput" setf 5`
      PPRINT_COLOR_TXTYLW=`"$pprint_tput" setf 6`
      PPRINT_COLOR_TXTWHT=`"$pprint_tput" setf 7`
      PPRINT_COLOR_BLD=`"$pprint_tput" bold`
      PPRINT_COLOR_BLDBLK=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTBLK
      PPRINT_COLOR_BLDBLU=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTBLU
      PPRINT_COLOR_BLDGRN=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTGRN
      PPRINT_COLOR_BLDCYN=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTCYN
      PPRINT_COLOR_BLDRED=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTRED
      PPRINT_COLOR_BLDPUR=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTPUR
      PPRINT_COLOR_BLDYLW=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTYLW
      PPRINT_COLOR_BLDWHT=$PPRINT_COLOR_BLD$PPRINT_COLOR_TXTWHT
      PPRINT_COLOR_RST=`"$pprint_tput" sgr0`

      # colored yes and no
      PPRINT_YES_MSG="$PPRINT_COLOR_BLDGRN$PPRINT_YES_MSG$PPRINT_COLOR_RST"
      PPRINT_NO_MSG="$PPRINT_COLOR_BLDRED$PPRINT_NO_MSG$PPRINT_COLOR_RST"

      # subtitle color
      PPRINT_COLOR_SUBTITLE=$PPRINT_COLOR_BLDCYN
    ])
  ])
])

# PPRINT_SET_INDENT(indent): sets the current indentation.
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_SET_INDENT], [
  m4_define([PPRINT_CONFIG_INDENT], $1)
])

# PPRINT_SET_TS(ts): sets the current tab stop.
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_SET_TS], [
  m4_define([PPRINT_CONFIG_TS], $1)
])

# PPRINT_SUBTITLE(subtitle): pretty prints a subtitle.
#
# The subtitle is put as is in a double-quoted shell string so the user
# needs to escape ".
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_SUBTITLE], [
  AS_ECHO("${PPRINT_COLOR_SUBTITLE}$1$PPRINT_COLOR_RST")
])

AC_DEFUN([_PPRINT_INDENT], [
  m4_if(PPRINT_CONFIG_INDENT, 0, [
  ], [
    m4_for([pprint_i], 0, m4_eval(PPRINT_CONFIG_INDENT * 2 - 1), 1, [
      AS_ECHO_N(" ")
    ])
  ])
])

# PPRINT_PROP_STRING(title, value, title_color?): pretty prints a
# string property.
#
# The title is put as is in a double-quoted shell string so the user
# needs to escape ".
#
# The $PPRINT_CONFIG_INDENT variable must be set to the desired indentation
# level.
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_PROP_STRING], [
  m4_pushdef([pprint_title], [$1])
  m4_pushdef([pprint_value], [$2])

  m4_if([$#], 3, [
    m4_pushdef([pprint_title_color], [$3])
  ], [
    m4_pushdef([pprint_title_color], [])
  ])

  m4_pushdef([pprint_title_len], m4_len(pprint_title))
  m4_pushdef([pprint_spaces_cnt], m4_eval(PPRINT_CONFIG_TS - pprint_title_len - (PPRINT_CONFIG_INDENT * 2) - 1))

  m4_if(m4_eval(pprint_spaces_cnt <= 0), 1, [
    m4_define([pprint_spaces_cnt], 1)
  ])

  m4_pushdef([pprint_spaces], [])

  m4_for([pprint_i], 0, m4_eval(pprint_spaces_cnt - 1), 1, [
    m4_append([pprint_spaces], [ ])
  ])

  _PPRINT_INDENT

  AS_ECHO_N("pprint_title_color""pprint_title$PPRINT_COLOR_RST:pprint_spaces")
  AS_ECHO("${PPRINT_COLOR_BLD}pprint_value$PPRINT_COLOR_RST")

  m4_popdef([pprint_spaces])
  m4_popdef([pprint_spaces_cnt])
  m4_popdef([pprint_title_len])
  m4_popdef([pprint_title_color])
  m4_popdef([pprint_value])
  m4_popdef([pprint_title])
])

# PPRINT_PROP_BOOL(title, value, title_color?): pretty prints a boolean
# property.
#
# The title is put as is in a double-quoted shell string so the user
# needs to escape ".
#
# The value is evaluated at shell runtime. Its evaluation must be
# 0 (false) or 1 (true).
#
# Uses the PPRINT_PROP_STRING() with the "yes" or "no" string.
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_PROP_BOOL], [
  m4_pushdef([pprint_title], [$1])
  m4_pushdef([pprint_value], [$2])

  test pprint_value -eq 0 && pprint_msg="$PPRINT_NO_MSG" || pprint_msg="$PPRINT_YES_MSG"

  m4_if([$#], 3, [
    PPRINT_PROP_STRING(pprint_title, $pprint_msg, $3)
  ], [
    PPRINT_PROP_STRING(pprint_title, $pprint_msg)
  ])

  m4_popdef([pprint_value])
  m4_popdef([pprint_title])
])

# PPRINT_WARN(msg): pretty prints a warning message.
#
# The message is put as is in a double-quoted shell string so the user
# needs to escape ".
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_WARN], [
  m4_pushdef([pprint_msg], [$1])

  _PPRINT_INDENT
  AS_ECHO("${PPRINT_COLOR_TXTYLW}WARNING:$PPRINT_COLOR_RST ${PPRINT_COLOR_BLDYLW}pprint_msg$PPRINT_COLOR_RST")

  m4_popdef([pprint_msg])
])

# PPRINT_ERROR(msg): pretty prints an error message and exits.
#
# The message is put as is in a double-quoted shell string so the user
# needs to escape ".
#
# Use PPRINT_INIT() before using this macro.
AC_DEFUN([PPRINT_ERROR], [
  m4_pushdef([pprint_msg], [$1])

  AC_MSG_ERROR(${PPRINT_COLOR_BLDRED}pprint_msg$PPRINT_COLOR_RST)

  m4_popdef([pprint_msg])
])
