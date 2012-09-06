#ifndef _BABELTRACE_CLOCK_INTERNAL_H
#define _BABELTRACE_CLOCK_INTERNAL_H

/*
 * BabelTrace
 *
 * clocks header (internal)
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

static inline
uint64_t clock_cycles_to_ns(struct ctf_clock *clock, uint64_t cycles)
{
	if (clock->freq == 1000000000ULL) {
		/* 1GHZ freq, no need to scale cycles value */
		return cycles;
	} else {
		return (double) cycles * 1000000000.0
				/ (double) clock->freq;
	}
}

#endif /* _BABELTRACE_CLOCK_INTERNAL_H */
