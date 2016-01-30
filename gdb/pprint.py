from __future__ import print_function
from termcolor import colored
import traceback


def _cbold(s, color):
    return colored(s, color, attrs=['bold'])


def _bool_to_yesno(b):
    return _cbold('yes', 'green') if b else _cbold('no', 'magenta')


def _cvalue(v):
    return _cbold(v, 'blue')


def _cerror(v):
    return _cbold(v, 'red')


def _cprop(p):
    return colored(p, 'yellow')


def _ctypename(n):
    return colored(n, 'white', 'on_blue', attrs=['bold'])


def _cprop_common(p):
    return _cbold(p, 'yellow')


class _Attr(object):
    def __init__(self, name, value, common=False):
        self.name = name
        self.common = common
        self.value = value


class _AttrValue(object):
    pass


class _TextValue(_AttrValue):
    def __init__(self, text):
        self.text = text


class _FieldTypeValue(_AttrValue):
    def __init__(self, name, field_type):
        self.name = name
        self.field_type = field_type


class _PrettyPrintFieldType(gdb.Command):
    def __init__(self):
        super(_PrettyPrintFieldType, self).__init__('pprint-ir',
                                                    gdb.COMMAND_USER,
                                                    gdb.COMPLETE_EXPRESSION)
        self._typeid_to_append_attrs = {
            'CTF_TYPE_INTEGER': self._append_int_attrs,
            'CTF_TYPE_FLOAT': self._append_float_attrs,
            'CTF_TYPE_ENUM': self._append_enum_attrs,
            'CTF_TYPE_STRING': self._append_string_attrs,
            'CTF_TYPE_STRUCT': self._append_struct_attrs,
            'CTF_TYPE_ARRAY': self._append_array_attrs,
            'CTF_TYPE_SEQUENCE': self._append_sequence_attrs,
            'CTF_TYPE_VARIANT': self._append_variant_attrs,
        }

    def _print_indent(self):
        print('  ' * self._indent_level, end='')

    def _indent(self):
        self._indent_level += 1

    def _unindent(self):
        self._indent_level -= 1

    def _get_enum_member_text(self, member):
        member_int = member.cast(gdb.lookup_type('int'))

        return '{} ({})'.format(_cvalue(member), _cvalue(member_int))

    def _append_byte_order_attr(self, bo, attrs):
        if bo == 1234:
            value = '{} ({})'.format(_cvalue('BIG_ENDIAN'),
                                     _cvalue('1234'))
        elif bo == 4321:
            value = '{} ({})'.format(_cvalue('LITTLE_ENDIAN'),
                                     _cvalue('4321'))
        else:
            # try public API values
            bo_type = gdb.lookup_type('enum bt_ctf_byte_order')
            bo_enum = bo.cast(bo_type)
            value = self._get_enum_member_text(bo_enum)

        attrs.append(_Attr('byte order', _TextValue(value)))

    def _get_field_spec_type(self, field_type, type_name):
        spec_type = gdb.lookup_type('struct bt_ctf_field_type_' + type_name)
        spec_type = spec_type.pointer()

        return field_type.cast(spec_type)

    def _get_string_text(self, string):
        return '"{}"'.format(_cvalue(string))

    def _append_int_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'integer')
        decl = field_type['declaration']
        attrs.append(_Attr('size', _TextValue(_cvalue(decl['len']))))
        attrs.append(_Attr('alignment',
                           _TextValue(_cvalue(decl['p']['alignment']))))
        self._append_byte_order_attr(decl['byte_order'], attrs)
        attrs.append(_Attr('signed?',
                           _TextValue(_bool_to_yesno(decl['signedness']))))
        attrs.append(_Attr('base',
                           _TextValue(_cvalue(decl['base']))))
        encoding_text = self._get_enum_member_text(decl['encoding'])
        attrs.append(_Attr('encoding', _TextValue(encoding_text)))

    def _append_float_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'floating_point')
        decl = field_type['declaration']
        attrs.append(_Attr('exponent size',
                           _TextValue(_cvalue(decl['exp']['len']))))
        attrs.append(_Attr('mantissa size',
                     _TextValue(_cvalue(decl['mantissa']['len'] + 1))))
        attrs.append(_Attr('alignment',
                           _TextValue(_cvalue(decl['p']['alignment']))))
        self._append_byte_order_attr(decl['byte_order'], attrs)

    def _append_enum_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'enumeration')
        entries = field_type['entries']
        entries_len = int(entries['len'])
        entries_pdata = entries['pdata']
        enum_mapping_type = gdb.lookup_type('struct enumeration_mapping')
        enum_mapping_type = enum_mapping_type.pointer()

        if entries_len > 0:
            mappings = []

            for i in range(entries_len):
                entry = entries_pdata[i].cast(enum_mapping_type)
                string_quark = entry['string']
                call = 'g_quark_to_string({})'.format(string_quark)
                string = gdb.parse_and_eval(call)
                string = string.cast(gdb.lookup_type('char').pointer())
                start = entry['range_start']['_signed']
                end = entry['range_end']['_signed']
                label = self._get_string_text(string.string())
                line = '{} [{}, {}]'.format(label, _cvalue(start), _cvalue(end))
                mappings.append(_TextValue(line))
        else:
            mappings = _TextValue('none')

        attrs.append(_Attr('mappings', mappings))
        container_value = _FieldTypeValue(None, field_type['container'])
        attrs.append(_Attr('container', container_value))

    def _append_string_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'string')
        decl = field_type['declaration']
        encoding_text = self._get_enum_member_text(decl['encoding'])
        attrs.append(_Attr('encoding', _TextValue(encoding_text)))

    def _append_struct_variant_types(self, field_type, values):
        fields = field_type['fields']
        fields_len = int(fields['len'])
        fields_pdata = fields['pdata']
        structure_field_type = gdb.lookup_type('struct structure_field')
        structure_field_type = structure_field_type.pointer()

        for i in range(fields_len):
            field = fields_pdata[i].cast(structure_field_type)
            name_quark = field['name']
            call = 'g_quark_to_string({})'.format(name_quark)
            name = gdb.parse_and_eval(call)
            name = name.cast(gdb.lookup_type('char').pointer()).string()
            nested_type = field['type']
            values.append(_FieldTypeValue(name, nested_type))

    def _append_struct_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'structure')
        decl = field_type['declaration']
        call = 'bt_ctf_field_type_get_alignment({})'.format(field_type)
        alignment = gdb.parse_and_eval(call)
        attrs.append(_Attr('alignment', _TextValue(_cvalue(alignment))))
        values = []
        self._append_struct_variant_types(field_type, values)
        attrs.append(_Attr('fields', values))

    def _append_array_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'array')
        decl = field_type['declaration']
        attrs.append(_Attr('length', _TextValue(_cvalue(decl['len']))))
        ft_value = _FieldTypeValue(None, field_type['element_type'])
        attrs.append(_Attr('element type', ft_value))

    def _append_sequence_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'sequence')
        decl = field_type['declaration']
        length = field_type['length_field_name']['str']
        length = length.cast(gdb.lookup_type('char').pointer())
        length = self._get_string_text(length.string())
        attrs.append(_Attr('length', _TextValue(length)))
        ft_value = _FieldTypeValue(None, field_type['element_type'])
        attrs.append(_Attr('element type', ft_value))

    def _append_variant_attrs(self, field_type, attrs):
        field_type = self._get_field_spec_type(field_type, 'variant')
        decl = field_type['declaration']
        tag_name = field_type['tag_name']['str']
        tag_name = tag_name.cast(gdb.lookup_type('char').pointer())
        tag_name = self._get_string_text(tag_name.string())
        attrs.append(_Attr('tag name', _TextValue(tag_name)))
        ft = gdb.lookup_type('struct bt_ctf_field_type').pointer()
        tag = field_type['tag'].cast(ft)
        ft_value = _FieldTypeValue(None, tag)
        attrs.append(_Attr('tag type', ft_value))
        values = []
        self._append_struct_variant_types(field_type, values)
        attrs.append(_Attr('types', values))

    def _append_attrs(self, field_type, attrs):
        typeid = str(self._get_typeid(field_type))

        if typeid in self._typeid_to_append_attrs:
            func = self._typeid_to_append_attrs[typeid]
            func(field_type, attrs)
        else:
            raise RuntimeError()

    def _get_typeid(self, field_type):
        return field_type['declaration']['id']

    def _append_common_attrs(self, field_type, attrs):
        base = field_type['base']
        refcount = base['ref_count']['count']
        frozen = field_type['frozen']
        #valid = field_type['valid']
        typeid = self._get_typeid(field_type)
        typeid_text = self._get_enum_member_text(typeid)
        attrs.append(_Attr('type ID', _TextValue(typeid_text), True))
        attrs.append(_Attr('address', _TextValue(_cvalue(field_type)), True))
        attrs.append(_Attr('ref count', _TextValue(_cvalue(refcount)), True))
        attrs.append(_Attr('frozen?', _TextValue(_bool_to_yesno(frozen)), True))
        #attrs.append(_Attr('valid?', _TextValue(_bool_to_yesno(valid)), True))

    def _print_value(self, value):
        if type(value) is _TextValue:
            self._print_indent()
            print(value.text)
        elif type(value) is _FieldTypeValue:
            if value.name:
                self._print_indent()
                print(_ctypename(value.name))
                self._indent()
                self._print_object(value.field_type)
                self._unindent()
            else:
                self._print_object(value.field_type)

    def _print_attrs(self, attrs):
        max_name_length = max([len(a.name) for a in attrs])

        for attr in attrs:
            name = attr.name
            diff = max_name_length - len(name)
            add = ' ' * diff
            self._print_indent()

            if attr.common:
                name = _cprop_common(name)
            else:
                name = _cprop(name)

            print('{}:'.format(name), end='')

            if type(attr.value) is list:
                print()
                self._indent()

                for value in attr.value:
                    self._print_value(value)

                self._unindent()
            elif type(attr.value) is _TextValue:
                print(' {}{}'.format(add, attr.value.text))
            elif type(attr.value) is _FieldTypeValue:
                print()
                self._indent()
                self._print_value(attr.value)
                self._unindent()

    def _print_nested_types(self, nested_types):
        if not nested_types:
            return

        max_name_length = max([len(nt.name) for nt in nested_types])
        self._print_indent()
        print('{}:'.format(_cprop('nested types')))
        self._indent()

        for nt in nested_types:
            name = nt.name
            diff = max_name_length - len(nt.name)
            add = ' ' * diff
            self._print_indent()
            print('{}:'.format(_ctypename(name)))
            self._indent()
            self._print_object(nt.field_type)
            self._unindent()

        self._unindent()

    def _print_field_type(self, field_type):
        attrs = []

        try:
            self._append_common_attrs(field_type, attrs)
            self._append_attrs(field_type, attrs)
        except Exception as e:
            self._print_indent()
            print(_cerror('Error: cannot get field type infos'))
            traceback.print_exc()
            raise e

        self._print_attrs(attrs)

    def _print_event_class(self, event_class):
        pass

    def _print_object(self, obj):
        obj_type = str(obj.type.target())

        if obj == 0:
            self._print_indent()
            print('NULL')
            return

        if obj_type == 'struct bt_ctf_field_type':
            self._print_field_type(obj)
        else:
            print(_cerror('Error: unsupported type: {}'.format(obj.type)))

    def invoke(self, arg, from_tty):
        self._indent_level = 0
        obj = gdb.parse_and_eval(arg)
        self._print_object(obj)


_PrettyPrintFieldType()
