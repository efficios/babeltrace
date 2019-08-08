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


session_rotation_trace_path = os.path.join(
    os.environ['BT_CTF_TRACES_PATH'], 'succeed', 'session-rotation'
)


trace_10352_1 = os.path.join(
    session_rotation_trace_path,
    'archives',
    '20190826T155748-0400-20190826T155751-0400-1',
    'ust',
    'pid',
    'hello-ust-10352-20190826-155748',
)
trace_10353_1 = os.path.join(
    session_rotation_trace_path,
    'archives',
    '20190826T155748-0400-20190826T155751-0400-1',
    'ust',
    'pid',
    'hello-ust-10353-20190826-155748',
)
trace_10352_2 = os.path.join(
    session_rotation_trace_path,
    'archives',
    '20190826T155750-0400-20190826T155753-0400-2',
    'ust',
    'pid',
    'hello-ust-10352-20190826-155748',
)
trace_10353_2 = os.path.join(
    session_rotation_trace_path,
    'archives',
    '20190826T155750-0400-20190826T155753-0400-2',
    'ust',
    'pid',
    'hello-ust-10353-20190826-155748',
)
trace_10352_3 = os.path.join(
    session_rotation_trace_path,
    '20190826T155753-0400-3',
    'ust',
    'pid',
    'hello-ust-10352-20190826-155748',
)
trace_10353_3 = os.path.join(
    session_rotation_trace_path,
    '20190826T155753-0400-3',
    'ust',
    'pid',
    'hello-ust-10353-20190826-155748',
)


class QuerySupportInfoTestCase(unittest.TestCase):
    def test_support_info_with_uuid(self):
        # Test that the right group is reported for each trace.

        def do_one_query(input, expected_group):
            qe = bt2.QueryExecutor(
                fs, 'babeltrace.support-info', {'input': input, 'type': 'directory'}
            )

            result = qe.query()
            self.assertEqual(result['group'], expected_group)

        ctf = bt2.find_plugin('ctf')
        fs = ctf.source_component_classes['fs']

        do_one_query(trace_10352_1, '21cdfa5e-9a64-490a-832c-53aca6c101ba')
        do_one_query(trace_10352_2, '21cdfa5e-9a64-490a-832c-53aca6c101ba')
        do_one_query(trace_10352_3, '21cdfa5e-9a64-490a-832c-53aca6c101ba')
        do_one_query(trace_10353_1, '83656eb1-b131-40e7-9666-c04ae279b58c')
        do_one_query(trace_10353_2, '83656eb1-b131-40e7-9666-c04ae279b58c')
        do_one_query(trace_10353_3, '83656eb1-b131-40e7-9666-c04ae279b58c')


if __name__ == '__main__':
    unittest.main()
