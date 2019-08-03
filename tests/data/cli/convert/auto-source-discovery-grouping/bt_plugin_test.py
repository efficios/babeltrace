import bt2
import os


class TestIter(bt2._UserMessageIterator):
    pass


class Base:
    @classmethod
    def _print_params(cls, params):
        inputs = sorted([str(x) for x in params['inputs']])
        print('{}: {}'.format(cls.__name__, ', '.join(inputs)))


@bt2.plugin_component_class
class TestSourceExt(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    """
    Recognize files whose name start with 'aaa', 'bbb' or 'ccc'.

    'aaa' files are grouped together, 'bbb' files are grouped together, 'ccc'
    files are not grouped.
    """

    def __init__(self, params, obj):
        self._print_params(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params):
        if obj == 'babeltrace.support-info':
            if params['type'] == 'file':
                name = os.path.basename(str(params['input']))

                if name.startswith('aaa'):
                    return {'weight': 1, 'group': 'aaa'}
                elif name.startswith('bbb'):
                    return {'weight': 0.5, 'group': 'bbb'}
                elif name.startswith('ccc'):
                    # Try two different ways of returning "no group", and two
                    # different ways of returning 1 (an int and a float).
                    if name[3] == '1':
                        return {'weight': 1, 'group': None}
                    elif name[3] == '2':
                        return {'weight': 1.0, 'group': None}
                    elif name[3] == '3':
                        return 1
                    else:
                        return 1.0
            else:
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

    def __init__(self, params, obj):
        self._print_params(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params):
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

    def __init__(self, params, obj):
        self._print_params(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params):
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
