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
import re


class QueryExecutorTestCase(unittest.TestCase):
    def test_default_interrupter(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        query_exec = bt2.QueryExecutor(MySink, 'obj')
        interrupter = query_exec.default_interrupter
        self.assertIs(type(interrupter), bt2.Interrupter)

    def test_query(self):
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
        self.assertIs(type(res), bt2._MapValueConst)
        self.assertIs(type(res['bt2']), bt2._StringValueConst)
        self.assertEqual(query_params, params)
        self.assertEqual(res, {'null': None, 'bt2': 'BT2'})
        del query_params

    def test_query_params_none(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_params
                query_params = params

        query_params = 23
        bt2.QueryExecutor(MySink, 'obj', None).query()
        self.assertIs(query_params, None)
        del query_params

    def test_query_no_params(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_params
                query_params = params

        query_params = 23
        bt2.QueryExecutor(MySink, 'obj').query()
        self.assertIs(query_params, None)
        del query_params

    def test_query_with_method_obj(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_method_obj
                query_method_obj = method_obj

        query_method_obj = None
        method_obj = object()
        bt2.QueryExecutor(MySink, 'obj', method_obj=method_obj).query()
        self.assertIs(query_method_obj, method_obj)
        del query_method_obj

    def test_query_with_method_obj_del_ref(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_method_obj
                query_method_obj = method_obj

        class Custom:
            pass

        query_method_obj = None
        method_obj = Custom()
        method_obj.hola = 'hello'
        query_exec = bt2.QueryExecutor(MySink, 'obj', method_obj=method_obj)
        del method_obj
        query_exec.query()
        self.assertIsInstance(query_method_obj, Custom)
        self.assertEqual(query_method_obj.hola, 'hello')
        del query_method_obj

    def test_query_with_none_method_obj(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal query_method_obj
                query_method_obj = method_obj

        query_method_obj = object()
        bt2.QueryExecutor(MySink, 'obj').query()
        self.assertIsNone(query_method_obj)
        del query_method_obj

    def test_query_with_method_obj_non_python_comp_cls(self):
        plugin = bt2.find_plugin('text', find_in_user_dir=False, find_in_sys_dir=False)
        assert plugin is not None
        cc = plugin.source_component_classes['dmesg']
        assert cc is not None

        with self.assertRaisesRegex(
            ValueError,
            re.escape(r'cannot pass a Python object to a non-Python component class'),
        ):
            bt2.QueryExecutor(cc, 'obj', method_obj=object()).query()

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
        query_exec.logging_level = bt2.LoggingLevel.INFO
        query_exec.query()
        self.assertEqual(query_log_level, bt2.LoggingLevel.INFO)
        del query_log_level

    def test_query_gen_error(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                raise ValueError

        with self.assertRaises(bt2._Error) as ctx:
            bt2.QueryExecutor(MySink, 'obj', [17, 23]).query()

        exc = ctx.exception
        self.assertEqual(len(exc), 3)
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
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                raise bt2.UnknownObject

        with self.assertRaises(bt2.UnknownObject):
            bt2.QueryExecutor(MySink, 'obj', [17, 23]).query()

    def test_query_logging_level_invalid_type(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                pass

        query_exec = bt2.QueryExecutor(MySink, 'obj', [17, 23])

        with self.assertRaises(TypeError):
            query_exec.logging_level = 'yeah'

    def test_query_logging_level_invalid_value(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                pass

        query_exec = bt2.QueryExecutor(MySink, 'obj', [17, 23])

        with self.assertRaises(ValueError):
            query_exec.logging_level = 12345

    def test_query_try_again(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                raise bt2.TryAgain

        with self.assertRaises(bt2.TryAgain):
            bt2.QueryExecutor(MySink, 'obj', [17, 23]).query()

    def test_query_add_interrupter(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal interrupter2
                test_self.assertFalse(query_exec.is_interrupted)
                interrupter2.set()
                test_self.assertTrue(query_exec.is_interrupted)
                interrupter2.reset()
                test_self.assertFalse(query_exec.is_interrupted)

        interrupter1 = bt2.Interrupter()
        interrupter2 = bt2.Interrupter()
        test_self = self
        query_exec = bt2.QueryExecutor(MySink, 'obj', [17, 23])
        query_exec.add_interrupter(interrupter1)
        query_exec.add_interrupter(interrupter2)
        query_exec.query()

    def test_query_interrupt(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                test_self.assertFalse(query_exec.is_interrupted)
                query_exec.default_interrupter.set()
                test_self.assertTrue(query_exec.is_interrupted)

        test_self = self
        query_exec = bt2.QueryExecutor(MySink, 'obj', [17, 23])
        query_exec.query()

    def test_query_priv_executor_invalid_after(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal test_priv_query_exec
                test_priv_query_exec = priv_query_exec

        test_priv_query_exec = None
        query_exec = bt2.QueryExecutor(MySink, 'obj', [17, 23])
        query_exec.query()
        assert test_priv_query_exec is not None

        with self.assertRaises(RuntimeError):
            test_priv_query_exec.logging_level

        del test_priv_query_exec


if __name__ == '__main__':
    unittest.main()
