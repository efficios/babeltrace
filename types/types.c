/*
 * types.c
 *
 * BabelTrace - Converter
 *
 * Types registry.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <ctf/ctf-types.h>
#include <glib.h>
#include <errno.h>

/*
 * Type hash table contains the registered types. Type registration is typically
 * performed by a type plugin.
 * TODO: support plugin unload (unregistration of types).
 */
GHashTable *types;

struct type_class *ctf_lookup_type(GQuark qname)
{
	return g_hash_table_lookup(type_classes,
				   (gconstpointer) (unsigned long) qname)
}

static void free_type(struct type_class *type_class)
{
	type_class->free(type_class);
}

int ctf_register_type(struct type_class *type_class)
{
	if (ctf_lookup_type_class(type_class->name))
		return -EEXIST;

	g_hash_table_insert(type_classes,
			    (gconstpointer) (unsigned long) type_class->name,
			    type_class);
	return 0;
}

int ctf_init_types(void)
{
	type_classes = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					     NULL, free_type);
	if (!type_classes)
		return -ENOMEM;
	return 0;
}

int ctf_finalize_types(void)
{
	g_hash_table_destroy(type_classes);
}
