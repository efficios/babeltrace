# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2020 Geneviève Bastien <gbastien@versatic.net>

import bt2


class TheIteratorOfProblems(bt2._UserMessageIterator):
    def __init__(self, config, port):
        tc, sc, ec1, params = port.user_data
        trace = tc()
        stream = trace.create_stream(sc)
        event_value = params["value"]
        self._msgs = []

        self._msgs.append(self._create_stream_beginning_message(stream))

        ev_msg1 = self._create_event_message(ec1, stream)
        ev_msg1.event.payload_field["enum_field"] = event_value

        self._msgs.append(ev_msg1)

        self._msgs.append(self._create_stream_end_message(stream))

        self._at = 0
        config.can_seek_forward = True

    def _user_seek_beginning(self):
        self._at = 0

    def __next__(self):
        if self._at < len(self._msgs):
            msg = self._msgs[self._at]
            self._at += 1
            return msg
        else:
            raise StopIteration


@bt2.plugin_component_class
class TheSourceOfProblems(
    bt2._UserSourceComponent, message_iterator_class=TheIteratorOfProblems
):
    def __init__(self, config, params, obj):
        tc = self._create_trace_class()

        enum_values_str = params["enum-values"]

        sc = tc.create_stream_class()

        # Create the enumeration field with the values in parameter
        if params["enum-signed"]:
            enumfc = tc.create_signed_enumeration_field_class()
        else:
            enumfc = tc.create_unsigned_enumeration_field_class()

        groups = str(enum_values_str).split(" ")
        mappings = {}
        range_set_type = (
            bt2.SignedIntegerRangeSet
            if params["enum-signed"]
            else bt2.UnsignedIntegerRangeSet
        )
        for group in groups:
            label, low, high = group.split(",")

            if label not in mappings.keys():
                mappings[label] = range_set_type()

            mappings[label].add((int(low), int(high)))

        for x, y in mappings.items():
            enumfc.add_mapping(x, y)

        # Create the struct field to contain the enum field class
        struct_fc = tc.create_structure_field_class()
        struct_fc.append_member("enum_field", enumfc)

        # Create an event class on this stream with the struct field
        ec1 = sc.create_event_class(name="with_enum", payload_field_class=struct_fc)
        self._add_output_port("out", (tc, sc, ec1, params))


bt2.register_plugin(__name__, "test-pretty")
