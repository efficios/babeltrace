/*
 * Common Trace Format
 *
 * Sequence format access functions.
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

void ctf_sequence_begin(struct stream_pos *pos,
			const struct type_sequence *sequence_type)
{
	align_pos(pos, sequence_type->p.alignment);
}

void ctf_sequence_end(struct stream_pos *pos,
		      const struct type_sequence *sequence_type)
{
}
