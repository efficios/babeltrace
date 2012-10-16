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

/*
 * Format registry hash table contains the registered formats. Format
 * registration is typically performed by a format plugin.
 */
static GHashTable *format_registry;
static int format_refcount;
static int init_done;

static __attribute__((constructor)) void format_init(void);

static
void format_refcount_inc(void)
{
	format_refcount++;
}

static
void format_cleanup(void)
{
	if (format_registry)
		g_hash_table_destroy(format_registry);
}

static
void format_refcount_dec(void)
{
	if (!--format_refcount)
		format_cleanup();
}

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

	assert(fp);

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
	if (!format)
		return -EINVAL;

	if (!init_done)
		format_init();

	if (bt_lookup_format(format->name))
		return -EEXIST;

	format_refcount_inc();
	g_hash_table_insert(format_registry,
			    (gpointer) (unsigned long) format->name,
			    format);
	return 0;
}

void bt_unregister_format(struct format *format)
{
	assert(bt_lookup_format(format->name));
	g_hash_table_remove(format_registry,
			    (gpointer) (unsigned long) format->name);
	format_refcount_dec();
}

/*
 * We cannot assume that the constructor and destructor order will be
 * right: another library might be loaded before us, and initialize us
 * from bt_register_format(). This is why we use a reference count to
 * handle cleanup of this module. The format_finalize destructor
 * refcount decrement matches format_init refcount increment.
 */
static __attribute__((constructor))
void format_init(void)
{
	if (init_done)
		return;
	format_refcount_inc();
	format_registry = g_hash_table_new(g_direct_hash, g_direct_equal);
	assert(format_registry);
	init_done = 1;
}

static __attribute__((destructor))
void format_finalize(void)
{
	format_refcount_dec();
}
