import unittest
import uuid
import copy
import bt2


@unittest.skip("this is broken")
class CtfWriterClockTestCase(unittest.TestCase):
    def setUp(self):
        self._clock = bt2.CtfWriterClock('salut')

    def tearDown(self):
        del self._clock

    def test_create_default(self):
        self.assertEqual(self._clock.name, 'salut')

    def test_create_invalid_no_name(self):
        with self.assertRaises(TypeError):
            bt2.CtfWriterClock()

    def test_create_full(self):
        my_uuid = uuid.uuid1()
        cc = bt2.CtfWriterClock(name='name', description='some description',
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

    def test_assign_description(self):
        self._clock.description = 'hi people'
        self.assertEqual(self._clock.description, 'hi people')

    def test_assign_invalid_description(self):
        with self.assertRaises(TypeError):
            self._clock.description = 23

    def test_assign_frequency(self):
        self._clock.frequency = 987654321
        self.assertEqual(self._clock.frequency, 987654321)

    def test_assign_invalid_frequency(self):
        with self.assertRaises(TypeError):
            self._clock.frequency = 'lel'

    def test_assign_precision(self):
        self._clock.precision = 12
        self.assertEqual(self._clock.precision, 12)

    def test_assign_invalid_precision(self):
        with self.assertRaises(TypeError):
            self._clock.precision = 'lel'

    def test_assign_offset(self):
        self._clock.offset = bt2.ClockClassOffset(12, 56)
        self.assertEqual(self._clock.offset, bt2.ClockClassOffset(12, 56))

    def test_assign_invalid_offset(self):
        with self.assertRaises(TypeError):
            self._clock.offset = object()

    def test_assign_absolute(self):
        self._clock.is_absolute = True
        self.assertTrue(self._clock.is_absolute)

    def test_assign_invalid_absolute(self):
        with self.assertRaises(TypeError):
            self._clock.is_absolute = 23

    def test_assign_uuid(self):
        the_uuid = uuid.uuid1()
        self._clock.uuid = the_uuid
        self.assertEqual(self._clock.uuid, the_uuid)

    def test_assign_invalid_uuid(self):
        with self.assertRaises(TypeError):
            self._clock.uuid = object()

    def test_assign_time(self):
        self._clock.time = 41232

    def test_assign_invalid_time(self):
        with self.assertRaises(TypeError):
            self._clock.time = object()

    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._clock)
        self.assertNotEqual(cpy.addr, self._clock.addr)
        self.assertEqual(cpy, self._clock)

    def test_copy(self):
        cpy = copy.copy(self._clock)
        self._test_copy(cpy)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._clock)
        self._test_copy(cpy)

    def test_eq(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        self.assertEqual(cc1, cc2)

    def test_ne_name(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='mane', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_description(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='name', description='some descripti2',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_frequency(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1003, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_precision(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=171,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_offset(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3001),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_absolute(self):
        my_uuid = uuid.uuid1()
        cc1 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=my_uuid)
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=False, uuid=my_uuid)
        self.assertNotEqual(cc1, cc2)

    def test_ne_uuid(self):
        cc1 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=uuid.uuid1())
        cc2 = bt2.CtfWriterClock(name='name', description='some description',
                                 frequency=1001, precision=176,
                                 offset=bt2.ClockClassOffset(45, 3003),
                                 is_absolute=True, uuid=uuid.uuid1())
        self.assertNotEqual(cc1, cc2)

    def test_eq_invalid(self):
        self.assertFalse(self._clock == 23)
