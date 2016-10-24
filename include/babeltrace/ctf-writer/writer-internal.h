#ifndef BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H
#define BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Writer internal
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 */

#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <dirent.h>
#include <sys/types.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/object-internal.h>

struct bt_ctf_writer {
	struct bt_object base;
	int frozen; /* Protects attributes that can't be changed mid-trace */
	struct bt_ctf_trace *trace;
	GString *path;
	int trace_dir_fd;
	int metadata_fd;
};

BT_HIDDEN
void bt_ctf_writer_freeze(struct bt_ctf_writer *writer);

/*
 * bt_ctf_writer_get_trace: Get a writer's associated trace.
 *
 * @param writer Writer instance.
 *
 * Return the writer's associated instance, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_trace *bt_ctf_writer_get_trace(struct bt_ctf_writer *writer);

#endif /* BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H */
