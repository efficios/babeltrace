import bt2
import os

# Source component classes in this file recognize and group inputs in
# various ways.  One stream is created by each component, with a name
# derived from the component class and the inputs.  This is then checked
# using the `sink.text.details` sink.


class TestIter(bt2._UserMessageIterator):
    def __init__(self, config, output_port):
        inputs = output_port.user_data['inputs']
        sc = output_port.user_data['sc']
        tc = sc.trace_class
        t = tc()
        s = t.create_stream(sc, name=self._make_stream_name(inputs))

        self._msgs = [
            self._create_stream_beginning_message(s),
            self._create_stream_end_message(s),
        ]

    def _make_stream_name(self, inputs):
        comp_cls_name = self._component.__class__.__name__
        return (
            comp_cls_name
            + ': '
            + ', '.join(sorted([os.path.basename(str(x)) for x in inputs]))
        )

    def __next__(self):
        if len(self._msgs) == 0:
            raise StopIteration

        return self._msgs.pop(0)


class Base:
    def __init__(self, params):
        tc = self._create_trace_class()
        sc = tc.create_stream_class()

        self._add_output_port('out', {'inputs': params['inputs'], 'sc': sc})


@bt2.plugin_component_class
class TestSourceExt(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    """
    Recognize files whose name start with 'aaa', 'bbb' or 'ccc'.

    'aaa' files are grouped together, 'bbb' files are grouped together, 'ccc'
    files are not grouped.
    """

    def __init__(self, config, params, obj):
        super().__init__(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params, method_obj):
        if obj == 'babeltrace.support-info':
            if params['type'] == 'file':
                name = os.path.basename(str(params['input']))

                if name.startswith('aaa'):
                    return {'weight': 1, 'group': 'aaa'}
                elif name.startswith('bbb'):
                    return {'weight': 0.5, 'group': 'bbb'}
                elif name.startswith('ccc'):
                    # Try two different ways of returning 1 (an int and a float).
                    if name[3] == '1':
                        return 1
                    else:
                        return 1.0

            return 0
        else:
            raise bt2.UnknownObject


@bt2.plugin_component_class
class TestSourceSomeDir(
    Base, bt2._UserSourceComponent, message_iterator_class=TestIter
):
    """Recognizes directories named "some-dir".  The file "aaa10" in the
    directory "some-dir" won't be found by TestSourceExt, because we won't
    recurse in "some-dir"."""

    def __init__(self, config, params, obj):
        super().__init__(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params, method_obj):
        if obj == 'babeltrace.support-info':
            if params['type'] == 'directory':
                name = os.path.basename(str(params['input']))
                return 1 if name == 'some-dir' else 0
            else:
                return 0
        else:
            raise bt2.UnknownObject


@bt2.plugin_component_class
class TestSourceABCDE(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    """A source that recognizes the arbitrary string input "ABCDE"."""

    def __init__(self, config, params, obj):
        super().__init__(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params, method_obj):
        if obj == 'babeltrace.support-info':
            return (
                1.0
                if params['type'] == 'string' and params['input'] == 'ABCDE'
                else 0.0
            )
        else:
            raise bt2.UnknownObject


class TestSourceNoQuery(bt2._UserSourceComponent, message_iterator_class=TestIter):
    """A source that does not implement _query at all."""


bt2.register_plugin(module_name=__name__, name="test")
