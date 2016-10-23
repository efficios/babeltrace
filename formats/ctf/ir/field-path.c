/*
 * field-path.c
 *
 * Babeltrace CTF IR - Field path
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-ir/field-path.h>
#include <limits.h>
#include <glib.h>

static
void field_path_destroy(struct bt_object *obj)
{
	struct bt_ctf_field_path *field_path = (struct bt_ctf_field_path *) obj;

	if (!field_path) {
		return;
	}

	if (field_path->indexes) {
		g_array_free(field_path->indexes, TRUE);
	}
	g_free(field_path);
}

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_path_create(void)
{
	struct bt_ctf_field_path *field_path = NULL;

	field_path = g_new0(struct bt_ctf_field_path, 1);
	if (!field_path) {
		goto error;
	}

	bt_object_init(field_path, field_path_destroy);
	field_path->root = BT_CTF_SCOPE_UNKNOWN;
	field_path->indexes = g_array_new(TRUE, FALSE, sizeof(int));
	if (!field_path->indexes) {
		goto error;
	}

	return field_path;

error:
	BT_PUT(field_path);
	return NULL;
}

BT_HIDDEN
void bt_ctf_field_path_clear(struct bt_ctf_field_path *field_path)
{
	if (field_path->indexes->len > 0) {
		g_array_remove_range(field_path->indexes, 0,
			field_path->indexes->len);
	}
}

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_path_copy(
		struct bt_ctf_field_path *path)
{
	struct bt_ctf_field_path *new_path = bt_ctf_field_path_create();

	if (!new_path) {
		goto end;
	}

	new_path->root = path->root;
	g_array_insert_vals(new_path->indexes, 0,
		path->indexes->data, path->indexes->len);
end:
	return new_path;
}

BT_HIDDEN
enum bt_ctf_scope bt_ctf_field_path_get_root_scope(
		const struct bt_ctf_field_path *field_path)
{
	enum bt_ctf_scope scope = BT_CTF_SCOPE_UNKNOWN;

	if (!field_path) {
		goto end;
	}

	scope = field_path->root;

end:
	return scope;
}

BT_HIDDEN
int bt_ctf_field_path_get_index_count(
		const struct bt_ctf_field_path *field_path)
{
	int ret = -1;

	if (!field_path) {
		goto end;
	}

	ret = field_path->indexes->len;

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_path_get_index(const struct bt_ctf_field_path *field_path,
		int index)
{
	int ret = INT_MIN;

	if (!field_path || index < 0) {
		goto end;
	}

	if (index >= field_path->indexes->len) {
		goto end;
	}

	ret = g_array_index(field_path->indexes, int, index);

end:
	return ret;
}
