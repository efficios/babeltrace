import unittest
import uuid
import copy
import bt2


@unittest.skip("this is broken")
class ClockClassOffsetTestCase(unittest.TestCase):
    def test_create_default(self):
        cco = bt2.ClockClassOffset()
        self.assertEqual(cco.seconds, 0)
        self.assertEqual(cco.cycles, 0)

    def test_create(self):
        cco = bt2.ClockClassOffset(23, 4871232)
        self.assertEqual(cco.seconds, 23)
        self.assertEqual(cco.cycles, 4871232)

    def test_create_kwargs(self):
        cco = bt2.ClockClassOffset(seconds=23, cycles=4871232)
        self.assertEqual(cco.seconds, 23)
        self.assertEqual(cco.cycles, 4871232)

    def test_create_invalid_seconds(self):
        with self.assertRaises(TypeError):
            bt2.ClockClassOffset('hello', 4871232)

    def test_create_invalid_cycles(self):
        with self.assertRaises(TypeError):
            bt2.ClockClassOffset(23, 'hello')

    def test_eq(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(23, 42)
        self.assertEqual(cco1, cco2)

    def test_ne_seconds(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(24, 42)
        self.assertNotEqual(cco1, cco2)

    def test_ne_cycles(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(23, 43)
        self.assertNotEqual(cco1, cco2)

    def test_eq_invalid(self):
        self.assertFalse(bt2.ClockClassOffset() == 23)


@unittest.skip("this is broken")
class ClockClassTestCase(unittest.TestCase):
    def setUp(self):
        self._cc = bt2.ClockClass('salut', 1000000)

    def tearDown(self):
        del self._cc

    def test_create_default(self):
        self.assertEqual(self._cc.name, 'salut')

    def test_create_invalid_no_name(self):
        with self.assertRaises(TypeError):
            bt2.ClockClass()

    def test_create_full(self):
        my_uuid = uuid.uuid1()
        cc = bt2.ClockClass(name='name', description='some description',
                            frequency=1001, precision=176,
                            offset=bt2.ClockClassOffset(45, 3003),
                            is_absolute=True, uuid=my_uuid)
        self.assertEqual(cc.name, 'name')
        self.assertEqual(cc.description, 'some description')
        self.assertEqual(cc.frequency, 1001)
        self.assertEqual(cc.precision, 176)
        self.assertEqual(cc.offset, bt2.ClockClassOffset(45, 3003))
        self.assertEqual(cc.is_absolute, True)
        self.assertEqual(cc.uuid, copy.deepcopy(my_uuid))

    def test_assign_name(self):
        self._cc.name = 'the_clock'
        self.assertEqual(self._cc.name, 'the_clock')

    def test_assign_invalid_name(self):
        with self.assertRaises(TypeError):
            self._cc.name = 23

    def test_assign_description(self):
        self._cc.description = 'hi people'
        self.assertEqual(self._cc.description, 'hi people')

    def test_assign_invalid_description(self):
        with self.assertRaises(TypeError):
            self._cc.description = 23

    def test_assign_frequency(self):
        self._cc.frequency = 987654321
        self.assertEqual(self._cc.frequency, 987654321)

    def test_assign_invalid_frequency(self):
        with self.assertRaises(TypeError):
            self._cc.frequency = 'lel'

    def test_assign_precision(self):
        self._cc.precision = 12
        self.assertEqual(self._cc.precision, 12)

    def test_assign_invalid_precision(self):
        with self.assertRaises(TypeError):
            self._cc.precision = 'lel'

    def test_assign_offset(self):
        self._cc.offset = bt2.ClockClassOffset(12, 56)
        self.assertEqual(self._cc.offset, bt2.ClockClassOffset(12, 56))

    def test_assign_invalid_offset(self):
        with self.assertRaises(TypeError):
            self._cc.offset = object()

    def test_assign_absolute(self):
        self._cc.is_absolute = True
        self.assertTrue(self._cc.is_absolute)

    def test_assign_invalid_absolute(self):
        with self.assertRaises(TypeError):
            self._cc.is_absolute = 23

    def test_assign_uuid(self):
        the_uuid = uuid.uuid1()
        self._cc.uuid = the_uuid
        self.assertEqual(self._cc.uuid, the_uuid)

    def test_assign_invalid_uuid(self):
        with self.assertRaises(TypeError):
            self._cc.uuid = object()

    def test_create_clock_snapshot(self):
        cs = self._cc(756)
        self.assertEqual(cs.clock_class.addr, self._cc.addr)

    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._cc)
        self.assertNotEqual(cpy.addr, self._cc.addr)
        self.assertEqual(cpy, self._cc)

    def test_copy(self):
        cpy = copy.copy(self._cc)
        self._test_copy(cpy)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._cc)
        self._test_copy(cpy)

    def test_eq(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        self.assertEqual(cc1, cc2)

    def test_ne_name(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='mane', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_description(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='name', description='some descripti2',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_frequency(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='name', description='some description',
                             frequency=1003, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_precision(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=171,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_offset(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3001),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_absolute(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=my_uuid)
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=False, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_uuid(self):
        cc1 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=uuid.uuid1())
        cc2 = bt2.ClockClass(name='name', description='some description',
                             frequency=1001, precision=176,
                             offset=bt2.ClockClassOffset(45, 3003),
                             is_absolute=True, uuid=uuid.uuid1())
        self.assertNotEqual(cc1, cc2)

    def test_eq_invalid(self):
        self.assertFalse(self._cc == 23)


@unittest.skip("this is broken")
class ClockSnapshotTestCase(unittest.TestCase):
    def setUp(self):
        self._cc = bt2.ClockClass('salut', 1000,
                                  offset=bt2.ClockClassOffset(45, 354))
        self._cs = self._cc(123)

    def tearDown(self):
        del self._cc
        del self._cs

    def test_create_default(self):
        self.assertEqual(self._cs.clock_class.addr, self._cc.addr)
        self.assertEqual(self._cs.cycles, 123)

    def test_create_invalid_cycles_type(self):
        with self.assertRaises(TypeError):
            self._cc('yes')

    def test_ns_from_epoch(self):
        s_from_epoch = 45 + ((354 + 123) / 1000)
        ns_from_epoch = int(s_from_epoch * 1e9)
        self.assertEqual(self._cs.ns_from_epoch, ns_from_epoch)

    def test_eq(self):
        cs1 = self._cc(123)
        cs2 = self._cc(123)
        self.assertEqual(cs1, cs2)

    def test_eq_int(self):
        cs1 = self._cc(123)
        self.assertEqual(cs1, 123)

    def test_ne_clock_class(self):
        cc1 = bt2.ClockClass('yes', 1500)
        cc2 = bt2.ClockClass('yes', 1501)
        cs1 = cc1(123)
        cs2 = cc2(123)
        self.assertNotEqual(cs1, cs2)

    def test_ne_cycles(self):
        cs1 = self._cc(123)
        cs2 = self._cc(125)
        self.assertNotEqual(cs1, cs2)

    def test_eq_invalid(self):
        self.assertFalse(self._cs == 23)

    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._cs)
        self.assertNotEqual(cpy.addr, self._cs.addr)
        self.assertEqual(cpy, self._cs)

    def test_copy(self):
        cpy = copy.copy(self._cs)
        self._test_copy(cpy)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._cs)
        self._test_copy(cpy)
