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


@unittest.skip("this is broken")
class ComponentSpecTestCase(unittest.TestCase):
    def test_create_good_no_params(self):
        spec = bt2.ComponentSpec('plugin', 'compcls')

    def test_create_good_with_params(self):
        spec = bt2.ComponentSpec('plugin', 'compcls', {'salut': 23})

    def test_create_good_with_path_params(self):
        spec = bt2.ComponentSpec('plugin', 'compcls', 'a path')
        self.assertEqual(spec.params['path'], 'a path')

    def test_create_wrong_plugin_name_type(self):
        with self.assertRaises(TypeError):
            spec = bt2.ComponentSpec(23, 'compcls')

    def test_create_wrong_component_class_name_type(self):
        with self.assertRaises(TypeError):
            spec = bt2.ComponentSpec('plugin', 190)

    def test_create_wrong_params_type(self):
        with self.assertRaises(TypeError):
            spec = bt2.ComponentSpec('dwdw', 'compcls', datetime.datetime.now())


@unittest.skip("this is broken")
class TraceCollectionMessageIteratorTestCase(unittest.TestCase):
    def test_create_wrong_stream_intersection_mode_type(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            msg_iter = bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=23)

    def test_create_wrong_begin_type(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            msg_iter = bt2.TraceCollectionMessageIterator(specs, begin='hi')

    def test_create_wrong_end_type(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            msg_iter = bt2.TraceCollectionMessageIterator(specs, begin='lel')

    def test_create_no_such_plugin(self):
        specs = [bt2.ComponentSpec('77', '101', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(bt2.Error):
            msg_iter = bt2.TraceCollectionMessageIterator(specs)

    def test_create_begin_s(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, begin=19457.918232)

    def test_create_end_s(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, end=123.12312)

    def test_create_begin_datetime(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, begin=datetime.datetime.now())

    def test_create_end_datetime(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, end=datetime.datetime.now())

    def test_iter_no_intersection(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        self.assertEqual(len(list(msg_iter)), 28)

    def test_iter_no_intersection_subscribe(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs,
                                                             message_types=[bt2.EventMessage])
        self.assertEqual(len(list(msg_iter)), 8)

    def test_iter_specs_not_list(self):
        spec = bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        msg_iter = bt2.TraceCollectionMessageIterator(spec,
                                                             message_types=[bt2.EventMessage])
        self.assertEqual(len(list(msg_iter)), 8)

    def test_iter_custom_filter(self):
        src_spec = bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        flt_spec = bt2.ComponentSpec('utils', 'trimmer', {
            'end': 13515309000000075,
        })
        msg_iter = bt2.TraceCollectionMessageIterator(src_spec, flt_spec,
                                                             message_types=[bt2.EventMessage])
        self.assertEqual(len(list(msg_iter)), 5)

    def test_iter_intersection(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=True)
        self.assertEqual(len(list(msg_iter)), 15)

    def test_iter_intersection_subscribe(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=True,
                                                             message_types=[bt2.EventMessage])
        self.assertEqual(len(list(msg_iter)), 3)

    def test_iter_intersection_no_path_param(self):
        specs = [bt2.ComponentSpec('text', 'dmesg', {'read-from-stdin': True})]

        with self.assertRaises(bt2.Error):
            msg_iter = bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=True,
                                                                 message_types=[bt2.EventMessage])

    def test_iter_no_intersection_two_traces(self):
        spec = bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        specs = [spec, spec]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        self.assertEqual(len(list(msg_iter)), 56)

    def test_iter_no_intersection_begin(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs,
                                                             message_types=[bt2.EventMessage],
                                                             begin=13515309.000000023)
        self.assertEqual(len(list(msg_iter)), 6)

    def test_iter_no_intersection_end(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs,
                                                             message_types=[bt2.EventMessage],
                                                             end=13515309.000000075)
        self.assertEqual(len(list(msg_iter)), 5)
