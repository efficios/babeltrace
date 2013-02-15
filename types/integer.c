/*
 * integer.c
 *
 * BabelTrace - Integer Type Converter
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
#include <babeltrace/align.h>
#include <babeltrace/format.h>
#include <babeltrace/types.h>
#include <stdint.h>

static
struct bt_definition *_integer_definition_new(struct bt_declaration *declaration,
			       struct definition_scope *parent_scope,
			       GQuark field_name, int index,
			       const char *root_name);
static
void _integer_definition_free(struct bt_definition *definition);

static
void _integer_declaration_free(struct bt_declaration *declaration)
{
	struct declaration_integer *integer_declaration =
		container_of(declaration, struct declaration_integer, p);
	g_free(integer_declaration);
}

struct declaration_integer *
	bt_integer_declaration_new(size_t len, int byte_order,
			 int signedness, size_t alignment, int base,
			 enum ctf_string_encoding encoding,
			 struct ctf_clock *clock)
{
	struct declaration_integer *integer_declaration;

	integer_declaration = g_new(struct declaration_integer, 1);
	integer_declaration->p.id = CTF_TYPE_INTEGER;
	integer_declaration->p.alignment = alignment;
	integer_declaration->p.declaration_free = _integer_declaration_free;
	integer_declaration->p.definition_free = _integer_definition_free;
	integer_declaration->p.definition_new = _integer_definition_new;
	integer_declaration->p.ref = 1;
	integer_declaration->len = len;
	integer_declaration->byte_order = byte_order;
	integer_declaration->signedness = signedness;
	integer_declaration->base = base;
	integer_declaration->encoding = encoding;
	integer_declaration->clock = clock;
	return integer_declaration;
}

static
struct bt_definition *
	_integer_definition_new(struct bt_declaration *declaration,
				struct definition_scope *parent_scope,
				GQuark field_name, int index,
				const char *root_name)
{
	struct declaration_integer *integer_declaration =
		container_of(declaration, struct declaration_integer, p);
	struct definition_integer *integer;
	int ret;

	integer = g_new(struct definition_integer, 1);
	bt_declaration_ref(&integer_declaration->p);
	integer->p.declaration = declaration;
	integer->declaration = integer_declaration;
	integer->p.ref = 1;
	/*
	 * Use INT_MAX order to ensure that all fields of the parent
	 * scope are seen as being prior to this scope.
	 */
	integer->p.index = root_name ? INT_MAX : index;
	integer->p.name = field_name;
	integer->p.path = bt_new_definition_path(parent_scope, field_name,
					root_name);
	integer->p.scope = NULL;
	integer->value._unsigned = 0;
	ret = bt_register_field_definition(field_name, &integer->p,
					parent_scope);
	assert(!ret);
	return &integer->p;
}

static
void _integer_definition_free(struct bt_definition *definition)
{
	struct definition_integer *integer =
		container_of(definition, struct definition_integer, p);

	bt_declaration_unref(integer->p.declaration);
	g_free(integer);
}

enum ctf_string_encoding bt_get_int_encoding(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	return integer_declaration->encoding;
}

int bt_get_int_base(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	return integer_declaration->base;
}

size_t bt_get_int_len(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	return integer_declaration->len;
}

int bt_get_int_byte_order(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	return integer_declaration->byte_order;
}

int bt_get_int_signedness(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	return integer_declaration->signedness;
}

uint64_t bt_get_unsigned_int(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	if (!integer_declaration->signedness) {
		return integer_definition->value._unsigned;
	}
	fprintf(stderr, "[warning] Extracting unsigned value from a signed int (%s)\n",
		g_quark_to_string(field->name));
	return (uint64_t)integer_definition->value._signed;
}

int64_t bt_get_signed_int(const struct bt_definition *field)
{
	struct definition_integer *integer_definition;
	const struct declaration_integer *integer_declaration;

	integer_definition = container_of(field, struct definition_integer, p);
	integer_declaration = integer_definition->declaration;

	if (integer_declaration->signedness) {
		return integer_definition->value._signed;
	}
	fprintf(stderr, "[warning] Extracting signed value from an unsigned int (%s)\n", 
		g_quark_to_string(field->name));
	return (int64_t)integer_definition->value._unsigned;
}
