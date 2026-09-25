# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2019-2026 EfficiOS Inc.

import bt2
import pytest


class TestClassDef:
    @staticmethod
    def _test_no_init(cls):
        with pytest.raises(RuntimeError):
            cls()

    def test_no_init_src(self):
        class MySrc(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

        self._test_no_init(MySrc)

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

    def test_incomplete_src_no_msg_iter_cls(self):
        with pytest.raises(bt2._IncompleteUserClass):

            class MySrc(bt2._UserSourceComponent):
                pass

    def test_incomplete_src_wrong_msg_iter_cls_type(self):
        with pytest.raises(bt2._IncompleteUserClass):

            class MySrc(bt2._UserSourceComponent, message_iterator_class=int):
                pass

    def test_incomplete_flt_no_msg_iter_cls(self):
        with pytest.raises(bt2._IncompleteUserClass):

            class MyFlt(bt2._UserFilterComponent):
                pass

    def test_incomplete_sink_no_consume_method(self):
        with pytest.raises(bt2._IncompleteUserClass):

            class MySink(bt2._UserSinkComponent):
                pass

    def test_minimal_src(self):
        class MySrc(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

    def test_minimal_flt(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

    def test_minimal_sink(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass


class TestUser:
    def test_default_name(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        assert MySink.name == "MySink"

    def test_custom_name(self):
        class MySink(bt2._UserSinkComponent, name="salut"):
            def _user_consume(self):
                pass

        assert MySink.name == "salut"

    def test_invalid_custom_name(self):
        with pytest.raises(TypeError):

            class MySink(bt2._UserSinkComponent, name=23):
                def _user_consume(self):
                    pass

    def test_descr(self):
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

        assert MySink.description == "The description."

    def test_empty_descr_no_lines(self):
        class MySink(bt2._UserSinkComponent):
            """"""

            def _user_consume(self):
                pass

        assert MySink.description is None

    def test_empty_descr_no_contents(self):
        class MySink(bt2._UserSinkComponent):
            # fmt: off
            """
            """
            # fmt: on

            def _user_consume(self):
                pass

        assert MySink.description is None

    def test_empty_descr_single_line(self):
        class MySink(bt2._UserSinkComponent):
            """my description"""

            def _user_consume(self):
                pass

        assert MySink.description == "my description"

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

        assert MySink.help == "The help\ntext is\nhere."

    def test_addr(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        assert isinstance(MySink.addr, int)
        assert MySink.addr != 0

    def test_eq(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        assert MySink == MySink


class TestUserQuery:
    def test_not_implemented(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

        with pytest.raises(bt2.UnknownObject):
            bt2.QueryExecutor(MySink, "obj", 23).query()

    def test_raises(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                raise ValueError

        with pytest.raises(bt2._Error):
            bt2.QueryExecutor(MySink, "obj", 23).query()

    def test_wrong_return_type(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                return ...

        with pytest.raises(bt2._Error):
            bt2.QueryExecutor(MySink, "obj", 23).query()

    def test_params_none(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal called

                called = True
                assert params is None

        called = False
        res = bt2.QueryExecutor(MySink, "obj", None).query()
        assert called
        assert res is None

    def test_log_level(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal called

                called = True
                assert priv_query_exec.logging_level == bt2.LoggingLevel.WARNING

        query_exec = bt2.QueryExecutor(MySink, "obj", None)
        query_exec.logging_level = bt2.LoggingLevel.WARNING
        called = False
        query_exec.query()
        assert called

    def test_returns_none(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @staticmethod
            def _user_query(priv_query_exec, obj, params, method_obj):
                return

        res = bt2.QueryExecutor(MySink, "obj", None).query()
        assert res is None

    def test_simple(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal called

                called = True
                assert params == test_params
                return 17.5

        test_params = ["coucou", 23, None]
        called = False
        assert bt2.QueryExecutor(MySink, "obj", test_params).query() == 17.5
        assert called

    def test_complex(self):
        class MySink(bt2._UserSinkComponent):
            def _user_consume(self):
                pass

            @classmethod
            def _user_query(cls, priv_query_exec, obj, params, method_obj):
                nonlocal called

                called = True
                assert params == test_params
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
        assert res == test_ret


class TestConst:
    @pytest.fixture(scope="class")
    def py_comp_cls_and_comp_cls(self):
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

        py_comp_cls = MySink
        graph = bt2.Graph()
        comp = graph.add_component(MySink, "salut")
        comp_cls = comp.cls
        assert type(comp_cls) is bt2._SinkComponentClassConst
        return py_comp_cls, comp_cls

    @pytest.fixture(scope="class")
    def py_comp_cls(self, py_comp_cls_and_comp_cls):
        return py_comp_cls_and_comp_cls[0]

    @pytest.fixture(scope="class")
    def comp_cls(self, py_comp_cls_and_comp_cls):
        return py_comp_cls_and_comp_cls[1]

    def test_description(self, comp_cls):
        assert comp_cls.description == "The description."

    def test_help(self, comp_cls):
        assert comp_cls.help == "The help."

    def test_name(self, comp_cls):
        assert comp_cls.name == "MySink"

    def test_addr(self, comp_cls):
        assert isinstance(comp_cls.addr, int)
        assert comp_cls.addr != 0

    def test_eq_invalid(self, comp_cls):
        assert not (comp_cls == 23)

    def test_eq(self, py_comp_cls, comp_cls):
        assert comp_cls == comp_cls
        assert py_comp_cls == comp_cls

    def test_query(self, comp_cls):
        test_params = {"yes": "no", "book": -17}
        expected = [
            "an object",
            test_params,
            23,
        ]
        assert bt2.QueryExecutor(comp_cls, "an object", test_params).query() == expected
