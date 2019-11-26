/*
 * Copyright EfficiOS, Inc.
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

#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "COMMON/FORMAT-ERROR"
#include <logging/log.h>

#include "format-error.h"

#include <stdint.h>
#include <string-format/format-plugin-comp-cls-name.h>

gchar *format_bt_error_cause(
		const bt_error_cause *error_cause,
		unsigned int columns,
		bt_logging_level log_level,
		enum bt_common_color_when use_colors)
{
	GString *str;
	gchar *comp_cls_str = NULL;
	GString *folded = NULL;
	struct bt_common_color_codes codes;

	str = g_string_new(NULL);
	BT_ASSERT(str);

	bt_common_color_get_codes(&codes, use_colors);

	/* Print actor name */
	g_string_append_c(str, '[');
	switch (bt_error_cause_get_actor_type(error_cause)) {
	case BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN:
		g_string_append_printf(str, "%s%s%s",
			codes.bold,
			bt_error_cause_get_module_name(error_cause),
			codes.reset);
		break;
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT:
		comp_cls_str = format_plugin_comp_cls_opt(
			bt_error_cause_component_actor_get_plugin_name(error_cause),
			bt_error_cause_component_actor_get_component_class_name(error_cause),
			bt_error_cause_component_actor_get_component_class_type(error_cause),
			use_colors);
		BT_ASSERT(comp_cls_str);

		g_string_append_printf(str, "%s%s%s: %s",
			codes.bold,
			bt_error_cause_component_actor_get_component_name(error_cause),
			codes.reset,
			comp_cls_str);

		break;
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS:
		comp_cls_str = format_plugin_comp_cls_opt(
			bt_error_cause_component_class_actor_get_plugin_name(error_cause),
			bt_error_cause_component_class_actor_get_component_class_name(error_cause),
			bt_error_cause_component_class_actor_get_component_class_type(error_cause),
			use_colors);
		BT_ASSERT(comp_cls_str);

		g_string_append(str, comp_cls_str);
		break;
	case BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR:
		comp_cls_str = format_plugin_comp_cls_opt(
			bt_error_cause_message_iterator_actor_get_plugin_name(error_cause),
			bt_error_cause_message_iterator_actor_get_component_class_name(error_cause),
			bt_error_cause_message_iterator_actor_get_component_class_type(error_cause),
			use_colors);
		BT_ASSERT(comp_cls_str);

		g_string_append_printf(str, "%s%s%s (%s%s%s): %s",
			codes.bold,
			bt_error_cause_message_iterator_actor_get_component_name(error_cause),
			codes.reset,
			codes.bold,
			bt_error_cause_message_iterator_actor_get_component_output_port_name(error_cause),
			codes.reset,
			comp_cls_str);

		break;
	default:
		bt_common_abort();
	}

	/* Print file name and line number */
	g_string_append_printf(str, "] (%s%s%s%s:%s%" PRIu64 "%s)\n",
		codes.bold,
		codes.fg_bright_magenta,
		bt_error_cause_get_file_name(error_cause),
		codes.reset,
		codes.fg_green,
		bt_error_cause_get_line_number(error_cause),
		codes.reset);

	/* Print message */
	folded = bt_common_fold(bt_error_cause_get_message(error_cause),
		columns, 2);
	if (folded) {
		g_string_append(str, folded->str);
		g_string_free(folded, TRUE);
		folded = NULL;
	} else {
		BT_LOGE_STR("Could not fold string.");
		g_string_append(str, bt_error_cause_get_message(error_cause));
	}

	g_free(comp_cls_str);

	return g_string_free(str, FALSE);
}

gchar *format_bt_error(
		const bt_error *error,
		unsigned int columns,
		bt_logging_level log_level,
		enum bt_common_color_when use_colors)
{
	GString *str;
	int64_t i;
	gchar *error_cause_str = NULL;
	struct bt_common_color_codes codes;

	BT_ASSERT(error);
	BT_ASSERT(bt_error_get_cause_count(error) > 0);

	str = g_string_new(NULL);
	BT_ASSERT(str);

	bt_common_color_get_codes(&codes, use_colors);

	/* Reverse order: deepest (root) cause printed at the end */
	for (i = bt_error_get_cause_count(error) - 1; i >= 0; i--) {
		const bt_error_cause *cause =
			bt_error_borrow_cause_by_index(error, (uint64_t) i);
		const char *prefix_fmt =
			i == bt_error_get_cause_count(error) - 1 ?
				"%s%sERROR%s:    " : "%s%sCAUSED BY%s ";

		/* Print prefix */
		g_string_append_printf(str, prefix_fmt,
			codes.bold, codes.fg_bright_red, codes.reset);

		g_free(error_cause_str);
		error_cause_str = format_bt_error_cause(cause, columns,
			log_level, use_colors);
		BT_ASSERT(error_cause_str);

		g_string_append(str, error_cause_str);

		/*
		 * Don't append a newline at the end, since that is used to
		 * generate the Python __str__, which doesn't need a newline
		 * at the end.
		 */
		if (i > 0) {
			g_string_append_c(str, '\n');
		}
	}

	g_free(error_cause_str);

	return g_string_free(str, FALSE);
}
