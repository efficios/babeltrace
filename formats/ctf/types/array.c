/*
 * Common Trace Format
 *
 * Array format access functions.
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

#include <babeltrace/ctf/types.h>

void ctf_array_begin(struct stream_pos *pos,
		     const struct declaration_array *array_declaration)
{
	/* No need to align, because the first field will align itself. */
}

void ctf_array_end(struct stream_pos *pos,
		   const struct declaration_array *array_declaration)
{
}
