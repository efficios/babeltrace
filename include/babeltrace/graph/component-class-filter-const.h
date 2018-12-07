#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_FILTER_CONST_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_FILTER_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class;
struct bt_component_class_filter;

static inline
const struct bt_component_class *
bt_component_class_filter_as_component_class_const(
		const struct bt_component_class_filter *comp_cls_filter)
{
	return (const void *) comp_cls_filter;
}

extern void bt_component_class_filter_get_ref(
		const struct bt_component_class_filter *component_class_filter);

extern void bt_component_class_filter_put_ref(
		const struct bt_component_class_filter *component_class_filter);

#define BT_COMPONENT_CLASS_FILTER_PUT_REF_AND_RESET(_var)	\
	do {							\
		bt_component_class_filter_put_ref(_var);	\
		(_var) = NULL;					\
	} while (0)

#define BT_COMPONENT_CLASS_FILTER_MOVE_REF(_var_dst, _var_src)	\
	do {							\
		bt_component_class_filter_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);			\
		(_var_src) = NULL;				\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_FILTER_CONST_H */
