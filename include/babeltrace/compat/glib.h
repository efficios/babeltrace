#ifndef _BABELTRACE_COMPAT_GLIB_H
#define _BABELTRACE_COMPAT_GLIB_H

/*
 * babeltrace/compat/glib.h
 *
 * Copyright (C) 2015 Michael Jeanson <mjeanson@efficios.com>
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <glib.h>

#if GLIB_CHECK_VERSION(2,31,8)

static inline gboolean
babeltrace_g_hash_table_contains(GHashTable *hash_table, gconstpointer key)
{
	return g_hash_table_contains(hash_table, key);
}

#else

static inline gboolean
babeltrace_g_hash_table_contains(GHashTable *hash_table, gconstpointer key)
{
       const char *value;

       value = g_hash_table_lookup(hash_table, key);
       if (value == NULL) {
               return FALSE;
       }

       return TRUE;
}

#endif


#if GLIB_CHECK_VERSION(2,29,16)

static inline GPtrArray *
babeltrace_g_ptr_array_new_full(guint reserved_size,
		GDestroyNotify element_free_func)
{
	return g_ptr_array_new_full(reserved_size, element_free_func);
}

#else

static inline GPtrArray *
babeltrace_g_ptr_array_new_full(guint reserved_size,
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
