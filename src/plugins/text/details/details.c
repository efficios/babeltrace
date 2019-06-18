/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_OUTPUT_LEVEL (details_comp->log_level)
#define BT_LOG_TAG "PLUGIN/SINK.TEXT.DETAILS"
#include "logging/log.h"

#include <babeltrace2/babeltrace.h>

#include "common/common.h"
#include "common/assert.h"
#include "details.h"
#include "write.h"

#define LOG_WRONG_PARAM_TYPE(_name, _value, _exp_type)			\
	do {								\
		BT_LOGE("Wrong `%s` parameter type: type=%s, "		\
			"expected-type=%s",				\
			(_name), bt_common_value_type_string(		\
				bt_value_get_type(_value)),		\
			bt_common_value_type_string(_exp_type));	\
	} while (0)

static
const char * const in_port_name = "in";

static
const char * const color_param_name = "color";

static
const char * const with_metadata_param_name = "with-metadata";

static
const char * const with_time_param_name = "with-time";

static
const char * const with_trace_class_name_param_name = "with-trace-class-name";

static
const char * const with_trace_name_param_name = "with-trace-name";

static
const char * const with_stream_class_name_param_name = "with-stream-class-name";

static
const char * const with_stream_name_param_name = "with-stream-name";

static
const char * const with_uuid_param_name = "with-uuid";

static
const char * const compact_param_name = "compact";

BT_HIDDEN
void details_destroy_details_trace_class_meta(
		struct details_trace_class_meta *details_tc_meta)
{
	if (!details_tc_meta) {
		goto end;
	}

	if (details_tc_meta->objects) {
		g_hash_table_destroy(details_tc_meta->objects);
		details_tc_meta->objects = NULL;
	}

	g_free(details_tc_meta);

end:
	return;
}

BT_HIDDEN
struct details_trace_class_meta *details_create_details_trace_class_meta(void)
{
	struct details_trace_class_meta *details_tc_meta =
		g_new0(struct details_trace_class_meta, 1);

	if (!details_tc_meta) {
		goto end;
	}

	details_tc_meta->objects = g_hash_table_new(
		g_direct_hash, g_direct_equal);
	if (!details_tc_meta->objects) {
		details_destroy_details_trace_class_meta(details_tc_meta);
		details_tc_meta = NULL;
		goto end;
	}

	details_tc_meta->tc_destruction_listener_id = UINT64_C(-1);

end:
	return details_tc_meta;
}

static
void destroy_details_comp(struct details_comp *details_comp)
{
	GHashTableIter iter;
	gpointer key, value;

	if (!details_comp) {
		goto end;
	}

	if (details_comp->meta) {
		/*
		 * Remove trace class destruction listeners, because
		 * otherwise, when they are called, `details_comp`
		 * (their user data) won't exist anymore (we're
		 * destroying it here).
		 */
		g_hash_table_iter_init(&iter, details_comp->meta);

		while (g_hash_table_iter_next(&iter, &key, &value)) {
			struct details_trace_class_meta *details_tc_meta =
				value;

			if (details_tc_meta->tc_destruction_listener_id !=
					UINT64_C(-1)) {
				bt_trace_class_remove_destruction_listener(
					(const void *) key,
					details_tc_meta->tc_destruction_listener_id);
			}
		}

		g_hash_table_destroy(details_comp->meta);
		details_comp->meta = NULL;
	}

	if (details_comp->traces) {
		/*
		 * Remove trace destruction listeners, because
		 * otherwise, when they are called, `details_comp` won't
		 * exist anymore (we're destroying it here).
		 */
		g_hash_table_iter_init(&iter, details_comp->traces);

		while (g_hash_table_iter_next(&iter, &key, &value)) {
			struct details_trace *details_trace = value;

			bt_trace_remove_destruction_listener(
				(const void *) key,
				details_trace->trace_destruction_listener_id);
		}

		g_hash_table_destroy(details_comp->traces);
		details_comp->traces = NULL;
	}

	if (details_comp->str) {
		g_string_free(details_comp->str, TRUE);
		details_comp->str = NULL;
	}

	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(
		details_comp->msg_iter);
	g_free(details_comp);

end:
	return;
}

static
struct details_comp *create_details_comp(
		bt_self_component_sink *self_comp_sink)
{
	struct details_comp *details_comp = g_new0(struct details_comp, 1);
	bt_self_component *self_comp =
		bt_self_component_sink_as_self_component(self_comp_sink);

	if (!details_comp) {
		goto error;
	}

	details_comp->log_level = bt_component_get_logging_level(
		bt_self_component_as_component(self_comp));
	details_comp->meta = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,
		(GDestroyNotify) details_destroy_details_trace_class_meta);
	if (!details_comp->meta) {
		goto error;
	}

	details_comp->traces = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
	if (!details_comp->traces) {
		goto error;
	}

	details_comp->str = g_string_new(NULL);
	if (!details_comp->str) {
		goto error;
	}

	goto end;

error:
	destroy_details_comp(details_comp);
	details_comp = NULL;

end:
	return details_comp;
}

BT_HIDDEN
void details_finalize(bt_self_component_sink *comp)
{
	struct details_comp *details_comp;

	BT_ASSERT(comp);
	details_comp = bt_self_component_get_data(
		bt_self_component_sink_as_self_component(comp));
	BT_ASSERT(details_comp);
	destroy_details_comp(details_comp);
}

static
int configure_bool_opt(struct details_comp *details_comp,
		const bt_value *params, const char *param_name,
		bool default_value, bool *opt_value)
{
	int ret = 0;
	const bt_value *value;

	*opt_value = default_value;
	value = bt_value_map_borrow_entry_value_const(params, param_name);
	if (value) {
		if (!bt_value_is_bool(value)) {
			LOG_WRONG_PARAM_TYPE(param_name, value,
				BT_VALUE_TYPE_BOOL);
			ret = -1;
			goto end;
		}

		*opt_value = (bool) bt_value_bool_get(value);
	}

end:
	return ret;
}

static
int configure_details_comp(struct details_comp *details_comp,
		const bt_value *params)
{
	int ret = 0;
	const bt_value *value;
	const char *str;

	/* Colorize output? */
	details_comp->cfg.with_color = bt_common_colors_supported();
	value = bt_value_map_borrow_entry_value_const(params, color_param_name);
	if (value) {
		if (!bt_value_is_string(value)) {
			LOG_WRONG_PARAM_TYPE(color_param_name, value,
				BT_VALUE_TYPE_STRING);
			goto error;
		}

		str = bt_value_string_get(value);

		if (strcmp(str, "never") == 0) {
			details_comp->cfg.with_color = false;
		} else if (strcmp(str, "auto") == 0) {
			details_comp->cfg.with_color =
				bt_common_colors_supported();
		} else if (strcmp(str, "always") == 0) {
			details_comp->cfg.with_color = true;
		} else {
			BT_LOGE("Invalid `%s` parameter: unknown value "
				"(expecting `never`, `auto`, or `always`): "
				"value=\"%s\"", color_param_name, str);
			goto error;
		}
	}

	/* With metadata objects? */
	ret = configure_bool_opt(details_comp, params, with_metadata_param_name,
		true, &details_comp->cfg.with_meta);
	if (ret) {
		goto error;
	}

	/* Compact? */
	ret = configure_bool_opt(details_comp, params, compact_param_name,
		false, &details_comp->cfg.compact);
	if (ret) {
		goto error;
	}

	/* With time? */
	ret = configure_bool_opt(details_comp, params, with_time_param_name,
		true, &details_comp->cfg.with_time);
	if (ret) {
		goto error;
	}

	/* With trace class name? */
	ret = configure_bool_opt(details_comp, params,
		with_trace_class_name_param_name,
		true, &details_comp->cfg.with_trace_class_name);
	if (ret) {
		goto error;
	}

	/* With trace name? */
	ret = configure_bool_opt(details_comp, params,
		with_trace_name_param_name,
		true, &details_comp->cfg.with_trace_name);
	if (ret) {
		goto error;
	}

	/* With stream class name? */
	ret = configure_bool_opt(details_comp, params,
		with_stream_class_name_param_name,
		true, &details_comp->cfg.with_stream_class_name);
	if (ret) {
		goto error;
	}

	/* With stream name? */
	ret = configure_bool_opt(details_comp, params,
		with_stream_name_param_name,
		true, &details_comp->cfg.with_stream_name);
	if (ret) {
		goto error;
	}

	/* With UUID? */
	ret = configure_bool_opt(details_comp, params,
		with_uuid_param_name, true, &details_comp->cfg.with_uuid);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static
void log_configuration(bt_self_component_sink *comp,
		struct details_comp *details_comp)
{
	BT_LOGI("Configuration for `sink.text.details` component `%s`:",
		bt_component_get_name(bt_self_component_as_component(
			bt_self_component_sink_as_self_component(comp))));
	BT_LOGI("  Colorize output: %d", details_comp->cfg.with_color);
	BT_LOGI("  Compact: %d", details_comp->cfg.compact);
	BT_LOGI("  With metadata: %d", details_comp->cfg.with_meta);
	BT_LOGI("  With time: %d", details_comp->cfg.with_time);
	BT_LOGI("  With trace class name: %d",
		details_comp->cfg.with_trace_class_name);
	BT_LOGI("  With trace name: %d", details_comp->cfg.with_trace_name);
	BT_LOGI("  With stream class name: %d",
		details_comp->cfg.with_stream_class_name);
	BT_LOGI("  With stream name: %d", details_comp->cfg.with_stream_name);
	BT_LOGI("  With UUID: %d", details_comp->cfg.with_uuid);
}

BT_HIDDEN
bt_self_component_status details_init(bt_self_component_sink *comp,
		const bt_value *params,
		__attribute__((unused)) void *init_method_data)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct details_comp *details_comp = NULL;

	status = bt_self_component_sink_add_input_port(comp, in_port_name,
		NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	details_comp = create_details_comp(comp);
	if (!details_comp) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto error;
	}

	if (configure_details_comp(details_comp, params)) {
		BT_LOGE_STR("Failed to configure component.");
		goto error;
	}

	log_configuration(comp, details_comp);
	bt_self_component_set_data(
		bt_self_component_sink_as_self_component(comp), details_comp);
	goto end;

error:
	if (status == BT_SELF_COMPONENT_STATUS_OK) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

	destroy_details_comp(details_comp);

end:
	return status;
}

BT_HIDDEN
bt_self_component_status details_graph_is_configured(
		bt_self_component_sink *comp)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	bt_self_component_port_input_message_iterator *iterator;
	struct details_comp *details_comp;
	bt_self_component_port_input *in_port;

	details_comp = bt_self_component_get_data(
		bt_self_component_sink_as_self_component(comp));
	BT_ASSERT(details_comp);
	in_port = bt_self_component_sink_borrow_input_port_by_name(comp,
		in_port_name);
	if (!bt_port_is_connected(bt_port_input_as_port_const(
			bt_self_component_port_input_as_port_input(in_port)))) {
		BT_LOGE("Single input port is not connected: "
			"port-name=\"%s\"", in_port_name);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	iterator = bt_self_component_port_input_message_iterator_create(
		bt_self_component_sink_borrow_input_port_by_name(comp,
			in_port_name));
	if (!iterator) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_MOVE_REF(
		details_comp->msg_iter, iterator);

end:
	return status;
}

BT_HIDDEN
bt_self_component_status details_consume(bt_self_component_sink *comp)
{
	bt_self_component_status ret = BT_SELF_COMPONENT_STATUS_OK;
	bt_message_array_const msgs;
	uint64_t count;
	struct details_comp *details_comp;
	bt_message_iterator_status it_ret;
	uint64_t i;

	details_comp = bt_self_component_get_data(
		bt_self_component_sink_as_self_component(comp));
	BT_ASSERT(details_comp);
	BT_ASSERT(details_comp->msg_iter);

	/* Consume messages */
	it_ret = bt_self_component_port_input_message_iterator_next(
		details_comp->msg_iter, &msgs, &count);
	switch (it_ret) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
		ret = BT_SELF_COMPONENT_STATUS_OK;

		for (i = 0; i < count; i++) {
			int print_ret = details_write_message(details_comp,
				msgs[i]);

			if (print_ret) {
				for (; i < count; i++) {
					/* Put all remaining messages */
					bt_message_put_ref(msgs[i]);
				}

				ret = BT_SELF_COMPONENT_STATUS_ERROR;
				goto end;
			}

			/* Print output buffer to standard output and flush */
			if (details_comp->str->len > 0) {
				printf("%s", details_comp->str->str);
				fflush(stdout);
				details_comp->printed_something = true;
			}

			/* Put this message */
			bt_message_put_ref(msgs[i]);
		}

		break;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		ret = BT_SELF_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_END:
		ret = BT_SELF_COMPONENT_STATUS_END;
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_ERROR:
		ret = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		ret = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	default:
		abort();
	}

end:
	return ret;
}
