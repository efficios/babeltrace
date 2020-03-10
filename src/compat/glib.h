/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2015 Michael Jeanson <mjeanson@efficios.com>
 */

#ifndef _BABELTRACE_COMPAT_GLIB_H
#define _BABELTRACE_COMPAT_GLIB_H

#include <glib.h>

#if GLIB_CHECK_VERSION(2,31,8)

static inline gboolean
bt_g_hash_table_contains(GHashTable *hash_table, gconstpointer key)
{
	return g_hash_table_contains(hash_table, key);
}

#else

static inline gboolean
bt_g_hash_table_contains(GHashTable *hash_table, gconstpointer key)
{
	gpointer orig_key;
	gpointer value;

	return g_hash_table_lookup_extended(hash_table, key, &orig_key,
		&value);
}

#endif


#if GLIB_CHECK_VERSION(2,29,16)

static inline GPtrArray *
bt_g_ptr_array_new_full(guint reserved_size,
		GDestroyNotify element_free_func)
{
	return g_ptr_array_new_full(reserved_size, element_free_func);
}

#else

static inline GPtrArray *
bt_g_ptr_array_new_full(guint reserved_size,
		GDestroyNotify element_free_func)
{
       GPtrArray *array;

       array = g_ptr_array_sized_new(reserved_size);
       if (!array) {
	       goto end;
       }
       g_ptr_array_set_free_func(array, element_free_func);
end:
       return array;
}
#endif

#endif /* _BABELTRACE_COMPAT_GLIB_H */
