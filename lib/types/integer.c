/*
 * Common Trace Format
 *
 * Integers read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <ctf/ctf-types.h>
#include <stdint.h>
#include <glib.h>
#include <endian.h>

uint64_t uint_read(const uint8_t *ptr, size_t len, int byte_order)
{
	int rbo = (byte_order != BYTE_ORDER);	/* reverse byte order */

	switch (len) {
	case 8:
	{
		uint8_t v;

		v = *(const uint8_t *)ptr;
		return v;
	}
	case 16:
	{
		uint16_t v;

		v = *(const uint16_t *)ptr;
		return rbo ? GUINT16_SWAP_LE_BE(v) : v;
	}
	case 32:
	{
		uint32_t v;

		v = *(const uint32_t *)ptr;
		return rbo ? GUINT32_SWAP_LE_BE(v) : v;
	}
	case 64:
	{
		uint64_t v;

		v = *(const uint64_t *)ptr;
		return rbo ? GUINT64_SWAP_LE_BE(v) : v;
	}
	default:
		assert(0);
	}
}

int64_t int_read(const uint8_t *ptr, size_t len, int byte_order)
{
	int rbo = (byte_order != BYTE_ORDER);	/* reverse byte order */

	switch (len) {
	case 8:
	{
		int8_t v;

		v = *(const int8_t *)ptr;
		return v;
	}
	case 16:
	{
		int16_t v;

		v = *(const int16_t *)ptr;
		return rbo ? GUINT16_SWAP_LE_BE(v) : v;
	}
	case 32:
	{
		int32_t v;

		v = *(const int32_t *)ptr;
		return rbo ? GUINT32_SWAP_LE_BE(v) : v;
	}
	case 64:
	{
		int64_t v;

		v = *(const int64_t *)ptr;
		return rbo ? GUINT64_SWAP_LE_BE(v) : v;
	}
	default:
		assert(0);
	}
}

size_t uint_write(uint8_t *ptr, size_t len, int byte_order, uint64_t v)
{
	int rbo = (byte_order != BYTE_ORDER);	/* reverse byte order */

	if (!ptr)
		goto end;

	switch (len) {
	case 8:	*(uint8_t *)ptr = (uint8_t) v;
		break;
	case 16:
		*(uint16_t *)ptr = rbo ? GUINT16_SWAP_LE_BE((uint16_t) v) :
					 (uint16_t) v;
		break;
	case 32:
		*(uint32_t *)ptr = rbo ? GUINT32_SWAP_LE_BE((uint32_t) v) :
					 (uint32_t) v;
		break;
	case 64:
		*(uint64_t *)ptr = rbo ? GUINT64_SWAP_LE_BE(v) : v;
		break;
	default:
		assert(0);
	}
end:
	return len;
}

size_t int_write(uint8_t *ptr, size_t len, int byte_order, int64_t v)
{
	int rbo = (byte_order != BYTE_ORDER);	/* reverse byte order */

	if (!ptr)
		goto end;

	switch (len) {
	case 8:	*(int8_t *)ptr = (int8_t) v;
		break;
	case 16:
		*(int16_t *)ptr = rbo ? GUINT16_SWAP_LE_BE((int16_t) v) :
					 (int16_t) v;
		break;
	case 32:
		*(int32_t *)ptr = rbo ? GUINT32_SWAP_LE_BE((int32_t) v) :
					 (int32_t) v;
		break;
	case 64:
		*(int64_t *)ptr = rbo ? GUINT64_SWAP_LE_BE(v) : v;
		break;
	default:
		assert(0);
	}
end:
	return len;
}
