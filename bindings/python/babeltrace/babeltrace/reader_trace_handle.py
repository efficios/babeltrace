# The MIT License (MIT)
#
# Copyright (c) 2013-2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

import bt2
import itertools
from babeltrace import reader_event_declaration


class TraceHandle:
    """
    A :class:`TraceHandle` is a handle allowing the user to manipulate
    a specific trace directly. It is a unique identifier representing a
    trace, and is not meant to be instantiated by the user.
    """

    def __init__(self):
        raise NotImplementedError("TraceHandle cannot be instantiated")

    def __repr__(self):
        # TODO print an id or some information about component / query result?
        return "Babeltrace TraceHandle: trace_id('{0}')".format(self._id)

    def __hash__(self):
        return hash((self.path, self.id))

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return (self.path, self.id) == (other.path, other.id)

    @property
    def id(self):
        """
        Numeric ID of this trace handle.
        """

        return self._id

    @property
    def path(self):
        """
        Path of the underlying trace.
        """

        return self._path

    def _query_trace_info(self):
        try:
            result = bt2.QueryExecutor().query(self._trace_collection._fs_comp_cls,
                                               'trace-info', {'path': self._path})
        except:
            raise ValueError

        assert(len(result) == 1)
        return result

    @property
    def timestamp_begin(self):
        """
        Buffers creation timestamp (nanoseconds since Epoch) of the
        underlying trace.
        """

        result = self._query_trace_info()

        try:
            return int(result[0]['range-ns']['begin'])
        except:
            raise ValueError

    @property
    def timestamp_end(self):
        """
        Buffers destruction timestamp (nanoseconds since Epoch) of the
        underlying trace.
        """

        result = self._query_trace_info()

        try:
            return int(result[0]['range-ns']['end'])
        except:
            raise ValueError

    @property
    def _has_intersection(self):
        result = self._query_trace_info()

        try:
            return 'intersection-range-ns' in result[0]
        except:
            raise ValueError

    def _get_event_declarations(self):
        notif_iter = bt2.TraceCollectionNotificationIterator([
            bt2.ComponentSpec('ctf', 'fs', self._path)
        ])

        # raises if the trace contains no streams
        first_notif = next(notif_iter)
        assert(type(first_notif) is bt2.StreamBeginningNotification)
        trace = first_notif.stream.stream_class.trace
        ec_iters = [sc.values() for sc in trace.values()]
        return map(reader_event_declaration._create_event_declaration,
                   itertools.chain(*ec_iters))

    @property
    def events(self):
        """
        Generates all the :class:`EventDeclaration` objects of the
        underlying trace.
        """

        try:
            return self._get_event_declarations()
        except:
            return
