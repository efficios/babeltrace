# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import sys

# import all public names
from bt2.clock_class import ClockClassOffset
from bt2.clock_snapshot import _ClockSnapshotConst
from bt2.clock_snapshot import _UnknownClockSnapshot
from bt2.component import _SourceComponentClassConst
from bt2.component import _FilterComponentClassConst
from bt2.component import _SinkComponentClassConst
from bt2.component import _SourceComponentConst
from bt2.component import _FilterComponentConst
from bt2.component import _SinkComponentConst
from bt2.component import _UserSourceComponent
from bt2.component import _UserFilterComponent
from bt2.component import _UserSinkComponent
from bt2.component_descriptor import ComponentDescriptor
from bt2.error import ComponentClassType
from bt2.error import _ErrorCause
from bt2.error import _ComponentErrorCause
from bt2.error import _ComponentClassErrorCause
from bt2.error import _MessageIteratorErrorCause
from bt2.error import _Error
from bt2.event_class import EventClassLogLevel
from bt2.field import _BoolField
from bt2.field import _BitArrayField
from bt2.field import _IntegerField
from bt2.field import _UnsignedIntegerField
from bt2.field import _SignedIntegerField
from bt2.field import _RealField
from bt2.field import _SinglePrecisionRealField
from bt2.field import _DoublePrecisionRealField
from bt2.field import _EnumerationField
from bt2.field import _UnsignedEnumerationField
from bt2.field import _SignedEnumerationField
from bt2.field import _StringField
from bt2.field import _StructureField
from bt2.field import _OptionField
from bt2.field import _VariantField
from bt2.field import _ArrayField
from bt2.field import _StaticArrayField
from bt2.field import _DynamicArrayField
from bt2.field import _BoolFieldConst
from bt2.field import _BitArrayFieldConst
from bt2.field import _IntegerFieldConst
from bt2.field import _UnsignedIntegerFieldConst
from bt2.field import _SignedIntegerFieldConst
from bt2.field import _RealFieldConst
from bt2.field import _SinglePrecisionRealFieldConst
from bt2.field import _DoublePrecisionRealFieldConst
from bt2.field import _EnumerationFieldConst
from bt2.field import _UnsignedEnumerationFieldConst
from bt2.field import _SignedEnumerationFieldConst
from bt2.field import _StringFieldConst
from bt2.field import _StructureFieldConst
from bt2.field import _OptionFieldConst
from bt2.field import _VariantFieldConst
from bt2.field import _ArrayFieldConst
from bt2.field import _StaticArrayFieldConst
from bt2.field import _DynamicArrayFieldConst
from bt2.field_class import IntegerDisplayBase
from bt2.field_class import _BoolFieldClass
from bt2.field_class import _BitArrayFieldClass
from bt2.field_class import _IntegerFieldClass
from bt2.field_class import _UnsignedIntegerFieldClass
from bt2.field_class import _SignedIntegerFieldClass
from bt2.field_class import _RealFieldClass
from bt2.field_class import _EnumerationFieldClass
from bt2.field_class import _UnsignedEnumerationFieldClass
from bt2.field_class import _SignedEnumerationFieldClass
from bt2.field_class import _StringFieldClass
from bt2.field_class import _StructureFieldClass
from bt2.field_class import _OptionFieldClass
from bt2.field_class import _OptionWithSelectorFieldClass
from bt2.field_class import _OptionWithBoolSelectorFieldClass
from bt2.field_class import _OptionWithIntegerSelectorFieldClass
from bt2.field_class import _OptionWithUnsignedIntegerSelectorFieldClass
from bt2.field_class import _OptionWithSignedIntegerSelectorFieldClass
from bt2.field_class import _VariantFieldClass
from bt2.field_class import _VariantFieldClassWithoutSelector
from bt2.field_class import _VariantFieldClassWithIntegerSelector
from bt2.field_class import _VariantFieldClassWithUnsignedIntegerSelector
from bt2.field_class import _VariantFieldClassWithSignedIntegerSelector
from bt2.field_class import _ArrayFieldClass
from bt2.field_class import _StaticArrayFieldClass
from bt2.field_class import _DynamicArrayFieldClass
from bt2.field_class import _DynamicArrayWithLengthFieldFieldClass
from bt2.field_class import _BoolFieldClassConst
from bt2.field_class import _BitArrayFieldClassConst
from bt2.field_class import _IntegerFieldClassConst
from bt2.field_class import _UnsignedIntegerFieldClassConst
from bt2.field_class import _SignedIntegerFieldClassConst
from bt2.field_class import _RealFieldClassConst
from bt2.field_class import _EnumerationFieldClassConst
from bt2.field_class import _UnsignedEnumerationFieldClassConst
from bt2.field_class import _SignedEnumerationFieldClassConst
from bt2.field_class import _StringFieldClassConst
from bt2.field_class import _StructureFieldClassConst
from bt2.field_class import _OptionFieldClassConst
from bt2.field_class import _OptionWithSelectorFieldClassConst
from bt2.field_class import _OptionWithBoolSelectorFieldClassConst
from bt2.field_class import _OptionWithIntegerSelectorFieldClassConst
from bt2.field_class import _OptionWithUnsignedIntegerSelectorFieldClassConst
from bt2.field_class import _OptionWithSignedIntegerSelectorFieldClassConst
from bt2.field_class import _VariantFieldClassConst
from bt2.field_class import _VariantFieldClassWithoutSelectorConst
from bt2.field_class import _VariantFieldClassWithIntegerSelectorConst
from bt2.field_class import _VariantFieldClassWithUnsignedIntegerSelectorConst
from bt2.field_class import _VariantFieldClassWithSignedIntegerSelectorConst
from bt2.field_class import _ArrayFieldClassConst
from bt2.field_class import _StaticArrayFieldClassConst
from bt2.field_class import _DynamicArrayFieldClassConst
from bt2.field_class import _DynamicArrayWithLengthFieldFieldClassConst
from bt2.field_path import FieldPathScope
from bt2.field_path import _IndexFieldPathItem
from bt2.field_path import _CurrentArrayElementFieldPathItem
from bt2.field_path import _CurrentOptionContentFieldPathItem
from bt2.graph import Graph
from bt2.integer_range_set import SignedIntegerRange
from bt2.integer_range_set import UnsignedIntegerRange
from bt2.integer_range_set import SignedIntegerRangeSet
from bt2.integer_range_set import UnsignedIntegerRangeSet
from bt2.integer_range_set import _SignedIntegerRangeConst
from bt2.integer_range_set import _UnsignedIntegerRangeConst
from bt2.integer_range_set import _SignedIntegerRangeSetConst
from bt2.integer_range_set import _UnsignedIntegerRangeSetConst
from bt2.interrupter import Interrupter
from bt2.logging import LoggingLevel
from bt2.logging import get_minimal_logging_level
from bt2.logging import get_global_logging_level
from bt2.logging import set_global_logging_level
from bt2.message import _EventMessage
from bt2.message import _PacketBeginningMessage
from bt2.message import _PacketEndMessage
from bt2.message import _StreamBeginningMessage
from bt2.message import _StreamEndMessage
from bt2.message import _MessageIteratorInactivityMessage
from bt2.message import _DiscardedEventsMessage
from bt2.message import _DiscardedPacketsMessage
from bt2.message import _EventMessageConst
from bt2.message import _PacketBeginningMessageConst
from bt2.message import _PacketEndMessageConst
from bt2.message import _StreamBeginningMessageConst
from bt2.message import _StreamEndMessageConst
from bt2.message import _MessageIteratorInactivityMessageConst
from bt2.message import _DiscardedEventsMessageConst
from bt2.message import _DiscardedPacketsMessageConst
from bt2.message_iterator import _UserMessageIterator
from bt2.mip import get_greatest_operative_mip_version
from bt2.mip import get_maximal_mip_version
from bt2.plugin import find_plugins_in_path
from bt2.plugin import find_plugins
from bt2.plugin import find_plugin
from bt2.py_plugin import plugin_component_class
from bt2.py_plugin import register_plugin
from bt2.query_executor import QueryExecutor
from bt2.trace_collection_message_iterator import AutoSourceComponentSpec
from bt2.trace_collection_message_iterator import ComponentSpec
from bt2.trace_collection_message_iterator import TraceCollectionMessageIterator
from bt2.value import create_value
from bt2.value import BoolValue
from bt2.value import _IntegerValue
from bt2.value import UnsignedIntegerValue
from bt2.value import SignedIntegerValue
from bt2.value import RealValue
from bt2.value import StringValue
from bt2.value import ArrayValue
from bt2.value import MapValue
from bt2.value import _BoolValueConst
from bt2.value import _IntegerValueConst
from bt2.value import _UnsignedIntegerValueConst
from bt2.value import _SignedIntegerValueConst
from bt2.value import _RealValueConst
from bt2.value import _StringValueConst
from bt2.value import _ArrayValueConst
from bt2.value import _MapValueConst
from bt2.version import __version__

if (sys.version_info.major, sys.version_info.minor) != (3, 4):

    def _del_global_name(name):
        if name in globals():
            del globals()[name]

    # remove private module names from the package
    _del_global_name('_native_bt')
    _del_global_name('clock_class')
    _del_global_name('clock_snapshot')
    _del_global_name('component')
    _del_global_name('connection')
    _del_global_name('error')
    _del_global_name('event')
    _del_global_name('event_class')
    _del_global_name('field')
    _del_global_name('field_class')
    _del_global_name('field_path')
    _del_global_name('graph')
    _del_global_name('integer_range_set')
    _del_global_name('interrupter')
    _del_global_name('logging')
    _del_global_name('message')
    _del_global_name('message_iterator')
    _del_global_name('native_bt')
    _del_global_name('object')
    _del_global_name('packet')
    _del_global_name('plugin')
    _del_global_name('port')
    _del_global_name('py_plugin')
    _del_global_name('query_executor')
    _del_global_name('stream')
    _del_global_name('stream_class')
    _del_global_name('trace')
    _del_global_name('trace_class')
    _del_global_name('trace_collection_message_iterator')
    _del_global_name('utils')
    _del_global_name('value')
    _del_global_name('version')

    # remove private `_del_global_name` name from the package
    del _del_global_name


# remove sys module name from the package
del sys


class _MemoryError(_Error):
    '''Raised when an operation fails due to memory issues.'''


class UnknownObject(Exception):
    """
    Raised when a component class handles a query for an object it doesn't
    know about.
    """

    pass


class _OverflowError(_Error, OverflowError):
    pass


class TryAgain(Exception):
    pass


class Stop(StopIteration):
    pass


class _IncompleteUserClass(Exception):
    pass


def _init_and_register_exit():
    from bt2 import native_bt
    import atexit

    atexit.register(native_bt.bt2_exit_handler)
    native_bt.bt2_init_from_bt2()


_init_and_register_exit()

# remove private `_init_and_register_exit` name from the package
del _init_and_register_exit
