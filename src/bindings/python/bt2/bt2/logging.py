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

from bt2 import native_bt


class LoggingLevel:
    TRACE = native_bt.LOGGING_LEVEL_TRACE
    DEBUG = native_bt.LOGGING_LEVEL_DEBUG
    INFO = native_bt.LOGGING_LEVEL_INFO
    WARNING = native_bt.LOGGING_LEVEL_WARNING
    ERROR = native_bt.LOGGING_LEVEL_ERROR
    FATAL = native_bt.LOGGING_LEVEL_FATAL
    NONE = native_bt.LOGGING_LEVEL_NONE


def get_minimal_logging_level():
    return native_bt.logging_get_minimal_level()


def get_global_logging_level():
    return native_bt.logging_get_global_level()


def set_global_logging_level(level):
    levels = (
        LoggingLevel.TRACE,
        LoggingLevel.DEBUG,
        LoggingLevel.INFO,
        LoggingLevel.WARNING,
        LoggingLevel.ERROR,
        LoggingLevel.FATAL,
        LoggingLevel.NONE,
    )

    if level not in levels:
        raise TypeError("'{}' is not a valid logging level".format(level))

    return native_bt.logging_set_global_level(level)
