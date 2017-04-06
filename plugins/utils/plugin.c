/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/plugin/plugin-dev.h>
#include "dummy/dummy.h"
#include "trimmer/trimmer.h"
#include "trimmer/iterator.h"
#include "muxer/muxer.h"

BT_PLUGIN(utils);
BT_PLUGIN_DESCRIPTION("Graph utilities");
BT_PLUGIN_AUTHOR("Julien Desfossez, Philippe Proulx");
BT_PLUGIN_LICENSE("MIT");

/* dummy sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(dummy, dummy_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD(dummy, dummy_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(dummy, dummy_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_PORT_CONNECTED_METHOD(dummy,
	dummy_port_connected);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(dummy,
	"Dummy sink component class: does absolutely nothing!");

/* trimmer filter */
BT_PLUGIN_FILTER_COMPONENT_CLASS(trimmer, trimmer_iterator_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(trimmer,
	"Ensure that trace notifications outside of a given range are filtered-out.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD(trimmer, trimmer_component_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(trimmer, finalize_trimmer);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(trimmer,
	trimmer_iterator_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_FINALIZE_METHOD(trimmer,
	trimmer_iterator_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_SEEK_TIME_METHOD(trimmer,
	trimmer_iterator_seek_time);

/* muxer filter */
BT_PLUGIN_FILTER_COMPONENT_CLASS(muxer, muxer_notif_iter_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(muxer,
	"Notification multiplexer");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD(muxer, muxer_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(muxer, muxer_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_PORT_DISCONNECTED_METHOD(muxer,
	muxer_port_disconnected);
BT_PLUGIN_FILTER_COMPONENT_CLASS_PORT_CONNECTED_METHOD(muxer,
	muxer_port_connected);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(muxer,
	muxer_notif_iter_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_FINALIZE_METHOD(muxer,
	muxer_notif_iter_finalize);
