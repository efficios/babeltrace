# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import bt2
from cli_params_to_string import to_string


@bt2.plugin_component_class
class SinkThatPrintsParams(bt2._UserSinkComponent):
    def __init__(self, config, params, obj):
        self._add_input_port('in')
        print(to_string(params))

    def _user_consume(self):
        raise bt2.Stop


bt2.register_plugin(__name__, "params")
