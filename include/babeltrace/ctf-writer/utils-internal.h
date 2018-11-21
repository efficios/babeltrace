#ifndef BABELTRACE_CTF_WRITER_UTILS_INTERNAL_H
#define BABELTRACE_CTF_WRITER_UTILS_INTERNAL_H

/*
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/field-path-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <stdint.h>

#define BT_CTF_TO_COMMON(_obj)		(&(_obj)->common)
#define BT_CTF_FROM_COMMON(_obj)	((void *) _obj)

struct bt_ctf_search_query {
	gpointer value;
	int found;
};

BT_HIDDEN
const char *bt_ctf_get_byte_order_string(enum bt_ctf_byte_order byte_order);

static inline
const char *bt_ctf_field_type_id_string(enum bt_ctf_field_type_id type_id)
{
	switch (type_id) {
	case BT_CTF_FIELD_TYPE_ID_UNKNOWN:
		return "BT_CTF_FIELD_TYPE_ID_UNKNOWN";
	case BT_CTF_FIELD_TYPE_ID_INTEGER:
		return "BT_CTF_FIELD_TYPE_ID_INTEGER";
	case BT_CTF_FIELD_TYPE_ID_FLOAT:
		return "BT_CTF_FIELD_TYPE_ID_FLOAT";
	case BT_CTF_FIELD_TYPE_ID_ENUM:
		return "BT_CTF_FIELD_TYPE_ID_ENUM";
	case BT_CTF_FIELD_TYPE_ID_STRING:
		return "BT_CTF_FIELD_TYPE_ID_STRING";
	case BT_CTF_FIELD_TYPE_ID_STRUCT:
		return "BT_CTF_FIELD_TYPE_ID_STRUCT";
	case BT_CTF_FIELD_TYPE_ID_ARRAY:
		return "BT_CTF_FIELD_TYPE_ID_ARRAY";
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		return "BT_CTF_FIELD_TYPE_ID_SEQUENCE";
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		return "BT_CTF_FIELD_TYPE_ID_VARIANT";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_ctf_byte_order_string(enum bt_ctf_byte_order bo)
{
	switch (bo) {
	case BT_CTF_BYTE_ORDER_UNKNOWN:
		return "BT_CTF_BYTE_ORDER_UNKNOWN";
	case BT_CTF_BYTE_ORDER_UNSPECIFIED:
		return "BT_CTF_BYTE_ORDER_UNSPECIFIED";
	case BT_CTF_BYTE_ORDER_NATIVE:
		return "BT_CTF_BYTE_ORDER_NATIVE";
	case BT_CTF_BYTE_ORDER_LITTLE_ENDIAN:
		return "BT_CTF_BYTE_ORDER_LITTLE_ENDIAN";
	case BT_CTF_BYTE_ORDER_BIG_ENDIAN:
		return "BT_CTF_BYTE_ORDER_BIG_ENDIAN";
	case BT_CTF_BYTE_ORDER_NETWORK:
		return "BT_CTF_BYTE_ORDER_NETWORK";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_ctf_string_encoding_string(enum bt_ctf_string_encoding encoding)
{
	switch (encoding) {
	case BT_CTF_STRING_ENCODING_UNKNOWN:
		return "BT_CTF_STRING_ENCODING_UNKNOWN";
	case BT_CTF_STRING_ENCODING_NONE:
		return "BT_CTF_STRING_ENCODING_NONE";
	case BT_CTF_STRING_ENCODING_UTF8:
		return "BT_CTF_STRING_ENCODING_UTF8";
	case BT_CTF_STRING_ENCODING_ASCII:
		return "BT_CTF_STRING_ENCODING_ASCII";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_ctf_integer_base_string(enum bt_ctf_integer_base base)
{
	switch (base) {
	case BT_CTF_INTEGER_BASE_UNKNOWN:
		return "BT_CTF_INTEGER_BASE_UNKNOWN";
	case BT_CTF_INTEGER_BASE_UNSPECIFIED:
		return "BT_CTF_INTEGER_BASE_UNSPECIFIED";
	case BT_CTF_INTEGER_BASE_BINARY:
		return "BT_CTF_INTEGER_BASE_BINARY";
	case BT_CTF_INTEGER_BASE_OCTAL:
		return "BT_CTF_INTEGER_BASE_OCTAL";
	case BT_CTF_INTEGER_BASE_DECIMAL:
		return "BT_CTF_INTEGER_BASE_DECIMAL";
	case BT_CTF_INTEGER_BASE_HEXADECIMAL:
		return "BT_CTF_INTEGER_BASE_HEXADECIMAL";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_ctf_scope_string(enum bt_ctf_scope scope)
{
	switch (scope) {
	case BT_CTF_SCOPE_UNKNOWN:
		return "BT_CTF_SCOPE_UNKNOWN";
	case BT_CTF_SCOPE_TRACE_PACKET_HEADER:
		return "BT_CTF_SCOPE_TRACE_PACKET_HEADER";
	case BT_CTF_SCOPE_STREAM_PACKET_CONTEXT:
		return "BT_CTF_SCOPE_STREAM_PACKET_CONTEXT";
	case BT_CTF_SCOPE_STREAM_EVENT_HEADER:
		return "BT_CTF_SCOPE_STREAM_EVENT_HEADER";
	case BT_CTF_SCOPE_STREAM_EVENT_CONTEXT:
		return "BT_CTF_SCOPE_STREAM_EVENT_CONTEXT";
	case BT_CTF_SCOPE_EVENT_CONTEXT:
		return "BT_CTF_SCOPE_EVENT_CONTEXT";
	case BT_CTF_SCOPE_EVENT_PAYLOAD:
		return "BT_CTF_SCOPE_EVENT_PAYLOAD";
	case BT_CTF_SCOPE_ENV:
		return "BT_CTF_SCOPE_ENV";
	default:
		return "(unknown)";
	}
}

static inline
const char *bt_ctf_event_class_log_level_string(
		enum bt_ctf_event_class_log_level level)
{
	switch (level) {
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_EMERGENCY";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_ALERT:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_ALERT";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_CRITICAL:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_CRITICAL";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_ERROR:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_ERROR";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_WARNING:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_WARNING";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_NOTICE:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_NOTICE";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE";
	case BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG:
		return "BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG";
	default:
		return "(unknown)";
	}
};

static inline
GString *bt_ctf_field_path_string(struct bt_ctf_field_path *path)
{
	GString *str = g_string_new(NULL);
	size_t i;

	BT_ASSERT(path);

	if (!str) {
		goto end;
	}

	g_string_append_printf(str, "[%s", bt_common_scope_string(
		bt_ctf_field_path_get_root_scope(path)));

	for (i = 0; i < bt_ctf_field_path_get_index_count(path); i++) {
		int index = bt_ctf_field_path_get_index(path, i);

		g_string_append_printf(str, ", %d", index);
	}

	g_string_append(str, "]");

end:
	return str;
}

#endif /* BABELTRACE_CTF_WRITER_UTILS_INTERNAL_H */
