from bt2 import value
import unittest
import copy
import bt2


class QueryExecutorTestCase(unittest.TestCase):
    def test_query(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
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

    def test_query_params_none(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                nonlocal query_params
                query_params = params

        query_params = 23
        res = bt2.QueryExecutor().query(MySink, 'obj', None)
        self.assertEqual(query_params, None)
        del query_params

    def test_query_logging_level(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                nonlocal query_log_level
                query_log_level = log_level

        query_log_level = None
        res = bt2.QueryExecutor().query(MySink, 'obj', None,
                                        bt2.LoggingLevel.INFO)
        self.assertEqual(query_log_level, bt2.LoggingLevel.INFO)
        del query_log_level

    def test_query_gen_error(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                raise ValueError

        with self.assertRaises(bt2.Error):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

    def test_query_invalid_object(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                raise bt2.InvalidQueryObject

        with self.assertRaises(bt2.InvalidQueryObject):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

    def test_query_logging_level_invalid_type(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                pass

        with self.assertRaises(TypeError):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23], 'yeah')

    def test_query_logging_level_invalid_value(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                pass

        with self.assertRaises(ValueError):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23], 12345)

    def test_query_invalid_params(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                raise bt2.InvalidQueryParams

        with self.assertRaises(bt2.InvalidQueryParams):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

    def test_query_try_again(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                raise bt2.TryAgain

        with self.assertRaises(bt2.TryAgain):
            res = bt2.QueryExecutor().query(MySink, 'obj', [17, 23])

    def test_cancel_no_query(self):
        query_exec = bt2.QueryExecutor()
        self.assertFalse(query_exec.is_canceled)
        query_exec.cancel()
        self.assertTrue(query_exec.is_canceled)

    def test_query_canceled(self):
        class MySink(bt2._UserSinkComponent):
            def _consume(self):
                pass

            @classmethod
            def _query(cls, query_exec, obj, params, log_level):
                raise bt2.TryAgain

        query_exec = bt2.QueryExecutor()
        query_exec.cancel()

        with self.assertRaises(bt2.QueryExecutorCanceled):
            res = query_exec.query(MySink, 'obj', [17, 23])
