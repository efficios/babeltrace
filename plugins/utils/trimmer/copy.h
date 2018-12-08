#ifndef BABELTRACE_PLUGIN_TRIMMER_COPY_H
#define BABELTRACE_PLUGIN_TRIMMER_COPY_H

/*
 * BabelTrace - Copy Trace Structure
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

BT_HIDDEN
const bt_event *trimmer_output_event(struct trimmer_iterator *trim_it,
		const bt_event *event);
BT_HIDDEN
const bt_packet *trimmer_new_packet(struct trimmer_iterator *trim_it,
		const bt_packet *packet);
BT_HIDDEN
const bt_packet *trimmer_close_packet(struct trimmer_iterator *trim_it,
		const bt_packet *packet);
BT_HIDDEN
enum bt_component_status update_packet_context_field(FILE *err,
		const bt_packet *writer_packet,
		const char *name, int64_t value);

#endif /* BABELTRACE_PLUGIN_TRIMMER_COPY_H */
