/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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

/* Output argument typemap for self port output (always appends) */
%typemap(in, numinputs=0)
	(bt_self_component_port_input **OUT)
	(bt_self_component_port_input *temp_self_port = NULL) {
	$1 = &temp_self_port;
}

%typemap(argout) bt_self_component_port_input **OUT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_self_component_port_input, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for self port output (always appends) */
%typemap(in, numinputs=0)
	(bt_self_component_port_output **OUT)
	(bt_self_component_port_output *temp_self_port = NULL) {
	$1 = &temp_self_port;
}

%typemap(argout) (bt_self_component_port_output **OUT) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_self_component_port_output, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* From component-const.h */

extern const char *bt_component_get_name(const bt_component *component);

extern const bt_component_class *bt_component_borrow_class_const(
		const bt_component *component);

extern bt_component_class_type bt_component_get_class_type(
		const bt_component *component);

bt_bool bt_component_is_source(const bt_component *component);

bt_bool bt_component_is_filter(const bt_component *component);

bt_bool bt_component_is_sink(const bt_component *component);

extern bt_bool bt_component_graph_is_canceled(
		const bt_component *component);

extern void bt_component_get_ref(const bt_component *component);

extern void bt_component_put_ref(const bt_component *component);

/* From component-source-const.h */

const bt_component *bt_component_source_as_component_const(
		const bt_component_source *component);

extern uint64_t bt_component_source_get_output_port_count(
		const bt_component_source *component);

extern const bt_port_output *
bt_component_source_borrow_output_port_by_name_const(
		const bt_component_source *component, const char *name);

extern const bt_port_output *
bt_component_source_borrow_output_port_by_index_const(
		const bt_component_source *component, uint64_t index);

extern void bt_component_source_get_ref(
		const bt_component_source *component_source);

extern void bt_component_source_put_ref(
		const bt_component_source *component_source);

/* From component-filter-const.h */

const bt_component *bt_component_filter_as_component_const(
		const bt_component_filter *component);

extern uint64_t bt_component_filter_get_input_port_count(
		const bt_component_filter *component);

extern const bt_port_input *
bt_component_filter_borrow_input_port_by_name_const(
		const bt_component_filter *component, const char *name);

extern const bt_port_input *
bt_component_filter_borrow_input_port_by_index_const(
		const bt_component_filter *component, uint64_t index);

extern uint64_t bt_component_filter_get_output_port_count(
		const bt_component_filter *component);

extern const bt_port_output *
bt_component_filter_borrow_output_port_by_name_const(
		const bt_component_filter *component, const char *name);

extern const bt_port_output *
bt_component_filter_borrow_output_port_by_index_const(
		const bt_component_filter *component, uint64_t index);

extern void bt_component_filter_get_ref(
		const bt_component_filter *component_filter);

extern void bt_component_filter_put_ref(
		const bt_component_filter *component_filter);

/* From component-sink-const.h */

const bt_component *bt_component_sink_as_component_const(
		const bt_component_sink *component);

extern uint64_t bt_component_sink_get_input_port_count(
		const bt_component_sink *component);

extern const bt_port_input *
bt_component_sink_borrow_input_port_by_name_const(
		const bt_component_sink *component, const char *name);

extern const bt_port_input *
bt_component_sink_borrow_input_port_by_index_const(
		const bt_component_sink *component, uint64_t index);

extern void bt_component_sink_get_ref(
		const bt_component_sink *component_sink);

extern void bt_component_sink_put_ref(
		const bt_component_sink *component_sink);

/* From self-component.h */

typedef enum bt_self_component_status {
	BT_SELF_COMPONENT_STATUS_OK = 0,
	BT_SELF_COMPONENT_STATUS_END = 1,
	BT_SELF_COMPONENT_STATUS_AGAIN = 11,
	BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION = 111,
	BT_SELF_COMPONENT_STATUS_ERROR = -1,
	BT_SELF_COMPONENT_STATUS_NOMEM = -12,
} bt_self_component_status;

const bt_component *bt_self_component_as_component(
		bt_self_component *self_component);

extern void *bt_self_component_get_data(
		const bt_self_component *self_component);

extern void bt_self_component_set_data(
		bt_self_component *self_component, void *data);

/* From self-component-source.h */

bt_self_component *bt_self_component_source_as_self_component(
		bt_self_component_source *self_comp_source);

const bt_component_source *
bt_self_component_source_as_component_source(
		bt_self_component_source *self_comp_source);

extern bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_name(
		bt_self_component_source *self_component,
		const char *name);

extern bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_index(
		bt_self_component_source *self_component,
		uint64_t index);

extern bt_self_component_status
bt_self_component_source_add_output_port(
		bt_self_component_source *self_component,
		const char *name, void *user_data,
		bt_self_component_port_output **OUT);

/* From self-component-filter.h */

bt_self_component *bt_self_component_filter_as_self_component(
		bt_self_component_filter *self_comp_filter);

const bt_component_filter *
bt_self_component_filter_as_component_filter(
		bt_self_component_filter *self_comp_filter);

extern bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_name(
		bt_self_component_filter *self_component,
		const char *name);

extern bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_index(
		bt_self_component_filter *self_component,
		uint64_t index);

extern bt_self_component_status
bt_self_component_filter_add_output_port(
		bt_self_component_filter *self_component,
		const char *name, void *data,
		bt_self_component_port_output **OUT);

extern bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_name(
		bt_self_component_filter *self_component,
		const char *name);

extern bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_index(
		bt_self_component_filter *self_component,
		uint64_t index);

extern bt_self_component_status
bt_self_component_filter_add_input_port(
		bt_self_component_filter *self_component,
		const char *name, void *data,
		bt_self_component_port_input **OUT);

/* From self-component-sink.h */

bt_self_component *bt_self_component_sink_as_self_component(
		bt_self_component_sink *self_comp_sink);

const bt_component_sink *
bt_self_component_sink_as_component_sink(
		bt_self_component_sink *self_comp_sink);

extern bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_name(
		bt_self_component_sink *self_component,
		const char *name);

extern bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_index(
		bt_self_component_sink *self_component, uint64_t index);

extern bt_self_component_status
bt_self_component_sink_add_input_port(
		bt_self_component_sink *self_component,
		const char *name, void *user_data,
		bt_self_component_port_input **OUT);
