# SPDX-License-Identifier: MIT
#
# Copyright (c) 2023 EfficiOS, Inc.

# The purpose of this import is to make the typing module easily accessible
# elsewhere, without having to do the try-except everywhere.
try:
    import typing as typing_mod  # noqa: F401
except ImportError:
    import local_typing as typing_mod  # noqa: F401
