/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
struct bt_trace;

/* Functions */
struct bt_trace *bt_trace_create(void);
const char *bt_trace_get_name(struct bt_trace *trace_class);
int bt_trace_set_name(struct bt_trace *trace_class,
		const char *name);
enum bt_byte_order bt_trace_get_native_byte_order(
		struct bt_trace *trace_class);
int bt_trace_set_native_byte_order(struct bt_trace *trace_class,
		enum bt_byte_order native_byte_order);
BTUUID bt_trace_get_uuid(
		struct bt_trace *trace_class);
int bt_trace_set_uuid(struct bt_trace *trace_class,
		BTUUID uuid);
int64_t bt_trace_get_environment_field_count(
		struct bt_trace *trace_class);
const char *
bt_trace_get_environment_field_name_by_index(
		struct bt_trace *trace_class, uint64_t index);
struct bt_value *
bt_trace_get_environment_field_value_by_index(struct bt_trace *trace_class,
		uint64_t index);
struct bt_value *
bt_trace_get_environment_field_value_by_name(
		struct bt_trace *trace_class, const char *name);
int bt_trace_set_environment_field(
		struct bt_trace *trace_class, const char *name,
		struct bt_value *value);
struct bt_field_type *bt_trace_get_packet_header_type(
		struct bt_trace *trace_class);
int bt_trace_set_packet_header_type(struct bt_trace *trace_class,
		struct bt_field_type *packet_header_type);
int64_t bt_trace_get_clock_class_count(
		struct bt_trace *trace_class);
struct bt_clock_class *bt_trace_get_clock_class_by_index(
		struct bt_trace *trace_class, uint64_t index);
struct bt_clock_class *bt_trace_get_clock_class_by_name(
		struct bt_trace *trace_class, const char *name);
int bt_trace_add_clock_class(struct bt_trace *trace_class,
		struct bt_clock_class *clock_class);
int64_t bt_trace_get_stream_class_count(
		struct bt_trace *trace_class);
struct bt_stream_class *bt_trace_get_stream_class_by_index(
		struct bt_trace *trace_class, uint64_t index);
struct bt_stream_class *bt_trace_get_stream_class_by_id(
		struct bt_trace *trace_class, uint64_t id);
int bt_trace_add_stream_class(struct bt_trace *trace_class,
		struct bt_stream_class *stream_class);
int64_t bt_trace_get_stream_count(struct bt_trace *trace_class);
struct bt_stream *bt_trace_get_stream_by_index(
		struct bt_trace *trace_class, uint64_t index);
int bt_trace_is_static(struct bt_trace *trace_class);
int bt_trace_set_is_static(struct bt_trace *trace_class);

/* Helper functions for Python */
%{
void trace_is_static_listener(struct bt_trace *trace, void *py_callable)
{
	PyObject *py_trace_ptr = NULL;
	PyObject *py_res = NULL;

	py_trace_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(trace),
		SWIGTYPE_p_bt_trace, 0);
	if (!py_trace_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_trace_ptr);
	BT_ASSERT(py_res == Py_None);
	Py_DECREF(py_trace_ptr);
	Py_DECREF(py_res);
}

void trace_listener_removed(struct bt_trace *trace, void *py_callable)
{
	BT_ASSERT(py_callable);
	Py_DECREF(py_callable);
}

static int bt_py3_trace_add_is_static_listener(unsigned long long trace_addr,
		PyObject *py_callable)
{
	struct bt_trace *trace = (void *) trace_addr;
	int ret = 0;

	BT_ASSERT(trace);
	BT_ASSERT(py_callable);
	ret = bt_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, py_callable);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	}

	return ret;
}
%}

int bt_py3_trace_add_is_static_listener(unsigned long long trace_addr,
		PyObject *py_callable);
