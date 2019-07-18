# The MIT License (MIT)
#
# Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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

from tap import TAPTestRunner
import unittest
import sys
import argparse


if __name__ == '__main__':
    argparser = argparse.ArgumentParser()
    argparser.add_argument(
        '-f', '--failfast', help='Stop on first fail or error', action='store_true'
    )

    argparser.add_argument(
        'start_dir', help='Base directory where to search for tests', type=str
    )

    mut_exclu_group = argparser.add_mutually_exclusive_group(required=True)

    mut_exclu_group.add_argument(
        '-p',
        '--pattern',
        help='Glob-style pattern of test files to run ' '(e.g. test_event*.py)',
        type=str,
    )

    mut_exclu_group.add_argument(
        '-t',
        '--test-case',
        help='Run a specfic test module name, test class '
        'name, or test method name '
        '(e.g. test_event.EventTestCase.test_clock_value)',
        type=str,
    )

    args = argparser.parse_args()

    loader = unittest.TestLoader()

    start_dir = args.start_dir
    pattern = args.pattern
    failfast = args.failfast
    test_case = args.test_case

    if test_case:
        sys.path.append(start_dir)
        tests = loader.loadTestsFromName(test_case)
    elif pattern:
        tests = loader.discover(start_dir, pattern)
    else:
        # This will never happen because the mutual exclusion group has the
        # `required` parameter set to True. So one or the other must be set or
        # else it will fail to parse.
        sys.exit(1)

    if tests.countTestCases() < 1:
        print("No tests matching '%s' found in '%s'" % (pattern, start_dir))
        sys.exit(1)

    runner = TAPTestRunner(failfast=failfast)
    runner.set_stream(True)
    runner.set_format('{method_name}')
    sys.exit(0 if runner.run(tests).wasSuccessful() else 1)
