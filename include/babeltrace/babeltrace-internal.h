#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

/*
 * babeltrace/babeltrace-internal.h
 *
 * Copyright 2012 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <glib.h>
#include <stdint.h>
#include <babeltrace/compat/string.h>

#define PERROR_BUFLEN	200

extern int babeltrace_verbose, babeltrace_debug;

#define printf_verbose(fmt, args...)					\
	do {								\
		if (babeltrace_verbose)					\
			fprintf(stdout, "[verbose] " fmt, ## args);	\
	} while (0)

#define printf_debug(fmt, args...)					\
	do {								\
		if (babeltrace_debug)					\
			fprintf(stdout, "[debug] " fmt, ## args);	\
	} while (0)

#define _bt_printf(fp, kindstr, fmt, args...)				\
	fprintf(fp, "[%s]%s%s%s: " fmt "\n",				\
		kindstr,						\
		babeltrace_debug ? " \"" : "",				\
		babeltrace_debug ? __func__ : "",			\
		babeltrace_debug ? "\"" : "",				\
		## args)

#define _bt_printfl(fp, kindstr, lineno, fmt, args...)			\
	fprintf(fp, "[%s]%s%s%s at line %u: " fmt "\n",			\
		kindstr,						\
		babeltrace_debug ? " \"" : "",				\
		babeltrace_debug ? __func__ : "",			\
		babeltrace_debug ? "\"" : "",				\
		lineno,							\
		## args)

#define _bt_printfe(fp, kindstr, perrorstr, fmt, args...)		\
	fprintf(fp, "[%s]%s%s%s: %s: " fmt "\n",			\
		kindstr,						\
		babeltrace_debug ? " \"" : "",				\
		babeltrace_debug ? __func__ : "",			\
		babeltrace_debug ? "\"" : "",				\
		perrorstr,						\
		## args)

#define _bt_printfle(fp, kindstr, lineno, perrorstr, fmt, args...)	\
	fprintf(fp, "[%s]%s%s%s at line %u: %s: " fmt "\n",		\
		kindstr,						\
		babeltrace_debug ? " \"" : "",				\
		babeltrace_debug ? __func__ : "",			\
		babeltrace_debug ? "\"" : "",				\
		lineno,							\
		perrorstr,						\
		## args)

#define _bt_printf_perror(fp, fmt, args...)				\
	({								\
		char buf[PERROR_BUFLEN] = "Error in strerror_r()";	\
		compat_strerror_r(errno, buf, sizeof(buf));		\
		_bt_printfe(fp, "error", buf, fmt, ## args);		\
	})

#define _bt_printfl_perror(fp, lineno, fmt, args...)			\
	({								\
		char buf[PERROR_BUFLEN] = "Error in strerror_r()";	\
		compat_strerror_r(errno, buf, sizeof(buf));		\
		_bt_printfle(fp, "error", lineno, buf, fmt, ## args);	\
	})

/* printf without lineno information */
#define printf_fatal(fmt, args...)					\
	_bt_printf(stderr, "fatal", fmt, ## args)
#define printf_error(fmt, args...)					\
	_bt_printf(stderr, "error", fmt, ## args)
#define printf_warning(fmt, args...)					\
	_bt_printf(stderr, "warning", fmt, ## args)
#define printf_perror(fmt, args...)					\
	_bt_printf_perror(stderr, fmt, ## args)

/* printf with lineno information */
#define printfl_fatal(lineno, fmt, args...)				\
	_bt_printfl(stderr, "fatal", lineno, fmt, ## args)
#define printfl_error(lineno, fmt, args...)				\
	_bt_printfl(stderr, "error", lineno, fmt, ## args)
#define printfl_warning(lineno, fmt, args...)				\
	_bt_printfl(stderr, "warning", lineno, fmt, ## args)
#define printfl_perror(lineno, fmt, args...)				\
	_bt_printfl_perror(stderr, lineno, fmt, ## args)

/* printf with node lineno information */
#define printfn_fatal(node, fmt, args...)				\
	_bt_printfl(stderr, "fatal", (node)->lineno, fmt, ## args)
#define printfn_error(node, fmt, args...)				\
	_bt_printfl(stderr, "error", (node)->lineno, fmt, ## args)
#define printfn_warning(node, fmt, args...)				\
	_bt_printfl(stderr, "warning", (node)->lineno, fmt, ## args)
#define printfn_perror(node, fmt, args...)				\
	_bt_printfl_perror(stderr, (node)->lineno, fmt, ## args)

/* fprintf with Node lineno information */
#define fprintfn_fatal(fp, node, fmt, args...)				\
	_bt_printfl(fp, "fatal", (node)->lineno, fmt, ## args)
#define fprintfn_error(fp, node, fmt, args...)				\
	_bt_printfl(fp, "error", (node)->lineno, fmt, ## args)
#define fprintfn_warning(fp, node, fmt, args...)			\
	_bt_printfl(fp, "warning", (node)->lineno, fmt, ## args)
#define fprintfn_perror(fp, node, fmt, args...)				\
	_bt_printfl_perror(fp, (node)->lineno, fmt, ## args)

#ifndef likely
# ifdef __GNUC__
#  define likely(x)      __builtin_expect(!!(x), 1)
# else
#  define likely(x)      (!!(x))
# endif
#endif

#ifndef unlikely
# ifdef __GNUC__
#  define unlikely(x)    __builtin_expect(!!(x), 0)
# else
#  define unlikely(x)    (!!(x))
# endif
#endif

/*
 * BT_HIDDEN: set the hidden attribute for internal functions
 */
#define BT_HIDDEN __attribute__((visibility("hidden")))

#define BT_CTF_MAJOR	1
#define BT_CTF_MINOR	8

struct bt_trace_descriptor;
struct trace_collection {
	GPtrArray *array;	/* struct bt_trace_descriptor */
	GHashTable *clocks;	/* struct ctf_clock */

	uint64_t single_clock_offset_avg;
	uint64_t offset_first;
	int64_t delta_offset_first_sum;
	int offset_nr;
	int clock_use_offset_avg;
};

extern int opt_all_field_names,
	opt_scope_field_names,
	opt_header_field_names,
	opt_context_field_names,
	opt_payload_field_names,
	opt_all_fields,
	opt_trace_field,
	opt_trace_domain_field,
	opt_trace_procname_field,
	opt_trace_vpid_field,
	opt_trace_hostname_field,
	opt_trace_default_fields,
	opt_loglevel_field,
	opt_emf_field,
	opt_callsite_field,
	opt_delta_field,
	opt_clock_cycles,
	opt_clock_seconds,
	opt_clock_date,
	opt_clock_gmt,
	opt_clock_force_correlate;

extern uint64_t opt_clock_offset;
extern uint64_t opt_clock_offset_ns;
extern int babeltrace_ctf_console_output;

#endif
