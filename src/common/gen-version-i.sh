#!/usr/bin/env sh
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2023 EfficiOS, Inc.

# This file generates an include file that contains the git version
# string of the current branch, it must be continuously updated when
# we build in the git repo and shipped in dist tarballs to reflect the
# status of the tree when it was generated. If the tree is clean and
# the current commit is a tag starting with "v", consider this a
# release version and set an empty git version.

set -o nounset

if test "${TOP_SRCDIR:-}" = ""; then
	echo "$0: TOP_SRCDIR is not set" >&2
	exit 1
fi

GREP=${GREP:-grep}
SED=${SED:-sed}

# Delete any stale "version.i.tmp" file.
rm -f version.i.tmp

if test ! -f version.i && test -f "$TOP_SRCDIR/include/version.i"; then
	cp "$TOP_SRCDIR/include/version.i" version.i
fi

# If "bootstrap" and ".git" exists in the top source directory and the git
# executable is available, get the current git version string in the form:
#
#  "latest_tag"(-"number_of_commits_on_top")(-g"latest_commit_hash")(-dirty)
#
# And store it in "version.i.tmp", if the current commit is tagged, the tag
# starts with "v" and the tree is clean, consider this a release version and
# overwrite the git version with an empty string in "version.i.tmp".
if test -r "$TOP_SRCDIR/bootstrap" && test -r "$TOP_SRCDIR/.git" &&
		(command -v git > /dev/null 2>&1); then
	GIT_VERSION_STR="$(cd "$TOP_SRCDIR" && git describe --tags --dirty)"
	GIT_CURRENT_TAG="$(cd "$TOP_SRCDIR" && (git describe --tags --exact-match --match="v[0-9]*" HEAD || true) 2> /dev/null)"
	echo "#define BT_VERSION_GIT \"$GIT_VERSION_STR\"" > version.i.tmp

	if ! $GREP -- "-dirty" version.i.tmp > /dev/null &&
			test "x$GIT_CURRENT_TAG" != "x"; then
		echo "#define BT_VERSION_GIT \"\"" > version.i.tmp
	fi
fi

# If we don't have a "version.i.tmp" nor a "version.i", generate an empty
# string as a failover. If a "version.i" is present, for example when building
# from a distribution tarball, get the git_version using grep.
if test ! -f version.i.tmp; then
	if test -f version.i; then
		$GREP "^#define \bBT_VERSION_GIT\b.*" version.i > version.i.tmp
	else
		echo '#define BT_VERSION_GIT ""' > version.i.tmp
	fi
fi

{
	# Fetch the BT_VERSION_EXTRA_NAME define from "version/extra_version_name" and output it
	# to "version.i.tmp".
	echo "#define BT_VERSION_EXTRA_NAME \"$($SED -n '1p' "$TOP_SRCDIR/version/extra_version_name" 2> /dev/null)\""

	# Fetch the BT_VERSION_EXTRA_DESCRIPTION define from "version/extra_version_description",
	# sanitize and format it with a sed script to replace all non-alpha-numeric values
	# with "-" and join all lines by replacing "\n" with litteral string c-style "\n" and
	# output it to "version.i.tmp".
	echo "#define BT_VERSION_EXTRA_DESCRIPTION \"$($SED -E ':a ; N ; $!ba ; s/[^a-zA-Z0-9 \n\t\.,]/-/g ; s/\r{0,1}\n/\\n/g' "$TOP_SRCDIR/version/extra_version_description" 2> /dev/null)\""

	# Repeat the same logic for the "version/extra_patches" directory.
	# Data fetched from "version/extra_patches" must be sanitized and
	# formatted.
	# The data is fetched using "find" with an ignore pattern for the README.adoc file.
	# The sanitize step uses sed with a script to replace all
	# non-alpha-numeric values, except " " (space), to "-".
	# The formatting step uses sed with a script to join all lines
	# by replacing "\n" with litteral string c-style "\n".
	# shellcheck disable=SC2012
	echo "#define BT_VERSION_EXTRA_PATCHES \"$(ls -1 "$TOP_SRCDIR/version/extra_patches" | $GREP -v '^README.adoc' | $SED -E ':a ; N ; $!ba ; s/[^a-zA-Z0-9 \n\t\.]/-/g ; s/\r{0,1}\n/\\n/g' 2> /dev/null)\""
} >> version.i.tmp

# If we don't have a "version.i" or we have both files (version.i, version.i.tmp)
# and they are different, copy "version.i.tmp" over "version.i".
# This way the dependent targets are only rebuilt when the git version
# string or either one of extra version string change.
if test ! -f version.i ||
		test x"$(cat version.i.tmp)" != x"$(cat version.i)"; then
	mv -f version.i.tmp version.i
fi

rm -f version.i.tmp
