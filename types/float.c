/*
 * float.c
 *
 * BabelTrace - Float Type Converter
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
#include <babeltrace/endian.h>

static
struct bt_definition *_float_definition_new(struct bt_declaration *declaration,
				   struct definition_scope *parent_scope,
				   GQuark field_name, int index,
				   const char *root_name);
static
void _float_definition_free(struct bt_definition *definition);

static
void _float_declaration_free(struct bt_declaration *declaration)
{
	struct declaration_float *float_declaration =
		container_of(declaration, struct declaration_float, p);

	bt_declaration_unref(&float_declaration->exp->p);
	bt_declaration_unref(&float_declaration->mantissa->p);
	bt_declaration_unref(&float_declaration->sign->p);
	g_free(float_declaration);
}

struct declaration_float *
	bt_float_declaration_new(size_t mantissa_len,
		       size_t exp_len, int byte_order, size_t alignment)
{
	struct declaration_float *float_declaration;
	struct bt_declaration *declaration;

	float_declaration = g_new(struct declaration_float, 1);
	declaration = &float_declaration->p;
	declaration->id = CTF_TYPE_FLOAT;
	declaration->alignment = alignment;
	declaration->declaration_free = _float_declaration_free;
	declaration->definition_new = _float_definition_new;
	declaration->definition_free = _float_definition_free;
	declaration->ref = 1;
	float_declaration->byte_order = byte_order;

	float_declaration->sign = bt_integer_declaration_new(1,
						byte_order, false, 1, 2,
						CTF_STRING_NONE, NULL);
	float_declaration->mantissa = bt_integer_declaration_new(mantissa_len - 1,
						byte_order, false, 1, 10,
						CTF_STRING_NONE, NULL);
	float_declaration->exp = bt_integer_declaration_new(exp_len,
						byte_order, true, 1, 10,
						CTF_STRING_NONE, NULL);
	return float_declaration;
}

static
struct bt_definition *
	_float_definition_new(struct bt_declaration *declaration,
			      struct definition_scope *parent_scope,
			      GQuark field_name, int index,
			      const char *root_name)
{
	struct declaration_float *float_declaration =
		container_of(declaration, struct declaration_float, p);
	struct definition_float *_float;
	struct bt_definition *tmp;

	_float = g_new(struct definition_float, 1);
	bt_declaration_ref(&float_declaration->p);
	_float->p.declaration = declaration;
	_float->declaration = float_declaration;
	_float->p.scope = bt_new_definition_scope(parent_scope, field_name, root_name);
	_float->p.path = bt_new_definition_path(parent_scope, field_name, root_name);
	if (float_declaration->byte_order == LITTLE_ENDIAN) {
		tmp = float_declaration->mantissa->p.definition_new(&float_declaration->mantissa->p,
			_float->p.scope, g_quark_from_static_string("mantissa"), 0, NULL);
		_float->mantissa = container_of(tmp, struct definition_integer, p);
		tmp = float_declaration->exp->p.definition_new(&float_declaration->exp->p,
			_float->p.scope, g_quark_from_static_string("exp"), 1, NULL);
		_float->exp = container_of(tmp, struct definition_integer, p);
		tmp = float_declaration->sign->p.definition_new(&float_declaration->sign->p,
			_float->p.scope, g_quark_from_static_string("sign"), 2, NULL);
		_float->sign = container_of(tmp, struct definition_integer, p);
	} else {
		tmp = float_declaration->sign->p.definition_new(&float_declaration->sign->p,
			_float->p.scope, g_quark_from_static_string("sign"), 0, NULL);
		_float->sign = container_of(tmp, struct definition_integer, p);
		tmp = float_declaration->exp->p.definition_new(&float_declaration->exp->p,
			_float->p.scope, g_quark_from_static_string("exp"), 1, NULL);
		_float->exp = container_of(tmp, struct definition_integer, p);
		tmp = float_declaration->mantissa->p.definition_new(&float_declaration->mantissa->p,
			_float->p.scope, g_quark_from_static_string("mantissa"), 2, NULL);
		_float->mantissa = container_of(tmp, struct definition_integer, p);
	}
	_float->p.ref = 1;
	/*
	 * Use INT_MAX order to ensure that all fields of the parent
	 * scope are seen as being prior to this scope.
	 */
	_float->p.index = root_name ? INT_MAX : index;
	_float->p.name = field_name;
	_float->value = 0.0;
	if (parent_scope) {
		int ret;

		ret = bt_register_field_definition(field_name, &_float->p,
						parent_scope);
		assert(!ret);
	}
	return &_float->p;
}

static
void _float_definition_free(struct bt_definition *definition)
{
	struct definition_float *_float =
		container_of(definition, struct definition_float, p);

	bt_definition_unref(&_float->sign->p);
	bt_definition_unref(&_float->exp->p);
	bt_definition_unref(&_float->mantissa->p);
	bt_free_definition_scope(_float->p.scope);
	bt_declaration_unref(_float->p.declaration);
	g_free(_float);
}
