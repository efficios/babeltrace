# SPDX-FileCopyrightText: 2026 EfficiOS Inc.
# SPDX-License-Identifier: GPL-2.0-only

import enum
import types

import bt2
import pytest
import bt_tests_utils as btu


class _Scenario(enum.Enum):
    FAIL_IMMEDIATELY = enum.auto()
    EMPTY_STREAM = enum.auto()
    ALL_MSGS = enum.auto()


# Message iterator class which produces a predefined sequence of every
# message type the `sink.utils.dummy` component is expected to consume.
class _Iter(bt2._UserMessageIterator):
    def __init__(self, config, port):
        test_cfg = port.user_data

        if test_cfg.scenario is _Scenario.FAIL_IMMEDIATELY:
            self._msgs = None
            return

        stream = test_cfg.tc().create_stream(test_cfg.sc, name=test_cfg.name)

        if test_cfg.scenario is _Scenario.EMPTY_STREAM:
            self._msgs = [
                self._create_stream_beginning_message(stream, 0),
                self._create_stream_end_message(stream, 100),
            ]
        elif test_cfg.scenario is _Scenario.ALL_MSGS:
            pkt = stream.create_packet()

            self._msgs = [
                self._create_stream_beginning_message(stream, 0),
                self._create_packet_beginning_message(pkt, 10),
                self._create_event_message(test_cfg.ec, pkt, 20),
                self._create_message_iterator_inactivity_message(test_cfg.clk_cls, 25),
                self._create_discarded_events_message(stream, 3, 26, 28),
                self._create_event_message(test_cfg.ec, pkt, 30),
                self._create_packet_end_message(pkt, 40),
                self._create_discarded_packets_message(stream, 5, 41, 50),
                self._create_stream_end_message(stream, 60),
            ]
        else:
            raise AssertionError(f"unknown scenario `{test_cfg.scenario}`")

    def __next__(self):
        if self._msgs is None:
            raise ValueError("source failure")

        if not self._msgs:
            raise bt2.Stop

        return self._msgs.pop(0)


# Source component class to feed the `sink.utils.dummy` component.
class _Src(bt2._UserSourceComponent, message_iterator_class=_Iter):
    def __init__(self, config, params, obj):
        tc = self._create_trace_class()
        clk_cls = self._create_clock_class()
        sc = tc.create_stream_class(
            default_clock_class=clk_cls,
            supports_packets=True,
            packets_have_beginning_default_clock_snapshot=True,
            packets_have_end_default_clock_snapshot=True,
            supports_discarded_events=True,
            discarded_events_have_default_clock_snapshots=True,
            supports_discarded_packets=True,
            discarded_packets_have_default_clock_snapshots=True,
        )
        ec = sc.create_event_class(name="the-event")
        self._add_output_port(
            "out",
            user_data=types.SimpleNamespace(
                tc=tc,
                sc=sc,
                ec=ec,
                clk_cls=clk_cls,
                scenario=obj,
                name=str(params["name"]) if "name" in params else "the-stream",
            ),
        )

    @staticmethod
    def _user_get_supported_mip_versions(params, obj, log_level):
        return [0, 1]


# Verifies that a `sink.utils.dummy` component class can be located
# within the `utils` plugin and has the expected name.
def test_comp_cls_available(dummy_comp_cls):
    assert dummy_comp_cls.name == "dummy"


# Verifies that a `sink.utils.dummy` component accepts no parameters
# (the `params` argument left to its default).
#
# This also verifies that it consumes a stream that contains only stream
# beginning and stream end messages.
def test_accepts_no_params(dummy_comp_cls):
    btu.convert(
        bt2.ComponentSpec(_Src, obj=_Scenario.EMPTY_STREAM),
        btu.SinkComponentSpec(dummy_comp_cls),
    )


# Verifies that a `sink.utils.dummy` component accepts an explicit empty
# parameter map.
def test_accepts_empty_params(dummy_comp_cls):
    btu.convert(
        bt2.ComponentSpec(_Src, obj=_Scenario.EMPTY_STREAM),
        btu.SinkComponentSpec(dummy_comp_cls, params={}),
    )


# Verifies that a `sink.utils.dummy` component rejects any non-empty
# parameter map.
@pytest.mark.parametrize(
    "params",
    [
        pytest.param({"unexpected": "value"}, id="single-str"),
        pytest.param({"a": 1, "b": 2}, id="multiple"),
        pytest.param({"flag": True}, id="bool"),
    ],
)
def test_rejects_non_empty_params(dummy_comp_cls, params):
    with pytest.raises(bt2._Error) as exc_info:
        btu.convert(
            bt2.ComponentSpec(_Src, obj=_Scenario.EMPTY_STREAM),
            btu.SinkComponentSpec(dummy_comp_cls, params=params),
        )

    assert any(
        "This component expects no parameters" in cause.message
        for cause in exc_info.value
    )


# Verifies that a `sink.utils.dummy` component consumes every expected
# message type without error, at both supported MIP versions.
@pytest.mark.parametrize(
    "mip_version", [pytest.param(0, id="mip-0"), pytest.param(1, id="mip-1")]
)
def test_consumes_all_msg_types(dummy_comp_cls, mip_version):
    btu.convert(
        bt2.ComponentSpec(_Src, obj=_Scenario.ALL_MSGS),
        btu.SinkComponentSpec(dummy_comp_cls),
        mip_version=mip_version,
    )


# Verifies that a `sink.utils.dummy` component consumes messages coming
# from multiple upstream streams (muxed by `flt.utils.muxer`).
def test_multiple_streams(dummy_comp_cls):
    btu.convert(
        [
            bt2.ComponentSpec(
                _Src,
                params={"name": f"stream-{i}"},
                obj=_Scenario.ALL_MSGS,
            )
            for i in range(3)
        ],
        btu.SinkComponentSpec(dummy_comp_cls),
    )


# Verifies that an error raised by the upstream message iterator is
# propagated through a `sink.utils.dummy` component.
def test_propagates_upstream_error(dummy_comp_cls):
    with pytest.raises(bt2._Error) as exc_info:
        btu.convert(
            bt2.ComponentSpec(_Src, obj=_Scenario.FAIL_IMMEDIATELY),
            btu.SinkComponentSpec(dummy_comp_cls),
        )

    causes = list(exc_info.value)
    assert any("source failure" in c.message for c in causes)
    assert any(
        "Failed to get messages from upstream component" in c.message for c in causes
    )
