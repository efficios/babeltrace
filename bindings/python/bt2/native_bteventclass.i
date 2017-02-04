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

%{
#include <babeltrace/ctf-ir/event-class.h>
%}

/* Type */
struct bt_ctf_event_class;

/* Functions */
struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);
struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class);
const char *bt_ctf_event_class_get_name(
		struct bt_ctf_event_class *event_class);
int64_t bt_ctf_event_class_get_id(
		struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_id(
		struct bt_ctf_event_class *event_class, uint32_t id);
int bt_ctf_event_class_get_attribute_count(
		struct bt_ctf_event_class *event_class);
const char *bt_ctf_event_class_get_attribute_name(
		struct bt_ctf_event_class *event_class, int index);
struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name);
int bt_ctf_event_class_set_attribute(
		struct bt_ctf_event_class *event_class, const char *name,
		struct bt_value *value);
struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context_type);
struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
		struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_payload_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload_type);
