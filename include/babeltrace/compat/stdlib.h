#ifndef _BABELTRACE_COMPAT_STDLIB_H
#define _BABELTRACE_COMPAT_STDLIB_H

/*
 * babeltrace/compat/stdlib.h
 *
 * Copyright (C) 2015 Michael Jeanson <mjeanson@efficios.com>
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

#include <stdlib.h>
#include <sys/stat.h>

#ifdef HAVE_MKDTEMP
static inline
char *bt_mkdtemp(char *template)
{
	return mkdtemp(template);
}
#else
static inline
char *bt_mkdtemp(char *template)
{
	char *ret;

	ret = mktemp(template);
	if (!ret) {
		goto end;
	}

	if(mkdir(template, 0700)) {
		ret = NULL;
		goto end;
	}

	ret = template;
end:
	return ret;
}
#endif


#endif /* _BABELTRACE_COMPAT_STDLIB_H */
