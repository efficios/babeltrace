#ifndef BABELTRACE_CTF_WRITER_TRACE_INTERNAL_H
#define BABELTRACE_CTF_WRITER_TRACE_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Trace
 *
 * Copyright 2014 EfficiOS Inc.
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/ctf-ir/trace-internal.h>

struct bt_ctf_trace {
	struct bt_trace_common common;
};

/*
 * bt_ctf_trace_get_metadata_string: get metadata string.
 *
 * Get the trace's TSDL metadata. The caller assumes the ownership of the
 * returned string.
 *
 * @param trace Trace instance.
 *
 * Returns the metadata string on success, NULL on error.
 */
BT_HIDDEN
char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace);

BT_HIDDEN
struct bt_ctf_trace *bt_ctf_trace_create(void);

#endif /* BABELTRACE_CTF_WRITER_TRACE_INTERNAL_H */
