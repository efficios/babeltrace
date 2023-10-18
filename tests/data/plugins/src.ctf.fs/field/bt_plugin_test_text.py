# SPDX-License-Identifier: MIT
#
# Copyright (c) 2023 Olivier Dion <odion@efficios.com>
# Copyright (c) 2024 Philippe Proulx <pproulx@efficios.com>

import bt2

_array_elem = object()


# Recursively prints the contents of `field` with the indentation level
# `indent` and the introduction `intro`.
#
# `intro` is one of:
#
# `None`:
#     No introduction (root field).
#
# A string:
#     Structure field member name.
#
# `_array_elem`:
#     `field` is an array field element.
def _print_field(intro, field, indent=0):
    indent_str = " " * indent * 2
    intro_str = ""

    if intro is _array_elem:
        intro_str = "- "
    elif intro is not None:
        intro_str = "{}: ".format(intro)

    if isinstance(field, bt2._StringFieldConst):
        print('{}{}"{}"'.format(indent_str, intro_str, field))
    elif isinstance(field, bt2._StructureFieldConst):
        print(indent_str + intro_str, end="")

        if len(field) == 0:
            # Special case for an empty structure field
            print("{}")
        else:
            if intro is _array_elem:
                # Structure field is an array field element itself:
                # print the first element first, and then print the
                # remaining ones indented.
                #
                # Example:
                #
                #     - meow: "mix"
                #       montant: -23.599312
                #       bateau: "jacques"
                sub_field_names = list(field)
                _print_field(sub_field_names[0], field[sub_field_names[0]], 0)

                for sub_field_name in sub_field_names[1:]:
                    _print_field(sub_field_name, field[sub_field_name], indent + 1)
            else:
                add_indent = 0

                if intro is not None:
                    # Structure field has a name (already printed at
                    # this point): print a newline, and then print all
                    # the members indented (one more level):
                    #
                    # Example:
                    #
                    #     struct_name:
                    #       meow: "mix"
                    #       montant: -23.599312
                    #       bateau: "jacques"
                    add_indent = 1
                    print()

                for sub_field_name in field:
                    _print_field(
                        sub_field_name,
                        field[sub_field_name],
                        indent + add_indent,
                    )
    elif isinstance(field, bt2._ArrayFieldConst):
        add_indent = 0

        if intro is not None:
            # Array field has an intro: print it, then print a newline,
            # and then print all the elements indented (one more level).
            #
            # Example 1 (parent is an structure field):
            #
            #     array_name:
            #       - -17
            #       - "salut"
            #       - 23
            #
            # Example 2 (parent is an array field):
            #
            #     -
            #       - -17
            #       - "salut"
            #       - 23
            add_indent = 1
            print(indent_str + intro_str.rstrip())

        for sub_field in field:
            _print_field(_array_elem, sub_field, indent + add_indent)
    elif isinstance(field.cls, bt2._IntegerFieldClassConst):
        # Honor the preferred display base
        base = field.cls.preferred_display_base
        print(indent_str + intro_str, end="")

        if base == 10:
            print(int(field))
        elif base == 16:
            print(hex(field))
        elif base == 8:
            print(oct(field))
        elif base == 2:
            print(bin(field))
    elif isinstance(field, bt2._BitArrayFieldConst):
        print(indent_str + intro_str + bin(field))
    elif isinstance(field, bt2._BoolFieldConst):
        print(indent_str + intro_str + ("yes" if field else "no"))
    elif isinstance(field, bt2._RealFieldConst):
        print("{}{}{:.6f}".format(indent_str, intro_str, float(field)))
    elif isinstance(field, bt2._OptionFieldConst):
        if field.has_field:
            _print_field(intro, field.field, indent)
        else:
            # Special case for an option field without a field
            print("{}{}~".format(indent_str, intro_str))
    elif isinstance(field, bt2._VariantFieldConst):
        _print_field(intro, field.selected_option, indent)
    else:
        print(indent_str + intro_str + field)


@bt2.plugin_component_class
class _SingleSinkComponent(bt2._UserSinkComponent, name="single"):
    def __init__(self, config, params, obj):
        self._input = self._add_input_port("input")
        self._field_name = str(params.get("field-name", "root"))

    def _user_graph_is_configured(self):
        self._it = self._create_message_iterator(self._input)

    def _user_consume(self):
        msg = next(self._it)

        if type(msg) is bt2._EventMessageConst:
            assert self._field_name in msg.event.payload_field
            assert len(msg.event.payload_field) == 1
            _print_field(None, msg.event.payload_field[self._field_name])


bt2.register_plugin(__name__, "test-text")
