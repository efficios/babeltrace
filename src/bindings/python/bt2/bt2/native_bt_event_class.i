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

/* Output argument typemap for initialized event class log level output
 * parameter (always appends).
 */
%typemap(in, numinputs=0)
	(bt_event_class_log_level *OUT)
	(bt_event_class_log_level temp = -1) {
	$1 = &temp;
}

%typemap(argout) bt_event_class_log_level *OUT {
	/* SWIG_Python_AppendOutput() steals the created object */
	$result = SWIG_Python_AppendOutput($result, SWIG_From_int(*$1));
}

/* From event-class-const.h */

typedef enum bt_event_class_status {
	BT_EVENT_CLASS_STATUS_OK = 0,
	BT_EVENT_CLASS_STATUS_NOMEM = -12,
} bt_event_class_status;

typedef enum bt_event_class_log_level {
	BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY,
	BT_EVENT_CLASS_LOG_LEVEL_ALERT,
	BT_EVENT_CLASS_LOG_LEVEL_CRITICAL,
	BT_EVENT_CLASS_LOG_LEVEL_ERROR,
	BT_EVENT_CLASS_LOG_LEVEL_WARNING,
	BT_EVENT_CLASS_LOG_LEVEL_NOTICE,
	BT_EVENT_CLASS_LOG_LEVEL_INFO,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE,
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG,
} bt_event_class_log_level;

extern const bt_stream_class *bt_event_class_borrow_stream_class_const(
		const bt_event_class *event_class);

extern const char *bt_event_class_get_name(const bt_event_class *event_class);

extern uint64_t bt_event_class_get_id(const bt_event_class *event_class);

extern bt_property_availability bt_event_class_get_log_level(
		const bt_event_class *event_class,
		bt_event_class_log_level *OUT);

extern const char *bt_event_class_get_emf_uri(
		const bt_event_class *event_class);

extern const bt_field_class *
bt_event_class_borrow_specific_context_field_class_const(
		const bt_event_class *event_class);

extern const bt_field_class *bt_event_class_borrow_payload_field_class_const(
		const bt_event_class *event_class);

extern void bt_event_class_get_ref(const bt_event_class *event_class);

extern void bt_event_class_put_ref(const bt_event_class *event_class);

/* From event-class.h */

extern bt_event_class *bt_event_class_create(
		bt_stream_class *stream_class);

extern bt_event_class *bt_event_class_create_with_id(
		bt_stream_class *stream_class, uint64_t id);

extern bt_stream_class *bt_event_class_borrow_stream_class(
		bt_event_class *event_class);

extern bt_event_class_status bt_event_class_set_name(
		bt_event_class *event_class, const char *name);

extern void bt_event_class_set_log_level(bt_event_class *event_class,
		bt_event_class_log_level log_level);

extern bt_event_class_status bt_event_class_set_emf_uri(
		bt_event_class *event_class, const char *emf_uri);

extern bt_event_class_status
bt_event_class_set_specific_context_field_class(bt_event_class *event_class,
		bt_field_class *field_class);

extern bt_field_class *
bt_event_class_borrow_specific_context_field_class(bt_event_class *event_class);

extern bt_event_class_status bt_event_class_set_payload_field_class(
		bt_event_class *event_class,
		bt_field_class *field_class);

extern bt_field_class *bt_event_class_borrow_payload_field_class(
		bt_event_class *event_class);
