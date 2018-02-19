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
import copy
import bt2
import babeltrace
import babeltrace.reader_field_declaration as field_declaration
import datetime
import random


class FieldDeclarationTestCase(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    @staticmethod
    def _get_declaration(field_type):
        return field_declaration._create_field_declaration(
            field_type, 'a_name', babeltrace.CTFScope.TRACE_PACKET_HEADER)

    def test_type(self):
        int_ft = bt2.IntegerFieldType(32)

        enum_ft = bt2.EnumerationFieldType(int_ft)
        enum_ft.add_mapping('corner', 23)
        enum_ft.add_mapping('zoom', 17, 20)
        enum_ft.add_mapping('mellotron', 1001)
        enum_ft.add_mapping('giorgio', 2000, 3000)

        array_ft = bt2.ArrayFieldType(int_ft, 5)
        seq_ft = bt2.SequenceFieldType(int_ft, 'the_len_field')
        float_ft = bt2.FloatingPointNumberFieldType()

        struct_ft = bt2.StructureFieldType()
        struct_ft.append_field('a', int_ft)
        struct_ft.append_field('b', int_ft)
        struct_ft.append_field('c', int_ft)

        _string_ft = bt2.StringFieldType()

        variant_ft = bt2.VariantFieldType('tag', enum_ft)
        variant_ft.append_field('corner', int_ft)
        variant_ft.append_field('zoom', array_ft)
        variant_ft.append_field('mellotron', float_ft)
        variant_ft.append_field('giorgio', struct_ft)

        expected_types = {
            babeltrace.CTFTypeId.INTEGER: int_ft,
            babeltrace.CTFTypeId.FLOAT: float_ft,
            babeltrace.CTFTypeId.ENUM: enum_ft,
            babeltrace.CTFTypeId.STRING: _string_ft,
            babeltrace.CTFTypeId.STRUCT: struct_ft,
            babeltrace.CTFTypeId.VARIANT: variant_ft,
            babeltrace.CTFTypeId.ARRAY: array_ft,
            babeltrace.CTFTypeId.SEQUENCE: seq_ft,
        }

        for type_id, ft in expected_types.items():
            declaration = self._get_declaration(ft)
            self.assertIsNotNone(declaration)
            self.assertEqual(declaration.type, type_id)

    def test_int_signedness(self):
        int_ft = bt2.IntegerFieldType(size=32, is_signed=True)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.signedness, 1)

        uint_ft = bt2.IntegerFieldType(size=32, is_signed=False)
        declaration = self._get_declaration(uint_ft)
        self.assertEqual(declaration.signedness, 0)

    def test_int_base(self):
        int_ft = bt2.IntegerFieldType(size=32, base=8)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.base, 8)

        int_ft = bt2.IntegerFieldType(size=32, base=16)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.base, 16)

        int_ft = bt2.IntegerFieldType(size=32, base=10)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.base, 10)

        int_ft = bt2.IntegerFieldType(size=32, base=2)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.base, 2)

    def test_int_byte_order(self):
        expected_byte_orders = {
            bt2.ByteOrder.NATIVE: babeltrace.ByteOrder.BYTE_ORDER_NATIVE,
            bt2.ByteOrder.LITTLE_ENDIAN: babeltrace.ByteOrder.BYTE_ORDER_LITTLE_ENDIAN,
            bt2.ByteOrder.BIG_ENDIAN: babeltrace.ByteOrder.BYTE_ORDER_BIG_ENDIAN,
            bt2.ByteOrder.NETWORK: babeltrace.ByteOrder.BYTE_ORDER_NETWORK,
        }

        for bt2_bo, bt_bo in expected_byte_orders.items():
            int_ft = bt2.IntegerFieldType(size=32, byte_order=bt2_bo)
            declaration = self._get_declaration(int_ft)
            self.assertEqual(declaration.byte_order, bt_bo)

    def test_int_size(self):
        int_ft = bt2.IntegerFieldType(size=32, base=8)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.size, 32)
        self.assertEqual(declaration.length, 32)

        int_ft = bt2.IntegerFieldType(size=12, base=8)
        declaration = self._get_declaration(int_ft)
        self.assertEqual(declaration.size, 12)
        self.assertEqual(declaration.length, 12)

    def test_int_encoding(self):
        expected_encodings = {
            bt2.Encoding.NONE: babeltrace.CTFStringEncoding.NONE,
            bt2.Encoding.ASCII: babeltrace.CTFStringEncoding.ASCII,
            bt2.Encoding.UTF8: babeltrace.CTFStringEncoding.UTF8,
        }

        for bt2_encoding, bt_encoding in expected_encodings.items():
            int_ft = bt2.IntegerFieldType(size=32, encoding=bt2_encoding)
            declaration = self._get_declaration(int_ft)
            self.assertEqual(declaration.encoding, bt_encoding)

    def test_array_length(self):
        int_ft = bt2.IntegerFieldType(32)
        array_ft = bt2.ArrayFieldType(int_ft, 5)
        declaration = self._get_declaration(array_ft)
        self.assertEqual(declaration.length, 5)

    def test_array_element_declaration(self):
        int_ft = bt2.IntegerFieldType(size=32, is_signed=True, base=8)
        array_ft = bt2.ArrayFieldType(int_ft, 5)
        declaration = self._get_declaration(array_ft)
        element_declaration = declaration.element_declaration
        self.assertEqual(element_declaration.type, babeltrace.CTFTypeId.INTEGER)
        self.assertEqual(element_declaration.size, 32)
        self.assertEqual(element_declaration.base, 8)

    def test_sequence_element_declaration(self):
        int_ft = bt2.IntegerFieldType(size=32, is_signed=True, base=8)
        seq_ft = bt2.SequenceFieldType(int_ft, 'len_field')
        declaration = self._get_declaration(seq_ft)
        element_declaration = declaration.element_declaration
        self.assertEqual(element_declaration.type, babeltrace.CTFTypeId.INTEGER)
        self.assertEqual(element_declaration.size, 32)
        self.assertEqual(element_declaration.base, 8)
