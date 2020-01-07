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
import re


test_ctf_traces_path = os.environ['BT_CTF_TRACES_PATH']


# Key to sort streams in a predictable order.
def sort_predictably(stream):
    return stream['port-name']


class QueryTraceInfoClockOffsetTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin('ctf')
        self._fs = ctf.source_component_classes['fs']

        self._inputs = [
            os.path.join(test_ctf_traces_path, 'intersection', '3eventsintersect')
        ]

    def _check(self, trace, offset):
        streams = sorted(trace['stream-infos'], key=sort_predictably)
        self.assertEqual(streams[0]['range-ns']['begin'], 13515309000000000 + offset)
        self.assertEqual(streams[0]['range-ns']['end'], 13515309000000100 + offset)
        self.assertEqual(streams[1]['range-ns']['begin'], 13515309000000070 + offset)
        self.assertEqual(streams[1]['range-ns']['end'], 13515309000000120 + offset)

    # Test various cominations of the clock-class-offset-s and
    # clock-class-offset-ns parameters to babeltrace.trace-infos queries.

    # Without clock class offset

    def test_no_clock_class_offset(self):
        res = bt2.QueryExecutor(
            self._fs, 'babeltrace.trace-infos', {'inputs': self._inputs}
        ).query()
        trace = res[0]
        self._check(trace, 0)

    # With clock-class-offset-s

    def test_clock_class_offset_s(self):
        res = bt2.QueryExecutor(
            self._fs,
            'babeltrace.trace-infos',
            {'inputs': self._inputs, 'clock-class-offset-s': 2},
        ).query()
        trace = res[0]
        self._check(trace, 2000000000)

    # With clock-class-offset-ns

    def test_clock_class_offset_ns(self):
        res = bt2.QueryExecutor(
            self._fs,
            'babeltrace.trace-infos',
            {'inputs': self._inputs, 'clock-class-offset-ns': 2},
        ).query()
        trace = res[0]
        self._check(trace, 2)

    # With both, negative

    def test_clock_class_offset_both(self):
        res = bt2.QueryExecutor(
            self._fs,
            'babeltrace.trace-infos',
            {
                'inputs': self._inputs,
                'clock-class-offset-s': -2,
                'clock-class-offset-ns': -2,
            },
        ).query()
        trace = res[0]
        self._check(trace, -2000000002)

    def test_clock_class_offset_s_wrong_type(self):
        with self.assertRaises(bt2._Error):
            bt2.QueryExecutor(
                self._fs,
                'babeltrace.trace-infos',
                {'inputs': self._inputs, 'clock-class-offset-s': "2"},
            ).query()

    def test_clock_class_offset_s_wrong_type_none(self):
        with self.assertRaises(bt2._Error):
            bt2.QueryExecutor(
                self._fs,
                'babeltrace.trace-infos',
                {'inputs': self._inputs, 'clock-class-offset-s': None},
            ).query()

    def test_clock_class_offset_ns_wrong_type(self):
        with self.assertRaises(bt2._Error):
            bt2.QueryExecutor(
                self._fs,
                'babeltrace.trace-infos',
                {'inputs': self._inputs, 'clock-class-offset-ns': "2"},
            ).query()

    def test_clock_class_offset_ns_wrong_type_none(self):
        with self.assertRaises(bt2._Error):
            bt2.QueryExecutor(
                self._fs,
                'babeltrace.trace-infos',
                {'inputs': self._inputs, 'clock-class-offset-ns': None},
            ).query()


class QueryTraceInfoPortNameTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin("ctf")
        self._fs = ctf.source_component_classes["fs"]

    def test_trace_uuid_stream_class_id_no_stream_id(self):
        res = bt2.QueryExecutor(
            self._fs,
            "babeltrace.trace-infos",
            {
                "inputs": [
                    os.path.join(
                        test_ctf_traces_path, "intersection", "3eventsintersect"
                    )
                ]
            },
        ).query()

        if os.environ['BT_OS_TYPE'] == 'mingw':
            os_stream_path = (
                '\\tests\\data\\ctf-traces\\intersection\\3eventsintersect\\'
            )
        else:
            os_stream_path = '/tests/data/ctf-traces/intersection/3eventsintersect/'

        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = sorted(trace["stream-infos"], key=sort_predictably)
        self.assertEqual(len(streams), 2)
        self.assertRegex(
            str(streams[0]["port-name"]),
            r"^7afe8fbe-79b8-4f6a-bbc7-d0c782e7ddaf \| 0 \| .*"
            + re.escape(os_stream_path + "test_stream_0")
            + r"$",
        )
        self.assertRegex(
            str(streams[1]["port-name"]),
            r"^7afe8fbe-79b8-4f6a-bbc7-d0c782e7ddaf \| 0 \| .*"
            + re.escape(os_stream_path + "test_stream_1")
            + r"$",
        )

    def test_trace_uuid_no_stream_class_id_no_stream_id(self):
        res = bt2.QueryExecutor(
            self._fs,
            "babeltrace.trace-infos",
            {"inputs": [os.path.join(test_ctf_traces_path, "succeed", "succeed1")]},
        ).query()

        if os.environ['BT_OS_TYPE'] == 'mingw':
            os_stream_path = '\\tests\\data\\ctf-traces\\succeed\\succeed1\\dummystream'
        else:
            os_stream_path = '/tests/data/ctf-traces/succeed/succeed1/dummystream'

        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = sorted(trace["stream-infos"], key=sort_predictably)
        self.assertEqual(len(streams), 1)
        self.assertRegex(
            str(streams[0]["port-name"]),
            r"^2a6422d0-6cee-11e0-8c08-cb07d7b3a564 \| .*"
            + re.escape(os_stream_path)
            + r"$",
        )


class QueryTraceInfoRangeTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin("ctf")
        self._fs = ctf.source_component_classes["fs"]

    def test_trace_no_range(self):
        # This trace has no `timestamp_begin` and `timestamp_end` in its
        # packet context. The `babeltrace.trace-infos` query should omit
        # the `range-ns` fields in the `trace` and `stream` data
        # structures.

        res = bt2.QueryExecutor(
            self._fs,
            "babeltrace.trace-infos",
            {"inputs": [os.path.join(test_ctf_traces_path, "succeed", "succeed1")]},
        ).query()

        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = trace["stream-infos"]
        self.assertEqual(len(streams), 1)

        self.assertRaises(KeyError, lambda: trace['range-ns'])
        self.assertRaises(KeyError, lambda: streams[0]['range-ns'])

    def test_trace_with_tracefile_rotation(self):
        res = bt2.QueryExecutor(
            self._fs,
            "babeltrace.trace-infos",
            {
                "inputs": [
                    os.path.join(
                        test_ctf_traces_path,
                        "succeed",
                        "lttng-tracefile-rotation",
                        "kernel",
                    )
                ]
            },
        ).query()

        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = trace["stream-infos"]
        self.assertEqual(len(streams), 4)

        # Note: the end timestamps are not the end timestamps found in the
        # index files, because fix_index_lttng_event_after_packet_bug changes
        # them based on the time of the last event in the stream.

        self.assertEqual(streams[0]['range-ns']['begin'], 1571261795455986789)
        self.assertEqual(streams[0]['range-ns']['end'], 1571261797582611840)

        self.assertEqual(streams[1]['range-ns']['begin'], 1571261795456368232)
        self.assertEqual(streams[1]['range-ns']['end'], 1571261797577754111)

        self.assertEqual(streams[2]['range-ns']['begin'], 1571261795456748255)
        self.assertEqual(streams[2]['range-ns']['end'], 1571261797577727795)

        self.assertEqual(streams[3]['range-ns']['begin'], 1571261795457285142)
        self.assertEqual(streams[3]['range-ns']['end'], 1571261797582522088)


class QueryTraceInfoPacketTimestampQuirksTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin('ctf')
        self._fs = ctf.source_component_classes['fs']
        self._path = os.path.join(test_ctf_traces_path, 'succeed')

    def _test_lttng_quirks(self, trace_name):
        res = bt2.QueryExecutor(
            self._fs,
            "babeltrace.trace-infos",
            {"inputs": [os.path.join(self._path, trace_name)]},
        ).query()

        self.assertEqual(len(res), 1)
        return res[0]

    def test_event_after_packet(self):
        trace = self._test_lttng_quirks("lttng-event-after-packet")
        streams = trace["stream-infos"]
        self.assertEqual(len(streams), 1)

        self.assertEqual(streams[0]['range-ns']['begin'], 1565957300948091100)
        self.assertEqual(streams[0]['range-ns']['end'], 1565957302180016069)

    def test_lttng_crash(self):
        trace = self._test_lttng_quirks("lttng-crash")
        streams = trace["stream-infos"]
        self.assertEqual(len(streams), 1)

        self.assertEqual(streams[0]['range-ns']['begin'], 1565891729288866738)
        self.assertEqual(streams[0]['range-ns']['end'], 1565891729293526525)


if __name__ == '__main__':
    unittest.main()
