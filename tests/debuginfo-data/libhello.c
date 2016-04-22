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
