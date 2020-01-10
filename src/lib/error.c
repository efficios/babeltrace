/*
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "LIB/ERROR"
#include "lib/logging.h"

#include <stdlib.h>
#include <stdint.h>
#include <babeltrace2/babeltrace.h>

#include "error.h"
#include "graph/message/iterator.h"
#include "graph/component.h"
#include "graph/component-class.h"
#include "common/assert.h"
#include "lib/assert-pre.h"
#include "lib/func-status.h"

#define BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(_cause, _exp_type)		\
	BT_ASSERT_PRE(((const struct bt_error_cause *) (_cause))->actor_type == _exp_type, \
		"Unexpected error cause's actor type: type=%s, exp-type=%s", \
		bt_error_cause_actor_type_string(((const struct bt_error_cause *) (_cause))->actor_type), \
		bt_error_cause_actor_type_string(_exp_type))

static
void fini_component_class_id(
		struct bt_error_cause_component_class_id *comp_class_id)
{
	BT_ASSERT(comp_class_id);

	if (comp_class_id->name) {
		g_string_free(comp_class_id->name, TRUE);
		comp_class_id->name = NULL;
	}

	if (comp_class_id->plugin_name) {
		g_string_free(comp_class_id->plugin_name, TRUE);
		comp_class_id->plugin_name = NULL;
	}
}

static
void fini_error_cause(struct bt_error_cause *cause)
{
	BT_ASSERT(cause);
	BT_LIB_LOGD("Finalizing error cause: %!+r", cause);

	if (cause->module_name) {
		g_string_free(cause->module_name, TRUE);
		cause->module_name = NULL;
	}

	if (cause->file_name) {
		g_string_free(cause->file_name, TRUE);
		cause->file_name = NULL;
	}

	if (cause->message) {
		g_string_free(cause->message, TRUE);
		cause->message = NULL;
	}
}

static
void destroy_error_cause(struct bt_error_cause *cause)
{
	if (!cause) {
		goto end;
	}

	BT_LIB_LOGD("Destroying error cause: %!+r", cause);

	switch (cause->actor_type) {
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT:
	{
		struct bt_error_cause_component_actor *spec_cause =
			(void *) cause;

		if (spec_cause->comp_name) {
			g_string_free(spec_cause->comp_name, TRUE);
			spec_cause->comp_name = NULL;
		}

		fini_component_class_id(&spec_cause->comp_class_id);
		break;
	}
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS:
	{
		struct bt_error_cause_component_class_actor *spec_cause =
			(void *) cause;

		fini_component_class_id(&spec_cause->comp_class_id);
		break;
	}
	case BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR:
	{
		struct bt_error_cause_message_iterator_actor *spec_cause =
			(void *) cause;

		if (spec_cause->comp_name) {
			g_string_free(spec_cause->comp_name, TRUE);
			spec_cause->comp_name = NULL;
		}

		if (spec_cause->output_port_name) {
			g_string_free(spec_cause->output_port_name, TRUE);
			spec_cause->output_port_name = NULL;
		}

		fini_component_class_id(&spec_cause->comp_class_id);
		break;
	}
	default:
		break;
	}

	fini_error_cause(cause);
	g_free(cause);

end:
	return;
}

static
int init_error_cause(struct bt_error_cause *cause,
		enum bt_error_cause_actor_type actor_type)
{
	int ret = 0;

	BT_ASSERT(cause);
	BT_LIB_LOGD("Initializing error cause: %!+r", cause);
	cause->actor_type = actor_type;
	cause->module_name = g_string_new(NULL);
	if (!cause->module_name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

	cause->message = g_string_new(NULL);
	if (!cause->message) {
		BT_LOGE_STR("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

	cause->file_name = g_string_new(NULL);
	if (!cause->file_name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

	BT_LIB_LOGD("Initialized error cause: %!+r", cause);

end:
	return ret;
}

static
int init_component_class_id(
		struct bt_error_cause_component_class_id *comp_class_id,
		struct bt_component_class *comp_cls)
{
	int ret = 0;

	BT_ASSERT(comp_class_id);
	comp_class_id->type = comp_cls->type;
	comp_class_id->name = g_string_new(comp_cls->name->str);
	if (!comp_class_id->name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

	comp_class_id->plugin_name = g_string_new(comp_cls->plugin_name->str);
	if (!comp_class_id->plugin_name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
void set_error_cause_props(struct bt_error_cause *cause,
		const char *file_name, uint64_t line_no)
{
	BT_ASSERT(cause);
	g_string_assign(cause->file_name, file_name);
	cause->line_no = line_no;
}

static
struct bt_error_cause *create_error_cause(const char *module_name,
		const char *file_name, uint64_t line_no)
{
	struct bt_error_cause *cause = g_new0(struct bt_error_cause, 1);
	int ret;

	BT_LOGD_STR("Creating error cause (unknown actor).");

	if (!cause) {
		BT_LOGE_STR("Failed to allocate one error cause.");
		goto error;
	}

	ret = init_error_cause(cause, BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN);
	if (ret) {
		goto error;
	}

	g_string_assign(cause->module_name, module_name);
	set_error_cause_props(cause, file_name, line_no);
	BT_LIB_LOGD("Created error cause: %!+r", cause);
	goto end;

error:
	destroy_error_cause(cause);
	cause = NULL;

end:
	return cause;
}

static
void append_component_class_id_str(GString *str,
		struct bt_error_cause_component_class_id *comp_class_id)
{
	const char *type_str = NULL;

	switch (comp_class_id->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		type_str = "src";
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		type_str = "flt";
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		type_str = "sink";
		break;
	default:
		bt_common_abort();
	}

	if (comp_class_id->plugin_name->len > 0) {
		g_string_append_printf(str, "%s.%s.%s",
			type_str, comp_class_id->plugin_name->str,
			comp_class_id->name->str);
	} else {
		g_string_append_printf(str, "%s.%s",
			type_str, comp_class_id->name->str);
	}
}

static
struct bt_error_cause_component_actor *create_error_cause_component_actor(
		struct bt_component *comp, const char *file_name,
		uint64_t line_no)
{
	struct bt_error_cause_component_actor *cause =
		g_new0(struct bt_error_cause_component_actor, 1);
	int ret;

	BT_LOGD_STR("Creating error cause object (component actor).");

	if (!cause) {
		goto error;
	}

	ret = init_error_cause(&cause->base,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT);
	if (ret) {
		goto error;
	}

	set_error_cause_props(&cause->base, file_name, line_no);
	cause->comp_name = g_string_new(comp->name->str);
	if (!cause->comp_name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		goto error;
	}

	ret = init_component_class_id(&cause->comp_class_id, comp->class);
	if (ret) {
		goto error;
	}

	g_string_append_printf(cause->base.module_name, "%s: ",
		comp->name->str);
	append_component_class_id_str(cause->base.module_name,
		&cause->comp_class_id);
	BT_LIB_LOGD("Created error cause object: %!+r", cause);
	goto end;

error:
	destroy_error_cause(&cause->base);
	cause = NULL;

end:
	return cause;
}

static
struct bt_error_cause_component_class_actor *
create_error_cause_component_class_actor(struct bt_component_class *comp_cls,
		const char *file_name, uint64_t line_no)
{
	struct bt_error_cause_component_class_actor *cause =
		g_new0(struct bt_error_cause_component_class_actor, 1);
	int ret;

	BT_LOGD_STR("Creating error cause object (component class actor).");

	if (!cause) {
		BT_LOGE_STR("Failed to allocate one error cause object.");
		goto error;
	}

	ret = init_error_cause(&cause->base,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS);
	if (ret) {
		goto error;
	}

	set_error_cause_props(&cause->base, file_name, line_no);
	ret = init_component_class_id(&cause->comp_class_id, comp_cls);
	if (ret) {
		goto error;
	}

	append_component_class_id_str(cause->base.module_name,
		&cause->comp_class_id);
	BT_LIB_LOGD("Created error cause object: %!+r", cause);
	goto end;

error:
	destroy_error_cause(&cause->base);
	cause = NULL;

end:
	return cause;
}

static
struct bt_error_cause_message_iterator_actor *
create_error_cause_message_iterator_actor(struct bt_message_iterator *iter,
		const char *file_name, uint64_t line_no)
{
	struct bt_error_cause_message_iterator_actor *cause;
	struct bt_message_iterator *input_port_iter;
	int ret;

	BT_LOGD_STR("Creating error cause object (message iterator actor).");

	/*
	 * This can only be created from within a graph, from a user
	 * message iterator, which is a self component port input
	 * message iterator.
	 */
	input_port_iter = (void *) iter;
	cause = g_new0(struct bt_error_cause_message_iterator_actor, 1);
	if (!cause) {
		BT_LOGE_STR("Failed to allocate one error cause object.");
		goto error;
	}

	ret = init_error_cause(&cause->base,
		BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR);
	if (ret) {
		goto error;
	}

	set_error_cause_props(&cause->base, file_name, line_no);
	cause->comp_name = g_string_new(
		input_port_iter->upstream_component->name->str);
	if (!cause->comp_name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		goto error;
	}

	cause->output_port_name = g_string_new(
		input_port_iter->upstream_port->name->str);
	if (!cause->output_port_name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		goto error;
	}

	ret = init_component_class_id(&cause->comp_class_id,
		input_port_iter->upstream_component->class);
	if (ret) {
		goto error;
	}

	g_string_append_printf(cause->base.module_name, "%s (%s): ",
		input_port_iter->upstream_component->name->str,
		input_port_iter->upstream_port->name->str);
	append_component_class_id_str(cause->base.module_name,
		&cause->comp_class_id);
	BT_LIB_LOGD("Created error cause object: %!+r", cause);
	goto end;

error:
	destroy_error_cause(&cause->base);
	cause = NULL;

end:
	return cause;
}

BT_HIDDEN
struct bt_error *bt_error_create(void)
{
	struct bt_error *error;

	BT_LOGD_STR("Creating error object.");
	error = g_new0(struct bt_error, 1);
	if (!error) {
		BT_LOGE_STR("Failed to allocate one error object.");
		goto error;
	}

	error->causes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_error_cause);
	if (!error->causes) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	BT_LOGD("Created error object: addr=%p", error);
	goto end;

error:
	bt_error_destroy(error);
	error = NULL;

end:
	return error;
}

BT_HIDDEN
void bt_error_destroy(struct bt_error *error)
{
	if (!error) {
		goto end;
	}

	if (error->causes) {
		g_ptr_array_free(error->causes, TRUE);
		error->causes = NULL;
	}

	g_free(error);

end:
	return;
}

BT_HIDDEN
int bt_error_append_cause_from_unknown(struct bt_error *error,
		const char *module_name, const char *file_name,
		uint64_t line_no, const char *msg_fmt, va_list args)
{
	struct bt_error_cause *cause = NULL;
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT(error);
	BT_ASSERT(module_name);
	BT_ASSERT(file_name);
	BT_ASSERT(msg_fmt);
	BT_LOGD("Appending error cause from unknown actor: "
		"module-name=\"%s\", func-name=\"%s\", line-no=%" PRIu64,
		module_name, file_name, line_no);
	cause = create_error_cause(module_name, file_name, line_no);
	if (!cause) {
		/* create_error_cause() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	g_string_append_vprintf(cause->message, msg_fmt, args);
	g_ptr_array_add(error->causes, cause);
	BT_LIB_LOGD("Appended error cause: %!+r", cause);
	cause = NULL;

end:
	destroy_error_cause(cause);
	return status;
}

BT_HIDDEN
int bt_error_append_cause_from_component(
		struct bt_error *error, bt_self_component *self_comp,
		const char *file_name, uint64_t line_no,
		const char *msg_fmt, va_list args)
{
	struct bt_error_cause_component_actor *cause = NULL;
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT(error);
	BT_ASSERT(self_comp);
	BT_ASSERT(file_name);
	BT_ASSERT(msg_fmt);
	BT_LIB_LOGD("Appending error cause from component actor: %![comp-]+c",
		self_comp);
	cause = create_error_cause_component_actor((void *) self_comp,
		file_name, line_no);
	if (!cause) {
		/* create_error_cause_component_actor() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	g_string_append_vprintf(cause->base.message, msg_fmt, args);
	g_ptr_array_add(error->causes, cause);
	BT_LIB_LOGD("Appended error cause: %!+r", cause);
	cause = NULL;

end:
	destroy_error_cause(&cause->base);
	return status;
}

BT_HIDDEN
int bt_error_append_cause_from_component_class(
		struct bt_error *error,
		bt_self_component_class *self_comp_class,
		const char *file_name, uint64_t line_no,
		const char *msg_fmt, va_list args)
{
	struct bt_error_cause_component_class_actor *cause = NULL;
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT(error);
	BT_ASSERT(self_comp_class);
	BT_ASSERT(file_name);
	BT_ASSERT(msg_fmt);
	BT_LIB_LOGD("Appending error cause from component class actor: "
		"%![comp-cls-]+C", self_comp_class);
	cause = create_error_cause_component_class_actor(
		(void *) self_comp_class, file_name, line_no);
	if (!cause) {
		/* create_error_cause_component_class_actor() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	g_string_append_vprintf(cause->base.message, msg_fmt, args);
	g_ptr_array_add(error->causes, cause);
	BT_LIB_LOGD("Appended error cause: %!+r", cause);
	cause = NULL;

end:
	destroy_error_cause(&cause->base);
	return status;
}

BT_HIDDEN
int bt_error_append_cause_from_message_iterator(
		struct bt_error *error, bt_self_message_iterator *self_iter,
		const char *file_name, uint64_t line_no,
		const char *msg_fmt, va_list args)
{
	struct bt_error_cause_message_iterator_actor *cause = NULL;
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT(error);
	BT_ASSERT(self_iter);
	BT_ASSERT(file_name);
	BT_ASSERT(msg_fmt);
	BT_LIB_LOGD("Appending error cause from message iterator actor: "
		"%![comp-]+i", self_iter);
	cause = create_error_cause_message_iterator_actor(
		(void *) self_iter, file_name, line_no);
	if (!cause) {
		/* create_error_cause_message_iterator_actor() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	g_string_append_vprintf(cause->base.message, msg_fmt, args);
	g_ptr_array_add(error->causes, cause);
	BT_LIB_LOGD("Appended error cause: %!+r", cause);
	cause = NULL;

end:
	destroy_error_cause(&cause->base);
	return status;
}

static
uint64_t error_cause_count(const bt_error *error)
{
	return error->causes ? error->causes->len : 0;
}

uint64_t bt_error_get_cause_count(const bt_error *error)
{
	BT_ASSERT_PRE_NON_NULL(error, "Error");
	return error_cause_count(error);
}

void bt_error_release(const struct bt_error *error)
{
	BT_ASSERT_PRE_NON_NULL(error, "Error");
	bt_error_destroy((void *) error);
}

const struct bt_error_cause *bt_error_borrow_cause_by_index(
		const bt_error *error, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(error, "Error");
	BT_ASSERT_PRE_VALID_INDEX(index, error_cause_count(error));
	return error->causes->pdata[index];
}

enum bt_error_cause_actor_type bt_error_cause_get_actor_type(
		const struct bt_error_cause *cause)
{
	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	return cause->actor_type;
}

const char *bt_error_cause_get_message(const struct bt_error_cause *cause)
{
	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	return cause->message->str;
}

const char *bt_error_cause_get_module_name(const struct bt_error_cause *cause)
{
	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	return cause->module_name->str;
}

const char *bt_error_cause_get_file_name(const struct bt_error_cause *cause)
{
	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	return cause->file_name->str;
}

uint64_t bt_error_cause_get_line_number(const bt_error_cause *cause)
{
	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	return cause->line_no;
}

const char *bt_error_cause_component_actor_get_component_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT);
	return spec_cause->comp_name->str;
}

bt_component_class_type bt_error_cause_component_actor_get_component_class_type(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT);
	return spec_cause->comp_class_id.type;
}

const char *bt_error_cause_component_actor_get_component_class_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT);
	return spec_cause->comp_class_id.name->str;
}

const char *bt_error_cause_component_actor_get_plugin_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT);
	return spec_cause->comp_class_id.plugin_name->len > 0 ?
		spec_cause->comp_class_id.plugin_name->str : NULL;
}

bt_component_class_type
bt_error_cause_component_class_actor_get_component_class_type(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_class_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS);
	return spec_cause->comp_class_id.type;
}

const char *bt_error_cause_component_class_actor_get_component_class_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_class_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS);
	return spec_cause->comp_class_id.name->str;
}

const char *bt_error_cause_component_class_actor_get_plugin_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_component_class_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS);
	return spec_cause->comp_class_id.plugin_name->len > 0 ?
		spec_cause->comp_class_id.plugin_name->str : NULL;
}

const char *bt_error_cause_message_iterator_actor_get_component_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_message_iterator_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR);
	return spec_cause->comp_name->str;
}

const char *
bt_error_cause_message_iterator_actor_get_component_output_port_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_message_iterator_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR);
	return spec_cause->output_port_name->str;
}

bt_component_class_type
bt_error_cause_message_iterator_actor_get_component_class_type(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_message_iterator_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR);
	return spec_cause->comp_class_id.type;
}

const char *bt_error_cause_message_iterator_actor_get_component_class_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_message_iterator_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR);
	return spec_cause->comp_class_id.name->str;
}

const char *bt_error_cause_message_iterator_actor_get_plugin_name(
		const struct bt_error_cause *cause)
{
	const struct bt_error_cause_message_iterator_actor *spec_cause =
		(const void *) cause;

	BT_ASSERT_PRE_NON_NULL(cause, "Error cause");
	BT_ASSERT_PRE_CAUSE_HAS_ACTOR_TYPE(cause,
		BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR);
	return spec_cause->comp_class_id.plugin_name->len > 0 ?
		spec_cause->comp_class_id.plugin_name->str : NULL;
}
