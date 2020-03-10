/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>
#include "dummy/dummy.h"
#include "counter/counter.h"
#include "muxer/muxer.h"
#include "trimmer/trimmer.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

BT_PLUGIN(utils);
BT_PLUGIN_DESCRIPTION("Common graph utilities");
BT_PLUGIN_AUTHOR("EfficiOS <https://www.efficios.com/>");
BT_PLUGIN_LICENSE("MIT");

/* sink.utils.dummy */
BT_PLUGIN_SINK_COMPONENT_CLASS(dummy, dummy_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(dummy, dummy_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(dummy, dummy_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(dummy,
	dummy_graph_is_configured);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(dummy,
	"Consume messages and discard them.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(dummy,
	"See the babeltrace2-sink.utils.dummy(7) manual page.");

/* sink.utils.counter */
BT_PLUGIN_SINK_COMPONENT_CLASS(counter, counter_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(counter, counter_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(counter, counter_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(counter,
	counter_graph_is_configured);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(counter,
	"Count messages and print the statistics.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(counter,
	"See the babeltrace2-sink.utils.counter(7) manual page.");

/* flt.utils.trimmer */
BT_PLUGIN_FILTER_COMPONENT_CLASS(trimmer, trimmer_msg_iter_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(trimmer,
	"Discard messages that occur outside a specific time range.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP(trimmer,
	"See the babeltrace2-filter.utils.trimmer(7) manual page.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD(trimmer, trimmer_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(trimmer, trimmer_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(trimmer,
	trimmer_msg_iter_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(trimmer,
	trimmer_msg_iter_finalize);

/* flt.utils.muxer */
BT_PLUGIN_FILTER_COMPONENT_CLASS(muxer, muxer_msg_iter_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(muxer,
	"Sort messages from multiple input ports to a single output port by time.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP(muxer,
	"See the babeltrace2-filter.utils.muxer(7) manual page.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD(muxer, muxer_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(muxer, muxer_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD(muxer,
	muxer_input_port_connected);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(muxer,
	muxer_msg_iter_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(muxer,
	muxer_msg_iter_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS(muxer,
	muxer_msg_iter_seek_beginning, muxer_msg_iter_can_seek_beginning);
