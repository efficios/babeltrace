#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# SPDX-FileCopyrightText: 2023 Michael Jeanson <mjeanson@efficios.com>

set -eu

shellcheck=${SHELLCHECK:-shellcheck}
retcode=0

while read -r script_file; do
	echo "Running ShellCheck on \`$script_file\`"
	pushd "${script_file%/*}" >/dev/null
	"$shellcheck" -x "${script_file##*/}" || retcode=$?
	popd >/dev/null
done <<< "$(find . -type f -name '*.sh' \
	! -path './.git/*' \
	! -path ./config/ltmain.sh \
	! -path ./tests/utils/tap-driver.sh \
	! -path ./tests/utils/tap/tap.sh)"

exit $retcode
