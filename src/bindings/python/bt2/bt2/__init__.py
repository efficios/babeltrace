# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import os
import sys

# With Python ≥ 3.8 on Windows, the DLL lookup mechanism to load native
# modules doesn't search the `PATH` environment variable like everything
# else on this platform.
#
# See <https://docs.python.org/3/whatsnew/3.8.html#bpo-36085-whatsnew>.
#
# Restore this behaviour by doing it manually.
if os.name == "nt" and sys.version_info >= (3, 8):
    for path in os.getenv("PATH", "").split(os.pathsep):
        if os.path.exists(path) and path != ".":
            os.add_dll_directory(path)

del os


from bt2.mip import get_maximal_mip_version, get_greatest_operative_mip_version
from bt2.port import (
    _InputPortConst,
    _OutputPortConst,
    _UserComponentInputPort,
    _UserComponentOutputPort,
)
from bt2.error import (
    ComponentClassType,
    _Error,
    _ErrorCause,
    _MemoryError,
    _ComponentErrorCause,
    _ComponentClassErrorCause,
    _MessageIteratorErrorCause,
)
from bt2.event import _Event, _EventConst
from bt2.field import (
    _Field,
    _BlobField,
    _BoolField,
    _RealField,
    _ArrayField,
    _FieldConst,
    _OptionField,
    _StringField,
    _IntegerField,
    _VariantField,
    _BitArrayField,
    _BlobFieldConst,
    _BoolFieldConst,
    _RealFieldConst,
    _StructureField,
    _ArrayFieldConst,
    _StaticBlobField,
    _DynamicBlobField,
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
    _StaticBlobFieldConst,
    _UnsignedIntegerField,
    _DynamicBlobFieldConst,
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
    _DynamicBlobFieldWithLengthField,
    _DynamicArrayFieldWithLengthField,
    _OptionFieldWithBoolSelectorField,
    _DynamicBlobFieldWithLengthFieldConst,
    _DynamicArrayFieldWithLengthFieldConst,
    _OptionFieldWithBoolSelectorFieldConst,
    _OptionFieldWithSignedIntegerSelectorField,
    _VariantFieldWithSignedIntegerSelectorField,
    _OptionFieldWithUnsignedIntegerSelectorField,
    _VariantFieldWithUnsignedIntegerSelectorField,
    _OptionFieldWithSignedIntegerSelectorFieldConst,
    _VariantFieldWithSignedIntegerSelectorFieldConst,
    _OptionFieldWithUnsignedIntegerSelectorFieldConst,
    _VariantFieldWithUnsignedIntegerSelectorFieldConst,
)
from bt2.graph import Graph
from bt2.trace import _Trace, _TraceConst, _TraceEnvironment, _TraceEnvironmentConst
from bt2.utils import Stop, TryAgain, UnknownObject, _OverflowError, _ListenerHandle
from bt2.value import (
    MapValue,
    BoolValue,
    RealValue,
    ArrayValue,
    StringValue,
    SignedIntegerValue,
    UnsignedIntegerValue,
    _Value,
    _ValueConst,
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
from bt2.packet import _Packet, _PacketConst
from bt2.plugin import (
    _Plugin,
    _PluginSet,
    find_plugin,
    find_plugins,
    _PluginProvider,
    plugin_providers,
    _PluginProviderSet,
    find_plugins_in_path,
)
from bt2.stream import _Stream, _StreamConst
from bt2.logging import (
    LoggingLevel,
    get_global_logging_level,
    set_global_logging_level,
    get_minimal_logging_level,
)
from bt2.message import (
    _Message,
    _EventMessage,
    _MessageConst,
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
    _ComponentConst,
    _ComponentParams,
    _UserComponentType,
    _UserSinkComponent,
    _SinkComponentConst,
    _ComponentClassConst,
    _IncompleteUserClass,
    _UserFilterComponent,
    _UserSourceComponent,
    _FilterComponentConst,
    _SourceComponentConst,
    _SinkComponentClassConst,
    _FilterComponentClassConst,
    _GenericSinkComponentConst,
    _SourceComponentClassConst,
    _GenericFilterComponentConst,
    _GenericSourceComponentConst,
    _UserSinkComponentConfiguration,
    _UserFilterComponentConfiguration,
    _UserSourceComponentConfiguration,
)
from bt2.py_plugin import register_plugin, plugin_component_class
from bt2.connection import _ConnectionConst
from bt2.field_path import (
    FieldPathScope,
    _FieldPathItem,
    _FieldPathConst,
    _IndexFieldPathItem,
    _CurrentArrayElementFieldPathItem,
    _CurrentOptionContentFieldPathItem,
)

# import all public names
from bt2.clock_class import (
    ClockOffset,
    ClockOrigin,
    ClockClassOffset,
    _ClockClass,
    _ClockClassConst,
    _UnknownClockOrigin,
    unknown_clock_origin,
    _UnixEpochClockOrigin,
    unix_epoch_clock_origin,
)
from bt2.event_class import EventClassLogLevel, _EventClass, _EventClassConst
from bt2.field_class import (
    IntegerDisplayBase,
    _FieldClass,
    _BlobFieldClass,
    _BoolFieldClass,
    _RealFieldClass,
    _ArrayFieldClass,
    _FieldClassConst,
    _OptionFieldClass,
    _StringFieldClass,
    _IntegerFieldClass,
    _VariantFieldClass,
    _BitArrayFieldClass,
    _BlobFieldClassConst,
    _BoolFieldClassConst,
    _RealFieldClassConst,
    _StructureFieldClass,
    _ArrayFieldClassConst,
    _StaticBlobFieldClass,
    _DynamicBlobFieldClass,
    _EnumerationFieldClass,
    _OptionFieldClassConst,
    _StaticArrayFieldClass,
    _StringFieldClassConst,
    _DynamicArrayFieldClass,
    _IntegerFieldClassConst,
    _VariantFieldClassConst,
    _BitArrayFieldClassConst,
    _SignedIntegerFieldClass,
    _VariantFieldClassOption,
    _StructureFieldClassConst,
    _StaticBlobFieldClassConst,
    _StructureFieldClassMember,
    _UnsignedIntegerFieldClass,
    _DynamicBlobFieldClassConst,
    _EnumerationFieldClassConst,
    _StaticArrayFieldClassConst,
    _BitArrayFieldClassFlagConst,
    _DynamicArrayFieldClassConst,
    _SignedEnumerationFieldClass,
    _OptionWithSelectorFieldClass,
    _SignedIntegerFieldClassConst,
    _VariantFieldClassOptionConst,
    _DoublePrecisionRealFieldClass,
    _SinglePrecisionRealFieldClass,
    _UnsignedEnumerationFieldClass,
    _StructureFieldClassMemberConst,
    _UnsignedIntegerFieldClassConst,
    _OptionWithBoolSelectorFieldClass,
    _SignedEnumerationFieldClassConst,
    _VariantFieldClassWithoutSelector,
    _OptionFieldClassWithSelectorField,
    _OptionWithSelectorFieldClassConst,
    _DoublePrecisionRealFieldClassConst,
    _SinglePrecisionRealFieldClassConst,
    _UnsignedEnumerationFieldClassConst,
    _OptionWithIntegerSelectorFieldClass,
    _DynamicBlobFieldClassWithLengthField,
    _VariantFieldClassWithIntegerSelector,
    _DynamicArrayFieldClassWithLengthField,
    _DynamicArrayWithLengthFieldFieldClass,
    _OptionFieldClassWithBoolSelectorField,
    _OptionWithBoolSelectorFieldClassConst,
    _VariantFieldClassWithoutSelectorConst,
    _VariantFieldClassWithoutSelectorField,
    _OptionFieldClassWithSelectorFieldConst,
    _SignedEnumerationFieldClassMappingConst,
    _OptionFieldClassWithIntegerSelectorField,
    _OptionWithIntegerSelectorFieldClassConst,
    _DynamicBlobFieldClassWithLengthFieldConst,
    _OptionWithSignedIntegerSelectorFieldClass,
    _UnsignedEnumerationFieldClassMappingConst,
    _VariantFieldClassWithIntegerSelectorConst,
    _VariantFieldClassWithIntegerSelectorField,
    _DynamicArrayFieldClassWithLengthFieldConst,
    _DynamicArrayWithLengthFieldFieldClassConst,
    _OptionFieldClassWithBoolSelectorFieldConst,
    _VariantFieldClassWithoutSelectorFieldConst,
    _VariantFieldClassWithSignedIntegerSelector,
    _OptionWithUnsignedIntegerSelectorFieldClass,
    _VariantFieldClassWithUnsignedIntegerSelector,
    _OptionFieldClassWithIntegerSelectorFieldConst,
    _OptionFieldClassWithSignedIntegerSelectorField,
    _OptionWithSignedIntegerSelectorFieldClassConst,
    _VariantFieldClassWithIntegerSelectorFieldConst,
    _VariantFieldClassWithIntegerSelectorFieldOption,
    _VariantFieldClassWithSignedIntegerSelectorConst,
    _VariantFieldClassWithSignedIntegerSelectorField,
    _OptionFieldClassWithUnsignedIntegerSelectorField,
    _OptionWithUnsignedIntegerSelectorFieldClassConst,
    _VariantFieldClassWithUnsignedIntegerSelectorConst,
    _VariantFieldClassWithUnsignedIntegerSelectorField,
    _OptionFieldClassWithSignedIntegerSelectorFieldConst,
    _VariantFieldClassWithIntegerSelectorFieldOptionConst,
    _VariantFieldClassWithSignedIntegerSelectorFieldConst,
    _OptionFieldClassWithUnsignedIntegerSelectorFieldConst,
    _VariantFieldClassWithSignedIntegerSelectorFieldOption,
    _VariantFieldClassWithUnsignedIntegerSelectorFieldConst,
    _VariantFieldClassWithUnsignedIntegerSelectorFieldOption,
    _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst,
    _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst,
)
from bt2.interrupter import Interrupter
from bt2.trace_class import _TraceClass, _TraceClassConst
from bt2.stream_class import _StreamClass, _StreamClassConst
from bt2.clock_snapshot import _ClockSnapshotConst, _UnknownClockSnapshot
from bt2.field_location import FieldLocationScope, _FieldLocationConst
from bt2.query_executor import QueryExecutor, _PrivateQueryExecutor
from bt2.message_iterator import (
    _UserMessageIterator,
    _MessageIteratorConfiguration,
    _UserComponentInputPortMessageIterator,
)
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
    source_component_specs_from_auto_source_component_specs,
)


def _del_global_name(name):
    if name in globals():
        del globals()[name]


# remove private module names from the package
_del_global_name("clock_class")
_del_global_name("clock_snapshot")
_del_global_name("component")
_del_global_name("component_descriptor")
_del_global_name("connection")
_del_global_name("error")
_del_global_name("event")
_del_global_name("event_class")
_del_global_name("field")
_del_global_name("field_class")
_del_global_name("field_location")
_del_global_name("field_path")
_del_global_name("graph")
_del_global_name("integer_range_set")
_del_global_name("interrupter")
_del_global_name("logging")
_del_global_name("message")
_del_global_name("message_iterator")
_del_global_name("mip")
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
_del_global_name("user_attributes")
_del_global_name("utils")
_del_global_name("value")
_del_global_name("version")

# remove private `_del_global_name` name from the package
del _del_global_name


# remove sys module name from the package
del sys
