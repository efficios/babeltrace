/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 */

#include <babeltrace2/babeltrace.h>
#include <stdlib.h>
#include <string.h>

#include "common/version.h"
#include "common/macros.h"

BT_EXPORT
unsigned int bt_version_get_major(void)
{
	return BT_VERSION_MAJOR;
}

BT_EXPORT
unsigned int bt_version_get_minor(void)
{
	return BT_VERSION_MINOR;
}

BT_EXPORT
unsigned int bt_version_get_patch(void)
{
	return BT_VERSION_PATCH;
}

BT_EXPORT
const char *bt_version_get_development_stage(void)
{
	return strlen(BT_VERSION_DEV_STAGE) == 0 ? NULL : BT_VERSION_DEV_STAGE;
}

BT_EXPORT
const char *bt_version_get_vcs_revision_description(void)
{
	return strlen(BT_VERSION_GIT) == 0 ? NULL : BT_VERSION_GIT;
}

BT_EXPORT
const char *bt_version_get_name(void)
{
	return strlen(BT_VERSION_NAME) == 0 ? NULL : BT_VERSION_NAME;
}

BT_EXPORT
const char *bt_version_get_name_description(void)
{
	return strlen(BT_VERSION_DESCRIPTION) == 0 ? NULL :
		BT_VERSION_DESCRIPTION;
}

BT_EXPORT
const char *bt_version_get_extra_name(void)
{
	return strlen(BT_VERSION_EXTRA_NAME) == 0 ? NULL :
		BT_VERSION_EXTRA_NAME;
}

BT_EXPORT
const char *bt_version_get_extra_description(void)
{
	return strlen(BT_VERSION_EXTRA_DESCRIPTION) == 0 ? NULL :
		BT_VERSION_EXTRA_DESCRIPTION;
}

BT_EXPORT
const char *bt_version_get_extra_patch_names(void)
{
	return strlen(BT_VERSION_EXTRA_PATCHES) == 0 ? NULL :
		BT_VERSION_EXTRA_PATCHES;
}
