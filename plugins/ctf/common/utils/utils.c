/*
 * Babeltrace - CTF Utils
 *
 * Copyright (c) 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-UTILS"
#include "logging.h"

#include "utils.h"

struct bt_stream_class *ctf_utils_stream_class_from_packet_header(
		struct bt_trace *trace,
		struct bt_field *packet_header_field)
{
	struct bt_field *stream_id_field = NULL;
	struct bt_stream_class *stream_class = NULL;
	uint64_t stream_id = -1ULL;
	int ret;

	if (!packet_header_field) {
		goto single_stream_class;
	}

	stream_id_field = bt_field_structure_get_field_by_name(
		packet_header_field, "stream_id");
	if (!stream_id_field) {
		goto single_stream_class;
	}

	ret = bt_field_integer_unsigned_get_value(stream_id_field,
		&stream_id);
	if (ret) {
		stream_id = -1ULL;
	}

	if (stream_id == -1ULL) {
single_stream_class:
		/* Single stream class */
		if (bt_trace_get_stream_class_count(trace) == 0) {
			goto end;
		}

		stream_class = bt_trace_get_stream_class_by_index(trace, 0);
	} else {
		stream_class = bt_trace_get_stream_class_by_id(trace,
			stream_id);
	}

end:
	bt_put(stream_id_field);
	return stream_class;
}
