# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import bt2
from cli_params_to_string import to_string


@bt2.plugin_component_class
class SourceWithQueryThatPrintsParams(
    bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
):
    @classmethod
    def _user_query(cls, executor, obj, params, method_obj):
        if obj == "please-fail":
            raise ValueError("catastrophic failure")

        return obj + ":" + to_string(params)


bt2.register_plugin(__name__, "query")
