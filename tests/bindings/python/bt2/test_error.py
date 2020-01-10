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

from bt2 import native_bt
import bt2
import unittest


class FailingIter(bt2._UserMessageIterator):
    def __next__(self):
        raise ValueError('User message iterator is failing')


class SourceWithFailingIter(
    bt2._UserSourceComponent, message_iterator_class=FailingIter
):
    def __init__(self, config, params, obj):
        self._add_output_port('out')


class SourceWithFailingInit(
    bt2._UserSourceComponent, message_iterator_class=FailingIter
):
    def __init__(self, config, params, obj):
        raise ValueError('Source is failing')


class WorkingSink(bt2._UserSinkComponent):
    def __init__(self, config, params, obj):
        self._in = self._add_input_port('in')

    def _user_graph_is_configured(self):
        self._iter = self._create_message_iterator(self._in)

    def _user_consume(self):
        next(self._iter)


class SinkWithExceptionChaining(bt2._UserSinkComponent):
    def __init__(self, config, params, obj):
        self._in = self._add_input_port('in')

    def _user_graph_is_configured(self):
        self._iter = self._create_message_iterator(self._in)

    def _user_consume(self):
        try:
            next(self._iter)
        except bt2._Error as e:
            raise ValueError('oops') from e


class SinkWithFailingQuery(bt2._UserSinkComponent):
    def _user_consume(self):
        pass

    @staticmethod
    def _user_query(priv_executor, obj, params, method_obj):
        raise ValueError('Query is failing')


class ErrorTestCase(unittest.TestCase):
    def _run_failing_graph(self, source_cc, sink_cc):
        with self.assertRaises(bt2._Error) as ctx:
            graph = bt2.Graph()
            src = graph.add_component(source_cc, 'src')
            snk = graph.add_component(sink_cc, 'snk')
            graph.connect_ports(src.output_ports['out'], snk.input_ports['in'])
            graph.run()

        return ctx.exception

    def test_current_thread_error_none(self):
        # When a bt2._Error is raised, it steals the current thread's error.
        # Verify that it is now NULL.
        self._run_failing_graph(SourceWithFailingInit, WorkingSink)
        self.assertIsNone(native_bt.current_thread_take_error())

    def test_len(self):
        exc = self._run_failing_graph(SourceWithFailingIter, WorkingSink)

        # The exact number of causes is not too important (it can change if we
        # append more or less causes along the way), but the idea is to verify is
        # has a value that makes sense.
        self.assertEqual(len(exc), 4)

    def test_iter(self):
        exc = self._run_failing_graph(SourceWithFailingIter, WorkingSink)

        for c in exc:
            # Each cause is an instance of _ErrorCause (including subclasses).
            self.assertIsInstance(c, bt2._ErrorCause)

    def test_getitem(self):
        exc = self._run_failing_graph(SourceWithFailingIter, WorkingSink)

        for i in range(len(exc)):
            c = exc[i]
            # Each cause is an instance of _ErrorCause (including subclasses).
            self.assertIsInstance(c, bt2._ErrorCause)

    def test_getitem_indexerror(self):
        exc = self._run_failing_graph(SourceWithFailingIter, WorkingSink)

        with self.assertRaises(IndexError):
            exc[len(exc)]

    def test_exception_chaining(self):
        # Test that if we do:
        #
        # try:
        #     ...
        # except bt2._Error as exc:
        #     raise ValueError('oh noes') from exc
        #
        # We are able to fetch the causes of the original bt2._Error in the
        # exception chain.  Also, each exception in the chain should become one
        # cause once caught.
        exc = self._run_failing_graph(SourceWithFailingIter, SinkWithExceptionChaining)

        self.assertEqual(len(exc), 5)

        self.assertIsInstance(exc[0], bt2._MessageIteratorErrorCause)
        self.assertEqual(exc[0].component_class_name, 'SourceWithFailingIter')
        self.assertIn('ValueError: User message iterator is failing', exc[0].message)

        self.assertIsInstance(exc[1], bt2._ErrorCause)

        self.assertIsInstance(exc[2], bt2._ComponentErrorCause)
        self.assertEqual(exc[2].component_class_name, 'SinkWithExceptionChaining')
        self.assertIn(
            'unexpected error: cannot advance the message iterator', exc[2].message
        )

        self.assertIsInstance(exc[3], bt2._ComponentErrorCause)
        self.assertEqual(exc[3].component_class_name, 'SinkWithExceptionChaining')
        self.assertIn('ValueError: oops', exc[3].message)

        self.assertIsInstance(exc[4], bt2._ErrorCause)

    def _common_cause_tests(self, cause):
        self.assertIsInstance(cause.module_name, str)
        self.assertIsInstance(cause.file_name, str)
        self.assertIsInstance(cause.line_number, int)

    def test_unknown_error_cause(self):
        exc = self._run_failing_graph(SourceWithFailingIter, SinkWithExceptionChaining)
        cause = exc[-1]
        self.assertIs(type(cause), bt2._ErrorCause)
        self._common_cause_tests(cause)

    def test_component_error_cause(self):
        exc = self._run_failing_graph(SourceWithFailingInit, SinkWithExceptionChaining)
        cause = exc[0]
        self.assertIs(type(cause), bt2._ComponentErrorCause)
        self._common_cause_tests(cause)

        self.assertIn('Source is failing', cause.message)
        self.assertEqual(cause.component_name, 'src')
        self.assertEqual(cause.component_class_type, bt2.ComponentClassType.SOURCE)
        self.assertEqual(cause.component_class_name, 'SourceWithFailingInit')
        self.assertIsNone(cause.plugin_name)

    def test_component_class_error_cause(self):
        q = bt2.QueryExecutor(SinkWithFailingQuery, 'hello')

        with self.assertRaises(bt2._Error) as ctx:
            q.query()

        cause = ctx.exception[0]
        self.assertIs(type(cause), bt2._ComponentClassErrorCause)
        self._common_cause_tests(cause)

        self.assertIn('Query is failing', cause.message)

        self.assertEqual(cause.component_class_type, bt2.ComponentClassType.SINK)
        self.assertEqual(cause.component_class_name, 'SinkWithFailingQuery')
        self.assertIsNone(cause.plugin_name)

    def test_message_iterator_error_cause(self):
        exc = self._run_failing_graph(SourceWithFailingIter, SinkWithExceptionChaining)
        cause = exc[0]
        self.assertIs(type(cause), bt2._MessageIteratorErrorCause)
        self._common_cause_tests(cause)

        self.assertIn('User message iterator is failing', cause.message)
        self.assertEqual(cause.component_name, 'src')
        self.assertEqual(cause.component_output_port_name, 'out')
        self.assertEqual(cause.component_class_type, bt2.ComponentClassType.SOURCE)
        self.assertEqual(cause.component_class_name, 'SourceWithFailingIter')
        self.assertIsNone(cause.plugin_name)

    def test_str(self):
        # Test __str__.  We don't need to test the precise format used, but
        # just that it doesn't miserably crash and that it contains some
        # expected bits.
        exc = self._run_failing_graph(SourceWithFailingIter, SinkWithExceptionChaining)
        s = str(exc)
        self.assertIn("[src (out): 'source.SourceWithFailingIter']", s)
        self.assertIn('ValueError: oops', s)


if __name__ == '__main__':
    unittest.main()
