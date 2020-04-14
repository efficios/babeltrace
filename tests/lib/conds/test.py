# SPDX-License-Identifier: MIT
#
# Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>

import unittest
import subprocess
import functools
import signal
import os
import os.path
import re
import json


# the `conds-triggers` program's full path
_CONDS_TRIGGERS_PATH = os.environ['BT_TESTS_LIB_CONDS_TRIGGER_BIN']


# test methods are added by _create_tests()
class LibPrePostCondsTestCase(unittest.TestCase):
    pass


# a condition trigger descriptor (base)
class _CondTriggerDescriptor:
    def __init__(self, index, trigger_name, cond_id):
        self._index = index
        self._trigger_name = trigger_name
        self._cond_id = cond_id

    @property
    def index(self):
        return self._index

    @property
    def trigger_name(self):
        return self._trigger_name

    @property
    def cond_id(self):
        return self._cond_id


# precondition trigger descriptor
class _PreCondTriggerDescriptor(_CondTriggerDescriptor):
    @property
    def type_str(self):
        return 'pre'


# postcondition trigger descriptor
class _PostCondTriggerDescriptor(_CondTriggerDescriptor):
    @property
    def type_str(self):
        return 'post'


# test method template for `LibPrePostCondsTestCase`
def _test(self, descriptor):
    # Execute:
    #
    #     $ conds-triggers run <index>
    #
    # where `<index>` is the descriptor's index.
    with subprocess.Popen(
        [_CONDS_TRIGGERS_PATH, 'run', str(descriptor.index)],
        stderr=subprocess.PIPE,
        universal_newlines=True,
    ) as proc:
        # wait for termination and get standard output/error data
        timeout = 5

        try:
            # wait for program end and get standard error pipe's contents
            _, stderr = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.fail('Process hanged for {} seconds'.format(timeout))
            return

        # assert that program aborted (only available on POSIX)
        if os.name == 'posix':
            self.assertEqual(proc.returncode, -int(signal.SIGABRT))

        # assert that the standard error text contains the condition ID
        text = 'Condition ID: `{}`.'.format(descriptor.cond_id)
        self.assertIn(text, stderr)


# Condition trigger descriptors from the JSON array returned by
#
#     $ conds-triggers list
def _cond_trigger_descriptors_from_json(json_descr_array):
    descriptors = []
    descriptor_names = set()

    for index, json_descr in enumerate(json_descr_array):
        # sanity check: check for duplicate
        trigger_name = json_descr['name']

        if trigger_name in descriptor_names:
            raise ValueError(
                'Duplicate condition trigger name `{}`'.format(trigger_name)
            )

        # condition ID
        cond_id = json_descr['cond-id']

        if cond_id.startswith('pre'):
            cond_type = _PreCondTriggerDescriptor
        elif cond_id.startswith('post'):
            cond_type = _PostCondTriggerDescriptor
        else:
            raise ValueError('Invalid condition ID `{}`'.format(cond_id))

        descriptors.append(cond_type(index, trigger_name, cond_id))
        descriptor_names.add(trigger_name)

    return descriptors


# creates the individual tests of `LibPrePostCondsTestCase`
def _create_tests():
    # Execute `conds-triggers list` to get a JSON array of condition
    # trigger descriptors.
    json_descr_array = json.loads(
        subprocess.check_output([_CONDS_TRIGGERS_PATH, 'list'], universal_newlines=True)
    )

    # get condition trigger descriptor objects from JSON
    descriptors = _cond_trigger_descriptors_from_json(json_descr_array)

    # create test methods
    for descriptor in descriptors:
        # test method name
        test_meth_name = 'test_{}'.format(
            re.sub(r'[^a-zA-Z0-9_]', '_', descriptor.trigger_name)
        )

        # test method
        meth = functools.partialmethod(_test, descriptor)
        setattr(LibPrePostCondsTestCase, test_meth_name, meth)


_create_tests()


if __name__ == '__main__':
    unittest.main()
