/*
 * compat/compat_uuid.h
 *
 * Copyright (C) 2013   Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#define BT_LOG_TAG "COMPAT-UUID"
#include "logging.h"

#ifdef __MINGW32__

#include <rpc.h>
#include <stdlib.h>
#include <babeltrace/compat/uuid-internal.h>

/* MinGW does not provide byteswap - implement our own version. */
static
void swap(unsigned char *ptr, unsigned int i, unsigned int j)
{
	unsigned char tmp;

	tmp = ptr[i];
	ptr[i] = ptr[j];
	ptr[j] = tmp;
}

static
void fix_uuid_endian(unsigned char * ptr)
{
	swap(ptr, 0, 3);
	swap(ptr, 1, 2);
	swap(ptr, 4, 5);
	swap(ptr, 6, 7);
}

int bt_uuid_generate(unsigned char *uuid_out)
{
	RPC_STATUS status;

	status = UuidCreate((UUID *) uuid_out);
	if (status == RPC_S_OK)
		return 0;
	else
		return -1;
}

int bt_uuid_unparse(const unsigned char *uuid_in, char *str_out)
{
	RPC_STATUS status;
	unsigned char *alloc_str;
	int ret;
	unsigned char copy_of_uuid_in[BABELTRACE_UUID_LEN];

	/* make a modifyable copy of uuid_in */
	memcpy(copy_of_uuid_in, uuid_in, BABELTRACE_UUID_LEN);

	fix_uuid_endian(copy_of_uuid_in);
	status = UuidToString((UUID *) copy_of_uuid_in, &alloc_str);

	if (status == RPC_S_OK) {
		strncpy(str_out, (char *) alloc_str, BABELTRACE_UUID_STR_LEN);
		str_out[BABELTRACE_UUID_STR_LEN - 1] = '\0';
		ret = 0;
	} else {
		ret = -1;
	}
	RpcStringFree(&alloc_str);
	return ret;
}

int bt_uuid_parse(const char *str_in, unsigned char *uuid_out)
{
	RPC_STATUS status;

	status = UuidFromString((unsigned char *) str_in,
			(UUID *) uuid_out);
	fix_uuid_endian(uuid_out);

	if (status == RPC_S_OK)
		return 0;
	else
		return -1;
}

int bt_uuid_compare(const unsigned char *uuid_a,
		const unsigned char *uuid_b)
{
	RPC_STATUS status;

	return !UuidCompare((UUID *) uuid_a, (UUID *) uuid_b, &status) ? 0 : -1;
}

#endif
