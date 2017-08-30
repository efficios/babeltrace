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
import babeltrace.common as common
import babeltrace.reader_trace_handle as reader_trace_handle
import os

class TraceCollection:
    """
    A :class:`TraceCollection` is a collection of opened traces.

    Once a trace collection is created, you can add traces to the
    collection by using the :meth:`add_trace` or
    :meth:`add_traces_recursive`, and then iterate on the merged
    events using :attr:`events`.

    You may use :meth:`remove_trace` to close and remove a specific
    trace from a trace collection.
    """

    def __init__(self, intersect_mode=False):
        """
        Creates an empty trace collection.
        """

        # TODO Use bt2 collection util.
        self._in_intersect_mode = intersect_mode
        self._sources = []

    def __del__(self):
        # TODO Something?
        pass

    def add_trace(self, path, format_str):
        """
        Adds a trace to the trace collection.

        *path* is the exact path of the trace on the filesystem.

        *format_str* is a string indicating the type of trace to
        add. ``ctf`` is currently the only supported trace format.

        Returns the corresponding :class:`TraceHandle` instance for
        this opened trace on success, or ``None`` on error.

        This function **does not** recurse directories to find a
        trace.  See :meth:`add_traces_recursive` for a recursive
        version of this function.
        """

        # TODO Use bt2 collection utils
        # keep all sources in the _sources dict in order to rebuild a new
        # graph on demand (on remove_trace) and to check for intersections.
        pass

    def add_traces_recursive(self, path, format_str):
        """
        Adds traces to this trace collection by recursively searching
        in the *path* directory.

        *format_str* is a string indicating the type of trace to add.
        ``ctf`` is currently the only supported trace format.

        Returns a :class:`dict` object mapping full paths to trace
        handles for each trace found, or ``None`` on error.

        See also :meth:`add_trace`.
        """

        trace_handles = {}
        noTrace = True
        error = False

        for fullpath, dirs, files in os.walk(path):
            if "metadata" in files:
                trace_handle = self.add_trace(fullpath, format_str)

                if trace_handle is None:
                    error = True
                    continue

                trace_handles[fullpath] = trace_handle
                noTrace = False

        if noTrace and error:
            return None

        return trace_handles

    def remove_trace(self, handle):
        """
        Removes a trace from the trace collection using its trace
        handle *trace_handle*.

        :class:`TraceHandle` objects are returned by :meth:`add_trace`
        and :meth:`add_traces_recursive`.
        """

        if not isinstance(handle, reader_trace_handle.TraceHandle):
            raise TypeError("'{}' is not a 'TraceHandle' object".format(
                handle.__class__.__name__))

        # TODO create a separate graph without that source and make it the
        # current graph.

    @property
    def intersect_mode(self):
        return self._in_intersect_mode

    @property
    def has_intersection(self):
        # Use query to determine whether all sources have a common intersection
        pass

    @property
    def events(self):
        """
        Generates the ordered :class:`Event` objects of all the opened
        traces contained in this trace collection.
        """

        # TODO Use bt2 trace collection utils
        pass

    def events_timestamps(self, timestamp_begin, timestamp_end):
        """
        Generates the ordered :class:`Event` objects of all the opened
        traces contained in this trace collection from *timestamp_begin*
        to *timestamp_end*.

        *timestamp_begin* and *timestamp_end* are given in nanoseconds
        since Epoch.

        See :attr:`events` for notes and limitations.
        """

        # TODO Create a graph with a trimmer
        pass

    @property
    def timestamp_begin(self):
        """
        Begin timestamp of this trace collection (nanoseconds since Epoch).
        """

        # TODO Use queries to determine timestamp_begin
        pass

    @property
    def timestamp_end(self):
        """
        End timestamp of this trace collection (nanoseconds since Epoch).
        """

        # TODO Use source queries to determine timestamp_end
        pass

    def _events(self, begin_pos_ptr, end_pos_ptr):
        # TODO Iterate on graph using bt2 utils
        pass
