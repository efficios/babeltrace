# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import sys

from bt2.mip import get_maximal_mip_version, get_greatest_operative_mip_version
from bt2.error import (
    ComponentClassType,
    _Error,
    _ErrorCause,
    _MemoryError,
    _ComponentErrorCause,
    _ComponentClassErrorCause,
    _MessageIteratorErrorCause,
)
from bt2.field import (
    _BoolField,
    _RealField,
    _ArrayField,
    _OptionField,
    _StringField,
    _IntegerField,
    _VariantField,
    _BitArrayField,
    _BoolFieldConst,
    _RealFieldConst,
    _StructureField,
    _ArrayFieldConst,
    _EnumerationField,
    _OptionFieldConst,
    _StaticArrayField,
    _StringFieldConst,
    _DynamicArrayField,
    _IntegerFieldConst,
    _VariantFieldConst,
    _BitArrayFieldConst,
    _SignedIntegerField,
    _StructureFieldConst,
    _UnsignedIntegerField,
    _EnumerationFieldConst,
    _StaticArrayFieldConst,
    _DynamicArrayFieldConst,
    _SignedEnumerationField,
    _SignedIntegerFieldConst,
    _DoublePrecisionRealField,
    _SinglePrecisionRealField,
    _UnsignedEnumerationField,
    _UnsignedIntegerFieldConst,
    _SignedEnumerationFieldConst,
    _DoublePrecisionRealFieldConst,
    _SinglePrecisionRealFieldConst,
    _UnsignedEnumerationFieldConst,
)
from bt2.graph import Graph
from bt2.utils import Stop, TryAgain, UnknownObject, _OverflowError
from bt2.value import (
    MapValue,
    BoolValue,
    RealValue,
    ArrayValue,
    StringValue,
    SignedIntegerValue,
    UnsignedIntegerValue,
    create_value,
    _IntegerValue,
    _MapValueConst,
    _BoolValueConst,
    _RealValueConst,
    _ArrayValueConst,
    _StringValueConst,
    _IntegerValueConst,
    _SignedIntegerValueConst,
    _UnsignedIntegerValueConst,
)
from bt2.plugin import find_plugin, find_plugins, find_plugins_in_path
from bt2.logging import (
    LoggingLevel,
    get_global_logging_level,
    set_global_logging_level,
    get_minimal_logging_level,
)
from bt2.message import (
    _EventMessage,
    _PacketEndMessage,
    _StreamEndMessage,
    _EventMessageConst,
    _PacketEndMessageConst,
    _StreamEndMessageConst,
    _DiscardedEventsMessage,
    _PacketBeginningMessage,
    _StreamBeginningMessage,
    _DiscardedPacketsMessage,
    _DiscardedEventsMessageConst,
    _PacketBeginningMessageConst,
    _StreamBeginningMessageConst,
    _DiscardedPacketsMessageConst,
    _MessageIteratorInactivityMessage,
    _MessageIteratorInactivityMessageConst,
)
from bt2.version import __version__
from bt2.component import (
    _UserSinkComponent,
    _SinkComponentConst,
    _IncompleteUserClass,
    _UserFilterComponent,
    _UserSourceComponent,
    _FilterComponentConst,
    _SourceComponentConst,
    _SinkComponentClassConst,
    _FilterComponentClassConst,
    _SourceComponentClassConst,
)
from bt2.py_plugin import register_plugin, plugin_component_class
from bt2.field_path import (
    FieldPathScope,
    _IndexFieldPathItem,
    _CurrentArrayElementFieldPathItem,
    _CurrentOptionContentFieldPathItem,
)

# import all public names
from bt2.clock_class import ClockClassOffset
from bt2.event_class import EventClassLogLevel
from bt2.field_class import (
    IntegerDisplayBase,
    _BoolFieldClass,
    _RealFieldClass,
    _ArrayFieldClass,
    _OptionFieldClass,
    _StringFieldClass,
    _IntegerFieldClass,
    _VariantFieldClass,
    _BitArrayFieldClass,
    _BoolFieldClassConst,
    _RealFieldClassConst,
    _StructureFieldClass,
    _ArrayFieldClassConst,
    _EnumerationFieldClass,
    _OptionFieldClassConst,
    _StaticArrayFieldClass,
    _StringFieldClassConst,
    _DynamicArrayFieldClass,
    _IntegerFieldClassConst,
    _VariantFieldClassConst,
    _BitArrayFieldClassConst,
    _SignedIntegerFieldClass,
    _StructureFieldClassConst,
    _UnsignedIntegerFieldClass,
    _EnumerationFieldClassConst,
    _StaticArrayFieldClassConst,
    _DynamicArrayFieldClassConst,
    _SignedEnumerationFieldClass,
    _OptionWithSelectorFieldClass,
    _SignedIntegerFieldClassConst,
    _UnsignedEnumerationFieldClass,
    _UnsignedIntegerFieldClassConst,
    _OptionWithBoolSelectorFieldClass,
    _SignedEnumerationFieldClassConst,
    _VariantFieldClassWithoutSelector,
    _OptionWithSelectorFieldClassConst,
    _UnsignedEnumerationFieldClassConst,
    _OptionWithIntegerSelectorFieldClass,
    _VariantFieldClassWithIntegerSelector,
    _DynamicArrayWithLengthFieldFieldClass,
    _OptionWithBoolSelectorFieldClassConst,
    _VariantFieldClassWithoutSelectorConst,
    _OptionWithIntegerSelectorFieldClassConst,
    _OptionWithSignedIntegerSelectorFieldClass,
    _VariantFieldClassWithIntegerSelectorConst,
    _DynamicArrayWithLengthFieldFieldClassConst,
    _VariantFieldClassWithSignedIntegerSelector,
    _OptionWithUnsignedIntegerSelectorFieldClass,
    _VariantFieldClassWithUnsignedIntegerSelector,
    _OptionWithSignedIntegerSelectorFieldClassConst,
    _VariantFieldClassWithSignedIntegerSelectorConst,
    _OptionWithUnsignedIntegerSelectorFieldClassConst,
    _VariantFieldClassWithUnsignedIntegerSelectorConst,
)
from bt2.interrupter import Interrupter
from bt2.clock_snapshot import _ClockSnapshotConst, _UnknownClockSnapshot
from bt2.query_executor import QueryExecutor
from bt2.message_iterator import _UserMessageIterator
from bt2.integer_range_set import (
    SignedIntegerRange,
    UnsignedIntegerRange,
    SignedIntegerRangeSet,
    UnsignedIntegerRangeSet,
    _SignedIntegerRangeConst,
    _UnsignedIntegerRangeConst,
    _SignedIntegerRangeSetConst,
    _UnsignedIntegerRangeSetConst,
)
from bt2.component_descriptor import ComponentDescriptor
from bt2.trace_collection_message_iterator import (
    ComponentSpec,
    AutoSourceComponentSpec,
    TraceCollectionMessageIterator,
)

if (sys.version_info.major, sys.version_info.minor) != (3, 4):

    def _del_global_name(name):
        if name in globals():
            del globals()[name]

    # remove private module names from the package
    _del_global_name("_native_bt")
    _del_global_name("clock_class")
    _del_global_name("clock_snapshot")
    _del_global_name("component")
    _del_global_name("connection")
    _del_global_name("error")
    _del_global_name("event")
    _del_global_name("event_class")
    _del_global_name("field")
    _del_global_name("field_class")
    _del_global_name("field_path")
    _del_global_name("graph")
    _del_global_name("integer_range_set")
    _del_global_name("interrupter")
    _del_global_name("logging")
    _del_global_name("message")
    _del_global_name("message_iterator")
    _del_global_name("native_bt")
    _del_global_name("object")
    _del_global_name("packet")
    _del_global_name("plugin")
    _del_global_name("port")
    _del_global_name("py_plugin")
    _del_global_name("query_executor")
    _del_global_name("stream")
    _del_global_name("stream_class")
    _del_global_name("trace")
    _del_global_name("trace_class")
    _del_global_name("trace_collection_message_iterator")
    _del_global_name("utils")
    _del_global_name("value")
    _del_global_name("version")

    # remove private `_del_global_name` name from the package
    del _del_global_name


# remove sys module name from the package
del sys


def _init_and_register_exit():
    import atexit

    from bt2 import native_bt

    atexit.register(native_bt.bt2_exit_handler)
    native_bt.bt2_init_from_bt2()


_init_and_register_exit()

# remove private `_init_and_register_exit` name from the package
del _init_and_register_exit
