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
		const struct bt_definition *scope)
{
	struct bt_definition **list;
	unsigned int count;
	int ret;

	ret = bt_ctf_get_field_list(ctf_event, scope,
		(const struct bt_definition * const **)&list, &count);

	if (ret < 0)	/* For python to know an error occured */
		list = NULL;
	else		/* For python to know the end is reached */
		list[count] = NULL;

	return list;
}

struct bt_definition *_bt_python_field_one_from_list(
		struct bt_definition **list, int index)
{
	return list[index];
}

/* event_decl_list */
struct bt_ctf_event_decl **_bt_python_event_decl_listcaller(
		int handle_id, struct bt_context *ctx)
{
	struct bt_ctf_event_decl **list;
	unsigned int count;
	int ret;

	ret = bt_ctf_get_event_decl_list(handle_id, ctx,
		(struct bt_ctf_event_decl * const **)&list, &count);

	if (ret < 0)	/* For python to know an error occured */
		list = NULL;
	else		/* For python to know the end is reached */
		list[count] = NULL;

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
		enum bt_ctf_scope scope)
{
	struct bt_ctf_field_decl **list;
	unsigned int count;
	int ret;

	ret = bt_ctf_get_decl_fields(event_decl, scope,
		(const struct bt_ctf_field_decl * const **)&list, &count);

	if (ret < 0)	/* For python to know an error occured */
		list = NULL;
	else		/* For python to know the end is reached */
		list[count] = NULL;

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

struct definition_sequence *_bt_python_get_sequence_from_def(
		struct bt_definition *field)
{
	if (field && bt_ctf_field_type(
		bt_ctf_get_decl_from_def(field)) == CTF_TYPE_SEQUENCE) {
		return container_of(field, struct definition_sequence, p);
	}

	return NULL;
}
