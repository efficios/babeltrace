/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_CTF_WRITER_UTILS_H
#define BABELTRACE2_CTF_WRITER_UTILS_H

/* For bt_ctf_bool */
#include <babeltrace2-ctf-writer/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_ctf_bool bt_ctf_identifier_is_valid(const char *identifier);

static inline
int bt_ctf_validate_identifier(const char *identifier)
{
	return bt_ctf_identifier_is_valid(identifier) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_CTF_WRITER_UTILS_H */
