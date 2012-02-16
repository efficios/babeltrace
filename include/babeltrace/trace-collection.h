#ifndef _BABELTRACE_TRACE_COLLECTION_H
#define _BABELTRACE_TRACE_COLLECTION_H
/*
 * BabelTrace lib
 *
 * trace collection header
 *
 * Copyright 2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Author: Yannick Brosseau <yannick.brosseau@gmail.com>
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

struct trace_collection;

void init_trace_collection(struct trace_collection *tc);
void finalize_trace_collection(struct trace_collection *tc);
int trace_collection_add(struct trace_collection *tc,
			 struct trace_descriptor *td);
int trace_collection_remove(struct trace_collection *tc,
			 struct trace_descriptor *td);

#endif /* _BABELTRACE_TRACE_COLLECTION_H */
