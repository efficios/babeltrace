import bt2


class MyIter(bt2._UserMessageIterator):
    pass


@bt2.plugin_component_class
class MySource(bt2._UserSourceComponent,
               message_iterator_class=MyIter):
    pass


@bt2.plugin_component_class
class MyFilter(bt2._UserFilterComponent,
               message_iterator_class=MyIter):
    pass


@bt2.plugin_component_class
class MySink(bt2._UserSinkComponent):
    def _consume(self):
        pass


bt2.register_plugin(__name__, 'sparkling', author='Philippe Proulx',
                    description='A delicious plugin.',
                    version=(1, 2, 3, 'EXTRA'),
                    license='MIT')
