# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020 EfficiOS Inc.
#

import bt2


class TheSourceIterator(bt2._UserMessageIterator):
    def __init__(self, config, port):
        tc, sc, ec = port.user_data

        trace = tc()

        # Make two streams with the same name, to verify the stream filenames
        # are de-duplicated properly.  Make one with the name "metadata" to
        # verify the resulting data file is not named "metadata".
        stream1 = trace.create_stream(sc, name='the-stream')
        stream2 = trace.create_stream(sc, name='the-stream')
        stream3 = trace.create_stream(sc, name='metadata')

        self._msgs = [
            self._create_stream_beginning_message(stream1),
            self._create_stream_beginning_message(stream2),
            self._create_stream_beginning_message(stream3),
            self._create_event_message(ec, stream1),
            self._create_event_message(ec, stream2),
            self._create_event_message(ec, stream3),
            self._create_stream_end_message(stream1),
            self._create_stream_end_message(stream2),
            self._create_stream_end_message(stream3),
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
