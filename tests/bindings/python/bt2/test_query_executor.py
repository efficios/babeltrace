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

from bt2 import value
import unittest
import copy
import bt2


class QueryExecutorTestCase(unittest.TestCase):
    def test_query(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                nonlocal query_params
                query_params = params
                return {'null': None, 'bt2': 'BT2'}

        query_params = None
        params = {
            'array': ['coucou', 23, None],
            'other_map': {'yes': 'yeah', '19': 19, 'minus 1.5': -1.5},
            'null': None,
        }

        res = bt2.QueryExecutor().query(MySink, 'obj', params)
        self.assertEqual(query_params, params)
        self.assertEqual(res, {'null': None, 'bt2': 'BT2'})
        del query_params

    def test_query_params_none(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                nonlocal query_params
                query_params = params

        query_params = 23
        res = bt2.QueryExecutor().query(MySink, 'obj', None)
        self.assertEqual(query_params, None)
        del query_params

    def test_query_logging_level(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                nonlocal query_log_level
                query_log_level = log_level

        query_log_level = None
        res = bt2.QueryExecutor().query(MySink, 'obj', None, bt2.LoggingLevel.INFO)
        self.assertEqual(query_log_level, bt2.LoggingLevel.INFO)
        del query_log_level

    def test_query_gen_error(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                raise ValueError

        with self.assertRaises(bt2._Error) as ctx:
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

        exc = ctx.exception
        self.assertEqual(len(exc), 2)
        cause = exc[0]
        self.assertIsInstance(cause, bt2._ComponentClassErrorCause)
        self.assertIn('raise ValueError', cause.message)
        self.assertEqual(cause.component_class_type, bt2.ComponentClassType.SINK)
        self.assertEqual(cause.component_class_name, 'MySink')

    def test_query_unknown_object(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                raise bt2.UnknownObject

        with self.assertRaises(bt2.UnknownObject):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

    def test_query_logging_level_invalid_type(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                pass

        with self.assertRaises(TypeError):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23], 'yeah')

    def test_query_logging_level_invalid_value(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                pass

        with self.assertRaises(ValueError):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23], 12345)

    def test_query_try_again(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                raise bt2.TryAgain

        with self.assertRaises(bt2.TryAgain):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

    def test_query_add_interrupter(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                nonlocal interrupter2
                test_self.assertFalse(query_exec.is_interrupted)
                interrupter2.set()
                test_self.assertTrue(query_exec.is_interrupted)
                interrupter2.reset()
                test_self.assertFalse(query_exec.is_interrupted)

        interrupter1 = bt2.Interrupter()
        interrupter2 = bt2.Interrupter()
        test_self = self
        query_exec = bt2.QueryExecutor()
        query_exec.add_interrupter(interrupter1)
        query_exec.add_interrupter(interrupter2)
        query_exec.query(MySink, 'obj', [17, 23])

    def test_query_interrupt(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, query_exec, obj, params, log_level):
                test_self.assertFalse(query_exec.is_interrupted)
                query_exec.interrupt()
                test_self.assertTrue(query_exec.is_interrupted)

        test_self = self
        query_exec = bt2.QueryExecutor()
        query_exec.query(MySink, 'obj', [17, 23])
