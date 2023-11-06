#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2023 EfficiOS, Inc.

exit_code=0

set -x

black --diff --check . || exit_code=1
flake8 || exit_code=1
isort . --diff --check || exit_code=1

exit $exit_code
