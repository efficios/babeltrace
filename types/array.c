/*
 * array.c
 *
 * BabelTrace - Array Type Converter
 *
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

#include <babeltrace/compiler.h>
#include <babeltrace/format.h>
#include <babeltrace/types.h>
#include <inttypes.h>

static
struct bt_definition *_array_definition_new(struct bt_declaration *declaration,
			struct definition_scope *parent_scope,
			GQuark field_name, int index, const char *root_name);
static
void _array_definition_free(struct bt_definition *definition);

int bt_array_rw(struct bt_stream_pos *pos, struct bt_definition *definition)
{
	struct definition_array *array_definition =
		container_of(definition, struct definition_array, p);
	const struct declaration_array *array_declaration =
		array_definition->declaration;
	uint64_t i;
	int ret;

	/* No need to align, because the first field will align itself. */
	for (i = 0; i < array_declaration->len; i++) {
		struct bt_definition *field =
			g_ptr_array_index(array_definition->elems, i);
		ret = generic_rw(pos, field);
		if (ret)
			return ret;
	}
	return 0;
}

static
void _array_declaration_free(struct bt_declaration *declaration)
{
	struct declaration_array *array_declaration =
		container_of(declaration, struct declaration_array, p);

	bt_free_declaration_scope(array_declaration->scope);
	bt_declaration_unref(array_declaration->elem);
	g_free(array_declaration);
}

struct declaration_array *
	bt_array_declaration_new(size_t len,
			      struct bt_declaration *elem_declaration,
			      struct declaration_scope *parent_scope)
{
	struct declaration_array *array_declaration;
	struct bt_declaration *declaration;

	array_declaration = g_new(struct declaration_array, 1);
	declaration = &array_declaration->p;
	array_declaration->len = len;
	bt_declaration_ref(elem_declaration);
	array_declaration->elem = elem_declaration;
	array_declaration->scope = bt_new_declaration_scope(parent_scope);
	declaration->id = CTF_TYPE_ARRAY;
	declaration->alignment = elem_declaration->alignment;
	declaration->declaration_free = _array_declaration_free;
	declaration->definition_new = _array_definition_new;
	declaration->definition_free = _array_definition_free;
	declaration->ref = 1;
	return array_declaration;
}

static
struct bt_definition *
	_array_definition_new(struct bt_declaration *declaration,
			      struct definition_scope *parent_scope,
			      GQuark field_name, int index, const char *root_name)
{
	struct declaration_array *array_declaration =
		container_of(declaration, struct declaration_array, p);
	struct definition_array *array;
	int ret;
	int i;

	array = g_new(struct definition_array, 1);
	bt_declaration_ref(&array_declaration->p);
	array->p.declaration = declaration;
	array->declaration = array_declaration;
	array->p.ref = 1;
	/*
	 * Use INT_MAX order to ensure that all fields of the parent
	 * scope are seen as being prior to this scope.
	 */
	array->p.index = root_name ? INT_MAX : index;
	array->p.name = field_name;
	array->p.path = bt_new_definition_path(parent_scope, field_name, root_name);
	array->p.scope = bt_new_definition_scope(parent_scope, field_name, root_name);
	ret = bt_register_field_definition(field_name, &array->p,
					parent_scope);
	assert(!ret);
	array->string = NULL;
	array->elems = NULL;

	if (array_declaration->elem->id == CTF_TYPE_INTEGER) {
		struct declaration_integer *integer_declaration =
			container_of(array_declaration->elem, struct declaration_integer, p);

		if (integer_declaration->encoding == CTF_STRING_UTF8
		      || integer_declaration->encoding == CTF_STRING_ASCII) {

			array->string = g_string_new("");

			if (integer_declaration->len == CHAR_BIT
			    && integer_declaration->p.alignment == CHAR_BIT) {
				return &array->p;
			}
		}
	}

	array->elems = g_ptr_array_sized_new(array_declaration->len);
	g_ptr_array_set_size(array->elems, array_declaration->len);
	for (i = 0; i < array_declaration->len; i++) {
		struct bt_definition **field;
		GString *str;
		GQuark name;

		str = g_string_new("");
		g_string_printf(str, "[%u]", (unsigned int) i);
		name = g_quark_from_string(str->str);
		(void) g_string_free(str, TRUE);

		field = (struct bt_definition **) &g_ptr_array_index(array->elems, i);
		*field = array_declaration->elem->definition_new(array_declaration->elem,
					  array->p.scope,
					  name, i, NULL);
		if (!*field)
			goto error;
	}

	return &array->p;

error:
	for (i--; i >= 0; i--) {
		struct bt_definition *field;

		field = g_ptr_array_index(array->elems, i);
		field->declaration->definition_free(field);
	}
	(void) g_ptr_array_free(array->elems, TRUE);
	bt_free_definition_scope(array->p.scope);
	bt_declaration_unref(array->p.declaration);
	g_free(array);
	return NULL;
}

static
void _array_definition_free(struct bt_definition *definition)
{
	struct definition_array *array =
		container_of(definition, struct definition_array, p);
	uint64_t i;

	if (array->string)
		(void) g_string_free(array->string, TRUE);
	if (array->elems) {
		for (i = 0; i < array->elems->len; i++) {
			struct bt_definition *field;

			field = g_ptr_array_index(array->elems, i);
			field->declaration->definition_free(field);
		}
		(void) g_ptr_array_free(array->elems, TRUE);
	}
	bt_free_definition_scope(array->p.scope);
	bt_declaration_unref(array->p.declaration);
	g_free(array);
}

uint64_t bt_array_len(struct definition_array *array)
{
	if (!array->elems)
		return array->string->len;
	return array->elems->len;
}

struct bt_definition *bt_array_index(struct definition_array *array, uint64_t i)
{
	if (!array->elems)
		return NULL;
	if (i >= array->elems->len)
		return NULL;
	return g_ptr_array_index(array->elems, i);
}

int bt_get_array_len(const struct bt_definition *field)
{
	struct definition_array *array_definition;
	struct declaration_array *array_declaration;

	array_definition = container_of(field, struct definition_array, p);
	array_declaration = array_definition->declaration;

	return array_declaration->len;
}

GString *bt_get_char_array(const struct bt_definition *field)
{
	struct definition_array *array_definition;
	struct declaration_array *array_declaration;
	struct bt_declaration *elem;

	array_definition = container_of(field, struct definition_array, p);
	array_declaration = array_definition->declaration;
	elem = array_declaration->elem;
	if (elem->id == CTF_TYPE_INTEGER) {
		struct declaration_integer *integer_declaration =
			container_of(elem, struct declaration_integer, p);

		if (integer_declaration->encoding == CTF_STRING_UTF8
				|| integer_declaration->encoding == CTF_STRING_ASCII) {

			if (integer_declaration->len == CHAR_BIT
					&& integer_declaration->p.alignment == CHAR_BIT) {

				return array_definition->string;
			}
		}
	}
	fprintf(stderr, "[warning] Extracting string\n");
	return NULL;
}
