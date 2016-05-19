#ifndef _BABELTRACE_DEBUG_INFO_H
#define _BABELTRACE_DEBUG_INFO_H

/*
 * Babeltrace - Debug information state tracker
 *
 * Copyright (c) 2015 EfficiOS Inc.
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
 * Copyright (c) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

struct debug_info;
struct ctf_event_definition;

#ifdef ENABLE_DEBUG_INFO

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <babeltrace/babeltrace-internal.h>

struct debug_info_source {
	/* Strings are owned by debug_info_source. */
	char *func;
	uint64_t line_no;
	char *src_path;
	/* short_src_path points inside src_path, no need to free. */
	const char *short_src_path;
	char *bin_path;
	/* short_bin_path points inside bin_path, no need to free. */
	const char *short_bin_path;
	/*
	 * Location within the binary. Either absolute (@0x1234) or
	 * relative (+0x4321).
	 */
	char *bin_loc;
};

BT_HIDDEN
struct debug_info *debug_info_create(void);

BT_HIDDEN
void debug_info_destroy(struct debug_info *debug_info);

BT_HIDDEN
void debug_info_handle_event(struct debug_info *debug_info,
		struct ctf_event_definition *event);

#else /* ifdef ENABLE_DEBUG_INFO */

static inline
struct debug_info *debug_info_create(void) { return malloc(1); }

static inline
void debug_info_destroy(struct debug_info *debug_info) { free(debug_info); }

static inline
void debug_info_handle_event(struct debug_info *debug_info,
		struct ctf_event_definition *event) { }

#endif /* ENABLE_DEBUG_INFO */

#endif /* _BABELTRACE_DEBUG_INFO_H */
