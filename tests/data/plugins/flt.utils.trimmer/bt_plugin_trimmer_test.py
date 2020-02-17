import bt2


class TheIteratorOfAllEvil(bt2._UserMessageIterator):
    def __init__(self, config, port):
        tc, sc, ec1, ec2, params = port.user_data
        trace = tc()
        stream = trace.create_stream(sc)

        if params['with-packet-msgs']:
            packet = stream.create_packet()

        if params['with-stream-msgs-cs']:
            sb_msg = self._create_stream_beginning_message(stream, 100)
        else:
            sb_msg = self._create_stream_beginning_message(stream)

        parent = packet if params['with-packet-msgs'] else stream

        ev_msg1 = self._create_event_message(ec1, parent, 300)
        ev_msg2 = self._create_event_message(ec2, parent, 400)

        if params['with-stream-msgs-cs']:
            se_msg = self._create_stream_end_message(stream, 1000)
        else:
            se_msg = self._create_stream_end_message(stream)

        self._msgs = []

        self._msgs.append(sb_msg)

        if params['with-packet-msgs']:
            self._msgs.append(self._create_packet_beginning_message(packet, 200))

        self._msgs.append(ev_msg1)
        self._msgs.append(ev_msg2)

        if params['with-packet-msgs']:
            self._msgs.append(self._create_packet_end_message(packet, 900))

        self._msgs.append(se_msg)

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
class TheSourceOfAllEvil(
    bt2._UserSourceComponent, message_iterator_class=TheIteratorOfAllEvil
):
    def __init__(self, config, params, obj):
        tc = self._create_trace_class()

        with_packets = bool(params['with-packet-msgs'])

        # Use a clock class with an offset, so we can test with --begin or --end
        # smaller than this offset (in other words, a time that it's not
        # possible to represent with this clock class).
        cc = self._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(10000))
        sc = tc.create_stream_class(
            default_clock_class=cc,
            supports_packets=with_packets,
            packets_have_beginning_default_clock_snapshot=with_packets,
            packets_have_end_default_clock_snapshot=with_packets,
        )
        ec1 = sc.create_event_class(name='event 1')
        ec2 = sc.create_event_class(name='event 2')
        self._add_output_port('out', (tc, sc, ec1, ec2, params))


bt2.register_plugin(__name__, 'test-trimmer')
