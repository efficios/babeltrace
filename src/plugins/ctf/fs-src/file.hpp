/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CTF_FS_FILE_H
#define CTF_FS_FILE_H

#include <stdio.h>
#include <glib.h>
#include "common/macros.h"
#include "fs.hpp"

BT_HIDDEN
void ctf_fs_file_destroy(struct ctf_fs_file *file);

BT_HIDDEN
struct ctf_fs_file *ctf_fs_file_create(bt_logging_level log_level, bt_self_component *self_comp);

BT_HIDDEN
int ctf_fs_file_open(struct ctf_fs_file *file, const char *mode);

#endif /* CTF_FS_FILE_H */
