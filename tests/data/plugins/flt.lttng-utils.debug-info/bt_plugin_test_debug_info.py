import bt2
import math

bt2.register_plugin(__name__, "test_debug_info")


class CompleteIter(bt2._UserMessageIterator):
    def __init__(self, config, output_port):
        ec = output_port.user_data
        sc = ec.stream_class
        tc = sc.trace_class

        trace = tc()
        stream = trace.create_stream(sc)

        ev = self._create_event_message(ec, stream, default_clock_snapshot=123)

        ev.event.payload_field["bool"] = False
        ev.event.payload_field["real_single"] = 2.0
        ev.event.payload_field["real_double"] = math.pi
        ev.event.payload_field["int32"] = 121
        ev.event.payload_field["int3"] = -1
        ev.event.payload_field["int9_hex"] = -92
        ev.event.payload_field["uint32"] = 121
        ev.event.payload_field["uint61"] = 299792458
        ev.event.payload_field["uint5_oct"] = 29
        ev.event.payload_field["struct"]['str'] = "Rotisserie St-Hubert"
        ev.event.payload_field["struct"]['option_real'] = math.pi
        ev.event.payload_field["string"] = "🎉"
        ev.event.payload_field["dyn_array"] = [1.2, 2 / 3, 42.3, math.pi]
        ev.event.payload_field["dyn_array_len"] = 4
        ev.event.payload_field["dyn_array_with_len"] = [5.2, 5 / 3, 42.5, math.pi * 12]
        ev.event.payload_field["sta_array"] = ['🕰', '🦴', ' 🎍']
        ev.event.payload_field["option_none"]
        ev.event.payload_field["option_some"] = "NORMANDIN"
        ev.event.payload_field["option_bool_selector"] = True
        ev.event.payload_field["option_bool"] = "Mike's"
        ev.event.payload_field["option_int_selector"] = 1
        ev.event.payload_field["option_int"] = "Barbies resto bar grill"
        ev.event.payload_field['variant'].selected_option_index = 0
        ev.event.payload_field["variant"] = "Couche-Tard"

        self._msgs = [
            self._create_stream_beginning_message(stream),
            ev,
            self._create_stream_end_message(stream),
        ]

    def __next__(self):
        if len(self._msgs) > 0:
            return self._msgs.pop(0)
        else:
            raise StopIteration


@bt2.plugin_component_class
class CompleteSrc(bt2._UserSourceComponent, message_iterator_class=CompleteIter):
    def __init__(self, config, params, obj):
        tc = self._create_trace_class()
        cc = self._create_clock_class()
        sc = tc.create_stream_class(default_clock_class=cc)

        dyn_array_elem_fc = tc.create_double_precision_real_field_class()
        dyn_array_with_len_elem_fc = tc.create_double_precision_real_field_class()
        dyn_array_with_len_fc = tc.create_unsigned_integer_field_class(19)
        sta_array_elem_fc = tc.create_string_field_class()
        option_some_fc = tc.create_string_field_class()
        variant_fc = tc.create_variant_field_class()
        variant_fc.append_option(
            name='var_str', field_class=tc.create_string_field_class()
        )
        option_none_fc = tc.create_double_precision_real_field_class()
        struct_fc = tc.create_structure_field_class()
        struct_option_fc = tc.create_double_precision_real_field_class()
        struct_fc.append_member('str', tc.create_string_field_class())
        struct_fc.append_member(
            'option_real',
            tc.create_option_without_selector_field_class(struct_option_fc),
        )
        option_bool_selector_fc = tc.create_bool_field_class()
        option_int_selector_fc = tc.create_unsigned_integer_field_class(8)

        payload = tc.create_structure_field_class()
        payload += [
            ("bool", tc.create_bool_field_class()),
            ("real_single", tc.create_single_precision_real_field_class()),
            ("real_double", tc.create_double_precision_real_field_class()),
            ("int32", tc.create_signed_integer_field_class(32)),
            ("int3", tc.create_signed_integer_field_class(3)),
            (
                "int9_hex",
                tc.create_signed_integer_field_class(
                    9, preferred_display_base=bt2.IntegerDisplayBase.HEXADECIMAL
                ),
            ),
            ("uint32", tc.create_unsigned_integer_field_class(32)),
            ("uint61", tc.create_unsigned_integer_field_class(61)),
            (
                "uint5_oct",
                tc.create_unsigned_integer_field_class(
                    5, preferred_display_base=bt2.IntegerDisplayBase.OCTAL
                ),
            ),
            ("struct", struct_fc),
            ("string", tc.create_string_field_class()),
            ("dyn_array", tc.create_dynamic_array_field_class(dyn_array_elem_fc)),
            ("dyn_array_len", dyn_array_with_len_fc),
            (
                "dyn_array_with_len",
                tc.create_dynamic_array_field_class(
                    dyn_array_with_len_elem_fc, length_fc=dyn_array_with_len_fc
                ),
            ),
            ("sta_array", tc.create_dynamic_array_field_class(sta_array_elem_fc)),
            (
                "option_none",
                tc.create_option_without_selector_field_class(option_none_fc),
            ),
            (
                "option_some",
                tc.create_option_without_selector_field_class(option_some_fc),
            ),
            ("option_bool_selector", option_bool_selector_fc),
            (
                "option_bool",
                tc.create_option_with_bool_selector_field_class(
                    tc.create_string_field_class(), option_bool_selector_fc
                ),
            ),
            (
                "option_bool_reversed",
                tc.create_option_with_bool_selector_field_class(
                    tc.create_string_field_class(),
                    option_bool_selector_fc,
                    selector_is_reversed=True,
                ),
            ),
            ("option_int_selector", option_int_selector_fc),
            (
                "option_int",
                tc.create_option_with_integer_selector_field_class(
                    tc.create_string_field_class(),
                    option_int_selector_fc,
                    bt2.UnsignedIntegerRangeSet([(1, 3), (18, 44)]),
                ),
            ),
            ("variant", variant_fc),
        ]
        ec = sc.create_event_class(name="my-event", payload_field_class=payload)

        self._add_output_port("some-name", ec)
