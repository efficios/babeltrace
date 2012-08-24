#ifndef _BABELTRACE_CLOCK_TYPES_H
#define _BABELTRACE_CLOCK_TYPES_H

/*
 * BabelTrace
 *
 * Clock types header
 *
 * Copyright 2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
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

/*
 * The Babeltrace clock representations
 */
enum bt_clock_type {
	BT_CLOCK_CYCLES = 0,
	BT_CLOCK_REAL,
};

#endif /* _BABELTRACE_CLOCK_TYPES_H */
