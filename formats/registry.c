/*
 * BabelTrace
 *
 * Format Registry
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

#include <glib.h>
#include <errno.h>

static int init_done;
void __attribute__((constructor)) format_init(void);
void __attribute__((destructor)) format_finalize(void);

/*
 * Format registry hash table contains the registered formats. Format
 * registration is typically performed by a format plugin.
 * TODO: support plugin unload (unregistration of formats).
 */
GHashTable *format_registry;

struct format *bt_lookup_format(GQuark qname)
{
	if (!init_done)
		return NULL;
	return g_hash_table_lookup(format_registry,
				   (gconstpointer) (unsigned long) qname)
}

int bt_register_format(const struct format *format)
{
	if (!init_done)
		format_init();

	if (bt_lookup_format(qname))
		return -EEXIST;

	g_hash_table_insert(format_registry,
			    (gconstpointer) (unsigned long) format->name,
			    format);
	return 0;
}

void format_init(void)
{
	format_registry = g_hash_table_new(g_direct_hash, g_direct_equal);
	assert(format_registry);
	init_done = 1;
}

int format_finalize(void)
{
	g_hash_table_destroy(format_registry);
}
