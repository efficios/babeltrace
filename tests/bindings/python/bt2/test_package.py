#
# Copyright (C) 2019 EfficiOS Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import bt2
import unittest


class PackageTestCase(unittest.TestCase):
    def _assert_in_bt2(self, name):
        self.assertIn(name, dir(bt2))

    def test_has_ClockClassOffset(self):
        self._assert_in_bt2('ClockClassOffset')

    def test_has__ClockSnapshotConst(self):
        self._assert_in_bt2('_ClockSnapshotConst')

    def test_has__UnknownClockSnapshot(self):
        self._assert_in_bt2('_UnknownClockSnapshot')

    def test_has__SourceComponentClassConst(self):
        self._assert_in_bt2('_SourceComponentClassConst')

    def test_has__FilterComponentClassConst(self):
        self._assert_in_bt2('_FilterComponentClassConst')

    def test_has__SinkComponentClassConst(self):
        self._assert_in_bt2('_SinkComponentClassConst')

    def test_has__SourceComponentConst(self):
        self._assert_in_bt2('_SourceComponentConst')

    def test_has__FilterComponentConst(self):
        self._assert_in_bt2('_FilterComponentConst')

    def test_has__SinkComponentConst(self):
        self._assert_in_bt2('_SinkComponentConst')

    def test_has__UserSourceComponent(self):
        self._assert_in_bt2('_UserSourceComponent')

    def test_has__UserFilterComponent(self):
        self._assert_in_bt2('_UserFilterComponent')

    def test_has__UserSinkComponent(self):
        self._assert_in_bt2('_UserSinkComponent')

    def test_has_ComponentClassType(self):
        self._assert_in_bt2('ComponentClassType')

    def test_has__ErrorCause(self):
        self._assert_in_bt2('_ErrorCause')

    def test_has__ComponentErrorCause(self):
        self._assert_in_bt2('_ComponentErrorCause')

    def test_has__ComponentClassErrorCause(self):
        self._assert_in_bt2('_ComponentClassErrorCause')

    def test_has__MessageIteratorErrorCause(self):
        self._assert_in_bt2('_MessageIteratorErrorCause')

    def test_has__Error(self):
        self._assert_in_bt2('_Error')

    def test_has_EventClassLogLevel(self):
        self._assert_in_bt2('EventClassLogLevel')

    def test_has__BoolField(self):
        self._assert_in_bt2('_BoolField')

    def test_has__BitArrayField(self):
        self._assert_in_bt2('_BitArrayField')

    def test_has__IntegerField(self):
        self._assert_in_bt2('_IntegerField')

    def test_has__UnsignedIntegerField(self):
        self._assert_in_bt2('_UnsignedIntegerField')

    def test_has__SignedIntegerField(self):
        self._assert_in_bt2('_SignedIntegerField')

    def test_has__RealField(self):
        self._assert_in_bt2('_RealField')

    def test_has__EnumerationField(self):
        self._assert_in_bt2('_EnumerationField')

    def test_has__UnsignedEnumerationField(self):
        self._assert_in_bt2('_UnsignedEnumerationField')

    def test_has__SignedEnumerationField(self):
        self._assert_in_bt2('_SignedEnumerationField')

    def test_has__StringField(self):
        self._assert_in_bt2('_StringField')

    def test_has__StructureField(self):
        self._assert_in_bt2('_StructureField')

    def test_has__OptionField(self):
        self._assert_in_bt2('_VariantField')

    def test_has__VariantField(self):
        self._assert_in_bt2('_VariantField')

    def test_has__ArrayField(self):
        self._assert_in_bt2('_ArrayField')

    def test_has__StaticArrayField(self):
        self._assert_in_bt2('_StaticArrayField')

    def test_has__DynamicArrayField(self):
        self._assert_in_bt2('_DynamicArrayField')

    def test_has__BoolFieldConst(self):
        self._assert_in_bt2('_BoolFieldConst')

    def test_has__BitArrayFieldConst(self):
        self._assert_in_bt2('_BitArrayFieldConst')

    def test_has__IntegerFieldConst(self):
        self._assert_in_bt2('_IntegerFieldConst')

    def test_has__UnsignedIntegerFieldConst(self):
        self._assert_in_bt2('_UnsignedIntegerFieldConst')

    def test_has__SignedIntegerFieldConst(self):
        self._assert_in_bt2('_SignedIntegerFieldConst')

    def test_has__RealFieldConst(self):
        self._assert_in_bt2('_RealFieldConst')

    def test_has__EnumerationFieldConst(self):
        self._assert_in_bt2('_EnumerationFieldConst')

    def test_has__UnsignedEnumerationFieldConst(self):
        self._assert_in_bt2('_UnsignedEnumerationFieldConst')

    def test_has__SignedEnumerationFieldConst(self):
        self._assert_in_bt2('_SignedEnumerationFieldConst')

    def test_has__StringFieldConst(self):
        self._assert_in_bt2('_StringFieldConst')

    def test_has__StructureFieldConst(self):
        self._assert_in_bt2('_StructureFieldConst')

    def test_has__OptionFieldConst(self):
        self._assert_in_bt2('_VariantFieldConst')

    def test_has__VariantFieldConst(self):
        self._assert_in_bt2('_VariantFieldConst')

    def test_has__ArrayFieldConst(self):
        self._assert_in_bt2('_ArrayFieldConst')

    def test_has__StaticArrayFieldConst(self):
        self._assert_in_bt2('_StaticArrayFieldConst')

    def test_has__DynamicArrayFieldConst(self):
        self._assert_in_bt2('_DynamicArrayFieldConst')

    def test_has_IntegerDisplayBase(self):
        self._assert_in_bt2('IntegerDisplayBase')

    def test_has__BoolFieldClass(self):
        self._assert_in_bt2('_BoolFieldClass')

    def test_has__BitArrayFieldClass(self):
        self._assert_in_bt2('_BitArrayFieldClass')

    def test_has__IntegerFieldClass(self):
        self._assert_in_bt2('_IntegerFieldClass')

    def test_has__UnsignedIntegerFieldClass(self):
        self._assert_in_bt2('_UnsignedIntegerFieldClass')

    def test_has__SignedIntegerFieldClass(self):
        self._assert_in_bt2('_SignedIntegerFieldClass')

    def test_has__RealFieldClass(self):
        self._assert_in_bt2('_RealFieldClass')

    def test_has__EnumerationFieldClass(self):
        self._assert_in_bt2('_EnumerationFieldClass')

    def test_has__UnsignedEnumerationFieldClass(self):
        self._assert_in_bt2('_UnsignedEnumerationFieldClass')

    def test_has__SignedEnumerationFieldClass(self):
        self._assert_in_bt2('_SignedEnumerationFieldClass')

    def test_has__StringFieldClass(self):
        self._assert_in_bt2('_StringFieldClass')

    def test_has__StructureFieldClass(self):
        self._assert_in_bt2('_StructureFieldClass')

    def test_has__OptionFieldClass(self):
        self._assert_in_bt2('_OptionFieldClass')

    def test_has__VariantFieldClass(self):
        self._assert_in_bt2('_VariantFieldClass')

    def test_has__VariantFieldClassWithoutSelector(self):
        self._assert_in_bt2('_VariantFieldClassWithoutSelector')

    def test_has__VariantFieldClassWithSelector(self):
        self._assert_in_bt2('_VariantFieldClassWithSelector')

    def test_has__VariantFieldClassWithUnsignedSelector(self):
        self._assert_in_bt2('_VariantFieldClassWithUnsignedSelector')

    def test_has__VariantFieldClassWithSignedSelector(self):
        self._assert_in_bt2('_VariantFieldClassWithSignedSelector')

    def test_has__ArrayFieldClass(self):
        self._assert_in_bt2('_ArrayFieldClass')

    def test_has__StaticArrayFieldClass(self):
        self._assert_in_bt2('_StaticArrayFieldClass')

    def test_has__DynamicArrayFieldClass(self):
        self._assert_in_bt2('_DynamicArrayFieldClass')

    def test_has__BoolFieldClassConst(self):
        self._assert_in_bt2('_BoolFieldClassConst')

    def test_has__BitArrayFieldClassConst(self):
        self._assert_in_bt2('_BitArrayFieldClassConst')

    def test_has__IntegerFieldClassConst(self):
        self._assert_in_bt2('_IntegerFieldClassConst')

    def test_has__UnsignedIntegerFieldClassConst(self):
        self._assert_in_bt2('_UnsignedIntegerFieldClassConst')

    def test_has__SignedIntegerFieldClassConst(self):
        self._assert_in_bt2('_SignedIntegerFieldClassConst')

    def test_has__RealFieldClassConst(self):
        self._assert_in_bt2('_RealFieldClassConst')

    def test_has__EnumerationFieldClassConst(self):
        self._assert_in_bt2('_EnumerationFieldClassConst')

    def test_has__UnsignedEnumerationFieldClassConst(self):
        self._assert_in_bt2('_UnsignedEnumerationFieldClassConst')

    def test_has__SignedEnumerationFieldClassConst(self):
        self._assert_in_bt2('_SignedEnumerationFieldClassConst')

    def test_has__StringFieldClassConst(self):
        self._assert_in_bt2('_StringFieldClassConst')

    def test_has__StructureFieldClassConst(self):
        self._assert_in_bt2('_StructureFieldClassConst')

    def test_has__OptionFieldClassConst(self):
        self._assert_in_bt2('_OptionFieldClassConst')

    def test_has__VariantFieldClassConst(self):
        self._assert_in_bt2('_VariantFieldClassConst')

    def test_has__VariantFieldClassWithoutSelectorConst(self):
        self._assert_in_bt2('_VariantFieldClassWithoutSelectorConst')

    def test_has__VariantFieldClassWithSelectorConst(self):
        self._assert_in_bt2('_VariantFieldClassWithSelectorConst')

    def test_has__VariantFieldClassWithUnsignedSelectorConst(self):
        self._assert_in_bt2('_VariantFieldClassWithUnsignedSelectorConst')

    def test_has__VariantFieldClassWithSignedSelectorConst(self):
        self._assert_in_bt2('_VariantFieldClassWithSignedSelectorConst')

    def test_has__ArrayFieldClassConst(self):
        self._assert_in_bt2('_ArrayFieldClassConst')

    def test_has__StaticArrayFieldClassConst(self):
        self._assert_in_bt2('_StaticArrayFieldClassConst')

    def test_has__DynamicArrayFieldClassConst(self):
        self._assert_in_bt2('_DynamicArrayFieldClassConst')

    def test_has_FieldPathScope(self):
        self._assert_in_bt2('FieldPathScope')

    def test_has__IndexFieldPathItem(self):
        self._assert_in_bt2('_IndexFieldPathItem')

    def test_has__CurrentArrayElementFieldPathItem(self):
        self._assert_in_bt2('_CurrentArrayElementFieldPathItem')

    def test_has__CurrentOptionContentFieldPathItem(self):
        self._assert_in_bt2('_CurrentOptionContentFieldPathItem')

    def test_has_ComponentDescriptor(self):
        self._assert_in_bt2('ComponentDescriptor')

    def test_has_Graph(self):
        self._assert_in_bt2('Graph')

    def test_has_SignedIntegerRange(self):
        self._assert_in_bt2('SignedIntegerRange')

    def test_has_UnsignedIntegerRange(self):
        self._assert_in_bt2('UnsignedIntegerRange')

    def test_has_SignedIntegerRangeSet(self):
        self._assert_in_bt2('SignedIntegerRangeSet')

    def test_has_UnsignedIntegerRangeSet(self):
        self._assert_in_bt2('UnsignedIntegerRangeSet')

    def test_has_Interrupter(self):
        self._assert_in_bt2('Interrupter')

    def test_has_LoggingLevel(self):
        self._assert_in_bt2('LoggingLevel')

    def test_has_get_minimal_logging_level(self):
        self._assert_in_bt2('get_minimal_logging_level')

    def test_has_get_global_logging_level(self):
        self._assert_in_bt2('get_global_logging_level')

    def test_has_set_global_logging_level(self):
        self._assert_in_bt2('set_global_logging_level')

    def test_has__EventMessage(self):
        self._assert_in_bt2('_EventMessage')

    def test_has__PacketBeginningMessage(self):
        self._assert_in_bt2('_PacketBeginningMessage')

    def test_has__PacketEndMessage(self):
        self._assert_in_bt2('_PacketEndMessage')

    def test_has__StreamBeginningMessage(self):
        self._assert_in_bt2('_StreamBeginningMessage')

    def test_has__StreamEndMessage(self):
        self._assert_in_bt2('_StreamEndMessage')

    def test_has__MessageIteratorInactivityMessage(self):
        self._assert_in_bt2('_MessageIteratorInactivityMessage')

    def test_has__DiscardedEventsMessage(self):
        self._assert_in_bt2('_DiscardedEventsMessage')

    def test_has__DiscardedPacketsMessage(self):
        self._assert_in_bt2('_DiscardedPacketsMessage')

    def test_has__EventMessageConst(self):
        self._assert_in_bt2('_EventMessageConst')

    def test_has__PacketBeginningMessageConst(self):
        self._assert_in_bt2('_PacketBeginningMessageConst')

    def test_has__PacketEndMessageConst(self):
        self._assert_in_bt2('_PacketEndMessageConst')

    def test_has__StreamBeginningMessageConst(self):
        self._assert_in_bt2('_StreamBeginningMessageConst')

    def test_has__StreamEndMessageConst(self):
        self._assert_in_bt2('_StreamEndMessageConst')

    def test_has__MessageIteratorInactivityMessageConst(self):
        self._assert_in_bt2('_MessageIteratorInactivityMessageConst')

    def test_has__DiscardedEventsMessageConst(self):
        self._assert_in_bt2('_DiscardedEventsMessageConst')

    def test_has__DiscardedPacketsMessageConst(self):
        self._assert_in_bt2('_DiscardedPacketsMessageConst')

    def test_has__UserMessageIterator(self):
        self._assert_in_bt2('_UserMessageIterator')

    def test_has_find_plugins_in_path(self):
        self._assert_in_bt2('find_plugins_in_path')

    def test_has_find_plugins(self):
        self._assert_in_bt2('find_plugins')

    def test_has_find_plugin(self):
        self._assert_in_bt2('find_plugin')

    def test_has_plugin_component_class(self):
        self._assert_in_bt2('plugin_component_class')

    def test_has_register_plugin(self):
        self._assert_in_bt2('register_plugin')

    def test_has_QueryExecutor(self):
        self._assert_in_bt2('QueryExecutor')

    def test_has_AutoSourceComponentSpec(self):
        self._assert_in_bt2('AutoSourceComponentSpec')

    def test_has_ComponentSpec(self):
        self._assert_in_bt2('ComponentSpec')

    def test_has_TraceCollectionMessageIterator(self):
        self._assert_in_bt2('TraceCollectionMessageIterator')

    def test_has_create_value(self):
        self._assert_in_bt2('create_value')

    def test_has_BoolValue(self):
        self._assert_in_bt2('BoolValue')

    def test_has__IntegerValue(self):
        self._assert_in_bt2('_IntegerValue')

    def test_has_UnsignedIntegerValue(self):
        self._assert_in_bt2('UnsignedIntegerValue')

    def test_has_SignedIntegerValue(self):
        self._assert_in_bt2('SignedIntegerValue')

    def test_has_RealValue(self):
        self._assert_in_bt2('RealValue')

    def test_has_StringValue(self):
        self._assert_in_bt2('StringValue')

    def test_has_ArrayValue(self):
        self._assert_in_bt2('ArrayValue')

    def test_has_MapValue(self):
        self._assert_in_bt2('MapValue')

    def test_has_BoolValueConst(self):
        self._assert_in_bt2('_BoolValueConst')

    def test_has__IntegerValueConst(self):
        self._assert_in_bt2('_IntegerValueConst')

    def test_has_UnsignedIntegerValueConst(self):
        self._assert_in_bt2('_UnsignedIntegerValueConst')

    def test_has_SignedIntegerValueConst(self):
        self._assert_in_bt2('_SignedIntegerValueConst')

    def test_has_RealValueConst(self):
        self._assert_in_bt2('_RealValueConst')

    def test_has_StringValueConst(self):
        self._assert_in_bt2('_StringValueConst')

    def test_has_ArrayValueConst(self):
        self._assert_in_bt2('_ArrayValueConst')

    def test_has_MapValueConst(self):
        self._assert_in_bt2('_MapValueConst')

    def test_has___version__(self):
        self._assert_in_bt2('__version__')
