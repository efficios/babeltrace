/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <babeltrace2-ctf-writer/writer.h>
#include <babeltrace2-ctf-writer/clock.h>
#include <babeltrace2-ctf-writer/clock-class.h>
#include <babeltrace2-ctf-writer/stream.h>
#include <babeltrace2-ctf-writer/event.h>
#include <babeltrace2-ctf-writer/event-types.h>
#include <babeltrace2-ctf-writer/event-fields.h>
#include <babeltrace2-ctf-writer/stream-class.h>
#include <babeltrace2-ctf-writer/trace.h>

#include "common/assert.h"

struct config {
	struct bt_ctf_writer *writer;
	struct bt_ctf_trace *trace;
	struct bt_ctf_clock *clock;
	struct bt_ctf_stream_class *sc;
	struct bt_ctf_stream *stream;
	struct bt_ctf_event_class *ec;
};

static
void fini_config(struct config *cfg)
{
	bt_ctf_object_put_ref(cfg->stream);
	bt_ctf_object_put_ref(cfg->sc);
	bt_ctf_object_put_ref(cfg->ec);
	bt_ctf_object_put_ref(cfg->clock);
	bt_ctf_object_put_ref(cfg->trace);
	bt_ctf_object_put_ref(cfg->writer);
}

static
void configure_writer(struct config *cfg, const char *path)
{
	struct bt_ctf_field_type *ft;
	int ret;

	cfg->writer = bt_ctf_writer_create(path);
	BT_ASSERT(cfg->writer);
	cfg->trace = bt_ctf_writer_get_trace(cfg->writer);
	BT_ASSERT(cfg->trace);
	cfg->clock = bt_ctf_clock_create("default");
	BT_ASSERT(cfg->clock);
	ret = bt_ctf_writer_add_clock(cfg->writer, cfg->clock);
	BT_ASSERT(ret == 0);
	ret = bt_ctf_writer_set_byte_order(cfg->writer,
		BT_CTF_BYTE_ORDER_BIG_ENDIAN);
	BT_ASSERT(ret == 0);
	cfg->sc = bt_ctf_stream_class_create("hello");
	BT_ASSERT(cfg->sc);
	ret = bt_ctf_stream_class_set_clock(cfg->sc, cfg->clock);
	BT_ASSERT(ret == 0);
	cfg->ec = bt_ctf_event_class_create("ev");
	BT_ASSERT(cfg->ec);
	ft = bt_ctf_field_type_floating_point_create();
	BT_ASSERT(ft);
	ret = bt_ctf_field_type_floating_point_set_exponent_digits(ft, 11);
	BT_ASSERT(ret == 0);
	ret = bt_ctf_field_type_floating_point_set_mantissa_digits(ft, 53);
	BT_ASSERT(ret == 0);
	ret = bt_ctf_event_class_add_field(cfg->ec, ft, "dbl");
	BT_ASSERT(ret == 0);
	bt_ctf_object_put_ref(ft);
	ret = bt_ctf_stream_class_add_event_class(cfg->sc, cfg->ec);
	BT_ASSERT(ret == 0);
	cfg->stream = bt_ctf_writer_create_stream(cfg->writer, cfg->sc);
	BT_ASSERT(cfg->stream);
}

static
void write_stream(struct config *cfg)
{
	struct bt_ctf_event *ev;
	struct bt_ctf_field *field;
	int ret;

	/* Create event and fill fields */
	ev = bt_ctf_event_create(cfg->ec);
	BT_ASSERT(ev);
	field = bt_ctf_event_get_payload(ev, "dbl");
	BT_ASSERT(field);
	ret = bt_ctf_field_floating_point_set_value(field, 17283.3881);
	BT_ASSERT(ret == 0);
	bt_ctf_object_put_ref(field);
	ret = bt_ctf_clock_set_time(cfg->clock, 0);
	BT_ASSERT(ret == 0);

	/* Append event */
	ret = bt_ctf_stream_append_event(cfg->stream, ev);
	BT_ASSERT(ret == 0);
	bt_ctf_object_put_ref(ev);

	/* Create packet */
	ret = bt_ctf_stream_flush(cfg->stream);
	BT_ASSERT(ret == 0);
}

int main(int argc, char **argv)
{
	struct config cfg = {0};

	BT_ASSERT(argc >= 2);
	configure_writer(&cfg, argv[1]);
	write_stream(&cfg);
	fini_config(&cfg);
	return 0;
}
