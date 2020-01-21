/*
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace2/babeltrace.h>
#include <stdlib.h>
#include <string.h>

#include "common/version.h"

unsigned int bt_version_get_major(void)
{
	return BT_VERSION_MAJOR;
}

unsigned int bt_version_get_minor(void)
{
	return BT_VERSION_MINOR;
}

unsigned int bt_version_get_patch(void)
{
	return BT_VERSION_PATCH;
}

const char *bt_version_get_development_stage(void)
{
	return strlen(BT_VERSION_DEV_STAGE) == 0 ? NULL : BT_VERSION_DEV_STAGE;
}

const char *bt_version_get_vcs_revision_description(void)
{
	return strlen(BT_VERSION_GIT) == 0 ? NULL : BT_VERSION_GIT;
}

const char *bt_version_get_name(void)
{
	return strlen(BT_VERSION_NAME) == 0 ? NULL : BT_VERSION_NAME;
}

const char *bt_version_get_name_description(void)
{
	return strlen(BT_VERSION_DESCRIPTION) == 0 ? NULL :
		BT_VERSION_DESCRIPTION;
}

const char *bt_version_get_extra_name(void)
{
	return strlen(BT_VERSION_EXTRA_NAME) == 0 ? NULL :
		BT_VERSION_EXTRA_NAME;
}

const char *bt_version_get_extra_description(void)
{
	return strlen(BT_VERSION_EXTRA_DESCRIPTION) == 0 ? NULL :
		BT_VERSION_EXTRA_DESCRIPTION;
}

const char *bt_version_get_extra_patch_names(void)
{
	return strlen(BT_VERSION_EXTRA_PATCHES) == 0 ? NULL :
		BT_VERSION_EXTRA_PATCHES;
}
