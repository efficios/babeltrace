import bt2
import os

# This file defines source component classes that print the parameters they
# receive in their __init__ which start with 'test-'.


class TestIter(bt2._UserMessageIterator):
    pass


class Base:
    @classmethod
    def _print_test_params(cls, params):
        items = sorted([str(x) for x in params.items() if x[0].startswith('test-')])
        print('{}: {}'.format(cls.__name__, ', '.join(items)))


@bt2.plugin_component_class
class TestSourceA(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    def __init__(self, params):
        self._print_test_params(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params):
        # Match files starting with 'aaa'.

        if obj == 'babeltrace.support-info':
            if params['type'] != 'file':
                return 0

            name = os.path.basename(str(params['input']))

            if name.startswith('aaa'):
                return {'weight': 1, 'group': 'aaa'}
            else:
                return 0
        else:
            raise bt2.UnknownObject


@bt2.plugin_component_class
class TestSourceB(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    def __init__(self, params):
        self._print_test_params(params)

    @staticmethod
    def _user_query(priv_query_exec, obj, params):
        # Match files starting with 'bbb'.

        if obj == 'babeltrace.support-info':
            if params['type'] != 'file':
                return 0

            name = os.path.basename(str(params['input']))

            if name.startswith('bbb'):
                return {'weight': 1, 'group': 'bbb'}
            else:
                return 0
        else:
            raise bt2.UnknownObject


bt2.register_plugin(module_name=__name__, name="test")
