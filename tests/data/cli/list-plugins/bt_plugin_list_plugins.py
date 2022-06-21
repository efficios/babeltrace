# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
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
    version=(1, 2, 3, "bob"),
    description="A plugin",
    author="Jorge Mario Bergoglio",
    license="The license",
)
