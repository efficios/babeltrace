#ifndef BABELTRACE2_CTF_WRITER_UTILS_H
#define BABELTRACE2_CTF_WRITER_UTILS_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

/* For bt_ctf_bool */
#include <babeltrace2-ctf-writer/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_ctf_bool bt_ctf_identifier_is_valid(const char *identifier);

static inline
int bt_ctf_validate_identifier(const char *identifier)
{
	return bt_ctf_identifier_is_valid(identifier) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_CTF_WRITER_UTILS_H */
