#ifndef _BABELTRACE_CTF_EVENTS_INTERNAL_H
#define _BABELTRACE_CTF_EVENTS_INTERNAL_H

/*
 * BabelTrace
 *
 * CTF events API (internal)
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 */

#include <babeltrace/iterator-internal.h>

struct bt_ctf_iter {
	struct bt_iter parent;
	struct bt_ctf_event current_ctf_event;	/* last read event */
};

#endif /*_BABELTRACE_CTF_EVENTS_INTERNAL_H */
