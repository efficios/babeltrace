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


@bt2.plugin_component_class
class ThisIsASource(
    bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
):
    pass


@bt2.plugin_component_class
class ThisIsAFilter(
    bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
):
    pass


@bt2.plugin_component_class
class ThisIsASink(bt2._UserSinkComponent):
    def _user_consume(self):
        pass


bt2.register_plugin(
    __name__,
    "this-is-a-plugin",
    version=(1, 2, 3, 'bob'),
    description='A plugin',
    author='Jorge Mario Bergoglio',
    license='The license',
)
