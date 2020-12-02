# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020 EfficiOS Inc.
#

import bt2


class TheSourceIterator(bt2._UserMessageIterator):
    def __init__(self, config, port):
        tc, sc, ec = port.user_data

        trace = tc()
        stream = trace.create_stream(sc, name='the-stream')

        self._msgs = [
            self._create_stream_beginning_message(stream),
            self._create_event_message(ec, stream),
            self._create_stream_end_message(stream),
        ]

    def __next__(self):
        if len(self._msgs) == 0:
            raise StopIteration

        return self._msgs.pop(0)


@bt2.plugin_component_class
class TheSource(bt2._UserSourceComponent, message_iterator_class=TheSourceIterator):
    def __init__(self, config, params, obj):
        tc = self._create_trace_class()
        sc = tc.create_stream_class()
        ec = sc.create_event_class(name='the-event')
        self._add_output_port('out', user_data=(tc, sc, ec))


bt2.register_plugin(__name__, "foo")
