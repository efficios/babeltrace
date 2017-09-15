import unittest
import datetime
import copy
import uuid
import bt2
import os
import os.path


_TEST_CTF_TRACES_PATH = os.environ['TEST_CTF_TRACES_PATH']
_3EVENTS_INTERSECT_TRACE_PATH = os.path.join(_TEST_CTF_TRACES_PATH,
                                             'intersection',
                                             '3eventsintersect')


class SourceComponentSpecTestCase(unittest.TestCase):
    def test_create_good_no_params(self):
        spec = bt2.SourceComponentSpec('plugin', 'compcls')

    def test_create_good_with_params(self):
        spec = bt2.SourceComponentSpec('plugin', 'compcls', {'salut': 23})

    def test_create_good_with_path_params(self):
        spec = bt2.SourceComponentSpec('plugin', 'compcls', 'a path')
        self.assertEqual(spec.params['path'], 'a path')

    def test_create_wrong_plugin_name_type(self):
        with self.assertRaises(TypeError):
            spec = bt2.SourceComponentSpec(23, 'compcls')

    def test_create_wrong_component_class_name_type(self):
        with self.assertRaises(TypeError):
            spec = bt2.SourceComponentSpec('plugin', 190)

    def test_create_wrong_params_type(self):
        with self.assertRaises(TypeError):
            spec = bt2.SourceComponentSpec('dwdw', 'compcls', datetime.datetime.now())


class TraceCollectionNotificationIteratorTestCase(unittest.TestCase):
    def test_create_wrong_stream_intersection_mode_type(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            notif_iter = bt2.TraceCollectionNotificationIterator(specs, stream_intersection_mode=23)

    def test_create_wrong_begin_type(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            notif_iter = bt2.TraceCollectionNotificationIterator(specs, begin='hi')

    def test_create_wrong_end_type(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            notif_iter = bt2.TraceCollectionNotificationIterator(specs, begin='lel')

    def test_create_no_such_plugin(self):
        specs = [bt2.SourceComponentSpec('77', '101', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(bt2.Error):
            notif_iter = bt2.TraceCollectionNotificationIterator(specs)

    def test_create_begin_s(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs, begin=19457.918232)

    def test_create_end_s(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs, end=123.12312)

    def test_create_begin_datetime(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs, begin=datetime.datetime.now())

    def test_create_end_datetime(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs, end=datetime.datetime.now())

    def test_iter_no_intersection(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs)
        self.assertEqual(len(list(notif_iter)), 28)

    def test_iter_no_intersection_subscribe(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs,
                                                             notification_types=[bt2.EventNotification])
        self.assertEqual(len(list(notif_iter)), 8)

    def test_iter_intersection(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs, stream_intersection_mode=True)
        self.assertEqual(len(list(notif_iter)), 15)

    def test_iter_intersection_subscribe(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs, stream_intersection_mode=True,
                                                             notification_types=[bt2.EventNotification])
        self.assertEqual(len(list(notif_iter)), 3)

    def test_iter_intersection_no_path_param(self):
        specs = [bt2.SourceComponentSpec('text', 'dmesg', {'read-from-stdin': True})]

        with self.assertRaises(bt2.Error):
            notif_iter = bt2.TraceCollectionNotificationIterator(specs, stream_intersection_mode=True,
                                                                 notification_types=[bt2.EventNotification])

    def test_iter_no_intersection_two_traces(self):
        spec = bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        specs = [spec, spec]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs)
        self.assertEqual(len(list(notif_iter)), 56)

    def test_iter_no_intersection_begin(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs,
                                                             notification_types=[bt2.EventNotification],
                                                             begin=13515309.000000023)
        self.assertEqual(len(list(notif_iter)), 6)

    def test_iter_no_intersection_end(self):
        specs = [bt2.SourceComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs,
                                                             notification_types=[bt2.EventNotification],
                                                             end=13515309.000000075)
        self.assertEqual(len(list(notif_iter)), 5)
