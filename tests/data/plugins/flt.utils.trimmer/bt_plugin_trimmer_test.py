import bt2


class TheIteratorOfAllEvil(bt2._UserMessageIterator):
    def __init__(self, port):
        tc, sc, ec1, ec2, params = port.user_data
        trace = tc()
        stream = trace.create_stream(sc)

        # Test with and without packets, once packets are optional.
        packet = stream.create_packet()

        if params['with-stream-msgs-cs']:
            sb_msg = self._create_stream_beginning_message(stream, 100)
        else:
            sb_msg = self._create_stream_beginning_message(stream)

        ev_msg1 = self._create_event_message(ec1, packet, 300)
        ev_msg2 = self._create_event_message(ec2, packet, 400)

        if params['with-stream-msgs-cs']:
            se_msg = self._create_stream_end_message(stream, 1000)
        else:
            se_msg = self._create_stream_end_message(stream)

        self._msgs = [
            sb_msg,
            self._create_packet_beginning_message(packet, 200),
            ev_msg1,
            ev_msg2,
            self._create_packet_end_message(packet, 900),
            se_msg,
        ]
        self._at = 0

    def _seek_beginning(self):
        self._at = 0

    def __next__(self):
        if self._at < len(self._msgs):
            msg = self._msgs[self._at]
            self._at += 1
            return msg
        else:
            raise StopIteration

@bt2.plugin_component_class
class TheSourceOfAllEvil(bt2._UserSourceComponent,
        message_iterator_class=TheIteratorOfAllEvil):
    def __init__(self, params):
        tc = self._create_trace_class()

        # Use a clock class with an offset, so we can test with --begin or --end
        # smaller than this offset (in other words, a time that it's not
        # possible to represent with this clock class).
        cc = self._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(10000))
        sc = tc.create_stream_class(default_clock_class=cc,
                                    packets_have_beginning_default_clock_snapshot=True,
                                    packets_have_end_default_clock_snapshot=True)
        ec1 = sc.create_event_class(name='event 1')
        ec2 = sc.create_event_class(name='event 2')
        self._add_output_port('out', (tc, sc, ec1, ec2, params))


bt2.register_plugin(__name__, 'test-trimmer')
