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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "common/assert.h"
#include "common/uuid.h"


/*
 * Generate a random UUID according to RFC4122, section 4.4.
 */
BT_HIDDEN
void bt_uuid_generate(bt_uuid_t uuid_out)
{
	int i;
	GRand *rand;

	BT_ASSERT(uuid_out);

	rand = g_rand_new();

	/*
	 * Generate 16 bytes of random bits.
	 */
	for (i = 0; i < BT_UUID_LEN; i++) {
		uuid_out[i] = (uint8_t) g_rand_int(rand);
	}

	/*
	 * Set the two most significant bits (bits 6 and 7) of the
	 * clock_seq_hi_and_reserved to zero and one, respectively.
	 */
	uuid_out[8] &= ~(1 << 6);
	uuid_out[8] |= (1 << 7);

	/*
	 * Set the four most significant bits (bits 12 through 15) of the
	 * time_hi_and_version field to the 4-bit version number from
	 * Section 4.1.3.
	 */
	uuid_out[6] &= 0x0f;
	uuid_out[6] |= (BT_UUID_VER << 4);

	g_rand_free(rand);
}

BT_HIDDEN
void bt_uuid_to_str(const bt_uuid_t uuid_in, char *str_out)
{
	BT_ASSERT_DBG(uuid_in);
	BT_ASSERT_DBG(str_out);

	sprintf(str_out, BT_UUID_FMT, BT_UUID_FMT_VALUES(uuid_in));
}

BT_HIDDEN
int bt_uuid_from_str(const char *str_in, bt_uuid_t uuid_out)
{
	int ret = 0;
	bt_uuid_t uuid_scan;

	BT_ASSERT_DBG(uuid_out);
	BT_ASSERT_DBG(str_in);

	if (strnlen(str_in, BT_UUID_STR_LEN + 1) != BT_UUID_STR_LEN) {
		ret = -1;
		goto end;
	}

	/* Scan to a temporary location in case of a partial match. */
	if (sscanf(str_in, BT_UUID_FMT, BT_UUID_SCAN_VALUES(uuid_scan)) != BT_UUID_LEN) {
		ret = -1;
	}

	bt_uuid_copy(uuid_out, uuid_scan);
end:
	return ret;
}

BT_HIDDEN
int bt_uuid_compare(const bt_uuid_t uuid_a, const bt_uuid_t uuid_b)
{
	return memcmp(uuid_a, uuid_b, BT_UUID_LEN);
}

BT_HIDDEN
void bt_uuid_copy(bt_uuid_t uuid_dest, const bt_uuid_t uuid_src)
{
	BT_ASSERT(uuid_dest);
	BT_ASSERT(uuid_src);
	BT_ASSERT(uuid_dest != uuid_src);

	memcpy(uuid_dest, uuid_src, BT_UUID_LEN);
}
