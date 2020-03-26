# SPDX-License-Identifier: MIT
#
# Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>

import bt2
import re

# project
project = 'Babeltrace 2 Python bindings'
copyright = '2020, EfficiOS, Inc'
author = 'EfficiOS, Inc'
release = bt2.__version__
version = re.match(r'^\d+\.\d+', release).group(0)

# index
master_doc = 'index'

# extensions
extensions = ['bt2sphinxurl']

# theme
html_theme = 'alabaster'
