/*
 * python-complements.c
 *
 * Babeltrace Python module complements, required for Python bindings
 *
 * Copyright 2012 EfficiOS Inc.
 *
 * Author: Danny Serres <danny.serres@efficios.com>
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
 */

#include "python-complements.h"
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/iterator.h>
#include <glib.h>

/* FILE functions
   ----------------------------------------------------
*/

FILE *_bt_file_open(char *file_path, char *mode)
{
	FILE *fp = stdout;
	if (file_path != NULL)
		fp = fopen(file_path, mode);
	return fp;
}

void _bt_file_close(FILE *fp)
{
	if (fp != NULL)
		fclose(fp);
}


/* List-related functions
   ----------------------------------------------------
*/

/* ctf-field-list */
struct bt_definition **_bt_python_field_listcaller(
		const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,
		unsigned int *len)
{
	struct bt_definition **list;
	int ret;

	ret = bt_ctf_get_field_list(ctf_event, scope,
		(const struct bt_definition * const **)&list, len);

	if (ret < 0)	/* For python to know an error occured */
		list = NULL;

	return list;
}

struct bt_definition *_bt_python_field_one_from_list(
		struct bt_definition **list, int index)
{
	return list[index];
}

/* event_decl_list */
struct bt_ctf_event_decl **_bt_python_event_decl_listcaller(
		int handle_id,
		struct bt_context *ctx,
		unsigned int *len)
{
	struct bt_ctf_event_decl **list;
	int ret;

	ret = bt_ctf_get_event_decl_list(handle_id, ctx,
		(struct bt_ctf_event_decl * const **)&list, len);

	if (ret < 0)	/* For python to know an error occured */
		list = NULL;

	return list;
}

struct bt_ctf_event_decl *_bt_python_decl_one_from_list(
		struct bt_ctf_event_decl **list, int index)
{
	return list[index];
}

/* decl_fields */
struct bt_ctf_field_decl **_by_python_field_decl_listcaller(
		struct bt_ctf_event_decl *event_decl,
		enum bt_ctf_scope scope,
		unsigned int *len)
{
	struct bt_ctf_field_decl **list;
	int ret;

	ret = bt_ctf_get_decl_fields(event_decl, scope,
		(const struct bt_ctf_field_decl * const **)&list, len);

	if (ret < 0)	/* For python to know an error occured */
		list = NULL;

	return list;
}

struct bt_ctf_field_decl *_bt_python_field_decl_one_from_list(
		struct bt_ctf_field_decl **list, int index)
{
	return list[index];
}

struct definition_array *_bt_python_get_array_from_def(
		struct bt_definition *field)
{
	const struct bt_declaration *array_decl;
	struct definition_array *array = NULL;

	if (!field) {
		goto end;
	}

	array_decl = bt_ctf_get_decl_from_def(field);
	if (bt_ctf_field_type(array_decl) == CTF_TYPE_ARRAY) {
		array = container_of(field, struct definition_array, p);
	}
end:
	return array;
}

struct bt_declaration *_bt_python_get_array_element_declaration(
		struct bt_declaration *field)
{
	struct declaration_array *array_decl;
	struct bt_declaration *ret = NULL;

	if (!field) {
		goto end;
	}

	array_decl = container_of(field, struct declaration_array, p);
	ret = array_decl->elem;
end:
	return ret;
}

struct bt_declaration *_bt_python_get_sequence_element_declaration(
		struct bt_declaration *field)
{
	struct declaration_sequence *sequence_decl;
	struct bt_declaration *ret = NULL;

	if (!field) {
		goto end;
	}

	sequence_decl = container_of(field, struct declaration_sequence, p);
	ret = sequence_decl->elem;
end:
	return ret;
}

const char *_bt_python_get_array_string(struct bt_definition *field)
{
	struct definition_array *array;
	const char *ret = NULL;

	if (!field) {
		goto end;
	}

	array = container_of(field, struct definition_array, p);
	ret = array->string->str;
end:
	return ret;
}

const char *_bt_python_get_sequence_string(struct bt_definition *field)
{
	struct definition_sequence *sequence;
	const char *ret = NULL;

	if (!field) {
		goto end;
	}

	sequence = container_of(field, struct definition_sequence, p);
	ret = sequence->string->str;
end:
	return ret;
}

struct definition_sequence *_bt_python_get_sequence_from_def(
		struct bt_definition *field)
{
	if (field && bt_ctf_field_type(
		bt_ctf_get_decl_from_def(field)) == CTF_TYPE_SEQUENCE) {
		return container_of(field, struct definition_sequence, p);
	}

	return NULL;
}

int _bt_python_field_integer_get_signedness(const struct bt_ctf_field *field)
{
	int ret;

	if (!field || field->type->declaration->id != CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	const struct bt_ctf_field_type_integer *type = container_of(field->type,
		const struct bt_ctf_field_type_integer, parent);
	ret = type->declaration.signedness;
end:
	return ret;
}

enum ctf_type_id _bt_python_get_field_type(const struct bt_ctf_field *field)
{
	enum ctf_type_id type_id = CTF_TYPE_UNKNOWN;

	if (!field) {
		goto end;
	}

	type_id = field->type->declaration->id;
end:
	return type_id;
}

/*
 * Python 3.5 changes the StopIteration exception clearing behaviour which
 * erroneously marks swig clean-up function as having failed. This explicit
 * allocation function is intended as a work-around so SWIG doesn't manage
 * the lifetime of a "temporary" object by itself.
 */
struct bt_iter_pos *_bt_python_create_iter_pos(void)
{
	return g_new0(struct bt_iter_pos, 1);
}
