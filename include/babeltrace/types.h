#ifndef BABELTRACE_TYPES_H
#define BABELTRACE_TYPES_H

/*
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctypes Babeltrace C types
@ingroup apiref
@brief Babeltrace C types.

@code
#include <babeltrace/types.h>
@endcode

This header contains custom type definitions used across the library.

@file
@brief Babeltrace C types.
@sa ctypes

@addtogroup ctypes
@{
*/

/// False boolean value for the #bt_bool type.
#define BT_FALSE	0

/// True boolean value for the #bt_bool type.
#define BT_TRUE		1

/**
@brief	Babeltrace's boolean type.

Use only the #BT_FALSE and #BT_TRUE definitions for #bt_bool parameters.
It is guaranteed that the library functions which return a #bt_bool
value return either #BT_FALSE or #BT_TRUE.

You can always test the truthness of a #bt_bool value directly, without
comparing it to #BT_TRUE directly:

@code
bt_bool ret = bt_some_function(...);

if (ret) {
	// ret is true
}
@endcode
*/
typedef int bt_bool;

typedef const uint8_t *bt_uuid;

typedef struct bt_clock_class bt_clock_class;
typedef struct bt_clock_value bt_clock_value;
typedef struct bt_component bt_component;
typedef struct bt_component_class bt_component_class;
typedef struct bt_component_class_filter bt_component_class_filter;
typedef struct bt_component_class_sink bt_component_class_sink;
typedef struct bt_component_class_source bt_component_class_source;
typedef struct bt_component_filter bt_component_filter;
typedef struct bt_component_graph bt_component_graph;
typedef struct bt_component_sink bt_component_sink;
typedef struct bt_component_source bt_component_source;
typedef struct bt_connection bt_connection;
typedef struct bt_event bt_event;
typedef struct bt_event_class bt_event_class;
typedef struct bt_event_header_field bt_event_header_field;
typedef struct bt_field bt_field;
typedef struct bt_field_class bt_field_class;
typedef struct bt_field_class_signed_enumeration_mapping_ranges bt_field_class_signed_enumeration_mapping_ranges;
typedef struct bt_field_class_unsigned_enumeration_mapping_ranges bt_field_class_unsigned_enumeration_mapping_ranges;
typedef struct bt_field_path bt_field_path;
typedef struct bt_graph bt_graph;
typedef struct bt_notification bt_notification;
typedef struct bt_notification_iterator bt_notification_iterator;
typedef struct bt_object bt_object;
typedef struct bt_packet bt_packet;
typedef struct bt_packet_context_field bt_packet_context_field;
typedef struct bt_packet_header_field bt_packet_header_field;
typedef struct bt_plugin bt_plugin;
typedef struct bt_plugin_set bt_plugin_set;
typedef struct bt_plugin_so_shared_lib_handle bt_plugin_so_shared_lib_handle;
typedef struct bt_port bt_port;
typedef struct bt_port_input bt_port_input;
typedef struct bt_port_output bt_port_output;
typedef struct bt_port_output_notification_iterator bt_port_output_notification_iterator;
typedef struct bt_query_executor bt_query_executor;
typedef struct bt_self_component bt_self_component;
typedef struct bt_self_component_class_filter bt_self_component_class_filter;
typedef struct bt_self_component_class_sink bt_self_component_class_sink;
typedef struct bt_self_component_class_source bt_self_component_class_source;
typedef struct bt_self_component_filter bt_self_component_filter;
typedef struct bt_self_component_port bt_self_component_port;
typedef struct bt_self_component_port_input bt_self_component_port_input;
typedef struct bt_self_component_port_input_notification_iterator bt_self_component_port_input_notification_iterator;
typedef struct bt_self_component_port_output bt_self_component_port_output;
typedef struct bt_self_component_sink bt_self_component_sink;
typedef struct bt_self_component_source bt_self_component_source;
typedef struct bt_self_notification_iterator bt_self_notification_iterator;
typedef struct bt_self_port_input bt_self_port_input;
typedef struct bt_self_port_output bt_self_port_output;
typedef struct bt_self_port bt_self_port;
typedef struct bt_stream bt_stream;
typedef struct bt_stream_class bt_stream_class;
typedef struct bt_trace bt_trace;
typedef struct bt_trace_class bt_trace_class;
typedef struct bt_value bt_value;

typedef const char * const *bt_field_class_enumeration_mapping_label_array;
typedef const struct bt_notification **bt_notification_array_const;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TYPES_H */
