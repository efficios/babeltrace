/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H
#define BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H

#include <dirent.h>
#include <glib.h>
#include <sys/types.h>

#include <babeltrace2-ctf-writer/trace.h>
#include <babeltrace2-ctf-writer/writer.h>

#include "common/macros.h"

#include "object.h"

struct metadata_context {
	GString *string;
	GString *field_name;
	unsigned int current_indentation_level;
};

struct bt_ctf_writer {
	struct bt_ctf_object base;
	int frozen; /* Protects attributes that can't be changed mid-trace */
	struct bt_ctf_trace *trace;
	GString *path;
	int metadata_fd;
};

enum field_type_alias {
	FIELD_TYPE_ALIAS_UINT5_T = 0,
	FIELD_TYPE_ALIAS_UINT8_T,
	FIELD_TYPE_ALIAS_UINT16_T,
	FIELD_TYPE_ALIAS_UINT27_T,
	FIELD_TYPE_ALIAS_UINT32_T,
	FIELD_TYPE_ALIAS_UINT64_T,
	NR_FIELD_TYPE_ALIAS,
};

struct bt_ctf_field_type *get_field_type(enum field_type_alias alias);

const char *bt_ctf_get_byte_order_string(enum bt_ctf_byte_order byte_order);

void bt_ctf_writer_freeze(struct bt_ctf_writer *writer);

#endif /* BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H */
