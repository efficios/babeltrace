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
import os


class PythonPluginProviderTestCase(unittest.TestCase):
    def test_python_plugin_provider(self):
        path = os.environ['PYTHON_PLUGIN_PROVIDER_TEST_PLUGIN_PATH']
        pset = bt2.find_plugins_in_path(path)
        self.assertEqual(len(pset), 1)
        plugin = pset[0]
        self.assertEqual(plugin.name, 'sparkling')
        self.assertEqual(plugin.author, 'Philippe Proulx')
        self.assertEqual(plugin.description, 'A delicious plugin.')
        self.assertEqual(plugin.version.major, 1)
        self.assertEqual(plugin.version.minor, 2)
        self.assertEqual(plugin.version.patch, 3)
        self.assertEqual(plugin.version.extra, 'EXTRA')
        self.assertEqual(plugin.license, 'MIT')
        self.assertEqual(len(plugin.source_component_classes), 1)
        self.assertEqual(len(plugin.filter_component_classes), 1)
        self.assertEqual(len(plugin.sink_component_classes), 1)
        self.assertEqual(plugin.source_component_classes['MySource'].name, 'MySource')
        self.assertEqual(plugin.filter_component_classes['MyFilter'].name, 'MyFilter')
        self.assertEqual(plugin.sink_component_classes['MySink'].name, 'MySink')
