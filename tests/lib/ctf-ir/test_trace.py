import unittest
import bt2


class TraceTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()

    def tearDown(self):
        del self._trace

    def _add_stream_class(self):
        sc = bt2.StreamClass()
        self._trace.add_stream_class(sc)

    def test_packet_header_ft_no_clock_class_simple(self):
        cc = bt2.ClockClass('hello', 1000)
        phft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        phft.append_field('salut', ft)
        self._trace.add_clock_class(cc)
        self._trace.packet_header_field_type = phft

        with self.assertRaises(bt2.Error):
            self._add_stream_class()

    def test_packet_header_ft_no_clock_class_struct(self):
        cc = bt2.ClockClass('hello', 1000)
        phft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        struct_ft = bt2.StructureFieldType()
        struct_ft.append_field('salut', ft)
        phft.append_field('boucane', struct_ft)
        self._trace.add_clock_class(cc)
        self._trace.packet_header_field_type = phft

        with self.assertRaises(bt2.Error):
            self._add_stream_class()

    def test_packet_header_ft_no_clock_class_variant(self):
        cc = bt2.ClockClass('hello', 1000)
        phft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        tag_ft = bt2.EnumerationFieldType(size=32)
        tag_ft.append_mapping('heille', 12)
        variant_ft = bt2.VariantFieldType('tag', tag_ft)
        variant_ft.append_field('heille', ft)
        phft.append_field('tag', tag_ft)
        phft.append_field('boucane', variant_ft)
        self._trace.add_clock_class(cc)
        self._trace.packet_header_field_type = phft

        with self.assertRaises(bt2.Error):
            self._add_stream_class()

    def test_packet_header_ft_no_clock_class_array(self):
        cc = bt2.ClockClass('hello', 1000)
        phft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        array_ft = bt2.ArrayFieldType(ft, 23)
        phft.append_field('boucane', array_ft)
        self._trace.add_clock_class(cc)
        self._trace.packet_header_field_type = phft

        with self.assertRaises(bt2.Error):
            self._add_stream_class()

    def test_packet_header_ft_no_clock_class_sequence(self):
        cc = bt2.ClockClass('hello', 1000)
        phft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        len_ft = bt2.IntegerFieldType(32)
        seq_ft = bt2.SequenceFieldType(ft, 'len')
        phft.append_field('len', len_ft)
        phft.append_field('boucane', seq_ft)
        self._trace.add_clock_class(cc)
        self._trace.packet_header_field_type = phft

        with self.assertRaises(bt2.Error):
            self._add_stream_class()

    def test_packet_header_ft_no_clock_class_set_static(self):
        cc = bt2.ClockClass('hello', 1000)
        phft = bt2.StructureFieldType()
        ft = bt2.IntegerFieldType(32, mapped_clock_class=cc)
        phft.append_field('salut', ft)
        self._trace.add_clock_class(cc)
        self._trace.packet_header_field_type = phft

        with self.assertRaises(bt2.Error):
            self._trace.set_is_static()
