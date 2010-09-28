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

struct type_class {
	GQuark qname;
	void (*read)();
	size_t (*write)();
};

struct type {
	struct type_class *class;
	size_t alignment;	/* type alignment, in bits */
	ssize_t len;		/* type length, in bits. -1 for dynamic size. */
};

/*
 * Type class hash table contains the registered type classes. Type class
 * registration is typically performed by a plugin.
 * TODO: support plugin unload (unregistration of type classes).
 */
GHashTable *type_classes;

struct type_class *ctf_lookup_type_class(GQuark qname)
{
	return g_hash_table_lookup(type_classes,
				   (gconstpointer) (unsigned long) qname)
}

int ctf_register_type_class(const char *name,
			    void (*read)(),
			    void (*write)())
{
	struct type_class tc = g_new(struct type_class, 1);
	GQuark qname = g_quark_from_string(name);

	if (ctf_lookup_type_class(qname))
		return -EEXIST;

	g_hash_table_insert(type_classes,
			    (gconstpointer) (unsigned long) qname,
			    tc);
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
