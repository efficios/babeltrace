# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

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
