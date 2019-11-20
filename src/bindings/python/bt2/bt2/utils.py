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

import bt2
from bt2 import logging as bt2_logging
from bt2 import native_bt


def _check_bool(o):
    if not isinstance(o, bool):
        raise TypeError("'{}' is not a 'bool' object".format(o.__class__.__name__))


def _check_int(o):
    if not isinstance(o, int):
        raise TypeError("'{}' is not an 'int' object".format(o.__class__.__name__))


def _check_float(o):
    if not isinstance(o, float):
        raise TypeError("'{}' is not a 'float' object".format(o.__class__.__name__))


def _check_str(o):
    if not isinstance(o, str):
        raise TypeError("'{}' is not a 'str' object".format(o.__class__.__name__))


def _check_type(o, expected_type):
    if not isinstance(o, expected_type):
        raise TypeError(
            "'{}' is not a '{}' object".format(o.__class__.__name__, expected_type)
        )


def _is_in_int64_range(v):
    assert isinstance(v, int)
    return v >= -(2 ** 63) and v <= (2 ** 63 - 1)


def _is_int64(v):
    if not isinstance(v, int):
        return False

    return _is_in_int64_range(v)


def _is_in_uint64_range(v):
    assert isinstance(v, int)
    return v >= 0 and v <= (2 ** 64 - 1)


def _is_uint64(v):
    if not isinstance(v, int):
        return False

    return _is_in_uint64_range(v)


def _check_int64(v, msg=None):
    _check_int(v)

    if not _is_in_int64_range(v):
        if msg is None:
            msg = 'expecting a signed 64-bit integral value'

        msg += ' (got {})'.format(v)
        raise ValueError(msg)


def _check_uint64(v, msg=None):
    _check_int(v)

    if not _is_in_uint64_range(v):
        if msg is None:
            msg = 'expecting an unsigned 64-bit integral value'

        msg += ' (got {})'.format(v)
        raise ValueError(msg)


def _is_m1ull(v):
    return v == 18446744073709551615


def _is_pow2(v):
    return v != 0 and ((v & (v - 1)) == 0)


def _check_alignment(a):
    _check_uint64(a)

    if not _is_pow2(a):
        raise ValueError('{} is not a power of two'.format(a))


def _check_log_level(log_level):
    _check_int(log_level)

    log_levels = (
        bt2_logging.LoggingLevel.TRACE,
        bt2_logging.LoggingLevel.DEBUG,
        bt2_logging.LoggingLevel.INFO,
        bt2_logging.LoggingLevel.WARNING,
        bt2_logging.LoggingLevel.ERROR,
        bt2_logging.LoggingLevel.FATAL,
        bt2_logging.LoggingLevel.NONE,
    )

    if log_level not in log_levels:
        raise ValueError("'{}' is not a valid logging level".format(log_level))


def _handle_func_status(status, msg=None):
    if status == native_bt.__BT_FUNC_STATUS_OK:
        # no error
        return

    if status == native_bt.__BT_FUNC_STATUS_ERROR:
        assert msg is not None
        raise bt2._Error(msg)
    elif status == native_bt.__BT_FUNC_STATUS_MEMORY_ERROR:
        assert msg is not None
        raise bt2._MemoryError(msg)
    elif status == native_bt.__BT_FUNC_STATUS_END:
        if msg is None:
            raise bt2.Stop
        else:
            raise bt2.Stop(msg)
    elif status == native_bt.__BT_FUNC_STATUS_AGAIN:
        if msg is None:
            raise bt2.TryAgain
        else:
            raise bt2.TryAgain(msg)
    elif status == native_bt.__BT_FUNC_STATUS_OVERFLOW_ERROR:
        if msg is None:
            raise bt2._OverflowError
        else:
            raise bt2._OverflowError(msg)
    elif status == native_bt.__BT_FUNC_STATUS_UNKNOWN_OBJECT:
        if msg is None:
            raise bt2.UnknownObject
        else:
            raise bt2.UnknownObject(msg)
    else:
        assert False


class _ListenerHandle:
    def __init__(self, addr):
        self._addr = addr
        self._listener_id = None

    def _set_listener_id(self, listener_id):
        self._listener_id = listener_id

    def _invalidate(self):
        self._listener_id = None
