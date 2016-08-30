#ifndef _CTF_SCANNER_SYMBOLS
#define _CTF_SCANNER_SYMBOLS

/*
 * ctf-scanner-symbols.h
 *
 * Copyright 2011-2012 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#define yy_create_buffer bt_yy_create_buffer
#define yy_delete_buffer bt_yy_delete_buffer
#define yy_flush_buffer bt_yy_flush_buffer
#define yy_scan_buffer bt_yy_scan_buffer
#define yy_scan_bytes bt_yy_scan_bytes
#define yy_scan_string bt_yy_scan_string
#define yy_switch_to_buffer bt_yy_switch_to_buffer
#define yyalloc bt_yyalloc
#define yyfree bt_yyfree
#define yyget_column bt_yyget_column
#define yyget_debug bt_yyget_debug
#define yyget_extra bt_yyget_extra
#define yyget_in bt_yyget_in
#define yyget_leng bt_yyget_leng
#define yyget_lineno bt_yyget_lineno
#define yyget_lval bt_yyget_lval
#define yyget_out bt_yyget_out
#define yyget_text bt_yyget_text
#define yylex_init bt_yylex_init
#define yypop_buffer_state bt_yypop_buffer_state
#define yypush_buffer_state bt_yypush_buffer_state
#define yyrealloc bt_yyrealloc
#define yyset_column bt_yyset_column
#define yyset_debug bt_yyset_debug
#define yyset_extra bt_yyset_extra
#define yyset_in bt_yyset_in
#define yyset_lineno bt_yyset_lineno
#define yyset_lval bt_yyset_lval
#define yyset_out bt_yyset_out

#endif /* _CTF_SCANNER_SYMBOLS */
