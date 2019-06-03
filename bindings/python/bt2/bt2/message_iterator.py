# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

from bt2 import native_bt, object, utils
import bt2.message
import collections.abc
import bt2.component
import bt2


class _MessageIterator(collections.abc.Iterator):
    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.MESSAGE_ITERATOR_STATUS_AGAIN:
            raise bt2.TryAgain
        elif status == native_bt.MESSAGE_ITERATOR_STATUS_END:
            raise bt2.Stop
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def __next__(self):
        raise NotImplementedError


class _GenericMessageIterator(object._SharedObject, _MessageIterator):
    def __init__(self, ptr):
            self._current_msgs = []
            self._at = 0
            super().__init__(ptr)

    def __next__(self):
        if len(self._current_msgs) == self._at:
            status, msgs = self._get_msg_range(self._ptr)
            self._handle_status(status,
                                'unexpected error: cannot advance the message iterator')
            self._current_msgs = msgs
            self._at = 0

        msg_ptr = self._current_msgs[self._at]
        self._at += 1

        return bt2.message._create_from_ptr(msg_ptr)


# This is created when a component wants to iterate on one of its input ports.
class _UserComponentInputPortMessageIterator(_GenericMessageIterator):
    _get_msg_range = staticmethod(native_bt.py3_self_component_port_input_get_msg_range)
    _get_ref = staticmethod(native_bt.self_component_port_input_message_iterator_get_ref)
    _put_ref = staticmethod(native_bt.self_component_port_input_message_iterator_put_ref)


# This is created when the user wants to iterate on a component's output port,
# from outside the graph.
class _OutputPortMessageIterator(_GenericMessageIterator):
    _get_msg_range = staticmethod(native_bt.py3_port_output_get_msg_range)
    _get_ref = staticmethod(native_bt.port_output_message_iterator_get_ref)
    _put_ref = staticmethod(native_bt.port_output_message_iterator_put_ref)


# This is extended by the user to implement component classes in Python.  It
# is created for a given output port when an input port message iterator is
# created on the input port on the other side of the connection.  It is also
# created when an output port message iterator is created on this output port.
#
# Its purpose is to feed the messages that should go out through this output
# port.
class _UserMessageIterator(_MessageIterator):
    def __new__(cls, ptr):
        # User iterator objects are always created by the native side,
        # that is, never instantiated directly by Python code.
        #
        # The native code calls this, then manually calls
        # self.__init__() without the `ptr` argument. The user has
        # access to self.component during this call, thanks to this
        # self._ptr argument being set.
        #
        # self._ptr is NOT owned by this object here, so there's nothing
        # to do in __del__().
        self = super().__new__(cls)
        self._ptr = ptr
        return self

    def _init_from_native(self, self_output_port_ptr):
        self_output_port = bt2.port._create_self_from_ptr_and_get_ref(
            self_output_port_ptr, native_bt.PORT_TYPE_OUTPUT)
        self.__init__(self_output_port)

    def __init__(self, output_port):
        pass

    @property
    def _component(self):
        return native_bt.py3_get_user_component_from_user_msg_iter(self._ptr)

    @property
    def addr(self):
        return int(self._ptr)

    def _finalize(self):
        pass

    def __next__(self):
        raise bt2.Stop

    def _next_from_native(self):
        # this can raise anything: it's catched by the native part
        try:
            msg = next(self)
        except StopIteration:
            raise bt2.Stop
        except:
            raise

        utils._check_type(msg, bt2.message._Message)

        # Release the reference to the native part.
        ptr = msg._release()
        return int(ptr)

    # Validate that the presence or lack of presence of a
    # `default_clock_snapshot` value is valid in the context of `stream_class`.
    @staticmethod
    def _validate_default_clock_snapshot(stream_class, default_clock_snapshot):
        stream_class_has_default_clock_class = stream_class.default_clock_class is not None

        if stream_class_has_default_clock_class and default_clock_snapshot is None:
            raise bt2.Error(
                'stream class has a default clock class, default_clock_snapshot should not be None')

        if not stream_class_has_default_clock_class and default_clock_snapshot is not None:
            raise bt2.Error(
                'stream class has no default clock class, default_clock_snapshot should be None')

    def _create_event_message(self, event_class, packet, default_clock_snapshot=None):
        utils._check_type(event_class, bt2.event_class.EventClass)
        utils._check_type(packet, bt2.packet._Packet)
        self._validate_default_clock_snapshot(packet.stream.stream_class, default_clock_snapshot)

        if default_clock_snapshot is not None:
            utils._check_uint64(default_clock_snapshot)
            ptr = native_bt.message_event_create_with_default_clock_snapshot(
                self._ptr, event_class._ptr, packet._ptr, default_clock_snapshot)
        else:
            ptr = native_bt.message_event_create(
                self._ptr, event_class._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create event message object')

        return bt2.message._EventMessage(ptr)

    def _create_message_iterator_inactivity_message(self, clock_class, clock_snapshot):
        utils._check_type(clock_class, bt2.clock_class._ClockClass)
        ptr = native_bt.message_message_iterator_inactivity_create(
            self._ptr, clock_class._ptr, clock_snapshot)

        if ptr is None:
            raise bt2.CreationError('cannot create inactivity message object')

        return bt2.message._MessageIteratorInactivityMessage(ptr)

    def _create_stream_beginning_message(self, stream):
        utils._check_type(stream, bt2.stream._Stream)

        ptr = native_bt.message_stream_beginning_create(self._ptr, stream._ptr)
        if ptr is None:
            raise bt2.CreationError('cannot create stream beginning message object')

        return bt2.message._StreamBeginningMessage(ptr)

    def _create_stream_activity_beginning_message(self, stream, default_clock_snapshot=None):
        utils._check_type(stream, bt2.stream._Stream)
        self._validate_default_clock_snapshot(stream.stream_class, default_clock_snapshot)

        ptr = native_bt.message_stream_activity_beginning_create(self._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError(
                'cannot create stream activity beginning message object')

        msg = bt2.message._StreamActivityBeginningMessage(ptr)

        if default_clock_snapshot is not None:
            msg._default_clock_snapshot = default_clock_snapshot

        return msg

    def _create_stream_activity_end_message(self, stream, default_clock_snapshot=None):
        utils._check_type(stream, bt2.stream._Stream)
        self._validate_default_clock_snapshot(stream.stream_class, default_clock_snapshot)

        ptr = native_bt.message_stream_activity_end_create(self._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError(
                'cannot create stream activity end message object')

        msg = bt2.message._StreamActivityEndMessage(ptr)

        if default_clock_snapshot is not None:
            msg._default_clock_snapshot = default_clock_snapshot

        return msg

    def _create_stream_end_message(self, stream):
        utils._check_type(stream, bt2.stream._Stream)

        ptr = native_bt.message_stream_end_create(self._ptr, stream._ptr)
        if ptr is None:
            raise bt2.CreationError('cannot create stream end message object')

        return bt2.message._StreamEndMessage(ptr)

    def _create_packet_beginning_message(self, packet, default_clock_snapshot=None):
        utils._check_type(packet, bt2.packet._Packet)

        if packet.stream.stream_class.packets_have_default_beginning_clock_snapshot:
            if default_clock_snapshot is None:
                raise ValueError("packet beginning messages in this stream must have a default clock snapshots")

            utils._check_uint64(default_clock_snapshot)
            ptr = native_bt.message_packet_beginning_create_with_default_clock_snapshot(
                self._ptr, packet._ptr, default_clock_snapshot)
        else:
            if default_clock_snapshot is not None:
                raise ValueError("packet beginning messages in this stream must not have a default clock snapshots")

            ptr = native_bt.message_packet_beginning_create(self._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet beginning message object')

        return bt2.message._PacketBeginningMessage(ptr)

    def _create_packet_end_message(self, packet, default_clock_snapshot=None):
        utils._check_type(packet, bt2.packet._Packet)

        if packet.stream.stream_class.packets_have_default_end_clock_snapshot:
            if default_clock_snapshot is None:
                raise ValueError("packet end messages in this stream must have a default clock snapshots")

            utils._check_uint64(default_clock_snapshot)
            ptr = native_bt.message_packet_end_create_with_default_clock_snapshot(
                self._ptr, packet._ptr, default_clock_snapshot)
        else:
            if default_clock_snapshot is not None:
                raise ValueError("packet end messages in this stream must not have a default clock snapshots")

            ptr = native_bt.message_packet_end_create(self._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet end message object')

        return bt2.message._PacketEndMessage(ptr)

    def _create_discarded_events_message(self, stream, count=None,
                                         beg_clock_snapshot=None,
                                         end_clock_snapshot=None):
        utils._check_type(stream, bt2.stream._Stream)

        if beg_clock_snapshot is None and end_clock_snapshot is None:
            ptr = native_bt.message_discarded_events_create(self._ptr, stream._ptr)
        elif beg_clock_snapshot is not None and end_clock_snapshot is not None:
            utils._check_uint64(beg_clock_snapshot)
            utils._check_uint64(end_clock_snapshot)
            ptr = native_bt.message_discarded_events_create_with_default_clock_snapshots(
                self._ptr, stream._ptr, beg_clock_snapshot, end_clock_snapshot)
        else:
            raise ValueError('begin and end clock snapshots must be both provided or both omitted')

        if ptr is None:
            raise bt2.CreationError('cannot discarded events message object')

        msg = bt2.message._DiscardedEventsMessage(ptr)

        if count is not None:
            msg._count = count

        return msg

    def _create_discarded_packets_message(self, stream, count=None, beg_clock_snapshot=None, end_clock_snapshot=None):
        utils._check_type(stream, bt2.stream._Stream)

        if beg_clock_snapshot is None and end_clock_snapshot is None:
            ptr = native_bt.message_discarded_packets_create(self._ptr, stream._ptr)
        elif beg_clock_snapshot is not None and end_clock_snapshot is not None:
            utils._check_uint64(beg_clock_snapshot)
            utils._check_uint64(end_clock_snapshot)
            ptr = native_bt.message_discarded_packets_create_with_default_clock_snapshots(
                self._ptr, stream._ptr, beg_clock_snapshot, end_clock_snapshot)
        else:
            raise ValueError('begin and end clock snapshots must be both provided or both omitted')

        if ptr is None:
            raise bt2.CreationError('cannot discarded packets message object')

        msg = bt2.message._DiscardedPacketsMessage(ptr)

        if count is not None:
            msg._count = count

        return msg

