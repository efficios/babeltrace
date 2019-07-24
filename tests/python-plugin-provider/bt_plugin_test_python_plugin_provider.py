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


class MyIter(bt2._UserMessageIterator):
    pass


@bt2.plugin_component_class
class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
    pass


@bt2.plugin_component_class
class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyIter):
    pass


@bt2.plugin_component_class
class MySink(bt2._UserSinkComponent):
    def _user_consume(self):
        pass


bt2.register_plugin(
    __name__,
    'sparkling',
    author='Philippe Proulx',
    description='A delicious plugin.',
    version=(1, 2, 3, 'EXTRA'),
    license='MIT',
)
