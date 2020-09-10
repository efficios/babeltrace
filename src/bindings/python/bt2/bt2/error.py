from bt2 import native_bt
from collections import abc


class ComponentClassType:
    SOURCE = native_bt.COMPONENT_CLASS_TYPE_SOURCE
    FILTER = native_bt.COMPONENT_CLASS_TYPE_FILTER
    SINK = native_bt.COMPONENT_CLASS_TYPE_SINK


_COMPONENT_CLASS_TYPE_TO_STR = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: 'source',
    native_bt.COMPONENT_CLASS_TYPE_FILTER: 'filter',
    native_bt.COMPONENT_CLASS_TYPE_SINK: 'sink',
}


def _create_error_cause_from_ptr(ptr):
    actor_type = native_bt.error_cause_get_actor_type(ptr)
    return _ACTOR_TYPE_TO_CLS[actor_type](ptr)


class _ErrorCause:
    def __init__(self, ptr):
        self._message = native_bt.error_cause_get_message(ptr)
        self._module_name = native_bt.error_cause_get_module_name(ptr)
        self._file_name = native_bt.error_cause_get_file_name(ptr)
        self._line_number = native_bt.error_cause_get_line_number(ptr)
        self._str = native_bt.bt2_format_bt_error_cause(ptr)

    def __str__(self):
        return self._str

    @property
    def message(self):
        return self._message

    @property
    def module_name(self):
        return self._module_name

    @property
    def file_name(self):
        return self._file_name

    @property
    def line_number(self):
        return self._line_number


class _ComponentErrorCause(_ErrorCause):
    def __init__(self, ptr):
        super().__init__(ptr)
        self._component_name = native_bt.error_cause_component_actor_get_component_name(
            ptr
        )
        self._component_class_type = (
            native_bt.error_cause_component_actor_get_component_class_type(ptr)
        )
        self._component_class_name = (
            native_bt.error_cause_component_actor_get_component_class_name(ptr)
        )
        self._plugin_name = native_bt.error_cause_component_actor_get_plugin_name(ptr)

    @property
    def component_name(self):
        return self._component_name

    @property
    def component_class_type(self):
        return self._component_class_type

    @property
    def component_class_name(self):
        return self._component_class_name

    @property
    def plugin_name(self):
        return self._plugin_name


class _ComponentClassErrorCause(_ErrorCause):
    def __init__(self, ptr):
        super().__init__(ptr)
        self._component_class_type = (
            native_bt.error_cause_component_class_actor_get_component_class_type(ptr)
        )
        self._component_class_name = (
            native_bt.error_cause_component_class_actor_get_component_class_name(ptr)
        )
        self._plugin_name = native_bt.error_cause_component_class_actor_get_plugin_name(
            ptr
        )

    @property
    def component_class_type(self):
        return self._component_class_type

    @property
    def component_class_name(self):
        return self._component_class_name

    @property
    def plugin_name(self):
        return self._plugin_name


class _MessageIteratorErrorCause(_ErrorCause):
    def __init__(self, ptr):
        super().__init__(ptr)
        self._component_name = (
            native_bt.error_cause_message_iterator_actor_get_component_name(ptr)
        )
        self._component_output_port_name = (
            native_bt.error_cause_message_iterator_actor_get_component_output_port_name(
                ptr
            )
        )
        self._component_class_type = (
            native_bt.error_cause_message_iterator_actor_get_component_class_type(ptr)
        )
        self._component_class_name = (
            native_bt.error_cause_message_iterator_actor_get_component_class_name(ptr)
        )
        self._plugin_name = (
            native_bt.error_cause_message_iterator_actor_get_plugin_name(ptr)
        )

    @property
    def component_name(self):
        return self._component_name

    @property
    def component_output_port_name(self):
        return self._component_output_port_name

    @property
    def component_class_type(self):
        return self._component_class_type

    @property
    def component_class_name(self):
        return self._component_class_name

    @property
    def plugin_name(self):
        return self._plugin_name


_ACTOR_TYPE_TO_CLS = {
    native_bt.ERROR_CAUSE_ACTOR_TYPE_UNKNOWN: _ErrorCause,
    native_bt.ERROR_CAUSE_ACTOR_TYPE_COMPONENT: _ComponentErrorCause,
    native_bt.ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS: _ComponentClassErrorCause,
    native_bt.ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR: _MessageIteratorErrorCause,
}


class _Error(Exception, abc.Sequence):
    """
    Babeltrace API call error.

    This exception is raised when a call to the Babeltrace API returns with
    the ERROR or MEMORY_ERROR status codes.
    """

    def __init__(self, msg):
        super().__init__(msg)
        # Steal the current thread's error.
        self._ptr = native_bt.current_thread_take_error()
        assert self._ptr is not None

        self._msg = msg
        self._str = msg + '\n' + native_bt.bt2_format_bt_error(self._ptr)

        # Read everything we might need from the error pointer, so we don't
        # depend on it.  It's possible for the user to keep an Error object
        # and to want to read its causes after the error pointer has been
        # restored as the current thread's error (and is therefore
        # inaccessible).
        cause_count = native_bt.error_get_cause_count(self._ptr)

        # We expect the library to append at least one cause (otherwise there
        # wouldn't be an bt_error object anyway).  Also, while formatting the
        # exception, the Python `traceback` module does:
        #
        #     if (exc_value and ...):
        #
        # If the cause list was empty, this would evaluate to False (which we
        # wouldn't want), because of the __bool__ implementation of
        # abc.Sequence.  If there's at least one cause, we are sure that
        # __bool__ will always return True and avoid any problem here.
        assert cause_count > 0

        self._causes = []

        for i in range(cause_count):
            cause_ptr = native_bt.error_borrow_cause_by_index(self._ptr, i)
            assert cause_ptr is not None
            cause = _create_error_cause_from_ptr(cause_ptr)
            self._causes.append(cause)

    def __del__(self):
        # If this exception escapes all the way out of the Python code, the
        # native code will steal `_ptr` to restore it as the current thread's
        # error.  If the exception is caught and discarded by the Python code,
        # the exception object still owns the error, so we must release it.
        if self._ptr is not None:
            native_bt.error_release(self._ptr)

    def __getitem__(self, index):
        return self._causes[index]

    def __len__(self):
        return len(self._causes)

    def __str__(self):
        return self._str
