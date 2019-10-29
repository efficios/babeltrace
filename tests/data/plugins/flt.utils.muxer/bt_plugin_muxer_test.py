import bt2


class TheIteratorOfConfusion(bt2._UserMessageIterator):
    def __init__(self, config, port):
        self._at = 0
        test_name = port.user_data[0]
        TEST_CASES[test_name].create_msgs(self, port.user_data[1:])

    def __next__(self):
        if self._at < len(self._msgs):
            msg = self._msgs[self._at]
            self._at += 1
            return msg
        raise StopIteration


@bt2.plugin_component_class
class TheSourceOfConfusion(
    bt2._UserSourceComponent, message_iterator_class=TheIteratorOfConfusion
):
    def __init__(self, config, params, obj):
        test_name = str(params['test-name'])

        TEST_CASES[test_name].source_setup(self, test_name)


class DiffTraceName:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class()
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class()
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        trace_name1 = 'rouyn'
        trace_name2 = 'noranda'

        src._add_output_port('out1', (test_name, 1, tc1, cc1, trace_name1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, trace_name2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, trace_name = params
        trace = tc(name=trace_name)
        sc = tc.create_stream_class(
            default_clock_class=cc, assigns_automatic_stream_id=False
        )
        stream = trace.create_stream(sc, 0)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class DiffStreamName:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class()
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class()
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        stream_name1 = 'port-daniel'
        stream_name2 = 'gascon'

        src._add_output_port('out1', (test_name, 1, tc1, cc1, stream_name1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, stream_name2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, stream_name = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc, assigns_automatic_stream_id=False
        )
        stream = trace.create_stream(sc, 0, stream_name)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class DiffStreamId:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class()
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class()
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        stream_id1 = 18
        stream_id2 = 23

        src._add_output_port('out1', (test_name, 1, tc1, cc1, stream_id1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, stream_id2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, stream_id = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc, assigns_automatic_stream_id=False
        )
        stream = trace.create_stream(sc, stream_id)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class DiffStreamNoName:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class()
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class()
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        stream_name1 = "one"
        stream_name2 = None

        src._add_output_port('out1', (test_name, 1, tc1, cc1, stream_name1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, stream_name2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, stream_no_name = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc, assigns_automatic_stream_id=False
        )
        stream = trace.create_stream(sc, 0, name=stream_no_name)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class DiffStreamClassId:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        stream_class_id1 = 18
        stream_class_id2 = 23

        src._add_output_port('out1', (test_name, 1, tc1, cc1, stream_class_id1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, stream_class_id2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, stream_class_id = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc,
            id=stream_class_id,
            assigns_automatic_stream_id=False,
        )
        stream = trace.create_stream(sc, 0)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class DiffStreamClassName:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        stream_class_name1 = 'one'
        stream_class_name2 = 'two'

        src._add_output_port('out1', (test_name, 1, tc1, cc1, stream_class_name1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, stream_class_name2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, stream_class_name = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc,
            id=0,
            name=stream_class_name,
            assigns_automatic_stream_id=False,
        )
        stream = trace.create_stream(sc, 0)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class DiffStreamClassNoName:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        stream_class_name1 = 'one'
        stream_class_name2 = None

        src._add_output_port('out1', (test_name, 1, tc1, cc1, stream_class_name1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, stream_class_name2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, stream_class_name = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc,
            id=0,
            name=stream_class_name,
            assigns_automatic_stream_id=False,
        )
        stream = trace.create_stream(sc, 0)

        sb_msg = msg_iter._create_stream_beginning_message(stream, 0)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class BasicTimestampOrdering:
    def source_setup(src, test_name):
        tc = src._create_trace_class()
        cc = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        timestamp1 = 0
        timestamp2 = 120
        timestamp3 = 4

        src._add_output_port('out1', (test_name, 1, tc, cc, timestamp1))
        src._add_output_port('out2', (test_name, 2, tc, cc, timestamp2))
        src._add_output_port('out3', (test_name, 3, tc, cc, timestamp3))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, timestamp = params
        trace = tc()
        sc = tc.create_stream_class(default_clock_class=cc)
        stream = trace.create_stream(sc)

        sb_msg = msg_iter._create_stream_beginning_message(stream, timestamp)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, se_msg]


class MultiIterOrdering:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        tc2 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        tc3 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        tc4 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        src._add_output_port('out1', (test_name, 1, tc1, cc))
        src._add_output_port('out2', (test_name, 2, tc2, cc))
        src._add_output_port('out3', (test_name, 3, tc3, cc))
        src._add_output_port('out4', (test_name, 4, tc4, cc))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc = params
        trace_hello = tc(name='hello')
        trace_meow = tc(name='meow')

        # Craft list of messages for each iterator so that the last messages of
        # each iterator are all sharing the same timestamp.
        clock_snapshot_value = 25

        if iter_id == 1:
            # Event, 2500 ns, trace "hello", stream class 0, stream 1
            stream_class0 = tc.create_stream_class(
                id=0, default_clock_class=cc, assigns_automatic_stream_id=False
            )
            sc_0_stream_1 = trace_hello.create_stream(stream_class0, id=1)
            event_class = stream_class0.create_event_class(name='saumon atlantique')

            msg_iter._msgs = [
                msg_iter._create_stream_beginning_message(sc_0_stream_1, 0),
                msg_iter._create_event_message(
                    event_class, sc_0_stream_1, clock_snapshot_value
                ),
                msg_iter._create_stream_end_message(sc_0_stream_1, iter_id * 193),
            ]
        elif iter_id == 2:
            # Packet beginning, 2500 ns, trace "meow", stream class 0, stream 1
            stream_class0 = tc.create_stream_class(
                id=0,
                default_clock_class=cc,
                supports_packets=True,
                packets_have_beginning_default_clock_snapshot=True,
                packets_have_end_default_clock_snapshot=True,
                assigns_automatic_stream_id=False,
            )

            sc_0_stream_1 = trace_meow.create_stream(stream_class0, id=1)
            packet = sc_0_stream_1.create_packet()

            msg_iter._msgs = [
                msg_iter._create_stream_beginning_message(sc_0_stream_1, 1),
                msg_iter._create_packet_beginning_message(packet, clock_snapshot_value),
                msg_iter._create_packet_end_message(packet, iter_id * 79),
                msg_iter._create_stream_end_message(sc_0_stream_1, iter_id * 193),
            ]
        elif iter_id == 3:
            # Stream beginning, 2500 ns, trace "hello", stream class 0, stream 0
            stream_class0 = tc.create_stream_class(
                id=0, default_clock_class=cc, assigns_automatic_stream_id=False
            )

            sc_0_stream_0 = trace_hello.create_stream(stream_class0, id=0)

            msg_iter._msgs = [
                msg_iter._create_stream_beginning_message(
                    sc_0_stream_0, clock_snapshot_value
                ),
                msg_iter._create_stream_end_message(sc_0_stream_0, iter_id * 193),
            ]
        elif iter_id == 4:
            # Event, 2500 ns, trace "meow", stream class 1, stream 1
            stream_class1 = tc.create_stream_class(
                id=1, default_clock_class=cc, assigns_automatic_stream_id=False
            )

            sc_1_stream_1 = trace_meow.create_stream(stream_class1, id=1)

            event_class = stream_class1.create_event_class(name='bar rayé')
            msg_iter._msgs = [
                msg_iter._create_stream_beginning_message(sc_1_stream_1, 3),
                msg_iter._create_event_message(
                    event_class, sc_1_stream_1, clock_snapshot_value
                ),
                msg_iter._create_stream_end_message(sc_1_stream_1, iter_id * 193),
            ]


class DiffEventClassName:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        event_class_name1 = 'Hull'
        event_class_name2 = 'Gatineau'

        src._add_output_port('out1', (test_name, 1, tc1, cc1, event_class_name1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, event_class_name2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, event_class_name = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc,
            id=0,
            assigns_automatic_stream_id=False,
            supports_packets=False,
        )
        ec = sc.create_event_class(name=event_class_name)

        stream = trace.create_stream(sc, 0)

        # Use event class name length as timestamp so that both stream
        # beginning message are not at the same time. This test is targetting
        # event message.
        sb_msg = msg_iter._create_stream_beginning_message(stream, len(ec.name))
        ev_msg = msg_iter._create_event_message(ec, stream, 50)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, ev_msg, se_msg]


class DiffEventClassId:
    def source_setup(src, test_name):
        tc1 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc1 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))
        tc2 = src._create_trace_class(assigns_automatic_stream_class_id=False)
        cc2 = src._create_clock_class(frequency=1, offset=bt2.ClockClassOffset(0))

        event_class_id1 = 1
        event_class_id2 = 2

        src._add_output_port('out1', (test_name, 1, tc1, cc1, event_class_id1))
        src._add_output_port('out2', (test_name, 2, tc2, cc2, event_class_id2))

    def create_msgs(msg_iter, params):
        iter_id, tc, cc, event_class_id = params
        trace = tc()
        sc = tc.create_stream_class(
            default_clock_class=cc,
            id=0,
            assigns_automatic_stream_id=False,
            assigns_automatic_event_class_id=False,
            supports_packets=False,
        )
        ec = sc.create_event_class(id=event_class_id)

        stream = trace.create_stream(sc, 0)

        # Use event class id as timestamp so that both stream beginning message
        # are not at the same time. This test is targetting event message.
        sb_msg = msg_iter._create_stream_beginning_message(stream, ec.id)
        ev_msg = msg_iter._create_event_message(ec, stream, 50)
        se_msg = msg_iter._create_stream_end_message(stream, iter_id * 193)

        msg_iter._msgs = [sb_msg, ev_msg, se_msg]


class DiffInactivityMsgCs:
    def source_setup(src, test_name):
        cc1 = src._create_clock_class(
            frequency=1, name='La Baie', offset=bt2.ClockClassOffset(0)
        )
        cc2 = src._create_clock_class(
            frequency=1, name='Chicoutimi', offset=bt2.ClockClassOffset(0)
        )

        src._add_output_port('out1', (test_name, cc1))
        src._add_output_port('out2', (test_name, cc2))

    def create_msgs(msg_iter, params):
        (cc,) = params
        sb_msg = msg_iter._create_message_iterator_inactivity_message(cc, 0)
        msg_iter._msgs = [sb_msg]


TEST_CASES = {
    'diff_trace_name': DiffTraceName,
    'diff_event_class_name': DiffEventClassName,
    'diff_event_class_id': DiffEventClassId,
    'diff_stream_name': DiffStreamName,
    'diff_stream_no_name': DiffStreamNoName,
    'diff_stream_id': DiffStreamId,
    'diff_stream_class_id': DiffStreamClassId,
    'diff_stream_class_name': DiffStreamClassName,
    'diff_stream_class_no_name': DiffStreamClassNoName,
    'diff_inactivity_msg_cs': DiffInactivityMsgCs,
    'basic_timestamp_ordering': BasicTimestampOrdering,
    'multi_iter_ordering': MultiIterOrdering,
}

bt2.register_plugin(__name__, 'test-muxer')
