#ifndef BABELTRACE_CTF_IR_PACKET_H
#define BABELTRACE_CTF_IR_PACKET_H

/*
 * BabelTrace - CTF IR: Stream packet
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_stream;
struct bt_ctf_packet;

BT_HIDDEN
struct bt_ctf_packet *bt_ctf_packet_create(
		struct bt_ctf_stream *stream);

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_packet_get_stream(
		struct bt_ctf_packet *packet);

BT_HIDDEN
struct bt_ctf_field *bt_ctf_packet_get_header(
		struct bt_ctf_packet *packet);

BT_HIDDEN
int bt_ctf_packet_set_header(
		struct bt_ctf_packet *packet, struct bt_ctf_field *header);

BT_HIDDEN
struct bt_ctf_field *bt_ctf_packet_get_context(
		struct bt_ctf_packet *context);

BT_HIDDEN
int bt_ctf_packet_set_context(
		struct bt_ctf_packet *packet, struct bt_ctf_field *context);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_PACKET_H */
