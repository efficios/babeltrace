#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import babeltrace
import os


# general configuration
needs_sphinx = '1.2'
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.ifconfig',
]
templates_path = ['_templates']
source_suffix = '.rst'
source_encoding = 'utf-8-sig'
master_doc = 'index'
project = 'Babeltrace Python bindings'
copyright = '2014, EfficiOS Inc.'
version = babeltrace.__version__
release = version
language = None
today = ''
today_fmt = '%B %d, %Y'
exclude_patterns = []
default_role = None
add_function_parentheses = True
add_module_names = True
show_authors = False
pygments_style = 'sphinx'
modindex_common_prefix = []
keep_warnings = False

# HTML output options
html_theme = 'default'

if os.path.isdir(os.path.join('_themes', 'sphinx_rtd_theme')):
    html_theme = 'sphinx_rtd_theme'

html_theme_options = {}
html_theme_path = ['_themes']
html_title = None
html_short_title = None
html_logo = None
html_favicon = None
html_static_path = ['_static']
html_extra_path = []
html_last_updated_fmt = '%b %d, %Y'
html_use_smartypants = False
html_sidebars = {}
html_additional_pages = {}
html_domain_indices = True
html_use_index = True
html_split_index = False
html_show_sourcelink = False
html_show_copyright = True
htmlhelp_basename = 'BabeltracePydoc'
