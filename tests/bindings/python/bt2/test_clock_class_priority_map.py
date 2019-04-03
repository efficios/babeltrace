import unittest
import uuid
import copy
import bt2


@unittest.skip("this is broken")
class ClockClassPriorityMapTestCase(unittest.TestCase):
    def test_create_empty(self):
        cc_prio_map = bt2.ClockClassPriorityMap()
        self.assertEqual(len(cc_prio_map), 0)
        self.assertIsNone(cc_prio_map.highest_priority_clock_class)

    def test_create_full(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2})
        self.assertEqual(len(cc_prio_map), 2)
        self.assertEqual(cc_prio_map[cc1], 17)
        self.assertEqual(cc_prio_map[cc2], 2)
        self.assertEqual(cc_prio_map.highest_priority_clock_class, cc2)

    def test_setitem(self):
        cc_prio_map = bt2.ClockClassPriorityMap()
        cc1 = bt2.ClockClass('mix', 5678)
        cc_prio_map[cc1] = 184
        self.assertEqual(len(cc_prio_map), 1)
        self.assertEqual(cc_prio_map[cc1], 184)
        self.assertEqual(cc_prio_map.highest_priority_clock_class, cc1)

    def test_setitem_wrong_key_type(self):
        cc_prio_map = bt2.ClockClassPriorityMap()

        with self.assertRaises(TypeError):
            cc_prio_map['the clock'] = 184

    def test_setitem_wrong_value_type(self):
        cc_prio_map = bt2.ClockClassPriorityMap()
        cc1 = bt2.ClockClass('mix', 5678)

        with self.assertRaises(TypeError):
            cc_prio_map[cc1] = 'priority'

    def test_getitem_wrong_key(self):
        cc_prio_map = bt2.ClockClassPriorityMap()
        cc1 = bt2.ClockClass('mix', 5678)
        cc2 = bt2.ClockClass('mix', 5678)
        cc_prio_map[cc1] = 2

        with self.assertRaises(KeyError):
            cc_prio_map[cc2]

    def test_getitem_wrong_key_type(self):
        cc_prio_map = bt2.ClockClassPriorityMap()

        with self.assertRaises(TypeError):
            cc_prio_map[23]

    def test_iter(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc3 = bt2.ClockClass('lel', 1548)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2, cc3: 25})
        cc_prios = {}

        for cc, prio in cc_prio_map.items():
            cc_prios[cc] = prio

        self.assertEqual(len(cc_prios), 3)
        self.assertEqual(cc_prios[cc1], 17)
        self.assertEqual(cc_prios[cc2], 2)
        self.assertEqual(cc_prios[cc3], 25)

    def test_eq(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc3 = bt2.ClockClass('lel', 1548)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2, cc3: 25})
        other_cc1 = bt2.ClockClass('meow', 1234)
        other_cc2 = bt2.ClockClass('mix', 5678)
        other_cc3 = bt2.ClockClass('lel', 1548)
        other_cc_prio_map = bt2.ClockClassPriorityMap({other_cc1: 17, other_cc2: 2, other_cc3: 25})
        self.assertEqual(cc_prio_map, other_cc_prio_map)

    def test_ne_clock_class(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc3 = bt2.ClockClass('lel', 1548)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2, cc3: 25})
        other_cc1 = bt2.ClockClass('meow', 1234)
        other_cc2 = bt2.ClockClass('coucou', 5678)
        other_cc3 = bt2.ClockClass('lel', 1548)
        other_cc_prio_map = bt2.ClockClassPriorityMap({other_cc1: 17, other_cc2: 2, other_cc3: 25})
        self.assertNotEqual(cc_prio_map, other_cc_prio_map)

    def test_ne_prio(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc3 = bt2.ClockClass('lel', 1548)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2, cc3: 25})
        other_cc1 = bt2.ClockClass('meow', 1234)
        other_cc2 = bt2.ClockClass('mix', 5678)
        other_cc3 = bt2.ClockClass('lel', 1548)
        other_cc_prio_map = bt2.ClockClassPriorityMap({other_cc1: 17, other_cc2: 3, other_cc3: 25})
        self.assertNotEqual(cc_prio_map, other_cc_prio_map)

    def test_eq_invalid(self):
        self.assertFalse(bt2.ClockClassPriorityMap() == 23)

    def test_copy(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc3 = bt2.ClockClass('lel', 1548)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2, cc3: 25})
        cc_prio_map_cpy = copy.copy(cc_prio_map)
        self.assertEqual(cc_prio_map, cc_prio_map_cpy)
        self.assertEqual(list(cc_prio_map.keys())[0].addr, list(cc_prio_map_cpy.keys())[0].addr)
        self.assertEqual(list(cc_prio_map.keys())[1].addr, list(cc_prio_map_cpy.keys())[1].addr)
        self.assertEqual(list(cc_prio_map.keys())[2].addr, list(cc_prio_map_cpy.keys())[2].addr)
        self.assertEqual(list(cc_prio_map.values())[0], list(cc_prio_map_cpy.values())[0])
        self.assertEqual(list(cc_prio_map.values())[1], list(cc_prio_map_cpy.values())[1])
        self.assertEqual(list(cc_prio_map.values())[2], list(cc_prio_map_cpy.values())[2])
        self.assertEqual(cc_prio_map.highest_priority_clock_class.addr,
                         cc_prio_map_cpy.highest_priority_clock_class.addr)

    def test_deep_copy(self):
        cc1 = bt2.ClockClass('meow', 1234)
        cc2 = bt2.ClockClass('mix', 5678)
        cc3 = bt2.ClockClass('lel', 1548)
        cc_prio_map = bt2.ClockClassPriorityMap({cc1: 17, cc2: 2, cc3: 25})
        cc_prio_map_cpy = copy.deepcopy(cc_prio_map)
        self.assertEqual(cc_prio_map, cc_prio_map_cpy)
        self.assertNotEqual(list(cc_prio_map.keys())[0].addr, list(cc_prio_map_cpy.keys())[0].addr)
        self.assertNotEqual(list(cc_prio_map.keys())[1].addr, list(cc_prio_map_cpy.keys())[1].addr)
        self.assertNotEqual(list(cc_prio_map.keys())[2].addr, list(cc_prio_map_cpy.keys())[2].addr)
        self.assertEqual(list(cc_prio_map.values())[0], list(cc_prio_map_cpy.values())[0])
        self.assertEqual(list(cc_prio_map.values())[1], list(cc_prio_map_cpy.values())[1])
        self.assertEqual(list(cc_prio_map.values())[2], list(cc_prio_map_cpy.values())[2])
        self.assertNotEqual(cc_prio_map.highest_priority_clock_class.addr,
                            cc_prio_map_cpy.highest_priority_clock_class.addr)
