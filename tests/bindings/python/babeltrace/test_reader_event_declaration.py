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
#import babeltrace.reader_event_declaration as event_declaration

@unittest.skip("this is broken")
class EventDeclarationTestCase(unittest.TestCase):
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
        self._ec.id = 42
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

    def _get_event_declaration(self):
        return event_declaration._create_event_declaration(self._ec)

    def test_name(self):
        declaration = self._get_event_declaration()
        self.assertEqual(declaration.name, 'event_class_name')

    def test_id(self):
        declaration = self._get_event_declaration()
        self.assertEqual(declaration.id, 42)

    def test_fields(self):
        declaration = self._get_event_declaration()
        fields = declaration.fields
        self.assertEqual(len(list(fields)), len(self._values))

    def test_fields_scope(self):
        declaration = self._get_event_declaration()
        event_fields = list(
            declaration.fields_scope(babeltrace.CTFScope.EVENT_FIELDS))
        self.assertEqual(len(event_fields), 1)
        self.assertEqual(event_fields[0].name, 'ef_field')
        self.assertEqual(event_fields[0].scope,
                         babeltrace.CTFScope.EVENT_FIELDS)

        event_ctx_fields = list(
            declaration.fields_scope(babeltrace.CTFScope.EVENT_CONTEXT))
        self.assertEqual(len(event_ctx_fields), 1)
        self.assertEqual(event_ctx_fields[0].name, 'ec_field')
        self.assertEqual(event_ctx_fields[0].scope,
                         babeltrace.CTFScope.EVENT_CONTEXT)

        stream_ectx_fields = list(
            declaration.fields_scope(babeltrace.CTFScope.STREAM_EVENT_CONTEXT))
        self.assertEqual(len(stream_ectx_fields), 1)
        self.assertEqual(stream_ectx_fields[0].name, 'sec_field')
        self.assertEqual(stream_ectx_fields[0].scope,
                         babeltrace.CTFScope.STREAM_EVENT_CONTEXT)

        stream_eh_fields = list(
            declaration.fields_scope(babeltrace.CTFScope.STREAM_EVENT_HEADER))
        self.assertEqual(len(stream_eh_fields), 1)
        self.assertEqual(stream_eh_fields[0].name, 'seh_field')
        self.assertEqual(stream_eh_fields[0].scope,
                         babeltrace.CTFScope.STREAM_EVENT_HEADER)

        stream_pctx_fields = list(
            declaration.fields_scope(babeltrace.CTFScope.STREAM_PACKET_CONTEXT))
        self.assertEqual(len(stream_pctx_fields), 1)
        self.assertEqual(stream_pctx_fields[0].name, 'spc_field')
        self.assertEqual(stream_pctx_fields[0].scope,
                         babeltrace.CTFScope.STREAM_PACKET_CONTEXT)

        stream_ph_fields = list(
            declaration.fields_scope(babeltrace.CTFScope.TRACE_PACKET_HEADER))
        self.assertEqual(len(stream_ph_fields), 2)
        self.assertEqual(stream_ph_fields[0].name, 'ph_field_1')
        self.assertEqual(stream_ph_fields[0].scope,
                         babeltrace.CTFScope.TRACE_PACKET_HEADER)
        self.assertEqual(stream_ph_fields[1].name, 'ph_field_2')
        self.assertEqual(stream_ph_fields[1].scope,
                         babeltrace.CTFScope.TRACE_PACKET_HEADER)
