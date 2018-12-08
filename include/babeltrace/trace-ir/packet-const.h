#ifndef BABELTRACE_TRACE_IR_PACKET_CONST_H
#define BABELTRACE_TRACE_IR_PACKET_CONST_H

/*
 * Copyright 2016-2018 Philippe Proulx <pproulx@efficios.com>
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

/* For enum bt_property_availability */
#include <babeltrace/property.h>

/* For enum bt_clock_value_status */
#include <babeltrace/trace-ir/clock-value-const.h>

/*
 * For bt_packet, bt_packet_header_field, bt_packet_context_field,
 * bt_stream, bt_clock_value
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const bt_stream *bt_packet_borrow_stream_const(
		const bt_packet *packet);

extern
const bt_field *bt_packet_borrow_header_field_const(
		const bt_packet *packet);

extern
const bt_field *bt_packet_borrow_context_field_const(
		const bt_packet *packet);

extern
enum bt_clock_value_status bt_packet_borrow_default_beginning_clock_value_const(
		const bt_packet *packet,
		const bt_clock_value **clock_value);

extern
enum bt_clock_value_status bt_packet_borrow_default_end_clock_valeu_const(
		const bt_packet *packet,
		const bt_clock_value **clock_value);

extern
enum bt_property_availability bt_packet_get_discarded_event_counter_snapshot(
		const bt_packet *packet, uint64_t *value);

extern
enum bt_property_availability bt_packet_get_packet_counter_snapshot(
		const bt_packet *packet, uint64_t *value);

extern void bt_packet_get_ref(const bt_packet *packet);

extern void bt_packet_put_ref(const bt_packet *packet);

#define BT_PACKET_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_packet_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_PACKET_MOVE_REF(_var_dst, _var_src)		\
	do {						\
		bt_packet_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_PACKET_CONST_H */
