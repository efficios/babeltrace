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
struct bt_ctf_event_class;

/* Log levels */
enum bt_ctf_event_class_log_level {
	BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN = -1,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED = 255,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_EMERGENCY = 0,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_ALERT = 1,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_CRITICAL = 2,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_ERROR = 3,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_WARNING = 4,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_NOTICE = 5,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO = 6,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM = 7,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM = 8,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS = 9,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE = 10,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT = 11,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION = 12,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE = 13,
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG = 14,
};

/* Functions */
struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);
struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class);
const char *bt_ctf_event_class_get_name(
		struct bt_ctf_event_class *event_class);
int64_t bt_ctf_event_class_get_id(
		struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_id(
		struct bt_ctf_event_class *event_class, uint64_t id);
enum bt_ctf_event_class_log_level bt_ctf_event_class_get_log_level(
		struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_log_level(
		struct bt_ctf_event_class *event_class,
		enum bt_ctf_event_class_log_level log_level);
const char *bt_ctf_event_class_get_emf_uri(
		struct bt_ctf_event_class *event_class);
int bt_ctf_event_class_set_emf_uri(
		struct bt_ctf_event_class *event_class,
		const char *emf_uri);
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
