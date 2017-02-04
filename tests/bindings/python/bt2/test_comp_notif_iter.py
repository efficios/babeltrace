from collections import OrderedDict
import unittest
import copy
import bt2.notification_iterator
import bt2


def _create_ec():
    # clock class
    cc = bt2.ClockClass('salut_clock')

    # event header
    eh = bt2.StructureFieldType()
    eh += OrderedDict((
        ('id', bt2.IntegerFieldType(8)),
        ('ts', bt2.IntegerFieldType(32, mapped_clock_class=cc)),
    ))

    # packet context
    pc = bt2.StructureFieldType()
    pc += OrderedDict((
        ('something', bt2.IntegerFieldType(8)),
    ))

    # stream class
    sc = bt2.StreamClass()
    sc.packet_context_field_type = pc
    sc.event_header_field_type = eh
    sc.event_context_field_type = None

    # event payload
    ep = bt2.StructureFieldType()
    ep += OrderedDict((
        ('mosquito', bt2.StringFieldType()),
    ))

    # event class
    event_class = bt2.EventClass('ec', id=0)
    event_class.context_field_type = None
    event_class.payload_field_type = ep
    sc.add_event_class(event_class)

    # packet header
    ph = bt2.StructureFieldType()
    ph += OrderedDict((
        ('magic', bt2.IntegerFieldType(32)),
        ('stream_id', bt2.IntegerFieldType(16)),
    ))

    # trace
    trace = bt2.Trace()
    trace.packet_header_field_type = ph
    trace.add_clock_class(cc)
    trace.add_stream_class(sc)
    return event_class


def _create_event(event_class, msg):
    ev = event_class()
    ev.header_field['id'] = 0
    ev.header_field['ts'] = 19487
    ev.payload_field['mosquito'] = msg
    return ev


def _create_packet(stream):
    packet = stream.create_packet()
    packet.header_field['magic'] = 0xc1fc1fc1
    packet.header_field['stream_id'] = 0
    packet.context_field['something'] = 194
    return packet


def _create_source():
    class MyIter(bt2.UserNotificationIterator):
        def __init__(self):
            self._event_class = self.component._event_class
            self._stream = self.component._stream
            self._packet = _create_packet(self._stream)
            self._at = 0
            self._cur_notif = None

        def _get(self):
            if self._cur_notif is None:
                raise bt2.Error('nothing here!')

            return self._cur_notif

        def _next(self):
            if self._at == 0:
                notif = bt2.BeginningOfPacketNotification(self._packet)
            elif self._at < 5:
                ev = _create_event(self._event_class, 'at {}'.format(self._at))
                ev.packet = self._packet
                notif = bt2.TraceEventNotification(ev)
            elif self._at == 5:
                notif = bt2.EndOfPacketNotification(self._packet)
            elif self._at == 6:
                notif = bt2.EndOfStreamNotification(self._stream)
            else:
                raise bt2.Stop

            self._at += 1
            self._cur_notif = notif

    class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
        def __init__(self):
            self._event_class = _create_ec()
            self._stream = self._event_class.stream_class(name='abcdef')

    return MySource()


class GenCompClassTestCase(unittest.TestCase):
    def test_attr_name(self):
        class MySink(bt2.UserSinkComponent):
            def _consume(self):
                pass

        sink = MySink()
        self.assertEqual(sink.component_class.name, 'MySink')

    def test_attr_description(self):
        class MySink(bt2.UserSinkComponent):
            'hello'

            def _consume(self):
                pass

        sink = MySink()
        self.assertEqual(sink.component_class.description, 'hello')

    def test_instantiate(self):
        class MySink(bt2.UserSinkComponent):
            'hello'

            def __init__(self, params=None, name=None):
                if params is None or name is None:
                    return

                nonlocal nl_params_a, nl_name
                nl_params_a = params['a']
                nl_name = name

            def _consume(self):
                pass

        nl_params_a = None
        nl_name = None
        sink = MySink()
        self.assertIsNone(nl_params_a)
        self.assertIsNone(nl_name)
        gen_comp_class = sink.component_class
        sink2 = gen_comp_class(params={'a': 23}, name='salut')
        self.assertEqual(nl_params_a, 23)
        self.assertEqual(nl_name, 'salut')


class UserCompClassTestCase(unittest.TestCase):
    def test_attr_name(self):
        class MySink(bt2.UserSinkComponent):
            def _consume(self):
                pass

        self.assertEqual(MySink.name, 'MySink')

    def test_attr_name_param(self):
        class MySink(bt2.UserSinkComponent, name='my custom name'):
            def _consume(self):
                pass

        self.assertEqual(MySink.name, 'my custom name')

    def test_attr_description(self):
        class MySink(bt2.UserSinkComponent):
            'here is your description'

            def _consume(self):
                pass

        self.assertEqual(MySink.description, 'here is your description')

    def test_init(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                nonlocal inited
                inited = True

            def _consume(self):
                pass

        inited = False
        sink = MySink()
        self.assertTrue(inited)

    def test_init_raises(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                raise RuntimeError('oops')

            def _consume(self):
                pass

        with self.assertRaises(RuntimeError):
            sink = MySink()

    def test_destroy(self):
        class MySink(bt2.UserSinkComponent):
            def _destroy(self):
                nonlocal destroyed
                destroyed = True

            def _consume(self):
                pass

        destroyed = False
        sink = MySink()
        del sink
        self.assertTrue(destroyed)

    def test_destroy_raises(self):
        class MySink(bt2.UserSinkComponent):
            def _destroy(self):
                raise RuntimeError('oh oh')
                nonlocal destroyed
                destroyed = True

            def _consume(self):
                pass

        destroyed = False
        sink = MySink()
        del sink
        self.assertFalse(destroyed)

    def test_comp_attr_name(self):
        class MySink(bt2.UserSinkComponent):
            def _consume(self):
                pass

        sink = MySink(name='salut')
        self.assertEqual(sink.name, 'salut')

    def test_comp_attr_no_name(self):
        class MySink(bt2.UserSinkComponent):
            def _consume(self):
                pass

        sink = MySink()
        self.assertIsNone(sink.name)

    def test_comp_attr_class(self):
        class MySink(bt2.UserSinkComponent):
            def _consume(self):
                pass

        sink = MySink()
        self.assertEqual(sink.component_class.name, 'MySink')


class UserSinkCompClassTestCase(unittest.TestCase):
    def test_missing_consume(self):
        with self.assertRaises(bt2.IncompleteUserClassError):
            class MySink(bt2.UserSinkComponent):
                pass

    def test_set_min_input(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                self._maximum_input_notification_iterator_count = 10
                self._minimum_input_notification_iterator_count = 5

            def _consume(self):
                pass

        sink = MySink()

    def test_set_max_input(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                self._maximum_input_notification_iterator_count = 5

            def _consume(self):
                pass

        sink = MySink()

    def test_consume(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                pass

            def _consume(self):
                nonlocal consumed
                consumed = True

        sink = MySink()
        consumed = False
        source = _create_source()
        notif_iter = source.create_notification_iterator()
        sink.add_notification_iterator(notif_iter)
        sink.consume()
        self.assertTrue(consumed)

    def test_consume_raises(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                pass

            def _consume(self):
                raise RuntimeError('hello')

        sink = MySink()
        source = _create_source()
        notif_iter = source.create_notification_iterator()
        sink.add_notification_iterator(notif_iter)

        with self.assertRaises(bt2.Error):
            sink.consume()

    def test_add_notification_iterator(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                pass

            def _consume(self):
                pass

            def _add_notification_iterator(self, notif_iter):
                nonlocal added
                added = True

        sink = MySink()
        added = False
        source = _create_source()
        notif_iter = source.create_notification_iterator()
        sink.add_notification_iterator(notif_iter)
        self.assertTrue(added)

    def test_input_notif_iterators(self):
        class MySink(bt2.UserSinkComponent):
            def __init__(self):
                self._maximum_input_notification_iterator_count = 5

            def _consume(self):
                nonlocal count
                count = 0

                for notif_iter in self._input_notification_iterators:
                    count += 1

        sink = MySink()
        count = 0
        source = _create_source()
        notif_iter1 = source.create_notification_iterator()
        notif_iter2 = source.create_notification_iterator()
        sink.add_notification_iterator(notif_iter1)
        sink.add_notification_iterator(notif_iter2)
        sink.consume()
        self.assertEqual(count, 2)


class UserSourceCompClassTestCase(unittest.TestCase):
    def test_missing_notif_iterator_class(self):
        with self.assertRaises(bt2.IncompleteUserClassError):
            class MySource(bt2.UserSourceComponent):
                pass

    def test_invalid_notif_iterator_class(self):
        class Lel:
            pass

        with self.assertRaises(bt2.IncompleteUserClassError):
            class MySource(bt2.UserSourceComponent, notification_iterator_class=Lel):
                pass

    def test_notif_iterator_class_missing_get(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

        with self.assertRaises(bt2.IncompleteUserClassError):
            class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
                pass

    def test_notif_iterator_class_missing_next(self):
        class MyIter(bt2.UserNotificationIterator):
            def _get(self):
                pass

        with self.assertRaises(bt2.IncompleteUserClassError):
            class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
                pass

    def test_create_notification_iterator(self):
        class MyIter(bt2.UserNotificationIterator):
            def __init__(self):
                nonlocal created
                created = True

            def _next(self):
                pass

            def _get(self):
                pass

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        created = False
        source = MySource()
        notif_iter = source.create_notification_iterator()
        self.assertTrue(created)


class UserSourceCompClassTestCase(unittest.TestCase):
    def test_missing_notif_iterator_class(self):
        with self.assertRaises(bt2.IncompleteUserClassError):
            class MyFilter(bt2.UserFilterComponent):
                pass

    def test_invalid_notif_iterator_class(self):
        class Lel:
            pass

        with self.assertRaises(bt2.IncompleteUserClassError):
            class MyFilter(bt2.UserFilterComponent, notification_iterator_class=Lel):
                pass

    def test_notif_iterator_class_missing_get(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

        with self.assertRaises(bt2.IncompleteUserClassError):
            class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
                pass

    def test_notif_iterator_class_missing_next(self):
        class MyIter(bt2.UserNotificationIterator):
            def _get(self):
                pass

        with self.assertRaises(bt2.IncompleteUserClassError):
            class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
                pass

    def test_create_notification_iterator(self):
        class MyIter(bt2.UserNotificationIterator):
            def __init__(self):
                nonlocal created
                created = True

            def _next(self):
                pass

            def _get(self):
                pass

        class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
            pass

        created = False
        filter = MyFilter()
        notif_iter = filter.create_notification_iterator()
        self.assertTrue(created)

    def test_set_min_input(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

            def _get(self):
                pass

        class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
            def __init__(self):
                self._maximum_input_notification_iterator_count = 10
                self._minimum_input_notification_iterator_count = 5

        filter = MyFilter()

    def test_set_max_input(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

            def _get(self):
                pass

        class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
            def __init__(self):
                self._maximum_input_notification_iterator_count = 5

        filter = MyFilter()

    def test_add_notification_iterator(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

            def _get(self):
                pass

        class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
            def __init__(self):
                pass

            def _add_notification_iterator(self, notif_iter):
                nonlocal added
                added = True

        filter = MyFilter()
        added = False
        source = _create_source()
        notif_iter = source.create_notification_iterator()
        filter.add_notification_iterator(notif_iter)
        self.assertTrue(added)

    def test_input_notif_iterators(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

            def _get(self):
                pass

        class MyFilter(bt2.UserFilterComponent, notification_iterator_class=MyIter):
            def __init__(self):
                self._maximum_input_notification_iterator_count = 5

            def _test(self):
                nonlocal count
                count = 0

                for notif_iter in self._input_notification_iterators:
                    count += 1

        filter = MyFilter()
        count = 0
        source = _create_source()
        notif_iter1 = source.create_notification_iterator()
        notif_iter2 = source.create_notification_iterator()
        filter.add_notification_iterator(notif_iter1)
        filter.add_notification_iterator(notif_iter2)
        filter._test()
        self.assertEqual(count, 2)


class UserNotificationIteratorTestCase(unittest.TestCase):
    def test_init(self):
        class MyIter(bt2.UserNotificationIterator):
            def __init__(self):
                nonlocal inited
                inited = True

            def _next(self):
                pass

            def _get(self):
                pass

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        inited = False
        source = MySource()
        notif_iter = source.create_notification_iterator()
        self.assertTrue(inited)

    def test_destroy(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                pass

            def _get(self):
                pass

            def _destroy(self):
                nonlocal destroyed
                destroyed = True

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        source = MySource()
        notif_iter = source.create_notification_iterator()
        destroyed = False
        del notif_iter
        self.assertTrue(destroyed)

    def test_attr_component(self):
        class MyIter(bt2.UserNotificationIterator):
            def __init__(self):
                nonlocal source, is_same
                is_same = self.component is source

            def _next(self):
                pass

            def _get(self):
                pass

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        source = MySource()
        is_same = False
        notif_iter = source.create_notification_iterator()
        self.assertTrue(is_same)

    def test_next_raises_stop(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                raise bt2.Stop

            def _get(self):
                pass

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        source = MySource()
        is_same = False
        notif_iter = source.create_notification_iterator()

        with self.assertRaises(bt2.Stop):
            notif_iter.next()

    def test_next_raises_unsupported_feature(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                raise bt2.UnsupportedFeature

            def _get(self):
                pass

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        source = MySource()
        is_same = False
        notif_iter = source.create_notification_iterator()

        with self.assertRaises(bt2.UnsupportedFeature):
            notif_iter.next()

    def test_next_raises_unknown(self):
        class MyIter(bt2.UserNotificationIterator):
            def _next(self):
                raise TypeError('lel')

            def _get(self):
                pass

        class MySource(bt2.UserSourceComponent, notification_iterator_class=MyIter):
            pass

        source = MySource()
        is_same = False
        notif_iter = source.create_notification_iterator()

        with self.assertRaises(bt2.Error):
            notif_iter.next()

    def test_iteration(self):
        source = _create_source()
        notif_iter = source.create_notification_iterator()
        pkt_addr = None

        for index, notif in enumerate(notif_iter):
            if index == 0:
                self.assertIsInstance(notif, bt2.BeginningOfPacketNotification)
                self.assertEqual(notif.packet.header_field['magic'], 0xc1fc1fc1)
                self.assertEqual(notif.packet.context_field['something'], 194)
                pkt_addr = notif.packet.addr
            elif index < 5:
                self.assertIsInstance(notif, bt2.TraceEventNotification)
                self.assertEqual(notif.event.header_field['ts'], 19487)
                self.assertEqual(notif.event.payload_field['mosquito'], 'at {}'.format(index))
            elif index == 5:
                self.assertIsInstance(notif, bt2.EndOfPacketNotification)
                self.assertEqual(notif.packet.header_field['magic'], 0xc1fc1fc1)
                self.assertEqual(notif.packet.context_field['something'], 194)
                self.assertEqual(notif.packet.addr, pkt_addr)
            elif index == 6:
                self.assertIsInstance(notif, bt2.EndOfStreamNotification)
                self.assertEqual(notif.stream.name, 'abcdef')
            else:
                raise RuntimeError('FAIL')


class GenNotificationIteratorTestCase(unittest.TestCase):
    def test_attr_component(self):
        source = _create_source()
        notif_iter = source.create_notification_iterator()
        self.assertIsInstance(notif_iter, bt2.notification_iterator._GenericNotificationIterator)
        self.assertEqual(notif_iter.component.addr, source.addr)
