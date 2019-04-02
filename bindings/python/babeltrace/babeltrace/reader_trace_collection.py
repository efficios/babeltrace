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
from babeltrace import reader_trace_handle
from babeltrace import reader_event
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

        self._intersect_mode = intersect_mode
        self._trace_handles = set()
        self._next_th_id = 0
        self._ctf_plugin = bt2.find_plugin('ctf')
        assert(self._ctf_plugin is not None)
        self._fs_comp_cls = self._ctf_plugin.source_component_classes['fs']

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

        if format_str != 'ctf':
            raise ValueError('only the "ctf" format is supported')

        if not os.path.isfile(os.path.join(path, 'metadata')):
            raise ValueError('no "metadata" file found in "{}"'.format(path))

        th = reader_trace_handle.TraceHandle.__new__(reader_trace_handle.TraceHandle)
        th._id = self._next_th_id
        self._next_th_id += 1
        th._trace_collection = self
        th._path = path
        self._trace_handles.add(th)
        return th

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

        # this can raise but it would mean the trace handle is not part
        # of this trace collection anyway
        self._trace_handles.remove(handle)

    @property
    def intersect_mode(self):
        return self._intersect_mode

    @property
    def has_intersection(self):
        return any(th._has_intersection for th in self._trace_handles)

    @property
    def events(self):
        """
        Generates the ordered :class:`Event` objects of all the opened
        traces contained in this trace collection.
        """

        return self._gen_events()

    def events_timestamps(self, timestamp_begin, timestamp_end):
        """
        Generates the ordered :class:`Event` objects of all the opened
        traces contained in this trace collection from *timestamp_begin*
        to *timestamp_end*.

        *timestamp_begin* and *timestamp_end* are given in nanoseconds
        since Epoch.

        See :attr:`events` for notes and limitations.
        """

        return self._gen_events(timestamp_begin / 1e9, timestamp_end / 1e9)

    def _gen_events(self, begin_s=None, end_s=None):
        specs = [bt2.ComponentSpec('ctf', 'fs', th.path) for th in self._trace_handles]

        try:
            iter_cls = bt2.TraceCollectionMessageIterator
            tc_iter = iter_cls(specs,
                               stream_intersection_mode=self._intersect_mode,
                               begin=begin_s, end=end_s,
                               message_types=[bt2.EventMessage])
            return map(reader_event._create_event, tc_iter)
        except:
            raise ValueError

    @property
    def timestamp_begin(self):
        """
        Begin timestamp of this trace collection (nanoseconds since Epoch).
        """

        if not self._trace_handles:
            return

        return min(th.timestamp_begin for th in self._trace_handles)

    @property
    def timestamp_end(self):
        """
        End timestamp of this trace collection (nanoseconds since Epoch).
        """

        if not self._trace_handles:
            return

        return max(th.timestamp_end for th in self._trace_handles)
