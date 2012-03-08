#ifndef _BABELTRACE_ENDIAN_H
#define _BABELTRACE_ENDIAN_H

/*
 * babeltrace/endian.h
 *
 * Copyright 2012 (c) - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * endian.h compatibility layer.
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

#ifdef __FreeBSD__
#include <machine/endian.h>
#else
#include <endian.h>
#endif

#ifndef FLOAT_WORD_ORDER
#ifdef __FLOAT_WORD_ORDER
#define FLOAT_WORD_ORDER	__FLOAT_WORD_ORDER
#else /* __FLOAT_WORD_ORDER */
#define FLOAT_WORD_ORDER	BYTE_ORDER
#endif /* __FLOAT_WORD_ORDER */
#endif /* FLOAT_WORD_ORDER */

#endif /* _BABELTRACE_ENDIAN_H */
