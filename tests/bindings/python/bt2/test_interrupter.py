# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
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
