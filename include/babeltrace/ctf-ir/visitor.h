#ifndef BABELTRACE_CTF_IR_VISITOR_H
#define BABELTRACE_CTF_IR_VISITOR_H

/*
 * BabelTrace - CTF IR: Visitor
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_ir_element;

enum bt_ctf_ir_type {
	BT_CTF_IR_TYPE_UNKNOWN = -1,
	BT_CTF_IR_TYPE_TRACE = 0,
	BT_CTF_IR_TYPE_STREAM_CLASS = 1,
	BT_CTF_IR_TYPE_STREAM = 2,
	BT_CTF_IR_TYPE_EVENT_CLASS = 3,
	BT_CTF_IR_TYPE_EVENT = 4,
	BT_CTF_IR_TYPE_NR,
};

typedef int (*bt_ctf_ir_visitor)(struct bt_ctf_ir_element *element,
		void *data);

/*
 * bt_ctf_ir_element_get_type: get an IR element's type.
 *
 * Get an IR element's type.
 *
 * @param element Element instance.
 *
 * Returns one of #bt_ctf_ir_type.
 */
enum bt_ctf_ir_type bt_ctf_ir_element_get_type(
		struct bt_ctf_ir_element *element);

/*
 * bt_ctf_ir_element_get_element: get an IR element's value.
 *
 * Get an IR element's value.
 *
 * @param element Element instance.
 *
 * Returns a CTF-IR type. Use #bt_ctf_ir_type to determine the
 * concrete type of the value returned.
 */
void *bt_ctf_ir_element_get_element(struct bt_ctf_ir_element *element);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_VISITOR_H */
