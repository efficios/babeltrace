/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>
#include "common/assert.h"

static bt_component_class_sink_consume_method_status sink_consume(
		bt_self_component_sink *self_comp __attribute__((unused)))
{
	return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
}

static bt_component_class_get_supported_mip_versions_method_status
sink_get_supported_mip_versions(
		bt_self_component_class_sink *source_component_class __attribute__((unused)),
		const bt_value *params __attribute__((unused)),
		void *initialize_method_data __attribute__((unused)),
		bt_logging_level logging_level __attribute__((unused)),
		bt_integer_range_set_unsigned *supported_versions)
{
	return (int) bt_integer_range_set_unsigned_add_range(
		supported_versions, 0, 0);
}

static bt_message_iterator_class_initialize_method_status
src_dummy_iterator_init_method(
		bt_self_message_iterator *self_msg_iter __attribute__((unused)),
		bt_self_message_iterator_configuration *config __attribute__((unused)),
		bt_self_component_port_output *self_port __attribute__((unused)))
{
	return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

static bt_message_iterator_class_initialize_method_status
flt_dummy_iterator_init_method(
		bt_self_message_iterator *self_msg_iter __attribute__((unused)),
		bt_self_message_iterator_configuration *config __attribute__((unused)),
		bt_self_component_port_output *self_port __attribute__((unused)))
{
	return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

static void dummy_iterator_finalize_method(
		bt_self_message_iterator *self_msg_iter __attribute__((unused)))
{
}

static bt_message_iterator_class_next_method_status
dummy_iterator_next_method(
		bt_self_message_iterator *self_msg_iter __attribute__((unused)),
		bt_message_array_const msgs __attribute__((unused)),
		uint64_t capacity __attribute__((unused)),
		uint64_t *count __attribute__((unused)))
{
	return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
}

static bt_component_class_query_method_status flt_query_method(
		bt_self_component_class_filter *component_class __attribute__((unused)),
		bt_private_query_executor *priv_query_exec __attribute__((unused)),
		const char *object, const bt_value *params,
		__attribute__((unused)) void *method_data,
		const bt_value **result)
{
	bt_value *res = bt_value_array_create();
	bt_value *val;
	*result = res;
	int iret;

	BT_ASSERT(*result);
	iret = bt_value_array_append_string_element(res, object);
	BT_ASSERT(iret == 0);
	iret = bt_value_copy(params, &val);
	BT_ASSERT(iret == 0);
	iret = bt_value_array_append_element(res, val);
	BT_ASSERT(iret == 0);
	bt_value_put_ref(val);
	return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
}

BT_PLUGIN_MODULE();
BT_PLUGIN(test_sfs);
BT_PLUGIN_DESCRIPTION("Babeltrace plugin with source, sink, and filter component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_VERSION(1, 2, 3, "yes");

BT_PLUGIN_SOURCE_COMPONENT_CLASS(source, dummy_iterator_next_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(source, "A source.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(source,
	src_dummy_iterator_init_method);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(source,
	dummy_iterator_finalize_method);

BT_PLUGIN_SINK_COMPONENT_CLASS(sink, sink_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(sink, "A sink.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(sink,
	"Bacon ipsum dolor amet strip steak cupim pastrami venison shoulder.\n"
	"Prosciutto beef ribs flank meatloaf pancetta brisket kielbasa drumstick\n"
	"venison tenderloin cow tail. Beef short loin shoulder meatball, sirloin\n"
	"ground round brisket salami cupim pork bresaola turkey bacon boudin.\n"
);
BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD(sink,
	sink_get_supported_mip_versions);

BT_PLUGIN_FILTER_COMPONENT_CLASS(filter, dummy_iterator_next_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(filter, "A filter.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(filter,
	flt_dummy_iterator_init_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(filter,
	dummy_iterator_finalize_method);
BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD(filter, flt_query_method);
