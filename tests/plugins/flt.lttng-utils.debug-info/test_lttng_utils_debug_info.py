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
import os.path
import bt2
import os


@unittest.skip('depends on Python bindings, which are broken')
class LttngUtilsDebugInfoTestCase(unittest.TestCase):
    def test_debug_info(self):
        debug_info_data_path = os.environ['BT_DEBUG_INFO_PATH']
        trace_path = os.path.join(debug_info_data_path, 'trace')
        target_prefix = os.path.join(debug_info_data_path, '..', '..')
        src = bt2.ComponentSpec('ctf', 'fs', trace_path)
        flt = bt2.ComponentSpec(
            'lttng-utils', 'debug-info', {'target-prefix': target_prefix}
        )
        it = bt2.TraceCollectionNotificationIterator(src, flt, [bt2.EventNotification])
        notifs = list(it)
        debug_info = notifs[2].event['debug_info']
        self.assertEqual(debug_info['bin'], 'libhello_so+0x14d4')
        self.assertEqual(debug_info['func'], 'foo+0xa9')
        self.assertEqual(debug_info['src'], 'libhello.c:7')
        debug_info = notifs[3].event['debug_info']
        self.assertEqual(debug_info['bin'], 'libhello_so+0x15a6')
        self.assertEqual(debug_info['func'], 'bar+0xa9')
        self.assertEqual(debug_info['src'], 'libhello.c:13')
