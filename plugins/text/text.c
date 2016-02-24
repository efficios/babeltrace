/*
 * text.c
 *
 * Babeltrace CTF Text Output Plugin
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
 */

#include <babeltrace/plugin/plugin-macros.h>
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/sink.h>
#include <babeltrace/plugin/notification/notification.h>
#include <stdio.h>
#include <stdbool.h>

static
enum bt_component_status ctf_text_init(struct bt_component *,
		struct bt_value *params);

/* Initialize plug-in entry points. */
BT_PLUGIN_NAME("ctf-text");
BT_PLUGIN_DESCRIPTION("Babeltrace text output plug-in.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");

BT_PLUGIN_COMPONENT_CLASSES_BEGIN
BT_PLUGIN_SINK_COMPONENT_CLASS_ENTRY("text", "Formats CTF-IR to text. Formerly known as ctf-text.", ctf_text_init)
BT_PLUGIN_COMPONENT_CLASSES_END

enum loglevel {
        LOGLEVEL_EMERG                  = 0,
        LOGLEVEL_ALERT                  = 1,
        LOGLEVEL_CRIT                   = 2,
        LOGLEVEL_ERR                    = 3,
        LOGLEVEL_WARNING                = 4,
        LOGLEVEL_NOTICE                 = 5,
        LOGLEVEL_INFO                   = 6,
        LOGLEVEL_DEBUG_SYSTEM           = 7,
        LOGLEVEL_DEBUG_PROGRAM          = 8,
        LOGLEVEL_DEBUG_PROCESS          = 9,
        LOGLEVEL_DEBUG_MODULE           = 10,
        LOGLEVEL_DEBUG_UNIT             = 11,
        LOGLEVEL_DEBUG_FUNCTION         = 12,
        LOGLEVEL_DEBUG_LINE             = 13,
        LOGLEVEL_DEBUG                  = 14,
};

static
const char *loglevel_str [] = {
	[LOGLEVEL_EMERG] =		"TRACE_EMERG",
	[LOGLEVEL_ALERT] =		"TRACE_ALERT",
	[LOGLEVEL_CRIT] =		"TRACE_CRIT",
	[LOGLEVEL_ERR] =		"TRACE_ERR",
	[LOGLEVEL_WARNING] =		"TRACE_WARNING",
	[LOGLEVEL_NOTICE] =		"TRACE_NOTICE",
	[LOGLEVEL_INFO] =		"TRACE_INFO",
	[LOGLEVEL_DEBUG_SYSTEM] =	"TRACE_DEBUG_SYSTEM",
	[LOGLEVEL_DEBUG_PROGRAM] =	"TRACE_DEBUG_PROGRAM",
	[LOGLEVEL_DEBUG_PROCESS] =	"TRACE_DEBUG_PROCESS",
	[LOGLEVEL_DEBUG_MODULE] =	"TRACE_DEBUG_MODULE",
	[LOGLEVEL_DEBUG_UNIT] =		"TRACE_DEBUG_UNIT",
	[LOGLEVEL_DEBUG_FUNCTION] =	"TRACE_DEBUG_FUNCTION",
	[LOGLEVEL_DEBUG_LINE] =		"TRACE_DEBUG_LINE",
	[LOGLEVEL_DEBUG] =		"TRACE_DEBUG",
};

struct ctf_text_component {
	bool opt_print_all_field_names : 1;
	bool opt_print_scope_field_names : 1;
	bool opt_print_header_field_names : 1;
	bool opt_print_context_field_names : 1;
	bool opt_print_payload_field_names : 1;
	bool opt_print_all_fields : 1;
	bool opt_print_trace_field : 1;
	bool opt_print_trace_domain_field : 1;
	bool opt_print_trace_procname_field : 1;
	bool opt_print_trace_vpid_field : 1;
	bool opt_print_trace_hostname_field : 1;
	bool opt_print_trace_default_fields : 1;
	bool opt_print_loglevel_field : 1;
	bool opt_print_emf_field : 1;
	bool opt_print_delta_field : 1;
};

static
enum bt_component_status ctf_text_init(
		struct bt_component *component, struct bt_value *params)
{
	printf("ctf_text_init\n");
	return BT_COMPONENT_STATUS_OK;
}
