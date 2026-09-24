# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2019-2026 EfficiOS Inc.

import bt2
import pytest
from bt2 import native_bt


class _FailingMsgIter(bt2._UserMessageIterator):
    def __next__(self):
        raise ValueError("User message iterator is failing")


class _SrcWithFailingIter(
    bt2._UserSourceComponent, message_iterator_class=_FailingMsgIter
):
    def __init__(self, config, params, obj):
        self._add_output_port("out")


class _SrcWithFailingInit(
    bt2._UserSourceComponent, message_iterator_class=_FailingMsgIter
):
    def __init__(self, config, params, obj):
        raise ValueError("Source is failing")


class _WorkingSink(bt2._UserSinkComponent):
    def __init__(self, config, params, obj):
        self._in = self._add_input_port("in")

    def _user_graph_is_configured(self):
        self._iter = self._create_message_iterator(self._in)

    def _user_consume(self):
        next(self._iter)


class _SinkWithExceptionChaining(bt2._UserSinkComponent):
    def __init__(self, config, params, obj):
        self._in = self._add_input_port("in")

    def _user_graph_is_configured(self):
        self._iter = self._create_message_iterator(self._in)

    def _user_consume(self):
        try:
            next(self._iter)
        except bt2._Error as e:
            raise ValueError("oops") from e


class _SinkWithFailingQuery(bt2._UserSinkComponent):
    def _user_consume(self):
        pass

    @staticmethod
    def _user_query(priv_executor, obj, params, method_obj):
        raise ValueError("Query is failing")


def _run_failing_graph(source_cc, sink_cc):
    with pytest.raises(bt2._Error) as exc_info:
        graph = bt2.Graph()
        src_comp = graph.add_component(source_cc, "src")
        sink_comp = graph.add_component(sink_cc, "snk")
        graph.connect_ports(src_comp.output_ports["out"], sink_comp.input_ports["in"])
        graph.run()

    return exc_info.value


def _check_common_cause(cause):
    assert isinstance(cause.module_name, str)
    assert isinstance(cause.file_name, str)
    assert isinstance(cause.line_number, int)


def test_current_thread_error_none():
    # When a `bt2._Error` is raised, it steals the error of the current
    # thread. Verify that it is now `None`.
    _run_failing_graph(_SrcWithFailingInit, _WorkingSink)
    assert native_bt.current_thread_take_error() is None


def test_len():
    # The exact number of causes is not very important (it can change if
    # we append more or less causes along the way), but the idea is to
    # verify it has a value which makes sense.
    assert len(_run_failing_graph(_SrcWithFailingIter, _WorkingSink)) == 4


def test_iter():
    for c in _run_failing_graph(_SrcWithFailingIter, _WorkingSink):
        # Each cause is an instance of `bt2._ErrorCause`
        # (including subclasses).
        assert isinstance(c, bt2._ErrorCause)


def test_getitem():
    exc = _run_failing_graph(_SrcWithFailingIter, _WorkingSink)

    for i in range(len(exc)):
        c = exc[i]

        # Each cause is an instance of `bt2._ErrorCause
        # (including subclasses).
        assert isinstance(c, bt2._ErrorCause)


def test_getitem_index_error():
    exc = _run_failing_graph(_SrcWithFailingIter, _WorkingSink)

    with pytest.raises(IndexError):
        exc[len(exc)]


# Test that if we do:
#
#     try:
#         ...
#     except bt2._Error as exc:
#         raise ValueError('oh noes') from exc
#
# then we are able to fetch the causes of the original `bt2._Error` in
# the exception chain.  Also, each exception in the chain should become
# one cause once caught.
def test_exc_chaining():
    exc = _run_failing_graph(_SrcWithFailingIter, _SinkWithExceptionChaining)
    assert len(exc) == 5
    assert isinstance(exc[0], bt2._MessageIteratorErrorCause)
    assert exc[0].component_class_name == "_SrcWithFailingIter"
    assert "ValueError: User message iterator is failing" in exc[0].message
    assert isinstance(exc[1], bt2._ErrorCause)
    assert isinstance(exc[2], bt2._ComponentErrorCause)
    assert exc[2].component_class_name == "_SinkWithExceptionChaining"
    assert "bt2._Error" in exc[2].message
    assert isinstance(exc[3], bt2._ComponentErrorCause)
    assert exc[3].component_class_name == "_SinkWithExceptionChaining"
    assert "ValueError: oops" in exc[3].message
    assert isinstance(exc[4], bt2._ErrorCause)


def test_unknown_error_cause():
    cause = _run_failing_graph(_SrcWithFailingIter, _SinkWithExceptionChaining)[-1]
    assert type(cause) is bt2._ErrorCause
    _check_common_cause(cause)


def test_comp_error_cause():
    cause = _run_failing_graph(_SrcWithFailingInit, _SinkWithExceptionChaining)[0]
    assert type(cause) is bt2._ComponentErrorCause
    _check_common_cause(cause)
    assert "Source is failing" in cause.message
    assert cause.component_name == "src"
    assert cause.component_class_type == bt2.ComponentClassType.SOURCE
    assert cause.component_class_name == "_SrcWithFailingInit"
    assert cause.plugin_name is None


def test_comp_cls_error_cause():
    query_exec = bt2.QueryExecutor(_SinkWithFailingQuery, "hello")

    with pytest.raises(bt2._Error) as exc_info:
        query_exec.query()

    cause = exc_info.value[0]
    assert type(cause) is bt2._ComponentClassErrorCause
    _check_common_cause(cause)
    assert "Query is failing" in cause.message
    assert cause.component_class_type == bt2.ComponentClassType.SINK
    assert cause.component_class_name == "_SinkWithFailingQuery"
    assert cause.plugin_name is None


def test_msg_iter_error_cause():
    cause = _run_failing_graph(_SrcWithFailingIter, _SinkWithExceptionChaining)[0]
    assert type(cause) is bt2._MessageIteratorErrorCause
    _check_common_cause(cause)
    assert "User message iterator is failing" in cause.message
    assert cause.component_name == "src"
    assert cause.component_output_port_name == "out"
    assert cause.component_class_type == bt2.ComponentClassType.SOURCE
    assert cause.component_class_name == "_SrcWithFailingIter"
    assert cause.plugin_name is None


# Test __str__().
#
# We don't need to test the precise format used, but just that it
# doesn't miserably crash and that it contains some expected bits.
def test_str():
    s = str(_run_failing_graph(_SrcWithFailingIter, _SinkWithExceptionChaining))
    assert "[src (out): 'source._SrcWithFailingIter']" in s
    assert "ValueError: oops" in s
