from bt2 import value
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class UserComponentClassTestCase(unittest.TestCase):
    def _test_no_init(self, cls):
        with self.assertRaises(bt2.Error):
            cls()

    def test_no_init_source(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            pass

        self._test_no_init(MySource)

    def test_no_init_filter(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            pass

        self._test_no_init(MyFilter)

    def test_no_init_sink(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        self._test_no_init(MySink)

    def test_incomplete_source_no_msg_iter_cls(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        with self.assertRaises(bt2.IncompleteUserClass):
            class MySource(bt2._UserSourceComponent):
                pass

    def test_incomplete_source_wrong_msg_iter_cls_type(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        with self.assertRaises(bt2.IncompleteUserClass):
            class MySource(bt2._UserSourceComponent,
                           message_iterator_class=int):
                pass

    def test_incomplete_filter_no_msg_iter_cls(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        with self.assertRaises(bt2.IncompleteUserClass):
            class MyFilter(bt2._UserFilterComponent):
                pass

    def test_incomplete_sink_no_consume_method(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        with self.assertRaises(bt2.IncompleteUserClass):
            class MySink(bt2._UserSinkComponent):
                pass

    def test_minimal_source(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            pass

    def test_minimal_filter(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            pass

    def test_minimal_sink(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

    def test_default_name(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        self.assertEqual(MySink.name, 'MySink')

    def test_custom_name(self):
        class MySink(bt2._UserSinkComponent, name='salut'):
            def _consume(self):
                pass

        self.assertEqual(MySink.name, 'salut')

    def test_invalid_custom_name(self):
        with self.assertRaises(TypeError):
            class MySink(bt2._UserSinkComponent, name=23):
                def _consume(self):
                    pass

    def test_description(self):
        class MySink(bt2._UserSinkComponent):
            """
            The description.

            Bacon ipsum dolor amet ribeye t-bone corned beef, beef jerky
            porchetta burgdoggen prosciutto chicken frankfurter boudin
            hamburger doner bacon turducken. Sirloin shank sausage,
            boudin meatloaf alcatra meatball t-bone tongue pastrami
            cupim flank tenderloin.
            """

            def _consume(self):
                pass

        self.assertEqual(MySink.description, 'The description.')

    def test_empty_description(self):
        class MySink(bt2._UserSinkComponent):
            """
            """

            def _consume(self):
                pass

        self.assertIsNone(MySink.description)

    def test_help(self):
        class MySink(bt2._UserSinkComponent):
            """
            The description.

            The help
            text is
            here.
            """

            def _consume(self):
                pass

        self.assertEqual(MySink.help, 'The help\ntext is\nhere.')

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        self.assertIsInstance(MySink.addr, int)
        self.assertNotEqual(MySink.addr, 0)

    def test_query_not_implemented(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        with self.assertRaises(bt2.Error):
            bt2.QueryExecutor().query(MySink, 'obj', 23)

    def test_query_raises(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params):
                raise ValueError

        with self.assertRaises(bt2.Error):
            bt2.QueryExecutor().query(MySink, 'obj', 23)

    def test_query_wrong_return_type(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params):
                return ...

        with self.assertRaises(bt2.Error):
            bt2.QueryExecutor().query(MySink, 'obj', 23)

    def test_query_params_none(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params):
                nonlocal query_params
                query_params = params
                return None

        query_params = None
        params = None
        res = bt2.QueryExecutor().query(MySink, 'obj', params)
        self.assertEqual(query_params, params)
        self.assertIsNone(res)
        del query_params

    def test_query_simple(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params):
                nonlocal query_params
                query_params = params
                return 17.5

        query_params = None
        params = ['coucou', 23, None]
        res = bt2.QueryExecutor().query(MySink, 'obj', params)
        self.assertEqual(query_params, params)
        self.assertEqual(res, 17.5)
        del query_params

    def test_query_complex(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params):
                nonlocal query_params
                query_params = params
                return {
                    'null': None,
                    'bt2': 'BT2',
                }

        query_params = None
        params = {
            'array': ['coucou', 23, None],
            'other_map': {
                'yes': 'yeah',
                '19': 19,
                'minus 1.5': -1.5,
            },
            'null': None,
        }

        res = bt2.QueryExecutor().query(MySink, 'obj', params)
        self.assertEqual(query_params, params)
        self.assertEqual(res, {
            'null': None,
            'bt2': 'BT2',
        })
        del query_params

    def test_eq(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

        self.assertEqual(MySink, MySink)


@unittest.skip("this is broken")
class GenericComponentClassTestCase(unittest.TestCase):
    def setUp(self):
        class MySink(bt2._UserSinkComponent):
            '''
            The description.

            The help.
            '''
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params):
                return [obj, params, 23]

        self._py_comp_cls = MySink
        graph = bt2.Graph()
        comp = graph.add_component(MySink, 'salut')
        self._comp_cls = comp.component_class
        self.assertTrue(issubclass(type(self._comp_cls),
                                   bt2.component._GenericComponentClass))

    def tearDown(self):
        del self._py_comp_cls
        del self._comp_cls

    def test_description(self):
        self.assertEqual(self._comp_cls.description, 'The description.')

    def test_help(self):
        self.assertEqual(self._comp_cls.help, 'The help.')

    def test_name(self):
        self.assertEqual(self._comp_cls.name, 'MySink')

    def test_addr(self):
        self.assertIsInstance(self._comp_cls.addr, int)
        self.assertNotEqual(self._comp_cls.addr, 0)

    def test_eq_invalid(self):
        self.assertFalse(self._comp_cls == 23)

    def test_eq(self):
        self.assertEqual(self._comp_cls, self._comp_cls)
        self.assertEqual(self._py_comp_cls, self._comp_cls)

    def test_query(self):
        res = bt2.QueryExecutor().query(self._comp_cls, 'an object',
                                        {'yes': 'no', 'book': -17})
        expected = ['an object', {'yes': 'no', 'book': -17}, 23]
        self.assertEqual(res, expected)
