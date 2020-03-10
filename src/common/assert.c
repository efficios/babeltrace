/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 EfficiOS Inc.
 */

#include <stdio.h>

#include "common/assert.h"
#include "common/common.h"

void bt_common_assert_failed(const char *file, int line, const char *func,
		const char *assertion)
{
	fprintf(stderr,
		"%s\n%s%s%s (╯°□°)╯︵ ┻━┻ %s %s%s%s%s:%s%d%s: %s%s()%s: "
		"%sAssertion %s%s`%s`%s%s failed.%s\n",
		bt_common_color_reset(),
		bt_common_color_bold(),
		bt_common_color_bg_yellow(),
		bt_common_color_fg_bright_red(),
		bt_common_color_reset(),
		bt_common_color_bold(),
		bt_common_color_fg_bright_magenta(),
		file,
		bt_common_color_reset(),
		bt_common_color_fg_green(),
		line,
		bt_common_color_reset(),
		bt_common_color_fg_cyan(),
		func,
		bt_common_color_reset(),
		bt_common_color_fg_red(),
		bt_common_color_bold(),
		bt_common_color_fg_bright_red(),
		assertion,
		bt_common_color_reset(),
		bt_common_color_fg_red(),
		bt_common_color_reset());
	bt_common_abort();
}
