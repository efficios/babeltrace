import bt2
import os

# This file defines source component classes to help verify the parameters an
# log levels passed to components.  Each component creates one stream, with a
# name derived from either:
#
#   - the received params that start with `test-`
#   - the received log level
#
# The `what` parameter determines what is used.


class TestIter(bt2._UserMessageIterator):
    def __init__(self, config, output_port):
        params = output_port.user_data['params']
        obj = output_port.user_data['obj']

        comp_cls_name = self._component.__class__.__name__

        if params['what'] == 'test-params':
            items = sorted([str(x) for x in params.items() if x[0].startswith('test-')])
            stream_name = '{}: {}'.format(comp_cls_name, ', '.join(items))
        elif params['what'] == 'log-level':
            log_level = self._component.logging_level
            stream_name = '{}: {}'.format(comp_cls_name, log_level)
        elif params['what'] == 'python-obj':
            assert type(obj) == str or obj is None
            stream_name = '{}: {}'.format(comp_cls_name, obj)
        else:
            assert False

        sc = output_port.user_data['sc']
        tc = sc.trace_class
        t = tc()
        s = t.create_stream(sc, name=stream_name)

        self._msgs = [
            self._create_stream_beginning_message(s),
            self._create_stream_end_message(s),
        ]

    def __next__(self):
        if len(self._msgs) == 0:
            raise StopIteration

        return self._msgs.pop(0)


class Base:
    def __init__(self, params, obj):
        tc = self._create_trace_class()
        sc = tc.create_stream_class()

        self._add_output_port('out', {'params': params, 'obj': obj, 'sc': sc})


@bt2.plugin_component_class
class TestSourceA(Base, bt2._UserSourceComponent, message_iterator_class=TestIter):
    def __init__(self, config, params, obj):
        super().__init__(params, obj)

    @staticmethod
    def _user_query(priv_query_exec, obj, params, method_obj):
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
    def __init__(self, config, params, obj):
        super().__init__(params, obj)

    @staticmethod
    def _user_query(priv_query_exec, obj, params, method_obj):
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
