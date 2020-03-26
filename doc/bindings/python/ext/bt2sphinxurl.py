# SPDX-License-Identifier: MIT
#
# Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
#
# This file is a Sphinx extension which adds the following roles:
#
# `bt2man`:
#     A typical manual page reference, like `grep(1)`.
#
#     Example:
#
#         :bt2man:`grep(1)`
#
#     This role creates a simple inline literal node with the role's
#     text if it's not a Babeltrace 2 manual page reference, or an
#     external link to the corresponding online manual page (on
#     `babeltrace.org`) with the appropriate project's version
#     (`version` configuration entry) otherwise.
#
# `bt2link`:
#     An external link with an URL in which a specific placeholder is
#     replaced with the project's version.
#
#     The role's text follows the typical external link format, for
#     example:
#
#         Link text <https://example.com/>
#
#     Any `@ver@` in the URL is replaced with the project's version
#     (`version` configuration entry).
#
#     Example:
#
#         :bt2link:`libbabeltrace2 <https://babeltrace.org/docs/v@ver@/libbabeltrace2/>`

import docutils
import docutils.utils
import docutils.nodes
import re
import functools


def _bt2man_role(
    bt2_version, name, rawtext, text, lineno, inliner, options=None, content=None
):
    # match a manual page reference
    m = re.match(r'^([a-zA-Z0-9_.:-]+)\(([a-zA-Z0-9]+)\)$', text)

    if not m:
        msg = 'Cannot parse manual page reference `{}`'.format(text)
        inliner.reporter.severe(msg, line=lineno)
        return [inliner.problematic(rawtext, rawtext, msg)], [msg]

    # matched manual page and volume
    page = m.group(1)
    vol = m.group(2)

    # create nodes: `ret_node` is the node to return
    page_node = docutils.nodes.strong(rawtext, page)
    vol_node = docutils.nodes.inline(rawtext, '({})'.format(vol))
    man_node = docutils.nodes.inline(rawtext, '', page_node, vol_node)
    ret_node = docutils.nodes.literal(rawtext, '', man_node)

    if page.startswith('babeltrace2'):
        # Babeltrace 2 manual page: wrap `ret_node` with an external
        # link node
        url_tmpl = 'https://babeltrace.org/docs/v{ver}/man{vol}/{page}.{vol}/'
        url = url_tmpl.format(ver=bt2_version, vol=vol, page=page)
        ret_node = docutils.nodes.reference(
            rawtext, '', ret_node, internal=False, refuri=url
        )

    return [ret_node], []


def _bt2link_role(
    bt2_version, name, rawtext, text, lineno, inliner, options=None, content=None
):
    # match link text and URL
    m = re.match(r'^([^<]+) <([^>]+)>$', text)

    if not m:
        msg = 'Cannot parse link template `{}`'.format(text)
        inliner.reporter.severe(msg, line=lineno)
        return [inliner.problematic(rawtext, rawtext, msg)], [msg]

    link_text = m.group(1)

    # replace `@ver@` with the project's version
    url = m.group(2).replace('@ver@', bt2_version)

    # create and return an external link node
    node = docutils.nodes.reference(rawtext, link_text, internal=False, refuri=url)
    return [node], []


def _add_roles(app):
    # add the extension's roles; the role functions above expect the
    # project's version as their first parameter
    app.add_role('bt2man', functools.partial(_bt2man_role, app.config.version))
    app.add_role('bt2link', functools.partial(_bt2link_role, app.config.version))


def setup(app):
    app.connect('builder-inited', _add_roles)
    return {
        'version': app.config.version,
        'parallel_read_safe': True,
    }
