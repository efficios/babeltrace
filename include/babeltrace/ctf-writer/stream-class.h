#ifndef BABELTRACE_CTF_WRITER_STREAM_CLASS_H
#define BABELTRACE_CTF_WRITER_STREAM_CLASS_H

/*
 * BabelTrace - CTF Writer: Stream Class
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

#include <babeltrace/ctf-ir/stream-class.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * bt_ctf_stream_class_set_clock: assign a clock to a stream class.
 *
 * Assign a clock to a stream class. This clock will be sampled each time an
 * event is appended to an instance of this stream class.
 *
 * @param stream_class Stream class.
 * @param clock Clock to assign to the provided stream class.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_stream_class_set_clock(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_stream_class_get and bt_ctf_stream_class_put: increment and
 * decrement the stream class' reference count.
 *
 * You may also use bt_ctf_get() and bt_ctf_put() with stream class objects.
 *
 * These functions ensure that the stream class won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a stream class.
 *
 * When the stream class' reference count is decremented to 0 by a
 * bt_ctf_stream_class_put, the stream class is freed.
 *
 * @param stream_class Stream class.
 */
extern void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class);
extern void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_STREAM_CLASS_H */
