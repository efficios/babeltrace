# The MIT License (MIT)
#
# Copyright (c) 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import collections
import unittest
import bt2
import babeltrace
import datetime


class EventTestCase(unittest.TestCase):
    def setUp(self):
        self._values = {
            'ph_field_1' : 42,
            'ph_field_2' : 'bla bla',
            'spc_field' : 'some string',
            'seh_field' : 'another string',
            'sec_field' : 68752,
            'ec_field' : 89,
            'ef_field' : 8476,
        }

        self._int_ft = bt2.IntegerFieldType(32)
        self._str_ft = bt2.StringFieldType()

        self._trace = bt2.Trace()
        self._trace.packet_header_field_type = bt2.StructureFieldType()
        self._trace.packet_header_field_type += collections.OrderedDict([
            ('ph_field_1', self._int_ft),
            ('ph_field_2', self._str_ft),
        ])

        self._sc = bt2.StreamClass()
        self._sc.packet_context_field_type = bt2.StructureFieldType()
        self._sc.packet_context_field_type += collections.OrderedDict([
            ('spc_field', self._str_ft),
        ])

        self._sc.event_header_field_type = bt2.StructureFieldType()
        self._sc.event_header_field_type += collections.OrderedDict([
            ('seh_field', self._str_ft),
        ])

        self._sc.event_context_field_type = bt2.StructureFieldType()
        self._sc.event_context_field_type += collections.OrderedDict([
            ('sec_field', self._int_ft),
        ])

        self._clock_class = bt2.ClockClass('allo', 1000)
        self._trace.add_clock_class(self._clock_class)

        self._ec = bt2.EventClass('event_class_name')
        self._ec.context_field_type = bt2.StructureFieldType()
        self._ec.context_field_type += collections.OrderedDict([
            ('ec_field', self._int_ft),
        ])
        self._ec.payload_field_type = bt2.StructureFieldType()
        self._ec.payload_field_type += collections.OrderedDict([
            ('ef_field', self._int_ft),
        ])

        self._sc.add_event_class(self._ec)

        self._trace.add_stream_class(self._sc)
        self._cc_prio_map = bt2.ClockClassPriorityMap()
        self._cc_prio_map[self._clock_class] = 231
        self._stream = self._sc()
        self._packet = self._stream.create_packet()
        self._packet.header_field['ph_field_1'] = self._values['ph_field_1']
        self._packet.header_field['ph_field_2'] = self._values['ph_field_2']
        self._packet.context_field['spc_field'] = self._values['spc_field']

        self._event = self._ec()
        self._event.clock_values.add(self._clock_class(1772))
        self._event.header_field['seh_field'] = self._values['seh_field']
        self._event.stream_event_context_field['sec_field'] = self._values[
            'sec_field']
        self._event.context_field['ec_field'] = self._values['ec_field']
        self._event.payload_field['ef_field'] = self._values['ef_field']
        self._event.packet = self._packet

    def tearDown(self):
        del self._trace
        del self._sc
        del self._ec
        del self._int_ft
        del self._str_ft
        del self._clock_class
        del self._cc_prio_map
        del self._stream
        del self._packet
        del self._event

    def _get_event(self):
        notif = bt2.EventNotification(self._event, self._cc_prio_map)
        return babeltrace.reader_event._create_event(notif)

    def test_attr_name(self):
        event = self._get_event()
        self.assertEqual(event.name, 'event_class_name')

    def test_attr_cycles(self):
        event = self._get_event()
        self.assertEqual(event.cycles, 1772)

    def test_attr_timestamp(self):
        event = self._get_event()
        clock_class = self._cc_prio_map.highest_priority_clock_class
        self.assertEqual(event.timestamp, 1772 * (1E9 / clock_class.frequency))

    def test_attr_datetime(self):
        event = self._get_event()
        clock_class = self._cc_prio_map.highest_priority_clock_class
        ns = self._event.clock_values[clock_class].ns_from_epoch
        self.assertEqual(datetime.date.fromtimestamp(ns / 1E9), event.datetime)

    def test_getitem(self):
        event = self._get_event()
        for name, value in self._values.items():
            self.assertEqual(event[name], value)

        with self.assertRaises(KeyError):
            field = event['non-existant-key']

    def test_field_list_with_scope(self):
        event = self._get_event()
        self.assertEqual(
            set(event.field_list_with_scope(
                babeltrace.CTFScope.TRACE_PACKET_HEADER)),
            set(['ph_field_1', 'ph_field_2']))

        self.assertEqual(
            set(event.field_list_with_scope(
                babeltrace.CTFScope.STREAM_PACKET_CONTEXT)),
            set(['spc_field']))

        self.assertEqual(
            set(event.field_list_with_scope(
                babeltrace.CTFScope.STREAM_EVENT_HEADER)),
            set(['seh_field']))

        self.assertEqual(
            set(event.field_list_with_scope(
                babeltrace.CTFScope.STREAM_EVENT_CONTEXT)),
            set(['sec_field']))

        self.assertEqual(
            set(event.field_list_with_scope(
                babeltrace.CTFScope.EVENT_CONTEXT)),
            set(['ec_field']))

        self.assertEqual(
            set(event.field_list_with_scope(
                babeltrace.CTFScope.EVENT_FIELDS)),
            set(['ef_field']))

    def test_field_with_scope(self):
        event = self._get_event()
        self.assertEqual(event.field_with_scope(
            'seh_field', babeltrace.CTFScope.STREAM_EVENT_HEADER),
                         self._values['seh_field'])

    def test_get(self):
        event = self._get_event()
        self.assertEqual(event.get('spc_field'), self._values['spc_field'])
        self.assertEqual(event.get('non-existant field', 'No field'),
                         'No field')

    def test_keys(self):
        event = self._get_event()
        self.assertEqual(set(self._values.keys()), set(event.keys()))

    def test_len(self):
        event = self._get_event()
        self.assertEqual(len(self._values), len(event))
