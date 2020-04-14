# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020 EfficiOS, Inc.

import unittest
import bt2


class Test(unittest.TestCase):
    # Test that the component returns an error if the graph is configured while
    # the component's input port is left disconnected.
    def test_unconnected_port_raises(self):
        graph = bt2.Graph()
        graph.add_component(
            bt2.find_plugin('text').sink_component_classes['pretty'], 'snk'
        )

        with self.assertRaisesRegex(
            bt2._Error, 'Single input port is not connected: port-name="in"'
        ):
            graph.run()


if __name__ == '__main__':
    unittest.main()
