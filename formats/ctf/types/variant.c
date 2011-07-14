/*
 * Common Trace Format
 *
 * Variant format access functions.
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

#include <babeltrace/ctf/types.h>

int ctf_variant_rw(struct stream_pos *ppos, struct definition *definition)
{
	struct declaration *declaration = definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);

	ctf_align_pos(pos, declaration->alignment);
	return variant_rw(ppos, definition);
}
