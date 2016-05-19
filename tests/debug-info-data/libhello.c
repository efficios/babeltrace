/*
 * libhello.c
 *
 * Debug Info - Tests
 *
 * Copyright 2016 Antoine Busque <antoine.busque@efficios.com>
 *
 * Author: Antoine Busque <antoine.busque@efficios.com>
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

#include <stdio.h>
#define TRACEPOINT_DEFINE
#include "tp.h"

void foo()
{
        tracepoint(my_provider, my_first_tracepoint, 42, "hello, tracer");
        printf("foo\n");
}

void bar()
{
        tracepoint(my_provider, my_first_tracepoint, 57,
                   "recoltes et semailles");
        printf("bar\n");
}

void baz()
{
        tracepoint(my_provider, my_other_tracepoint, 1729);
        printf("baz\n");
}
