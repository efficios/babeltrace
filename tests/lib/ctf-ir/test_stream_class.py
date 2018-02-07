import unittest
import bt2


class StreamClassSingleClockClassTestCase(unittest.TestCase):
    def setUp(self):
        self._sc = bt2.StreamClass()

    def tearDown(self):
        del self._sc

    def _test_add_sc_ft_vs_event_class(self, set_ft_func):
        cc = bt2.ClockClass('first_cc', 1000)
        scft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        scft.append_field('salut', ft)
        set_ft_func(scft)
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec = bt2.EventClass('ec', payload_field_type=payloadft)
        self._sc.add_event_class(ec)
        trace = bt2.Trace()

        with self.assertRaises(bt2.Error):
            trace.add_stream_class(self._sc)

    def test_add_sc_packet_context_vs_event_class(self):
        def func(ft):
            self._sc.packet_context_field_type = ft

        self._test_add_sc_ft_vs_event_class(func)

    def test_add_sc_event_context_vs_event_class(self):
        def func(ft):
            self._sc.event_context_field_type = ft

        self._test_add_sc_ft_vs_event_class(func)

    def test_add_sc_event_header_vs_event_class(self):
        def func(ft):
            self._sc.event_header_field_type = ft

        self._test_add_sc_ft_vs_event_class(func)

    def test_add_sc_event_class_vs_event_class(self):
        ehft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32)
        ehft.append_field('id', ft)
        self._sc.event_header_field_type = ehft
        cc = bt2.ClockClass('first_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec1 = bt2.EventClass('ec', payload_field_type=payloadft)
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('logi', ft)
        ec2 = bt2.EventClass('ec', payload_field_type=payloadft)
        self._sc.add_event_class(ec1)
        self._sc.add_event_class(ec2)
        trace = bt2.Trace()

        with self.assertRaises(bt2.Error):
            trace.add_stream_class(self._sc)

    def _test_create_event_ft_vs_event_class(self, set_ft_func):
        cc = bt2.ClockClass('first_cc', 1000)
        scft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        scft.append_field('salut', ft)
        set_ft_func(scft)
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec = bt2.EventClass('ec', payload_field_type=payloadft)
        self._sc.add_event_class(ec)

        with self.assertRaises(bt2.Error):
            ev = ec()

    def test_create_event_packet_context_vs_event_class(self):
        def func(ft):
            self._sc.packet_context_field_type = ft

        self._test_create_event_ft_vs_event_class(func)

    def test_create_event_event_context_vs_event_class(self):
        def func(ft):
            self._sc.event_context_field_type = ft

        self._test_create_event_ft_vs_event_class(func)

    def test_create_event_event_header_vs_event_class(self):
        def func(ft):
            self._sc.event_header_field_type = ft

        self._test_create_event_ft_vs_event_class(func)

    def test_create_event_event_class_vs_event_class(self):
        ehft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32)
        ehft.append_field('id', ft)
        self._sc.event_header_field_type = ehft
        cc = bt2.ClockClass('first_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec1 = bt2.EventClass('ec', payload_field_type=payloadft)
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('logi', ft)
        ec2 = bt2.EventClass('ec', payload_field_type=payloadft)
        self._sc.add_event_class(ec1)
        self._sc.add_event_class(ec2)

        with self.assertRaises(bt2.Error):
            ev = ec1()

    def test_add_ec_after_add_sc(self):
        cc = bt2.ClockClass('first_cc', 1000)
        scft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        scft.append_field('salut', ft)
        self._sc.packet_context_field_type = scft
        trace = bt2.Trace()
        trace.add_stream_class(self._sc)
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec = bt2.EventClass('ec', payload_field_type=payloadft)

        with self.assertRaises(bt2.Error):
            self._sc.add_event_class(ec)

    def test_add_ec_after_add_sc_single_cc(self):
        ehft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32)
        ehft.append_field('id', ft)
        self._sc.event_header_field_type = ehft
        trace = bt2.Trace()
        trace.add_stream_class(self._sc)
        cc = bt2.ClockClass('first_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec = bt2.EventClass('ec', payload_field_type=payloadft)
        self._sc.add_event_class(ec)
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('lel', ft)
        ec = bt2.EventClass('ec', payload_field_type=payloadft)

        with self.assertRaises(bt2.Error):
            self._sc.add_event_class(ec)

    def test_add_ec_after_create_event(self):
        ehft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32)
        ehft.append_field('id', ft)
        self._sc.event_header_field_type = ehft
        cc = bt2.ClockClass('first_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec1 = bt2.EventClass('ec', payload_field_type=payloadft)
        self._sc.add_event_class(ec1)
        ev = ec1()
        cc = bt2.ClockClass('second_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('logi', ft)
        ec2 = bt2.EventClass('ec', payload_field_type=payloadft)

        with self.assertRaises(bt2.Error):
            self._sc.add_event_class(ec2)

    def test_sc_clock_matches_expected_clock_class(self):
        clock = bt2.CtfWriterClock('sc_clock')
        self._sc.clock = clock
        writer = bt2.CtfWriter('/tmp')
        writer.add_clock(clock)
        writer.trace.add_stream_class(self._sc)
        cc = bt2.ClockClass('other_cc', 1000)
        payloadft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        payloadft.append_field('zorg', ft)
        ec = bt2.EventClass('ec', payload_field_type=payloadft)

        with self.assertRaises(bt2.Error):
            self._sc.add_event_class(ec)
