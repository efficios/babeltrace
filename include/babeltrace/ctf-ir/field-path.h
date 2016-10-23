#ifndef BABELTRACE_CTF_IR_FIELD_PATH
#define BABELTRACE_CTF_IR_FIELD_PATH

/*
 * BabelTrace - CTF IR: Field path
 *
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/ctf-ir/field-types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_field_path;

/*
 * bt_ctf_field_path_get_root_scope: get the root node of a field path.
 *
 * Get the field path's root node.
 *
 * @param field_path Field path.
 *
 * Returns the root node of a field path, or BT_CTF_SCOPE_UNKNOWN on error.
 */
BT_HIDDEN
enum bt_ctf_scope bt_ctf_field_path_get_root_scope(
		const struct bt_ctf_field_path *field_path);

/*
 * bt_ctf_field_path_get_index_count: get the number of indexes of a field path.
 *
 * Get the number of indexes of a field path.
 *
 * @param field_path Field path.
 *
 * Returns the field path's index count, or a negative value on error.
 */
BT_HIDDEN
int bt_ctf_field_path_get_index_count(
		const struct bt_ctf_field_path *field_path);

/*
 * bt_ctf_field_path_get_index: get the field path's index at a specific index.
 *
 * Get the field path's index at a specific index.
 *
 * @param field_path Field path.
 * @param index Index.
 *
 * Returns a field path index, or INT_MIN on error.
 */
BT_HIDDEN
int bt_ctf_field_path_get_index(
		const struct bt_ctf_field_path *field_path,
		int index);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_FIELD_PATH */
