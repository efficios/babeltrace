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
import bt2


class UserComponentClassTestCase(unittest.TestCase):
    def _test_no_init(self, cls):
        with self.assertRaises(RuntimeError):
            cls()

    def test_no_init_source(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

        self._test_no_init(MySource)

    def test_no_init_filter(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

        self._test_no_init(MyFilter)

    def test_no_init_sink(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        self._test_no_init(MySink)

    def test_incomplete_source_no_msg_iter_cls(self):
        with self.assertRaises(bt2._IncompleteUserClass):

            class MySource(bt2._UserSourceComponent):
                pass

    def test_incomplete_source_wrong_msg_iter_cls_type(self):
        with self.assertRaises(bt2._IncompleteUserClass):

            class MySource(bt2._UserSourceComponent, message_iterator_class=int):
                pass

    def test_incomplete_filter_no_msg_iter_cls(self):
        with self.assertRaises(bt2._IncompleteUserClass):

            class MyFilter(bt2._UserFilterComponent):
                pass

    def test_incomplete_sink_no_consume_method(self):
        with self.assertRaises(bt2._IncompleteUserClass):

            class MySink(bt2._UserSinkComponent):
                pass

    def test_minimal_source(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

    def test_minimal_filter(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

    def test_minimal_sink(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

    def test_default_name(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        self.assertEqual(MySink.name, 'MySink')

    def test_custom_name(self):
        class MySink(bt2._UserSinkComponent, name='salut'):
            def _user_consume(self):
                pass

        self.assertEqual(MySink.name, 'salut')

    def test_invalid_custom_name(self):
        with self.assertRaises(TypeError):

            class MySink(bt2._UserSinkComponent, name=23):
                def _user_consume(self):
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

            def _user_consume(self):
                pass

        self.assertEqual(MySink.description, 'The description.')

    def test_empty_description_no_lines(self):
        class MySink(bt2._UserSinkComponent):
            # fmt: off
            """"""
            # fmt: on

            def _user_consume(self):
                pass

        self.assertIsNone(MySink.description)

    def test_empty_description_no_contents(self):
        class MySink(bt2._UserSinkComponent):
            # fmt: off
            """
            """
            # fmt: on

            def _user_consume(self):
                pass

        self.assertIsNone(MySink.description)

    def test_empty_description_single_line(self):
        class MySink(bt2._UserSinkComponent):
            """my description"""

            def _user_consume(self):
                pass

        self.assertEqual(MySink.description, "my description")

    def test_help(self):
        class MySink(bt2._UserSinkComponent):
            """
            The description.

            The help
            text is
            here.
            """

            def _user_consume(self):
                pass

        self.assertEqual(MySink.help, 'The help\ntext is\nhere.')

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        self.assertIsInstance(MySink.addr, int)
        self.assertNotEqual(MySink.addr, 0)

    def test_query_not_implemented(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        with self.assertRaises(bt2.UnknownObject):
            bt2.QueryExecutor(MySink, 'obj', 23).query()

    def test_query_raises(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params):
                raise ValueError

        with self.assertRaises(bt2._Error):
            bt2.QueryExecutor(MySink, 'obj', 23).query()

    def test_query_wrong_return_type(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                return ...

        with self.assertRaises(bt2._Error):
            bt2.QueryExecutor(MySink, 'obj', 23).query()

    def test_query_params_none(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_params
                query_params = params
                return None

        query_params = None
        params = None
        res = bt2.QueryExecutor(MySink, 'obj', params).query()
        self.assertEqual(query_params, params)
        self.assertIsNone(res)
        del query_params

    def test_query_logging_level(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_log_level
                query_log_level = priv_query_exec.logging_level

        query_log_level = None
        query_exec = bt2.QueryExecutor(MySink, 'obj', None)
        query_exec.logging_level = bt2.LoggingLevel.WARNING
        query_exec.query()
        self.assertEqual(query_log_level, bt2.LoggingLevel.WARNING)
        del query_log_level

    def test_query_returns_none(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @staticmethod
            def _user_query(priv_query_exec, obj, params, method_obj):
                return

        res = bt2.QueryExecutor(MySink, 'obj', None).query()
        self.assertIsNone(res)

    def test_query_simple(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_params
                query_params = params
                return 17.5

        query_params = None
        params = ['coucou', 23, None]
        res = bt2.QueryExecutor(MySink, 'obj', params).query()
        self.assertEqual(query_params, params)
        self.assertEqual(res, 17.5)
        del query_params

    def test_query_complex(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_params
                query_params = params
                return {'null': None, 'bt2': 'BT2'}

        query_params = None
        params = {
            'array': ['coucou', 23, None],
            'other_map': {'yes': 'yeah', '19': 19, 'minus 1.5': -1.5},
            'null': None,
        }

        res = bt2.QueryExecutor(MySink, 'obj', params).query()
        self.assertEqual(query_params, params)
        self.assertEqual(res, {'null': None, 'bt2': 'BT2'})
        del query_params

    def test_eq(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        self.assertEqual(MySink, MySink)


class ComponentClassTestCase(unittest.TestCase):
    def setUp(self):
        class MySink(bt2._UserSinkComponent):
            """
            The description.

            The help.
            """

            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                return [obj, params, 23]

        self._py_comp_cls = MySink
        graph = bt2.Graph()
        comp = graph.add_component(MySink, 'salut')
        self._comp_cls = comp.cls
        self.assertIs(type(self._comp_cls), bt2._SinkComponentClassConst)

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
        res = bt2.QueryExecutor(
            self._comp_cls, 'an object', {'yes': 'no', 'book': -17}
        ).query()
        expected = ['an object', {'yes': 'no', 'book': -17}, 23]
        self.assertEqual(res, expected)


if __name__ == '__main__':
    unittest.main()
