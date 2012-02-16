/*
 * BabelTrace
 *
 * Format Registry
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
 */

#include <babeltrace/format.h>
#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

struct walk_data {
	FILE *fp;
	int iter;
};

static int init_done;
void __attribute__((constructor)) format_init(void);
void __attribute__((destructor)) format_finalize(void);

/*
 * Format registry hash table contains the registered formats. Format
 * registration is typically performed by a format plugin.
 * TODO: support plugin unload (unregistration of formats).
 */
GHashTable *format_registry;

struct format *bt_lookup_format(bt_intern_str name)
{
	if (!init_done)
		return NULL;
	return g_hash_table_lookup(format_registry,
				   (gconstpointer) (unsigned long) name);
}

static void show_format(gpointer key, gpointer value, gpointer user_data)
{
	struct walk_data *data = user_data;

	fprintf(data->fp, "%s%s", data->iter ? ", " : "",
		g_quark_to_string((GQuark) (unsigned long) key));
	data->iter++;
}

void bt_fprintf_format_list(FILE *fp)
{
	struct walk_data data;

	data.fp = fp;
	data.iter = 0;

	fprintf(fp, "Formats available: ");
	if (!init_done)
		return;
	g_hash_table_foreach(format_registry, show_format, &data);
	if (data.iter == 0)
		fprintf(fp, "<none>");
	fprintf(fp, ".\n");
}

int bt_register_format(struct format *format)
{
	if (!init_done)
		format_init();

	if (bt_lookup_format(format->name))
		return -EEXIST;

	g_hash_table_insert(format_registry,
			    (gpointer) (unsigned long) format->name,
			    format);
	return 0;
}

void format_init(void)
{
	if (init_done)
		return;
	format_registry = g_hash_table_new(g_direct_hash, g_direct_equal);
	assert(format_registry);
	init_done = 1;
}

void format_finalize(void)
{
	g_hash_table_destroy(format_registry);
}
