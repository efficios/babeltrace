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
import datetime
import bt2
import os
import os.path


_BT_TESTS_DATADIR = os.environ['BT_TESTS_DATADIR']
_BT_CTF_TRACES_PATH = os.environ['BT_CTF_TRACES_PATH']
_3EVENTS_INTERSECT_TRACE_PATH = os.path.join(
    _BT_CTF_TRACES_PATH, 'intersection', '3eventsintersect'
)
_NOINTERSECT_TRACE_PATH = os.path.join(
    _BT_CTF_TRACES_PATH, 'intersection', 'nointersect'
)
_SEQUENCE_TRACE_PATH = os.path.join(_BT_CTF_TRACES_PATH, 'succeed', 'sequence')
_AUTO_SOURCE_DISCOVERY_GROUPING_PATH = os.path.join(
    _BT_TESTS_DATADIR, 'auto-source-discovery', 'grouping'
)
_AUTO_SOURCE_DISCOVERY_PARAMS_LOG_LEVEL_PATH = os.path.join(
    _BT_TESTS_DATADIR, 'auto-source-discovery', 'params-log-level'
)


class _SomeSource(
    bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
):
    pass


class _SomeFilter(
    bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
):
    pass


class _SomeSink(bt2._UserSinkComponent):
    def _user_consume(self):
        pass


class ComponentSpecTestCase(unittest.TestCase):
    def setUp(self):
        # A source CC from a plugin.
        self._dmesg_cc = bt2.find_plugin('text').source_component_classes['dmesg']
        assert self._dmesg_cc is not None

        # A filter CC from a plugin.
        self._muxer_cc = bt2.find_plugin('utils').filter_component_classes['muxer']
        assert self._muxer_cc is not None

        # A sink CC from a plugin.
        self._pretty_cc = bt2.find_plugin('text').sink_component_classes['pretty']
        assert self._pretty_cc is not None

    def test_create_source_from_name(self):
        spec = bt2.ComponentSpec.from_named_plugin_and_component_class('text', 'dmesg')
        self.assertEqual(spec.component_class.name, 'dmesg')

    def test_create_source_from_plugin(self):
        spec = bt2.ComponentSpec(self._dmesg_cc)
        self.assertEqual(spec.component_class.name, 'dmesg')

    def test_create_source_from_user(self):
        spec = bt2.ComponentSpec(_SomeSource)
        self.assertEqual(spec.component_class.name, '_SomeSource')

    def test_create_filter_from_name(self):
        spec = bt2.ComponentSpec.from_named_plugin_and_component_class('utils', 'muxer')
        self.assertEqual(spec.component_class.name, 'muxer')

    def test_create_filter_from_object(self):
        spec = bt2.ComponentSpec(self._muxer_cc)
        self.assertEqual(spec.component_class.name, 'muxer')

    def test_create_sink_from_name(self):
        with self.assertRaisesRegex(
            KeyError,
            'source or filter component class `pretty` not found in plugin `text`',
        ):
            bt2.ComponentSpec.from_named_plugin_and_component_class('text', 'pretty')

    def test_create_sink_from_object(self):
        with self.assertRaisesRegex(
            TypeError,
            "'_SinkComponentClassConst' is not a source or filter component class",
        ):
            bt2.ComponentSpec(self._pretty_cc)

    def test_create_from_object_with_params(self):
        spec = bt2.ComponentSpec(self._dmesg_cc, {'salut': 23})
        self.assertEqual(spec.params['salut'], 23)

    def test_create_from_name_with_params(self):
        spec = bt2.ComponentSpec.from_named_plugin_and_component_class(
            'text', 'dmesg', {'salut': 23}
        )
        self.assertEqual(spec.params['salut'], 23)

    def test_create_from_object_with_path_params(self):
        spec = spec = bt2.ComponentSpec(self._dmesg_cc, 'a path')
        self.assertEqual(spec.params['inputs'], ['a path'])

    def test_create_from_name_with_path_params(self):
        spec = spec = bt2.ComponentSpec.from_named_plugin_and_component_class(
            'text', 'dmesg', 'a path'
        )
        self.assertEqual(spec.params['inputs'], ['a path'])

    def test_create_wrong_comp_class_type(self):
        with self.assertRaisesRegex(
            TypeError, "'int' is not a source or filter component class"
        ):
            bt2.ComponentSpec(18)

    def test_create_from_name_wrong_plugin_name_type(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'str' object"):
            bt2.ComponentSpec.from_named_plugin_and_component_class(23, 'compcls')

    def test_create_from_name_non_existent_plugin(self):
        with self.assertRaisesRegex(
            ValueError, "no such plugin: this_plugin_does_not_exist"
        ):
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'this_plugin_does_not_exist', 'compcls'
            )

    def test_create_from_name_wrong_component_class_name_type(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'str' object"):
            bt2.ComponentSpec.from_named_plugin_and_component_class('utils', 190)

    def test_create_wrong_params_type(self):
        with self.assertRaisesRegex(
            TypeError, "cannot create value object from 'datetime' object"
        ):
            bt2.ComponentSpec(self._dmesg_cc, params=datetime.datetime.now())

    def test_create_from_name_wrong_params_type(self):
        with self.assertRaisesRegex(
            TypeError, "cannot create value object from 'datetime' object"
        ):
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'text', 'dmesg', datetime.datetime.now()
            )

    def test_create_wrong_log_level_type(self):
        with self.assertRaisesRegex(TypeError, "'str' is not an 'int' object"):
            bt2.ComponentSpec(self._dmesg_cc, logging_level='banane')

    def test_create_from_name_wrong_log_level_type(self):
        with self.assertRaisesRegex(TypeError, "'str' is not an 'int' object"):
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'text', 'dmesg', logging_level='banane'
            )


# Return a map, msg type -> number of messages of this type.


def _count_msgs_by_type(msgs):
    res = {}

    for msg in msgs:
        t = type(msg)
        n = res.get(t, 0)
        res[t] = n + 1

    return res


class TraceCollectionMessageIteratorTestCase(unittest.TestCase):
    def test_create_wrong_stream_intersection_mode_type(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]

        with self.assertRaises(TypeError):
            bt2.TraceCollectionMessageIterator(specs, stream_intersection_mode=23)

    def test_create_wrong_begin_type(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]

        with self.assertRaises(TypeError):
            bt2.TraceCollectionMessageIterator(specs, begin='hi')

    def test_create_wrong_end_type(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]

        with self.assertRaises(TypeError):
            bt2.TraceCollectionMessageIterator(specs, begin='lel')

    def test_create_begin_s(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        bt2.TraceCollectionMessageIterator(specs, begin=19457.918232)

    def test_create_end_s(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        bt2.TraceCollectionMessageIterator(specs, end=123.12312)

    def test_create_begin_datetime(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        bt2.TraceCollectionMessageIterator(specs, begin=datetime.datetime.now())

    def test_create_end_datetime(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        bt2.TraceCollectionMessageIterator(specs, end=datetime.datetime.now())

    def test_iter_no_intersection(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 8)

    # Same as the above, but we pass a single spec instead of a spec list.
    def test_iter_specs_not_list(self):
        spec = bt2.ComponentSpec.from_named_plugin_and_component_class(
            'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
        )
        msg_iter = bt2.TraceCollectionMessageIterator(spec)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 8)

    def test_iter_custom_filter(self):
        src_spec = bt2.ComponentSpec.from_named_plugin_and_component_class(
            'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
        )
        flt_spec = bt2.ComponentSpec.from_named_plugin_and_component_class(
            'utils', 'trimmer', {'end': '13515309.000000075'}
        )
        msg_iter = bt2.TraceCollectionMessageIterator(src_spec, flt_spec)
        hist = _count_msgs_by_type(msg_iter)
        self.assertEqual(hist[bt2._EventMessageConst], 5)

    def test_iter_intersection(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        msg_iter = bt2.TraceCollectionMessageIterator(
            specs, stream_intersection_mode=True
        )
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 15)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 3)

    def test_iter_intersection_params(self):
        # Check that all params used to create the source component are passed
        # to the `babeltrace.trace-infos` query.
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf',
                'fs',
                {
                    'inputs': [_3EVENTS_INTERSECT_TRACE_PATH],
                    'clock-class-offset-s': 1000,
                },
            )
        ]

        msg_iter = bt2.TraceCollectionMessageIterator(
            specs, stream_intersection_mode=True
        )

        event_msgs = [x for x in msg_iter if type(x) is bt2._EventMessageConst]
        self.assertEqual(len(event_msgs), 3)
        self.assertEqual(
            event_msgs[0].default_clock_snapshot.ns_from_origin, 13516309000000071
        )
        self.assertEqual(
            event_msgs[1].default_clock_snapshot.ns_from_origin, 13516309000000072
        )
        self.assertEqual(
            event_msgs[2].default_clock_snapshot.ns_from_origin, 13516309000000082
        )

    def test_iter_no_intersection_two_traces(self):
        spec = bt2.ComponentSpec.from_named_plugin_and_component_class(
            'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
        )
        specs = [spec, spec]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 56)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 16)

    def test_iter_no_intersection_begin(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, begin=13515309.000000023)
        hist = _count_msgs_by_type(msg_iter)
        self.assertEqual(hist[bt2._EventMessageConst], 6)

    def test_iter_no_intersection_end(self):
        specs = [
            bt2.ComponentSpec.from_named_plugin_and_component_class(
                'ctf', 'fs', _3EVENTS_INTERSECT_TRACE_PATH
            )
        ]
        msg_iter = bt2.TraceCollectionMessageIterator(specs, end=13515309.000000075)
        hist = _count_msgs_by_type(msg_iter)
        self.assertEqual(hist[bt2._EventMessageConst], 5)

    def test_iter_auto_source_component_spec(self):
        specs = [bt2.AutoSourceComponentSpec(_3EVENTS_INTERSECT_TRACE_PATH)]
        msg_iter = bt2.TraceCollectionMessageIterator(specs)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 8)

    def test_iter_auto_source_component_spec_list_of_strings(self):
        msg_iter = bt2.TraceCollectionMessageIterator([_3EVENTS_INTERSECT_TRACE_PATH])
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 8)

    def test_iter_auto_source_component_spec_string(self):
        msg_iter = bt2.TraceCollectionMessageIterator(_3EVENTS_INTERSECT_TRACE_PATH)
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 28)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 8)

    def test_iter_mixed_inputs(self):
        msg_iter = bt2.TraceCollectionMessageIterator(
            [
                _3EVENTS_INTERSECT_TRACE_PATH,
                bt2.AutoSourceComponentSpec(_SEQUENCE_TRACE_PATH),
                bt2.ComponentSpec.from_named_plugin_and_component_class(
                    'ctf', 'fs', _NOINTERSECT_TRACE_PATH
                ),
            ]
        )
        msgs = list(msg_iter)
        self.assertEqual(len(msgs), 76)
        hist = _count_msgs_by_type(msgs)
        self.assertEqual(hist[bt2._EventMessageConst], 24)

    def test_auto_source_component_non_existent(self):
        with self.assertRaisesRegex(
            RuntimeError,
            'Some auto source component specs did not produce any component',
        ):
            # Test with one path known to contain a trace and one path known
            # to not contain any trace.
            bt2.TraceCollectionMessageIterator(
                [_SEQUENCE_TRACE_PATH, '/this/path/better/not/exist']
            )


class _TestAutoDiscoverSourceComponentSpecs(unittest.TestCase):
    def setUp(self):
        self._saved_babeltrace_plugin_path = os.environ['BABELTRACE_PLUGIN_PATH']
        os.environ['BABELTRACE_PLUGIN_PATH'] += os.pathsep + self._plugin_path

    def tearDown(self):
        os.environ['BABELTRACE_PLUGIN_PATH'] = self._saved_babeltrace_plugin_path


class TestAutoDiscoverSourceComponentSpecsGrouping(
    _TestAutoDiscoverSourceComponentSpecs
):
    _plugin_path = _AUTO_SOURCE_DISCOVERY_GROUPING_PATH

    def test_grouping(self):
        specs = [
            bt2.AutoSourceComponentSpec('ABCDE'),
            bt2.AutoSourceComponentSpec(_AUTO_SOURCE_DISCOVERY_GROUPING_PATH),
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 8)

        self.assertEqual(msgs[0].stream.name, 'TestSourceABCDE: ABCDE')
        self.assertEqual(msgs[1].stream.name, 'TestSourceExt: aaa1, aaa2, aaa3')
        self.assertEqual(msgs[2].stream.name, 'TestSourceExt: bbb1, bbb2')
        self.assertEqual(msgs[3].stream.name, 'TestSourceExt: ccc1')
        self.assertEqual(msgs[4].stream.name, 'TestSourceExt: ccc2')
        self.assertEqual(msgs[5].stream.name, 'TestSourceExt: ccc3')
        self.assertEqual(msgs[6].stream.name, 'TestSourceExt: ccc4')
        self.assertEqual(msgs[7].stream.name, 'TestSourceSomeDir: some-dir')


class TestAutoDiscoverSourceComponentSpecsParamsObjLogLevel(
    _TestAutoDiscoverSourceComponentSpecs
):
    _plugin_path = _AUTO_SOURCE_DISCOVERY_PARAMS_LOG_LEVEL_PATH

    _dir_a = os.path.join(_AUTO_SOURCE_DISCOVERY_PARAMS_LOG_LEVEL_PATH, 'dir-a')
    _dir_b = os.path.join(_AUTO_SOURCE_DISCOVERY_PARAMS_LOG_LEVEL_PATH, 'dir-b')
    _dir_ab = os.path.join(_AUTO_SOURCE_DISCOVERY_PARAMS_LOG_LEVEL_PATH, 'dir-ab')

    def _test_two_comps_from_one_spec(self, params, obj=None, logging_level=None):
        specs = [
            bt2.AutoSourceComponentSpec(
                self._dir_ab, params=params, obj=obj, logging_level=logging_level
            )
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 2)

        return msgs

    def test_params_two_comps_from_one_spec(self):
        msgs = self._test_two_comps_from_one_spec(
            params={'test-allo': 'madame', 'what': 'test-params'}
        )

        self.assertEqual(msgs[0].stream.name, "TestSourceA: ('test-allo', 'madame')")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: ('test-allo', 'madame')")

    def test_obj_two_comps_from_one_spec(self):
        msgs = self._test_two_comps_from_one_spec(
            params={'what': 'python-obj'}, obj='deore'
        )

        self.assertEqual(msgs[0].stream.name, "TestSourceA: deore")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: deore")

    def test_log_level_two_comps_from_one_spec(self):
        msgs = self._test_two_comps_from_one_spec(
            params={'what': 'log-level'}, logging_level=bt2.LoggingLevel.DEBUG
        )

        self.assertEqual(
            msgs[0].stream.name, "TestSourceA: {}".format(bt2.LoggingLevel.DEBUG)
        )
        self.assertEqual(
            msgs[1].stream.name, "TestSourceB: {}".format(bt2.LoggingLevel.DEBUG)
        )

    def _test_two_comps_from_two_specs(
        self,
        params_a=None,
        params_b=None,
        obj_a=None,
        obj_b=None,
        logging_level_a=None,
        logging_level_b=None,
    ):
        specs = [
            bt2.AutoSourceComponentSpec(
                self._dir_a, params=params_a, obj=obj_a, logging_level=logging_level_a
            ),
            bt2.AutoSourceComponentSpec(
                self._dir_b, params=params_b, obj=obj_b, logging_level=logging_level_b
            ),
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 2)

        return msgs

    def test_params_two_comps_from_two_specs(self):
        msgs = self._test_two_comps_from_two_specs(
            params_a={'test-allo': 'madame', 'what': 'test-params'},
            params_b={'test-bonjour': 'monsieur', 'what': 'test-params'},
        )

        self.assertEqual(msgs[0].stream.name, "TestSourceA: ('test-allo', 'madame')")
        self.assertEqual(
            msgs[1].stream.name, "TestSourceB: ('test-bonjour', 'monsieur')"
        )

    def test_obj_two_comps_from_two_specs(self):
        msgs = self._test_two_comps_from_two_specs(
            params_a={'what': 'python-obj'},
            params_b={'what': 'python-obj'},
            obj_a='deore',
            obj_b='alivio',
        )

        self.assertEqual(msgs[0].stream.name, "TestSourceA: deore")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: alivio")

    def test_log_level_two_comps_from_two_specs(self):
        msgs = self._test_two_comps_from_two_specs(
            params_a={'what': 'log-level'},
            params_b={'what': 'log-level'},
            logging_level_a=bt2.LoggingLevel.DEBUG,
            logging_level_b=bt2.LoggingLevel.TRACE,
        )

        self.assertEqual(
            msgs[0].stream.name, "TestSourceA: {}".format(bt2.LoggingLevel.DEBUG)
        )
        self.assertEqual(
            msgs[1].stream.name, "TestSourceB: {}".format(bt2.LoggingLevel.TRACE)
        )

    def _test_one_comp_from_one_spec_one_comp_from_both_1(
        self,
        params_a=None,
        params_ab=None,
        obj_a=None,
        obj_ab=None,
        logging_level_a=None,
        logging_level_ab=None,
    ):
        specs = [
            bt2.AutoSourceComponentSpec(
                self._dir_a, params=params_a, obj=obj_a, logging_level=logging_level_a
            ),
            bt2.AutoSourceComponentSpec(
                self._dir_ab,
                params=params_ab,
                obj=obj_ab,
                logging_level=logging_level_ab,
            ),
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 2)

        return msgs

    def test_params_one_comp_from_one_spec_one_comp_from_both_1(self):
        msgs = self._test_one_comp_from_one_spec_one_comp_from_both_1(
            params_a={'test-allo': 'madame', 'what': 'test-params'},
            params_ab={'test-bonjour': 'monsieur', 'what': 'test-params'},
        )

        self.assertEqual(
            msgs[0].stream.name,
            "TestSourceA: ('test-allo', 'madame'), ('test-bonjour', 'monsieur')",
        )
        self.assertEqual(
            msgs[1].stream.name, "TestSourceB: ('test-bonjour', 'monsieur')"
        )

    def test_obj_one_comp_from_one_spec_one_comp_from_both_1(self):
        msgs = self._test_one_comp_from_one_spec_one_comp_from_both_1(
            params_a={'what': 'python-obj'},
            params_ab={'what': 'python-obj'},
            obj_a='deore',
            obj_ab='alivio',
        )

        self.assertEqual(msgs[0].stream.name, "TestSourceA: alivio")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: alivio")

    def test_log_level_one_comp_from_one_spec_one_comp_from_both_1(self):
        msgs = self._test_one_comp_from_one_spec_one_comp_from_both_1(
            params_a={'what': 'log-level'},
            params_ab={'what': 'log-level'},
            logging_level_a=bt2.LoggingLevel.DEBUG,
            logging_level_ab=bt2.LoggingLevel.TRACE,
        )

        self.assertEqual(
            msgs[0].stream.name, "TestSourceA: {}".format(bt2.LoggingLevel.TRACE)
        )
        self.assertEqual(
            msgs[1].stream.name, "TestSourceB: {}".format(bt2.LoggingLevel.TRACE)
        )

    def _test_one_comp_from_one_spec_one_comp_from_both_2(
        self,
        params_ab=None,
        params_a=None,
        obj_ab=None,
        obj_a=None,
        logging_level_ab=None,
        logging_level_a=None,
    ):
        specs = [
            bt2.AutoSourceComponentSpec(
                self._dir_ab,
                params=params_ab,
                obj=obj_ab,
                logging_level=logging_level_ab,
            ),
            bt2.AutoSourceComponentSpec(
                self._dir_a, params=params_a, obj=obj_a, logging_level=logging_level_a
            ),
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 2)

        return msgs

    def test_params_one_comp_from_one_spec_one_comp_from_both_2(self):
        msgs = self._test_one_comp_from_one_spec_one_comp_from_both_2(
            params_ab={
                'test-bonjour': 'madame',
                'test-salut': 'les amis',
                'what': 'test-params',
            },
            params_a={'test-bonjour': 'monsieur', 'what': 'test-params'},
        )

        self.assertEqual(
            msgs[0].stream.name,
            "TestSourceA: ('test-bonjour', 'monsieur'), ('test-salut', 'les amis')",
        )
        self.assertEqual(
            msgs[1].stream.name,
            "TestSourceB: ('test-bonjour', 'madame'), ('test-salut', 'les amis')",
        )

    def test_obj_one_comp_from_one_spec_one_comp_from_both_2(self):
        msgs = self._test_one_comp_from_one_spec_one_comp_from_both_2(
            params_ab={'what': 'python-obj'},
            params_a={'what': 'python-obj'},
            obj_ab='deore',
            obj_a='alivio',
        )

        self.assertEqual(msgs[0].stream.name, "TestSourceA: alivio")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: deore")

    def test_log_level_one_comp_from_one_spec_one_comp_from_both_2(self):
        msgs = self._test_one_comp_from_one_spec_one_comp_from_both_2(
            params_ab={'what': 'log-level'},
            params_a={'what': 'log-level'},
            logging_level_ab=bt2.LoggingLevel.DEBUG,
            logging_level_a=bt2.LoggingLevel.TRACE,
        )

        self.assertEqual(
            msgs[0].stream.name, "TestSourceA: {}".format(bt2.LoggingLevel.TRACE)
        )
        self.assertEqual(
            msgs[1].stream.name, "TestSourceB: {}".format(bt2.LoggingLevel.DEBUG)
        )

    def test_obj_override_with_none(self):
        specs = [
            bt2.AutoSourceComponentSpec(
                self._dir_ab, params={'what': 'python-obj'}, obj='deore'
            ),
            bt2.AutoSourceComponentSpec(
                self._dir_a, params={'what': 'python-obj'}, obj=None
            ),
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 2)
        self.assertEqual(msgs[0].stream.name, "TestSourceA: None")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: deore")

    def test_obj_no_override_with_no_obj(self):
        specs = [
            bt2.AutoSourceComponentSpec(
                self._dir_ab, params={'what': 'python-obj'}, obj='deore'
            ),
            bt2.AutoSourceComponentSpec(self._dir_a, params={'what': 'python-obj'}),
        ]
        it = bt2.TraceCollectionMessageIterator(specs)
        msgs = [x for x in it if type(x) is bt2._StreamBeginningMessageConst]

        self.assertEqual(len(msgs), 2)
        self.assertEqual(msgs[0].stream.name, "TestSourceA: deore")
        self.assertEqual(msgs[1].stream.name, "TestSourceB: deore")


if __name__ == '__main__':
    unittest.main()
