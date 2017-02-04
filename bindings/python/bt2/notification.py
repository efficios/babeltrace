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
import bt2.packet
import bt2.stream
import bt2.event
import bt2


def _create_from_ptr(ptr):
    notif_type = native_bt.notification_get_type(ptr)
    cls = None

    if notif_type not in _NOTIF_TYPE_TO_CLS:
        raise bt2.Error('unknown notification type: {}'.format(notif_type))

    return _NOTIF_TYPE_TO_CLS[notif_type]._create_from_ptr(ptr)


class _Notification(object._Object):
    pass


class TraceEventNotification(_Notification):
    def __init__(self, event):
        utils._check_type(event, bt2.event._Event)
        ptr = native_bt.notification_event_create(event._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create trace event notification object')

        super().__init__(ptr)

    @property
    def event(self):
        event_ptr = native_bt.notification_event_get_event(self._ptr)
        utils._handle_ptr(event_ptr, "cannot get trace event notification object's event object")
        return bt2.event._create_from_ptr(event_ptr)


class BeginningOfPacketNotification(_Notification):
    def __init__(self, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ptr = native_bt.notification_packet_begin_create(packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create beginning of packet notification object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.notification_packet_begin_get_packet(self._ptr)
        utils._handle_ptr(packet_ptr, "cannot get beginning of packet notification object's packet object")
        return bt2.packet._Packet._create_from_ptr(packet_ptr)


class EndOfPacketNotification(_Notification):
    def __init__(self, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ptr = native_bt.notification_packet_end_create(packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create end of packet notification object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.notification_packet_end_get_packet(self._ptr)
        utils._handle_ptr(packet_ptr, "cannot get end of packet notification object's packet object")
        return bt2.packet._Packet._create_from_ptr(packet_ptr)


class EndOfStreamNotification(_Notification):
    def __init__(self, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.notification_stream_end_create(stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create end of stream notification object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.notification_stream_end_get_stream(self._ptr)
        utils._handle_ptr(stream_ptr, "cannot get end of stream notification object's stream object")
        return bt2.stream._create_from_ptr(stream_ptr)


class NewTraceNotification(_Notification):
    def __init__(self, trace):
        utils._check_type(trace, bt2.Trace)
        ptr = native_bt.notification_new_trace_create(trace._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create new trace notification object')

        super().__init__(ptr)

    @property
    def trace(self):
        trace_ptr = native_bt.notification_new_trace_get_trace(self._ptr)
        utils._handle_ptr(trace_ptr, "cannot get new trace notification object's trace object")
        return bt2.trace._create_from_ptr(trace_ptr)


class NewStreamClassNotification(_Notification):
    def __init__(self, stream_class):
        utils._check_type(stream_class, bt2.StreamClass)
        ptr = native_bt.notification_new_stream_class_create(stream_class._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create new stream class notification object')

        super().__init__(ptr)

    @property
    def stream_class(self):
        stream_class_ptr = native_bt.notification_new_stream_class_get_stream_class(self._ptr)
        utils._handle_ptr(stream_class_ptr, "cannot get new stream class notification object's stream class object")
        return bt2.stream_class.StreamClass._create_from_ptr(stream_class_ptr)


_NOTIF_TYPE_TO_CLS = {
    native_bt.NOTIFICATION_TYPE_EVENT: TraceEventNotification,
    native_bt.NOTIFICATION_TYPE_PACKET_BEGIN: BeginningOfPacketNotification,
    native_bt.NOTIFICATION_TYPE_PACKET_END: EndOfPacketNotification,
    native_bt.NOTIFICATION_TYPE_STREAM_END: EndOfStreamNotification,
    native_bt.NOTIFICATION_TYPE_NEW_TRACE: NewTraceNotification,
    native_bt.NOTIFICATION_TYPE_NEW_STREAM_CLASS: NewStreamClassNotification,
}
