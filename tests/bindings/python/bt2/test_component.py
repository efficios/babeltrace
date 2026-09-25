# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2019-2026 EfficiOS Inc.

import bt2


def _create_comp(comp_cls, name=None, log_level=bt2.LoggingLevel.NONE):
    graph = bt2.Graph()

    if name is None:
        name = "comp"

    return graph.add_component(comp_cls, name, logging_level=log_level)


def test_user_comp_name():
    name = None

    class MySink(bt2._UserSinkComponent):
        def __init__(comp_self, config, params, obj):
            nonlocal name
            name = comp_self.name

        def _user_consume(self):
            pass

    _create_comp(MySink, "yaes")
    assert name == "yaes"


def test_user_comp_log_level():
    log_level = None

    class MySink(bt2._UserSinkComponent):
        def __init__(comp_self, config, params, obj):
            nonlocal log_level
            log_level = comp_self.logging_level

        def _user_consume(self):
            pass

    _create_comp(MySink, "yaes", bt2.LoggingLevel.INFO)
    assert log_level == bt2.LoggingLevel.INFO


def test_user_comp_graph_mip_version():
    mip_version = None

    class MySink(bt2._UserSinkComponent):
        def __init__(comp_self, config, params, obj):
            nonlocal mip_version
            mip_version = comp_self._graph_mip_version

        def _user_consume(self):
            pass

    _create_comp(MySink, "yaes", bt2.LoggingLevel.INFO)
    assert mip_version == 0


def test_user_comp_cls():
    cls = None

    class MySink(bt2._UserSinkComponent):
        def __init__(comp_self, config, params, obj):
            nonlocal cls
            cls = comp_self.cls

        def _user_consume(self):
            pass

    _create_comp(MySink)
    assert cls == MySink


def test_user_comp_addr():
    addr = None

    class MySink(bt2._UserSinkComponent):
        def __init__(comp_self, config, params, obj):
            nonlocal addr
            addr = comp_self.addr

        def _user_consume(self):
            pass

    _create_comp(MySink)
    assert isinstance(addr, int)
    assert addr != 0


def test_user_comp_finalize():
    finalized = False

    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        def _user_finalize(comp_self):
            nonlocal finalized
            finalized = True

    graph = bt2.Graph()
    comp = graph.add_component(MySink, "lel")
    del graph
    del comp
    assert finalized


def test_src_comp_cfg():
    config_type = None

    class MySrc(
        bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
    ):
        def __init__(comp_self, config, params, obj):
            nonlocal config_type
            config_type = type(config)

    _create_comp(MySrc)
    assert config_type is bt2._UserSourceComponentConfiguration


def test_flt_comp_cfg():
    config_type = None

    class MyFlt(
        bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
    ):
        def __init__(comp_self, config, params, obj):
            nonlocal config_type
            config_type = type(config)

    _create_comp(MyFlt)
    assert config_type is bt2._UserFilterComponentConfiguration


def test_sink_comp_cfg():
    config_type = None

    class MySink(bt2._UserSinkComponent):
        def __init__(comp_self, config, params, obj):
            nonlocal config_type
            config_type = type(config)

        def _user_consume(self):
            pass

    _create_comp(MySink)
    assert config_type is bt2._UserSinkComponentConfiguration


class _MySink(bt2._UserSinkComponent):
    def _user_consume(self):
        pass


def test_generic_comp_name():
    comp = _create_comp(_MySink, "yaes")
    assert comp.name == "yaes"


def test_generic_comp_logging_level():
    comp = _create_comp(_MySink, "yaes", bt2.LoggingLevel.WARNING)
    assert comp.logging_level == bt2.LoggingLevel.WARNING


def test_generic_comp_class():
    comp = _create_comp(_MySink)
    assert comp.cls == _MySink


def test_generic_comp_addr():
    comp = _create_comp(_MySink)
    assert isinstance(comp.addr, int)
    assert comp.addr != 0
