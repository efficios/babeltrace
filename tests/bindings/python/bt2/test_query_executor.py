# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2019-2026 EfficiOS Inc.

import re
import types

import bt2
import pytest


def test_def_interrupter():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

    interrupter = bt2.QueryExecutor(MySink, "obj").default_interrupter
    assert type(interrupter) is bt2.Interrupter


def test_query():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, query_params, method_obj):
            nonlocal called

            called = True
            assert query_params == test_params
            return test_ret

    test_params = {
        "array": ["coucou", 23, None],
        "other_map": {"yes": "yeah", "19": 19, "minus 1.5": -1.5},
        "null": None,
    }
    test_ret = {"null": None, "bt2": "BT2"}
    called = False
    res = bt2.QueryExecutor(MySink, "obj", test_params).query()
    assert called
    assert type(res) is bt2._MapValueConst
    assert type(res["bt2"]) is bt2._StringValueConst
    assert res == test_ret


def test_query_params_none():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert params is None

    called = False
    bt2.QueryExecutor(MySink, "obj", None).query()
    assert called


def test_query_no_params():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert params is None

    called = False
    bt2.QueryExecutor(MySink, "obj").query()
    assert called


def test_query_with_method_obj():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert method_obj is test_method_obj

    test_method_obj = object()
    called = False
    bt2.QueryExecutor(MySink, "obj", method_obj=test_method_obj).query()
    assert called


def test_query_with_method_obj_del_ref():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert type(method_obj) is types.SimpleNamespace
            assert method_obj.hola == "hello"

    called = False
    method_obj = types.SimpleNamespace(hola="hello")
    query_exec = bt2.QueryExecutor(MySink, "obj", method_obj=method_obj)
    del method_obj
    query_exec.query()
    assert called


def test_query_with_none_method_obj():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert method_obj is None

    called = False
    bt2.QueryExecutor(MySink, "obj").query()
    assert called


def test_query_with_method_obj_non_python_comp_cls(dmesg_comp_cls):
    with pytest.raises(
        ValueError,
        match=re.escape(r"cannot pass a Python object to a non-Python component class"),
    ):
        bt2.QueryExecutor(dmesg_comp_cls, "obj", method_obj=object()).query()


def test_query_logging_level():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert priv_query_exec.logging_level == bt2.LoggingLevel.INFO

    called = False
    query_exec = bt2.QueryExecutor(MySink, "obj", None)
    query_exec.logging_level = bt2.LoggingLevel.INFO
    query_exec.query()
    assert called


def test_query_gen_error():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            raise ValueError

    with pytest.raises(bt2._Error) as exc_info:
        bt2.QueryExecutor(MySink, "obj", [17, 23]).query()

    exc = exc_info.value
    assert len(exc) == 2
    cause = exc[0]
    assert isinstance(cause, bt2._ComponentClassErrorCause)
    assert "raise ValueError" in cause.message
    assert cause.component_class_type == bt2.ComponentClassType.SINK
    assert cause.component_class_name == "MySink"


def test_query_unknown_object():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            raise bt2.UnknownObject

    with pytest.raises(bt2.UnknownObject):
        bt2.QueryExecutor(MySink, "obj", [17, 23]).query()


def test_query_logging_level_invalid_type():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            pass

    query_exec = bt2.QueryExecutor(MySink, "obj", [17, 23])

    with pytest.raises(TypeError):
        query_exec.logging_level = "yeah"


def test_query_try_again():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            raise bt2.TryAgain

    with pytest.raises(bt2.TryAgain):
        bt2.QueryExecutor(MySink, "obj", [17, 23]).query()


def test_query_add_interrupter():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert not query_exec.is_interrupted
            interrupter_2.set()
            assert query_exec.is_interrupted
            interrupter_2.reset()
            assert not query_exec.is_interrupted

    called = False
    interrupter_1 = bt2.Interrupter()
    interrupter_2 = bt2.Interrupter()
    query_exec = bt2.QueryExecutor(MySink, "obj", [17, 23])
    query_exec.add_interrupter(interrupter_1)
    query_exec.add_interrupter(interrupter_2)
    query_exec.query()
    assert called


def test_query_interrupt():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called

            called = True
            assert not query_exec.is_interrupted
            query_exec.default_interrupter.set()
            assert query_exec.is_interrupted

    called = False
    query_exec = bt2.QueryExecutor(MySink, "obj", [17, 23])
    query_exec.query()
    assert called


def test_query_priv_exec_invalid_after():
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal called, test_priv_query_exec

            called = True
            test_priv_query_exec = priv_query_exec

    called = False
    test_priv_query_exec = None
    query_exec = bt2.QueryExecutor(MySink, "obj", [17, 23])
    query_exec.query()
    assert called

    with pytest.raises(RuntimeError):
        test_priv_query_exec.logging_level
