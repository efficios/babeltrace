from collections import OrderedDict
import unittest
import bt2


class EventTestCase(unittest.TestCase):
    def _create_test_event_message(self, packet_fields_config=None,
                                   event_fields_config=None,
                                   with_clockclass=False,
                                   with_cc=False, with_sc=False,
                                   with_ep=False):

        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                self._at = 0

            def __next__(self):
                if self._at == 0:
                    msg = self._create_stream_beginning_message(test_obj.stream)
                elif self._at == 1:
                    assert test_obj.packet
                    msg = self._create_packet_beginning_message(test_obj.packet)
                elif self._at == 2:
                    default_clock_snapshot = 789 if with_clockclass else None
                    assert test_obj.packet
                    msg = self._create_event_message(test_obj.event_class, test_obj.packet, default_clock_snapshot)
                    if event_fields_config is not None:
                        event_fields_config(msg.event)
                elif self._at == 3:
                    msg = self._create_packet_end_message(test_obj.packet)
                elif self._at == 4:
                    msg = self._create_stream_end_message(test_obj.stream)
                elif self._at >= 5:
                    raise bt2.Stop

                self._at += 1
                return msg

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')
                tc = self._create_trace_class()

                clock_class = None
                if with_clockclass:
                    clock_class = self._create_clock_class(frequency=1000)

                # event common context (stream-class-defined)
                cc = None
                if with_cc:
                    cc = tc.create_structure_field_class()
                    cc += OrderedDict((
                        ('cpu_id', tc.create_signed_integer_field_class(8)),
                        ('stuff', tc.create_real_field_class()),
                    ))

                # packet context (stream-class-defined)
                pc = tc.create_structure_field_class()
                pc += OrderedDict((
                    ('something', tc.create_unsigned_integer_field_class(8)),
                    ('something_else', tc.create_real_field_class()),
                ))

                stream_class = tc.create_stream_class(default_clock_class=clock_class,
                                                      event_common_context_field_class=cc,
                                                      packet_context_field_class=pc)

                # specific context (event-class-defined)
                sc = None
                if with_sc:
                    sc = tc.create_structure_field_class()
                    sc += OrderedDict((
                        ('ant', tc.create_signed_integer_field_class(16)),
                        ('msg', tc.create_string_field_class()),
                    ))

                # event payload
                ep = None
                if with_ep:
                    ep = tc.create_structure_field_class()
                    ep += OrderedDict((
                        ('giraffe', tc.create_signed_integer_field_class(32)),
                        ('gnu', tc.create_signed_integer_field_class(8)),
                        ('mosquito', tc.create_signed_integer_field_class(8)),
                    ))

                event_class = stream_class.create_event_class(name='garou',
                                                              specific_context_field_class=sc,
                                                              payload_field_class=ep)

                trace = tc()
                stream = trace.create_stream(stream_class)
                packet = stream.create_packet()

                if packet_fields_config is not None:
                    packet_fields_config(packet)

                test_obj.packet = packet
                test_obj.stream = stream
                test_obj.event_class = event_class

        test_obj = self
        self._graph = bt2.Graph()
        self._src_comp = self._graph.add_component(MySrc, 'my_source')
        self._msg_iter = self._graph.create_output_port_message_iterator(self._src_comp.output_ports['out'])

        for i, msg in enumerate(self._msg_iter):
            if i == 2:
                return msg

    def test_attr_event_class(self):
        msg = self._create_test_event_message()
        self.assertEqual(msg.event.event_class.addr, self.event_class.addr)

    def test_attr_name(self):
        msg = self._create_test_event_message()
        self.assertEqual(msg.event.name, self.event_class.name)

    def test_attr_id(self):
        msg = self._create_test_event_message()
        self.assertEqual(msg.event.id, self.event_class.id)

    def test_get_common_context_field(self):
        def event_fields_config(event):
            event.common_context_field['cpu_id'] = 1
            event.common_context_field['stuff'] = 13.194

        msg = self._create_test_event_message(event_fields_config=event_fields_config, with_cc=True)

        self.assertEqual(msg.event.common_context_field['cpu_id'], 1)
        self.assertEqual(msg.event.common_context_field['stuff'], 13.194)

    def test_no_common_context_field(self):
        msg = self._create_test_event_message(with_cc=False)
        self.assertIsNone(msg.event.common_context_field)

    def test_get_specific_context_field(self):
        def event_fields_config(event):
            event.specific_context_field['ant'] = -1
            event.specific_context_field['msg'] = 'hellooo'

        msg = self._create_test_event_message(event_fields_config=event_fields_config, with_sc=True)

        self.assertEqual(msg.event.specific_context_field['ant'], -1)
        self.assertEqual(msg.event.specific_context_field['msg'], 'hellooo')

    def test_no_specific_context_field(self):
        msg = self._create_test_event_message(with_sc=False)
        self.assertIsNone(msg.event.specific_context_field)

    def test_get_event_payload_field(self):
        def event_fields_config(event):
            event.payload_field['giraffe'] = 1
            event.payload_field['gnu'] = 23
            event.payload_field['mosquito'] = 42

        msg = self._create_test_event_message(event_fields_config=event_fields_config, with_ep=True)

        self.assertEqual(msg.event.payload_field['giraffe'], 1)
        self.assertEqual(msg.event.payload_field['gnu'], 23)
        self.assertEqual(msg.event.payload_field['mosquito'], 42)

    def test_no_payload_field(self):
        msg = self._create_test_event_message(with_ep=False)
        self.assertIsNone(msg.event.payload_field)

    def test_clock_value(self):
        msg = self._create_test_event_message(with_clockclass=True)
        self.assertEqual(msg.default_clock_snapshot.value, 789)

    def test_no_clock_value(self):
        msg = self._create_test_event_message(with_clockclass=False)
        self.assertIsNone(msg.default_clock_snapshot)

    def test_stream(self):
        msg = self._create_test_event_message()
        self.assertEqual(msg.event.stream.addr, self.stream.addr)

    def test_getitem(self):
        def event_fields_config(event):
            event.payload_field['giraffe'] = 1
            event.payload_field['gnu'] = 23
            event.payload_field['mosquito'] = 42
            event.specific_context_field['ant'] = -1
            event.specific_context_field['msg'] = 'hellooo'
            event.common_context_field['cpu_id'] = 1
            event.common_context_field['stuff'] = 13.194

        def packet_fields_config(packet):
            packet.context_field['something'] = 154
            packet.context_field['something_else'] = 17.2

        msg = self._create_test_event_message(packet_fields_config=packet_fields_config,
                                              event_fields_config=event_fields_config,
                                              with_cc=True, with_sc=True, with_ep=True)
        ev = msg.event

        # Test event fields
        self.assertEqual(ev['giraffe'], 1)
        self.assertEqual(ev['gnu'], 23)
        self.assertEqual(ev['mosquito'], 42)
        self.assertEqual(ev['ant'], -1)
        self.assertEqual(ev['msg'], 'hellooo')
        self.assertEqual(ev['cpu_id'], 1)
        self.assertEqual(ev['stuff'], 13.194)

        # Test packet fields
        self.assertEqual(ev['something'], 154)
        self.assertEqual(ev['something_else'], 17.2)

        with self.assertRaises(KeyError):
            ev['yes']


if __name__ == "__main__":
    unittest.main()
