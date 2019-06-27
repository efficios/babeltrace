#ifndef _BABELTRACE_COMMON_UUID_H
#define _BABELTRACE_COMMON_UUID_H

/*
 * Copyright (C) 2019 Michael Jeanson <mjeanson@efficios.com>
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

#include <stdint.h>
#include "common/macros.h"

#define BT_UUID_STR_LEN 36 /* Excludes final \0 */
#define BT_UUID_LEN 16
#define BT_UUID_VER 4

#define BT_UUID_FMT \
	"%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 \
	"%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 \
	"-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 \
	"%02" SCNx8

#define BT_UUID_FMT_VALUES(uuid) \
	(uuid)[0], (uuid)[1], (uuid)[2], (uuid)[3], (uuid)[4], (uuid)[5], \
	(uuid)[6], (uuid)[7], (uuid)[8], (uuid)[9], (uuid)[10], (uuid)[11], \
	(uuid)[12], (uuid)[13], (uuid)[14], (uuid)[15]

#define BT_UUID_SCAN_VALUES(uuid) \
	&(uuid)[0], &(uuid)[1], &(uuid)[2], &(uuid)[3], &(uuid)[4], &(uuid)[5], \
	&(uuid)[6], &(uuid)[7], &(uuid)[8], &(uuid)[9], &(uuid)[10], &(uuid)[11], \
	&(uuid)[12], &(uuid)[13], &(uuid)[14], &(uuid)[15]

typedef uint8_t bt_uuid_t[BT_UUID_LEN];

BT_HIDDEN void bt_uuid_generate(bt_uuid_t uuid_out);
BT_HIDDEN void bt_uuid_to_str(const bt_uuid_t uuid_in, char *str_out);
BT_HIDDEN int bt_uuid_from_str(const char *str_in, bt_uuid_t uuid_out);
BT_HIDDEN int bt_uuid_compare(const bt_uuid_t uuid_a, const bt_uuid_t uuid_b);
BT_HIDDEN void bt_uuid_copy(bt_uuid_t uuid_dest, const bt_uuid_t uuid_src);

#endif /* _BABELTRACE_COMMON_UUID_H */
