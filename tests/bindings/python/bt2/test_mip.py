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

import unittest
import bt2


class MipTestCase(unittest.TestCase):
    def test_get_greatest_operative_mip_version(self):
        class Source1(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            @classmethod
            def _user_get_supported_mip_versions(cls, params, obj, log_level):
                return [0, 1]

        class Source2(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            @classmethod
            def _user_get_supported_mip_versions(cls, params, obj, log_level):
                return [0, 2]

        class Source3(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

        descriptors = [
            bt2.ComponentDescriptor(Source1),
            bt2.ComponentDescriptor(Source2),
            bt2.ComponentDescriptor(Source3),
        ]
        version = bt2.get_greatest_operative_mip_version(descriptors)
        self.assertEqual(version, 0)

    def test_get_greatest_operative_mip_version_no_match(self):
        class Source1(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            @classmethod
            def _user_get_supported_mip_versions(cls, params, obj, log_level):
                return [0]

        class Source2(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            @classmethod
            def _user_get_supported_mip_versions(cls, params, obj, log_level):
                return [1]

        descriptors = [
            bt2.ComponentDescriptor(Source1),
            bt2.ComponentDescriptor(Source2),
        ]

        version = bt2.get_greatest_operative_mip_version(descriptors)
        self.assertIsNone(version)

    def test_get_greatest_operative_mip_version_empty_descriptors(self):
        with self.assertRaises(ValueError):
            bt2.get_greatest_operative_mip_version([])

    def test_get_greatest_operative_mip_version_wrong_descriptor_type(self):
        class Source1(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            @classmethod
            def _user_get_supported_mip_versions(cls, params, obj, log_level):
                return [0, 1]

        descriptors = [bt2.ComponentDescriptor(Source1), object()]

        with self.assertRaises(TypeError):
            bt2.get_greatest_operative_mip_version(descriptors)

    def test_get_greatest_operative_mip_version_wrong_log_level_type(self):
        class Source1(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

        descriptors = [bt2.ComponentDescriptor(Source1)]

        with self.assertRaises(TypeError):
            bt2.get_greatest_operative_mip_version(descriptors, 'lel')

    def test_get_greatest_operative_mip_version_wrong_log_level_value(self):
        class Source1(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            pass

        descriptors = [bt2.ComponentDescriptor(Source1)]

        with self.assertRaises(ValueError):
            bt2.get_greatest_operative_mip_version(descriptors, 12345)

    def test_get_maximal_mip_version(self):
        self.assertEqual(bt2.get_maximal_mip_version(), 0)


if __name__ == '__main__':
    unittest.main()
