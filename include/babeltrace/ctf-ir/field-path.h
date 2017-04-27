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

#include <stdint.h>
#include <babeltrace/ctf-ir/field-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctfirfieldpath CTF IR field path
@ingroup ctfir
@brief CTF IR field path.

@code
#include <babeltrace/ctf-ir/field-path.h>
@endcode

A CTF IR <strong><em>field path</em></strong> represents an absolute
path to a field in the hierarchy of a
\link ctfirtraceclass CTF IR trace class\endlink, of a
\link ctfirstreamclass CTF IR stream class\endlink, or of a
\link ctfireventclass CTF IR event class\endlink.

As a reminder, here's the structure of a CTF packet:

@imgpacketstructure

Sequence and variant \link ctfirfieldtypes CTF IR field types\endlink
can return a field path to resp. their length field and tag field
with resp. bt_ctf_field_type_sequence_get_length_field_path() and
bt_ctf_field_type_variant_get_tag_field_path().

A field path has a <em>root scope</em> which indicates from which of the
six CTF scopes to begin. It also has a list of structure field <em>path
indexes</em> which indicate the path to take to reach the destination
field. A path index set to -1 means that you need to continue the lookup
within the current element of an array or sequence field.

As with any Babeltrace object, CTF IR field path objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

@file
@brief CTF IR field path type and functions.
@sa ctfirfieldpath

@addtogroup ctfirfieldpath
@{
*/

/**
@struct bt_ctf_field_path
@brief A CTF IR field path.
@sa ctfirfieldpath
*/
struct bt_ctf_field_path;

/**
@brief	Returns the root scope of the CTF IR field path \p field_path.

@param[in] field_path	Field path of which to get the root scope.
@returns		Root scope of \p field_path, or
			#BT_CTF_SCOPE_UNKNOWN on error.

@prenotnull{field_path}
@postrefcountsame{field_path}
*/
extern enum bt_ctf_scope bt_ctf_field_path_get_root_scope(
		const struct bt_ctf_field_path *field_path);

/**
@brief	Returns the number of path indexes contained in the CTF IR field
	path \p field_path.

@param[in] field_path	Field path of which to get the number of
			path indexes.
@returns		Number of path indexes contained in
			\p field_path, or a negative value on error.

@prenotnull{field_path}
@postrefcountsame{field_path}
*/
extern int64_t bt_ctf_field_path_get_index_count(
		const struct bt_ctf_field_path *field_path);

/**
@brief	Returns the path index contained in the CTF IR field
	path \p field_path at index \p index.

@param[in] field_path	Field path of which to get the path index
			at index \p index.
@param[in] index	Index of path index to get.
@returns		Path index of \p field_path at index \p index,
			or \c INT_MIN on error.

@prenotnull{field_path}
@pre \p index is lesser than the number of path indexes contained in the
	field path \p field_path (see
	bt_ctf_field_path_get_index_count()).
@postrefcountsame{field_path}
*/
extern int bt_ctf_field_path_get_index(
		const struct bt_ctf_field_path *field_path, uint64_t index);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_FIELD_PATH */
