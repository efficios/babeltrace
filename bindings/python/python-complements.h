/*
 * python-complements.h
 *
 * Babeltrace Python module complements header, required for Python bindings
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

#include <stdio.h>
#include <glib.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/iterator-internal.h>
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/ctf-writer/event-fields.h>

/* File */
FILE *_bt_file_open(char *file_path, char *mode);
void _bt_file_close(FILE *fp);

/* ctf-field-list */
struct bt_definition **_bt_python_field_listcaller(
		const struct bt_ctf_event *ctf_event,
		const struct bt_definition *scope,
		unsigned int *len);
struct bt_definition *_bt_python_field_one_from_list(
		struct bt_definition **list, int index);

/* event_decl_list */
struct bt_ctf_event_decl **_bt_python_event_decl_listcaller(
		int handle_id,
		struct bt_context *ctx,
		unsigned int *len);
struct bt_ctf_event_decl *_bt_python_decl_one_from_list(
		struct bt_ctf_event_decl **list, int index);

/* decl_fields */
struct bt_ctf_field_decl **_by_python_field_decl_listcaller(
		struct bt_ctf_event_decl *event_decl,
		enum bt_ctf_scope scope,
		unsigned int *len);
struct bt_ctf_field_decl *_bt_python_field_decl_one_from_list(
		struct bt_ctf_field_decl **list, int index);

/* definitions */
struct definition_array *_bt_python_get_array_from_def(
		struct bt_definition *field);
struct definition_sequence *_bt_python_get_sequence_from_def(
		struct bt_definition *field);
struct bt_declaration *_bt_python_get_array_element_declaration(
		struct bt_declaration *field);
struct bt_declaration *_bt_python_get_sequence_element_declaration(
		struct bt_declaration *field);
const char *_bt_python_get_array_string(struct bt_definition *field);
const char *_bt_python_get_sequence_string(struct bt_definition *field);

/* ctf writer */
int _bt_python_field_integer_get_signedness(const struct bt_ctf_field *field);
enum ctf_type_id _bt_python_get_field_type(const struct bt_ctf_field *field);
