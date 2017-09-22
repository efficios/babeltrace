# The MIT License (MIT)
#
# Copyright (c) 2013-2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

import bt2
import babeltrace.common as common
import babeltrace.reader_field_declaration as reader_field_declaration


class FieldError(Exception):
    """
    Field error, raised when the value of a field cannot be accessed.
    """

    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)


class _Definition:
    def __init__(self, scope_id, field, name):
        self._scope_id = scope_id
        self._field = field
        self._name = name

    @property
    def name(self):
        """Return the name of a field or None on error."""

        return self._name

    @property
    def type(self):
        """Return the type of a field or -1 if unknown."""

        try:
            return _OBJ_TO_TYPE_ID[type(self._field)]
        except KeyError:
            return -1

    @property
    def declaration(self):
        """Return the associated Definition object."""

        return reader_field_declaration._create_field_declaration(
            self._field.field_type, self._name, self.scope)

    @property
    def value(self):
        """
        Return the value associated with the field according to its type.
        Return None on error.
        """

        try:
            if type(self._field) in (bt2._ArrayField, bt2._SequenceField):
                elem_ft = self._field.field_type.element_field_type

                if type(elem_ft) is bt2.IntegerFieldType:
                    if elem_ft.size == 8 and elem_ft.encoding != bt2.Encoding.NONE:
                        return bytes(x for x in self._field._value if x != 0).decode()

            return self._field._value
        except bt2.Error:
            return

    @property
    def scope(self):
        """Return the scope of a field or None on error."""

        return self._scope_id


_OBJ_TO_TYPE_ID = {
    bt2._IntegerField: common.CTFTypeId.INTEGER,
    bt2._FloatingPointNumberField: common.CTFTypeId.FLOAT,
    bt2._EnumerationField: common.CTFTypeId.ENUM,
    bt2._StringField: common.CTFTypeId.STRING,
    bt2._StructureField: common.CTFTypeId.STRUCT,
    bt2._ArrayField: common.CTFTypeId.ARRAY,
    bt2._SequenceField: common.CTFTypeId.SEQUENCE,
    bt2._VariantField: common.CTFTypeId.VARIANT,
}
