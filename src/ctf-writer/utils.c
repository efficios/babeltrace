/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF writer - Utilities
 */

#define BT_LOG_TAG "CTF-WRITER/UTILS"
#include "logging.h"

#include <glib.h>
#include <stdlib.h>

#include <babeltrace2-ctf-writer/utils.h>
#include <babeltrace2-ctf-writer/object.h>

#include "common/assert.h"

#include "clock-class.h"
#include "field-types.h"

static
const char * const reserved_keywords_str[] = {"align", "callsite",
	"const", "char", "clock", "double", "enum", "env", "event",
	"floating_point", "float", "integer", "int", "long", "short", "signed",
	"stream", "string", "struct", "trace", "typealias", "typedef",
	"unsigned", "variant", "void", "_Bool", "_Complex", "_Imaginary"};

static GHashTable *reserved_keywords_set;
static int init_done;

static
void try_init_reserved_keywords(void)
{
	size_t i;
	const size_t reserved_keywords_count =
		sizeof(reserved_keywords_str) / sizeof(char *);

	if (reserved_keywords_set) {
		return;
	}

	reserved_keywords_set = g_hash_table_new(g_direct_hash, g_direct_equal);
	BT_ASSERT_DBG(reserved_keywords_set);

	for (i = 0; i < reserved_keywords_count; i++) {
		gpointer quark = GINT_TO_POINTER(g_quark_from_string(
			reserved_keywords_str[i]));

		g_hash_table_insert(reserved_keywords_set, quark, quark);
	}

	init_done = 1;
}

static __attribute__((destructor))
void trace_finalize(void)
{
	if (reserved_keywords_set) {
		g_hash_table_destroy(reserved_keywords_set);
	}
}

BT_EXPORT
bt_ctf_bool bt_ctf_identifier_is_valid(const char *identifier)
{
	bt_ctf_bool is_valid = BT_CTF_TRUE;
	char *string = NULL;
	char *save_ptr, *token;

	if (!identifier) {
		BT_LOGT_STR("Invalid parameter: input string is NULL.");
		is_valid = BT_CTF_FALSE;
		goto end;
	}

	try_init_reserved_keywords();

	if (identifier[0] == '\0') {
		is_valid = BT_CTF_FALSE;
		goto end;
	}

	string = strdup(identifier);
	if (!string) {
		BT_LOGE("strdup() failed.");
		is_valid = BT_CTF_FALSE;
		goto end;
	}

	token = strtok_r(string, " ", &save_ptr);
	while (token) {
		if (g_hash_table_lookup_extended(reserved_keywords_set,
			GINT_TO_POINTER(g_quark_from_string(token)),
			NULL, NULL)) {
			is_valid = BT_CTF_FALSE;
			goto end;
		}

		token = strtok_r(NULL, " ", &save_ptr);
	}
end:
	free(string);
	return is_valid;
}
