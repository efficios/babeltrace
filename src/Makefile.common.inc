# SPDX-FileCopyrightText: 2017-2024 EfficiOS, Inc.
# SPDX-License-Identifier: MIT

# Path to the plugins build directory.
PLUGINS_PATH = $(abs_top_builddir)/src/plugins

# Takes a plugin name and outputs the needed LDFLAGS to embed it.
#
# The --whole-archive option is important here. From the GNU linker's
# documentation:
#
#     For each archive mentioned on the command line after the
#     --whole-archive option, include every object file in the archive in
#     the link, rather than searching the archive for the required object
#     files.
#
# In our case, we find the plugins thanks to special sections in the
# binary that are filled by plugin objects. If the linker discards those
# symbols because the CLI does not use them directly, the CLI reports
# no plugins found (plugins are effectively not embedded).
pluginarchive = $(LD_WHOLE_ARCHIVE)$(PLUGINS_PATH)/$(1)/.libs/babeltrace-plugin-$(1).a$(LD_NO_WHOLE_ARCHIVE)
