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

        # TODO return info set at creation
        return self._path

    @property
    def timestamp_begin(self):
        """
        Buffers creation timestamp (nanoseconds since Epoch) of the
        underlying trace.
        """

        # TODO return info set at creation
        pass

    @property
    def timestamp_end(self):
        """
        Buffers destruction timestamp (nanoseconds since Epoch) of the
        underlying trace.
        """

        # TODO return info set at creation
        pass

    @property
    def events(self):
        """
        Generates all the :class:`EventDeclaration` objects of the
        underlying trace.
        """

        # TODO recreate a graph containing only this trace/component and
        # go through all stream classes and events.
        pass
