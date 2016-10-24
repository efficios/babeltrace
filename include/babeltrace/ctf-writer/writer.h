#ifndef BABELTRACE_CTF_WRITER_WRITER_H
#define BABELTRACE_CTF_WRITER_WRITER_H

/*
 * BabelTrace - CTF Writer: Writer
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/ctf-ir/field-types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_writer;
struct bt_ctf_stream;
struct bt_ctf_stream_class;
struct bt_ctf_clock;

/*
 * bt_ctf_writer_create: create a writer instance.
 *
 * Allocate a new writer that will produce a trace in the given path.
 * The creation of a writer sets its reference count to 1.
 *
 * @param path Path to the trace's containing folder (string is copied).
 *
 * Returns an allocated writer on success, NULL on error.
 */
extern struct bt_ctf_writer *bt_ctf_writer_create(const char *path);

/*
 * bt_ctf_writer_create_stream: create a stream instance.
 *
 * Allocate a new stream instance and register it to the writer. The creation of
 * a stream sets its reference count to 1.
 *
 * @param writer Writer instance.
 * @param stream_class Stream class to instantiate.
 *
 * Returns an allocated stream on success, NULL on error.
 */
extern struct bt_ctf_stream *bt_ctf_writer_create_stream(
		struct bt_ctf_writer *writer,
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_writer_add_environment_field: add an environment field to the trace.
 *
 * Add an environment field to the trace. The name and value parameters are
 * copied.
 *
 * @param writer Writer instance.
 * @param name Name of the environment field (will be copied).
 * @param value Value of the environment field (will be copied).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_writer_add_environment_field(struct bt_ctf_writer *writer,
		const char *name,
		const char *value);

/*
 * bt_ctf_writer_add_clock: add a clock to the trace.
 *
 * Add a clock to the trace. Clocks assigned to stream classes must be
 * registered to the writer.
 *
 * @param writer Writer instance.
 * @param clock Clock to add to the trace.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_writer_get_metadata_string: get meta-data string.
 *
 * Get the trace's TSDL meta-data. The caller assumes the ownership of the
 * returned string.
 *
 * @param writer Writer instance.
 *
 * Returns the metadata string on success, NULL on error.
 */
extern char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer);

/*
 * bt_ctf_writer_flush_metadata: flush the trace's metadata to disk.
 *
 * Flush the trace's metadata to the metadata file. Note that the metadata will
 * be flushed automatically when the Writer instance is released (last call to
 * bt_ctf_writer_put).
 *
 * @param writer Writer instance.
 */
extern void bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer);

/*
 * bt_ctf_writer_set_byte_order: set a field type's byte order.
 *
 * Set the trace's byte order. Defaults to the host machine's endianness.
 *
 * @param writer Writer instance.
 * @param byte_order Trace's byte order.
 *
 * Returns 0 on success, a negative value on error.
 *
 * Note: byte_order must not be BT_CTF_BYTE_ORDER_NATIVE since, according
 * to the CTF specification, is defined as "the byte order described in the
 * trace description".
 */
extern int bt_ctf_writer_set_byte_order(struct bt_ctf_writer *writer,
		enum bt_ctf_byte_order byte_order);

/*
 * bt_ctf_writer_get and bt_ctf_writer_put: increment and decrement the
 * writer's reference count.
 *
 * You may also use bt_ctf_get() and bt_ctf_put() with writer objects.
 *
 * These functions ensure that the writer won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a writer.
 *
 * When the writer's reference count is decremented to 0 by a
 * bt_ctf_writer_put, the writer is freed.
 *
 * @param writer Writer instance.
 */
extern void bt_ctf_writer_get(struct bt_ctf_writer *writer);
extern void bt_ctf_writer_put(struct bt_ctf_writer *writer);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_WRITER_H */
