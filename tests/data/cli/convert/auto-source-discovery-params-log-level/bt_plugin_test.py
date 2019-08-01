import bt2
import os

# This file defines source component classes that can print (depending the on
# the `print` param):
#
#  - Parameter names and values, for parameter whose name starts with `test-`.
#  - Component log levels as integers


class TestIter(bt2._UserMessageIterator):
    pass


class Base:
    @classmethod
    def _print_test_params(cls, params):
        items = sorted([str(x) for x in params.items() if x[0].startswith('test-')])
        print('{}: {}'.format(cls.__name__, ', '.join(items)))

    def _print_log_level(self):
        cls_name = self.__class__.__name__
        log_level = self.logging_level
        print('{}: {}'.format(cls_name, log_level))

    def _print_info(self, params):
        what = params['print']
        if what == 'test-params':
            self._print_test_params(params)
        elif what == 'log-level':
            self._print_log_level()
        else:
            assert False


@bt2.plugin_component_class
class TestSourceA(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    def __init__(self, params):
        self._print_info(params)

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
        self._print_info(params)

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
