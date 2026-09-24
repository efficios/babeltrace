# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import typing
import functools

from bt2 import mip as bt2_mip
from bt2 import port as bt2_port
from bt2 import error as bt2_error
from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import logging as bt2_logging
from bt2 import component as bt2_component
from bt2 import native_bt
from bt2 import connection as bt2_connection
from bt2 import interrupter as bt2_interrupter


def _graph_port_added_listener_from_native(
    user_listener, component_ptr, component_type, port_ptr, port_type
):
    user_listener(
        bt2_component._create_component_from_const_ptr_and_get_ref(
            component_ptr, component_type
        ),
        bt2_port._create_from_const_ptr_and_get_ref(port_ptr, port_type),
    )


class Graph(bt2_object._SharedObject):
    @staticmethod
    def _get_ref(ptr):
        native_bt.graph_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.graph_put_ref(ptr)

    def __init__(self, mip_version: int = 0):
        bt2_utils._check_uint64(mip_version)

        if mip_version > bt2_mip.get_maximal_mip_version():
            raise ValueError(f"unknown MIP version {mip_version}")

        ptr = native_bt.graph_create(mip_version)

        if ptr is None:
            raise bt2_error._MemoryError("cannot create graph object")

        super().__init__(ptr)

        # list of listener partials to keep a reference as long as
        # this graph exists
        self._listener_partials = []

    @typing.overload
    def add_component(
        self,
        component_class: typing.Union[
            typing.Type[bt2_component._UserSourceComponent],
            bt2_component._SourceComponentClassConst,
        ],
        name: str,
        params: bt2_component._ComponentParams = None,
        obj: object = None,
        logging_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
    ) -> bt2_component._GenericSourceComponentConst: ...

    @typing.overload
    def add_component(
        self,
        component_class: typing.Union[
            typing.Type[bt2_component._UserFilterComponent],
            bt2_component._FilterComponentClassConst,
        ],
        name: str,
        params: bt2_component._ComponentParams = None,
        obj: object = None,
        logging_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
    ) -> bt2_component._GenericFilterComponentConst: ...

    @typing.overload
    def add_component(
        self,
        component_class: typing.Union[
            typing.Type[bt2_component._UserSinkComponent],
            bt2_component._SinkComponentClassConst,
        ],
        name: str,
        params: bt2_component._ComponentParams = None,
        obj: object = None,
        logging_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
    ) -> bt2_component._GenericSinkComponentConst: ...

    def add_component(
        self,
        component_class: typing.Union[
            bt2_component._ComponentClassConst,
            typing.Type[bt2_component._UserComponent],
        ],
        name: str,
        params: bt2_component._ComponentParams = None,
        obj: object = None,
        logging_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
    ) -> bt2_component._ComponentConst:
        if isinstance(component_class, bt2_component._SourceComponentClassConst):
            cc_ptr = component_class._ptr
            add_fn = native_bt.graph_add_source_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
        elif isinstance(component_class, bt2_component._FilterComponentClassConst):
            cc_ptr = component_class._ptr
            add_fn = native_bt.graph_add_filter_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
        elif isinstance(component_class, bt2_component._SinkComponentClassConst):
            cc_ptr = component_class._ptr
            add_fn = native_bt.graph_add_sink_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SINK
        elif issubclass(component_class, bt2_component._UserSourceComponent):
            cc_ptr = component_class._bt_cc_ptr
            add_fn = native_bt.graph_add_source_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
        elif issubclass(component_class, bt2_component._UserSinkComponent):
            cc_ptr = component_class._bt_cc_ptr
            add_fn = native_bt.graph_add_sink_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SINK
        elif issubclass(component_class, bt2_component._UserFilterComponent):
            cc_ptr = component_class._bt_cc_ptr
            add_fn = native_bt.graph_add_filter_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
        else:
            raise TypeError(
                f"'{component_class.__class__.__name__}' is not a component class"
            )

        bt2_utils._check_str(name)
        bt2_utils._check_type(logging_level, bt2_logging.LoggingLevel)

        if obj is not None and not native_bt.component_class_is_python(
            component_class._bt_component_class_ptr()
        ):
            raise ValueError("cannot pass a Python object to a non-Python component")

        if params is not None and not isinstance(params, (dict, bt2_value.MapValue)):
            raise TypeError("'params' parameter is not a 'dict' or a 'bt2.MapValue'.")

        params = bt2_value.create_value(params)
        comp_ptr = add_fn(
            self._ptr,
            cc_ptr,
            name,
            params._ptr if params is not None else None,
            obj,
            logging_level.value,
        )
        return bt2_component._create_component_from_const_ptr_and_get_ref(
            comp_ptr, cc_type
        )

    def connect_ports(
        self,
        upstream_port: bt2_port._OutputPortConst,
        downstream_port: bt2_port._InputPortConst,
    ) -> bt2_connection._ConnectionConst:
        bt2_utils._check_type(upstream_port, bt2_port._OutputPortConst)
        bt2_utils._check_type(downstream_port, bt2_port._InputPortConst)

        # A user component port wraps a self component port pointer,
        # not a port pointer.
        for port in (upstream_port, downstream_port):
            if isinstance(port, bt2_port._UserComponentPort):
                raise TypeError(
                    f"cannot connect a '{port.__class__.__name__}' object: use the ports of the component object which `Graph.add_component()` returns"
                )

        conn_ptr = native_bt.graph_connect_ports(
            self._ptr, upstream_port._ptr, downstream_port._ptr
        )
        return bt2_connection._ConnectionConst._create_from_ptr_and_get_ref(conn_ptr)

    def add_port_added_listener(
        self,
        listener: typing.Callable[
            [bt2_component._ComponentConst, bt2_port._PortConst], None
        ],
    ):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        listener_from_native = functools.partial(
            _graph_port_added_listener_from_native, listener
        )

        # Keep the partial's reference, even if adding the listener
        # fails below: the graph may still hold some of the native
        # listeners, and there's no way to remove them.
        self._listener_partials.append(listener_from_native)

        if (
            native_bt.graph_add_port_added_listener(self._ptr, listener_from_native)
            is None
        ):
            raise bt2_error._Error("cannot add listener to graph object")

    def run_once(self):
        bt2_utils._handle_func_status(
            native_bt.graph_run_once(self._ptr), "graph object could not run once"
        )

    def run(self):
        bt2_utils._handle_func_status(
            native_bt.graph_run(self._ptr), "graph object stopped running"
        )

    def add_interrupter(self, interrupter: bt2_interrupter.Interrupter):
        bt2_utils._check_type(interrupter, bt2_interrupter.Interrupter)
        native_bt.graph_add_interrupter(self._ptr, interrupter._ptr)

    @property
    def default_interrupter(self) -> bt2_interrupter.Interrupter:
        return bt2_interrupter.Interrupter._create_from_ptr_and_get_ref(
            native_bt.graph_borrow_default_interrupter(self._ptr)
        )
