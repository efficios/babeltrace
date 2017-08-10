/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Type */
struct bt_ctf_packet;

/* Functions */
struct bt_ctf_packet *bt_ctf_packet_create(
		struct bt_ctf_stream *stream);
struct bt_ctf_stream *bt_ctf_packet_get_stream(
		struct bt_ctf_packet *packet);
struct bt_ctf_field *bt_ctf_packet_get_header(
		struct bt_ctf_packet *packet);
int bt_ctf_packet_set_header(
		struct bt_ctf_packet *packet, struct bt_ctf_field *header);
struct bt_ctf_field *bt_ctf_packet_get_context(
		struct bt_ctf_packet *packet);
int bt_ctf_packet_set_context(
		struct bt_ctf_packet *packet, struct bt_ctf_field *context);
