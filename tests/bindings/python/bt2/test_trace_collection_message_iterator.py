#
# Copyright (C) 2019 EfficiOS Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import unittest
import datetime
import bt2
import os
import os.path


_BT_CTF_TRACES_PATH = os.environ['BT_CTF_TRACES_PATH']
_3EVENTS_INTERSECT_TRACE_PATH = os.path.join(_BT_CTF_TRACES_PATH,
                                             'intersection',
                                             '3eventsintersect')


class ComponentSpecTestCase(unittest.TestCase):
    def test_create_good_no_params(self):
        bt2.ComponentSpec('plugin', 'compcls')

    def test_create_good_with_params(self):
        bt2.ComponentSpec('plugin', 'compcls', {'salut': 23})

    def test_create_good_with_path_params(self):
        spec = bt2.ComponentSpec('plugin', 'compcls', 'a path')
        self.assertEqual(spec.params['paths'], ['a path'])

    def test_create_wrong_plugin_name_type(self):
        with self.assertRaises(TypeError):
            bt2.ComponentSpec(23, 'compcls')

    def test_create_wrong_component_class_name_type(self):
        with self.assertRaises(TypeError):
            bt2.ComponentSpec('plugin', 190)

    def test_create_wrong_params_type(self):
        with self.assertRaises(TypeError):
            bt2.ComponentSpec('dwdw', 'compcls', datetime.datetime.now())


# Return a map, msg type -> number of messages of this type.

def _count_msgs_by_type(msgs):
    res = {}

    for msg in msgs:
        t = type(msg)
        n = res.get(t, 0)
        res[t] = n + 1

    return res


class TraceCollectionMessageIteratorTestCase(unittest.TestCase):
    def test_create_wrong_stream_intersection_mode_type(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=23)

    def test_create_wrong_begin_type(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            bt2.TraceCollectionMessageIterator(specs, begin='hi')

    def test_create_wrong_end_type(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(TypeError):
            bt2.TraceCollectionMessageIterator(specs, begin='lel')

    def test_create_no_such_plugin(self):
        specs = [bt2.ComponentSpec('77', '101', _3EVENTS_INTERSECT_TRACE_PATH)]

        with self.assertRaises(bt2.Error):
            bt2.TraceCollectionMessageIterator(specs)

    def test_create_begin_s(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        bt2.TraceCollectionMessageIterator(specs, begin=19457.918232)

    def test_create_end_s(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        bt2.TraceCollectionMessageIterator(specs, end=123.12312)

    def test_create_begin_datetime(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        bt2.TraceCollectionMessageIterator(specs, begin=datetime.datetime.now())

    def test_create_end_datetime(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        bt2.TraceCollectionMessageIterator(specs, end=datetime.datetime.now())

    def test_iter_no_intersection(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2.message._EventMessage], 8)

    # Same as the above, but we pass a single spec instead of a spec list.
    def test_iter_specs_not_list(self):
        spec = bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        msg_iter = bt2.TraceCollectionMessageIterator(spec)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2.message._EventMessage], 8)

    def test_iter_custom_filter(self):
        src_spec = bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        flt_spec = bt2.ComponentSpec('utils', 'trimmer', {
            'end': '13515309.000000075',
        })
        msg_iter = bt2.TraceCollectionMessageIterator(src_spec, flt_spec)
        hist = _count_msgs_by_type(msg_iter)
        self.assertEqual(hist[bt2.message._EventMessage], 5)

    def test_iter_intersection(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=True)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 15)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2.message._EventMessage], 3)

    def test_iter_intersection_no_path_param(self):
        specs = [bt2.ComponentSpec('text', 'dmesg', {'read-from-stdin': True})]

        with self.assertRaises(bt2.Error):
            bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=True)

    def test_iter_no_intersection_two_traces(self):
        spec = bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)
        specs = [spec, spec]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 56)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2.message._EventMessage], 16)

    def test_iter_no_intersection_begin(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, begin=13515309.000000023)
        hist = _count_msgs_by_type(msg_iter)
        self.assertEqual(hist[bt2.message._EventMessage], 6)

    def test_iter_no_intersection_end(self):
        specs = [bt2.ComponentSpec('ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, end=13515309.000000075)
        hist = _count_msgs_by_type(msg_iter)
        self.assertEqual(hist[bt2.message._EventMessage], 5)
