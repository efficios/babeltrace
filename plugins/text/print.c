/*
 * print.c
 *
 * Babeltrace CTF Text Output Plugin Event Printing
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock.h>
#include "text.h"

struct timestamp {
	int64_t real_timestamp;	/* Relative to UNIX epoch. */
	uint64_t clock_value;	/* In cycles. */
};

static
void print_timestamp_cycles(struct text_component *text,
		struct bt_ctf_clock *clock,
		struct bt_ctf_event *event)
{
	fputs("00000000000000000000", text->out);
}

static
void print_timestamp_wall(struct text_component *text,
		struct bt_ctf_clock *clock,
		struct bt_ctf_event *event)
{
	fputs("??:??:??.?????????", text->out);
}

static
enum bt_component_status get_event_timestamp(struct bt_ctf_event *event)
{
/*	int ret;
	uint64_t value, frequency;
	int64_t offset_s, offset;
*/
	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status print_event_timestamp(struct text_component *text,
		struct bt_ctf_event *event)
{
	bool print_names = text->options.print_header_field_names;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_ctf_stream *stream;
	struct bt_ctf_clock *clock = NULL;
	FILE *out = text->out;
	FILE *err = text->err;
	uint64_t real_timestamp;

	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	clock = bt_ctf_event_get_clock(event);
	if (!clock) {
		/* Stream has no timestamp. */
		//puts("no_timestamp!");
		//goto end;
	}

	fputs(print_names ? "timestamp = " : "[", out);
	if (text->options.print_timestamp_cycles) {
		print_timestamp_cycles(text, clock, event);
	} else {
		print_timestamp_wall(text, clock, event);
	}

	fputs(print_names ? ", " : "] ", out);
	if (!text->options.print_delta_field) {
		goto end;
	}
end:
	bt_put(stream);
	bt_put(clock);
	return ret;
}

static inline
enum bt_component_status print_event_header(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret;
	struct bt_ctf_event_class *event_class = bt_ctf_event_get_class(event);

	ret = print_event_timestamp(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	fputs(bt_ctf_event_class_get_name(event_class), text->out);
	bt_put(event_class);
end:
	return ret;
}

BT_HIDDEN
enum bt_component_status text_print_event(struct text_component *text,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret;

	ret = print_event_header(text, event);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	fputc('\n', text->out);
end:
	return ret;
}
