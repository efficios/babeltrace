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
struct bt_ctf_trace;

/* Functions */
struct bt_ctf_trace *bt_ctf_trace_create(void);
const char *bt_ctf_trace_get_name(struct bt_ctf_trace *trace_class);
int bt_ctf_trace_set_name(struct bt_ctf_trace *trace_class,
		const char *name);
enum bt_ctf_byte_order bt_ctf_trace_get_native_byte_order(
		struct bt_ctf_trace *trace_class);
int bt_ctf_trace_set_native_byte_order(struct bt_ctf_trace *trace_class,
		enum bt_ctf_byte_order native_byte_order);
BTUUID bt_ctf_trace_get_uuid(
		struct bt_ctf_trace *trace_class);
int bt_ctf_trace_set_uuid(struct bt_ctf_trace *trace_class,
		BTUUID uuid);
int64_t bt_ctf_trace_get_environment_field_count(
		struct bt_ctf_trace *trace_class);
const char *
bt_ctf_trace_get_environment_field_name_by_index(
		struct bt_ctf_trace *trace_class, uint64_t index);
struct bt_value *
bt_ctf_trace_get_environment_field_value_by_index(struct bt_ctf_trace *trace_class,
		uint64_t index);
struct bt_value *
bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace_class, const char *name);
int bt_ctf_trace_set_environment_field(
		struct bt_ctf_trace *trace_class, const char *name,
		struct bt_value *value);
struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
		struct bt_ctf_trace *trace_class);
int bt_ctf_trace_set_packet_header_type(struct bt_ctf_trace *trace_class,
		struct bt_ctf_field_type *packet_header_type);
int64_t bt_ctf_trace_get_clock_class_count(
		struct bt_ctf_trace *trace_class);
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_index(
		struct bt_ctf_trace *trace_class, uint64_t index);
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_name(
		struct bt_ctf_trace *trace_class, const char *name);
int bt_ctf_trace_add_clock_class(struct bt_ctf_trace *trace_class,
		struct bt_ctf_clock_class *clock_class);
int64_t bt_ctf_trace_get_stream_class_count(
		struct bt_ctf_trace *trace_class);
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_index(
		struct bt_ctf_trace *trace_class, uint64_t index);
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace_class, uint64_t id);
int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace_class,
		struct bt_ctf_stream_class *stream_class);
int64_t bt_ctf_trace_get_stream_count(struct bt_ctf_trace *trace_class);
struct bt_ctf_stream *bt_ctf_trace_get_stream_by_index(
		struct bt_ctf_trace *trace_class, uint64_t index);
int bt_ctf_trace_is_static(struct bt_ctf_trace *trace_class);
int bt_ctf_trace_set_is_static(struct bt_ctf_trace *trace_class);

/* Helper functions for Python */
%{
void trace_is_static_listener(struct bt_ctf_trace *trace, void *py_callable)
{
	PyObject *py_trace_ptr = NULL;
	PyObject *py_res = NULL;

	py_trace_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(trace),
		SWIGTYPE_p_bt_ctf_trace, 0);
	if (!py_trace_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_trace_ptr);
	assert(py_res == Py_None);
	Py_DECREF(py_trace_ptr);
	Py_DECREF(py_res);
}

void trace_listener_removed(struct bt_ctf_trace *trace, void *py_callable)
{
	assert(py_callable);
	Py_DECREF(py_callable);
}

static int bt_py3_trace_add_is_staitc_listener(unsigned long long trace_addr,
		PyObject *py_callable)
{
	struct bt_ctf_trace *trace = (void *) trace_addr;
	int ret = 0;

	assert(trace);
	assert(py_callable);
	ret = bt_ctf_trace_add_is_static_listener(trace,
		trace_is_static_listener, trace_listener_removed, py_callable);
	if (ret >= 0) {
		Py_INCREF(py_callable);
	}

	return ret;
}
%}

int bt_py3_trace_add_is_staitc_listener(unsigned long long trace_addr,
		PyObject *py_callable);
