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

import bt2
import unittest


class InterrupterTestCase(unittest.TestCase):
    def setUp(self):
        self._interrupter = bt2.Interrupter()

    def test_create(self):
        self.assertFalse(self._interrupter.is_set)

    def test_is_set(self):
        self.assertFalse(self._interrupter.is_set)

    def test_bool(self):
        self.assertFalse(self._interrupter)
        self._interrupter.set()
        self.assertTrue(self._interrupter)

    def test_set(self):
        self.assertFalse(self._interrupter)
        self._interrupter.set()
        self.assertTrue(self._interrupter)

    def test_reset(self):
        self._interrupter.set()
        self.assertTrue(self._interrupter)
        self._interrupter.reset()
        self.assertFalse(self._interrupter)


if __name__ == '__main__':
    unittest.main()
