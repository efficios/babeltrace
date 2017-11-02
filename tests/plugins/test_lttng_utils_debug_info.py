import unittest
import os.path
import bt2
import os


class LttngUtilsDebugInfoTestCase(unittest.TestCase):
    def test_debug_info(self):
        debug_info_data_dir = os.environ['DEBUG_INFO_DATA_DIR']
        trace_path = os.path.join(debug_info_data_dir, 'trace')
        target_prefix = os.path.join(debug_info_data_dir, '..', '..')
        src = bt2.ComponentSpec('ctf', 'fs', trace_path)
        flt = bt2.ComponentSpec('lttng-utils', 'debug-info', {
            'target-prefix': target_prefix,
        })
        it = bt2.TraceCollectionNotificationIterator(src, flt,
                                                     [bt2.EventNotification])
        notifs = list(it)
        debug_info = notifs[2].event['debug_info']
        self.assertEqual(debug_info['bin'], 'libhello_so+0x14d4')
        self.assertEqual(debug_info['func'], 'foo+0xa9')
        self.assertEqual(debug_info['src'], 'libhello.c:7')
        debug_info = notifs[3].event['debug_info']
        self.assertEqual(debug_info['bin'], 'libhello_so+0x15a6')
        self.assertEqual(debug_info['func'], 'bar+0xa9')
        self.assertEqual(debug_info['src'], 'libhello.c:13')
