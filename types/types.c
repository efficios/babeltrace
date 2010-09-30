/*
 * BabelTrace - Converter
 *
 * Types registry.
 *
 * Copyright (c) 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
					     NULL, g_free);
	if (!type_classes)
		return -ENOMEM;
	return 0;
}

int ctf_finalize_types(void)
{
	g_hash_table_destroy(type_classes);
}
