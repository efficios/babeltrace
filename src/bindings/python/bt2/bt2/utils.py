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
import bt2.logging


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
        raise TypeError("'{}' is not a '{}' object".format(o.__class__.__name__,
                                                           expected_type))


def _is_in_int64_range(v):
    assert(isinstance(v, int))
    return v >= -(2**63) and v <= (2**63 - 1)


def _is_int64(v):
    if not isinstance(v, int):
        return False

    return _is_in_int64_range(v)


def _is_in_uint64_range(v):
    assert(isinstance(v, int))
    return v >= 0 and v <= (2**64 - 1)


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


def _raise_bt2_error(msg):
    if msg is None:
        raise bt2.Error
    else:
        raise bt2.Error(msg)


def _handle_ret(ret, msg=None):
    if int(ret) < 0:
        _raise_bt2_error(msg)


def _handle_ptr(ptr, msg=None):
    if ptr is None:
        _raise_bt2_error(msg)


def _check_log_level(log_level):
    _check_int(log_level)

    log_levels = (
        bt2.logging.LoggingLevel.VERBOSE,
        bt2.logging.LoggingLevel.DEBUG,
        bt2.logging.LoggingLevel.INFO,
        bt2.logging.LoggingLevel.WARN,
        bt2.logging.LoggingLevel.ERROR,
        bt2.logging.LoggingLevel.FATAL,
        bt2.logging.LoggingLevel.NONE,
    )

    if log_level not in log_levels:
        raise ValueError("'{}' is not a valid logging level".format(log_level))
