# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
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

import unittest
import bt2
import os


test_ctf_traces_path = os.environ['TEST_CTF_TRACES_PATH']


# Key to streams by their being timestamp.  Used to get the list of streams in
# a predictable order.

def sort_by_begin(stream):
    return stream['range-ns']['begin']


class QueryTraceInfoClockOffsetTestCase(unittest.TestCase):

    def setUp(self):
        ctf = bt2.find_plugin('ctf')
        self._fs = ctf.source_component_classes['fs']

        self._paths = [os.path.join(test_ctf_traces_path, 'intersection', '3eventsintersect')]
        self._executor = bt2.QueryExecutor()

    def _check(self, trace, offset):
        self.assertEqual(trace['range-ns']['begin'], 13515309000000000 + offset)
        self.assertEqual(trace['range-ns']['end'], 13515309000000120 + offset)
        self.assertEqual(trace['intersection-range-ns']['begin'], 13515309000000070 + offset)
        self.assertEqual(trace['intersection-range-ns']['end'], 13515309000000100 + offset)

        streams = sorted(trace['streams'], key=sort_by_begin)
        self.assertEqual(streams[0]['range-ns']['begin'], 13515309000000000 + offset)
        self.assertEqual(streams[0]['range-ns']['end'], 13515309000000100 + offset)
        self.assertEqual(streams[1]['range-ns']['begin'], 13515309000000070 + offset)
        self.assertEqual(streams[1]['range-ns']['end'], 13515309000000120 + offset)

    # Test various cominations of the clock-class-offset-s and
    # clock-class-offset-ns parameters to trace-info queries.

    # Without clock class offset

    def test_no_clock_class_offset(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
        })
        trace = res[0]
        self._check(trace, 0)

    # With clock-class-offset-s

    def test_clock_class_offset_s(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
            'clock-class-offset-s': 2,
        })
        trace = res[0]
        self._check(trace, 2000000000)

    # With clock-class-offset-ns

    def test_clock_class_offset_ns(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
            'clock-class-offset-ns': 2,
        })
        trace = res[0]
        self._check(trace, 2)

    # With both, negative

    def test_clock_class_offset_both(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
            'clock-class-offset-s': -2,
            'clock-class-offset-ns': -2,
        })
        trace = res[0]
        self._check(trace, -2000000002)

    def test_clock_class_offset_s_wrong_type(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-s': "2",
            })

    def test_clock_class_offset_s_wrong_type_none(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-s': None,
            })

    def test_clock_class_offset_ns_wrong_type(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-ns': "2",
            })

    def test_clock_class_offset_ns_wrong_type_none(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-ns': None,
            })

if __name__ == '__main__':
    unittest.main()
