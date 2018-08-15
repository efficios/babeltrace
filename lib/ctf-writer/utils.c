/*
 * utils.c
 *
 * Babeltrace CTF writer - Utilities
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER-UTILS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/ctf-writer/clock-class-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/utils.h>
#include <babeltrace/ref.h>
#include <glib.h>
#include <stdlib.h>

static
const char * const reserved_keywords_str[] = {"align", "callsite",
	"const", "char", "clock", "double", "enum", "env", "event",
	"floating_point", "float", "integer", "int", "long", "short", "signed",
	"stream", "string", "struct", "trace", "typealias", "typedef",
	"unsigned", "variant", "void" "_Bool", "_Complex", "_Imaginary"};

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
	BT_ASSERT(reserved_keywords_set);

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

bt_bool bt_ctf_identifier_is_valid(const char *identifier)
{
	bt_bool is_valid = BT_TRUE;
	char *string = NULL;
	char *save_ptr, *token;

	if (!identifier) {
		BT_LOGV_STR("Invalid parameter: input string is NULL.");
		is_valid = BT_FALSE;
		goto end;
	}

	try_init_reserved_keywords();

	if (identifier[0] == '\0') {
		is_valid = BT_FALSE;
		goto end;
	}

	string = strdup(identifier);
	if (!string) {
		BT_LOGE("strdup() failed.");
		is_valid = BT_FALSE;
		goto end;
	}

	token = strtok_r(string, " ", &save_ptr);
	while (token) {
		if (g_hash_table_lookup_extended(reserved_keywords_set,
			GINT_TO_POINTER(g_quark_from_string(token)),
			NULL, NULL)) {
			is_valid = BT_FALSE;
			goto end;
		}

		token = strtok_r(NULL, " ", &save_ptr);
	}
end:
	free(string);
	return is_valid;
}
