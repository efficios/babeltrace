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
#define BT_LOG_TAG "COMMON/FORMAT-PLUGIN-COMP-CLS-NAME"
#include <logging/log.h>

#include "format-plugin-comp-cls-name.h"

#include <common/common.h>


static
const char *component_type_str(bt_component_class_type type)
{
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		return "source";
	case BT_COMPONENT_CLASS_TYPE_SINK:
		return "sink";
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		return "filter";
	default:
		return "(unknown)";
	}
}

gchar *format_plugin_comp_cls_opt(const char *plugin_name,
		const char *comp_cls_name, bt_component_class_type type,
		enum bt_common_color_when use_colors)
{
	GString *str;
	GString *shell_plugin_name = NULL;
	GString *shell_comp_cls_name = NULL;
	gchar *ret;
	struct bt_common_color_codes codes;

	str = g_string_new(NULL);
	if (!str) {
		goto end;
	}

	if (plugin_name) {
		shell_plugin_name = bt_common_shell_quote(plugin_name, false);
		if (!shell_plugin_name) {
			goto end;
		}
	}

	shell_comp_cls_name = bt_common_shell_quote(comp_cls_name, false);
	if (!shell_comp_cls_name) {
		goto end;
	}

	bt_common_color_get_codes(&codes, use_colors);

	g_string_append_printf(str, "'%s%s%s%s",
		codes.bold,
		codes.fg_bright_cyan,
		component_type_str(type),
		codes.fg_default);

	if (shell_plugin_name) {
		g_string_append_printf(str, ".%s%s%s",
			codes.fg_blue,
			shell_plugin_name->str,
			codes.fg_default);
	}

	g_string_append_printf(str, ".%s%s%s'",
		codes.fg_yellow,
		shell_comp_cls_name->str,
		codes.reset);

end:
	if (shell_plugin_name) {
		g_string_free(shell_plugin_name, TRUE);
	}

	if (shell_comp_cls_name) {
		g_string_free(shell_comp_cls_name, TRUE);
	}

	if (str) {
		ret = g_string_free(str, FALSE);
	} else {
		ret = NULL;
	}

	return ret;
}
