/*
 * Common Trace Format
 *
 * Floating point read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Reference: ISO C99 standard 5.2.4
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <ctf/ctf-types.h>
#include <glib.h>
#include <endian.h>

/*
 * Aliasing float/double and unsigned long is not strictly permitted by strict
 * aliasing, but in practice type prunning is well supported, and this permits
 * us to use per-word read/writes rather than per-byte.
 */

#if defined(__GNUC__) || defined(__MINGW32__) || defined(_MSC_VER)
#define HAS_TYPE_PRUNING
#endif

union floatIEEE754 {
	float v;
#ifdef HAS_TYPE_PRUNING
	unsigned long bits[(sizeof(float) + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
#else
	unsigned char bits[sizeof(float)];
#endif
};

union doubleIEEE754 {
	double v;
#ifdef HAS_TYPE_PRUNING
	unsigned long bits[(sizeof(double) + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
#else
	unsigned char bits[sizeof(double)];
#endif
};

double float_read(const uint8_t *ptr, size_t len, int byte_order)
{
	int rbo = (byte_order != __FLOAT_WORD_ORDER);	/* reverse byte order */

	switch (len) {
	case 32:
	{
		union floatIEEE754 u;
		uint32_t tmp;

		if (!rbo)
			return (double) *(const float *) ptr;
		/*
		 * Need to reverse byte order. Read the opposite from our
		 * architecture.
		 */
		if (__FLOAT_WORD_ORDER == LITTLE_ENDIAN) {
			/* Read big endian */
			tmp = bitfield_unsigned_read(ptr, 0, 1, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 31, 1, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 1, 8, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 23, 8, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 9, 23, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 23, LITTLE_ENDIAN,
						tmp);
		} else {
			/* Read little endian */
			tmp = bitfield_unsigned_read(ptr, 31, 1, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 1, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 23, 8, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 1, 8, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 0, 23, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 9, 23, BIG_ENDIAN,
						tmp);
		}
		return (double) u.v;
	}
	case 64:
	{
		union doubleIEEE754 u;
		uint64_t tmp;

		if (!rbo)
			return (double) *(const double *) ptr;
		/*
		 * Need to reverse byte order. Read the opposite from our
		 * architecture.
		 */
		if (__FLOAT_WORD_ORDER == LITTLE_ENDIAN) {
			/* Read big endian */
			tmp = bitfield_unsigned_read(ptr, 0, 1, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 63, 1, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 1, 11, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 52, 11, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 12, 52, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 52, LITTLE_ENDIAN,
						tmp);
		} else {
			/* Read little endian */
			tmp = bitfield_unsigned_read(ptr, 63, 1, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 1, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 52, 11, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 1, 11, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 0, 52, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 12, 52, BIG_ENDIAN,
						tmp);
		}
		return u.v;
	}
	default:
		printf("float read unavailable for size %u\n", len);
		assert(0);
	}
}

size_t float_write(uint8_t *ptr, size_t len, int byte_order, double v)
{
	int rbo = (byte_order != __FLOAT_WORD_ORDER);	/* reverse byte order */

	if (!ptr)
		goto end;

	switch (len) {
	case 32:
	{
		union floatIEEE754 u;
		uint32_t tmp;

		if (!rbo) {
			*(float *) ptr = (float) v;
			break;
		}
		u.v = v;
		/*
		 * Need to reverse byte order. Write the opposite from our
		 * architecture.
		 */
		if (__FLOAT_WORD_ORDER == LITTLE_ENDIAN) {
			/* Write big endian */
			tmp = bitfield_unsigned_read(ptr, 31, 1, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 1, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 23, 8, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 1, 8, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 0, 23, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 9, 23, BIG_ENDIAN,
						tmp);
		} else {
			/* Write little endian */
			tmp = bitfield_unsigned_read(ptr, 0, 1, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 31, 1, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 1, 8, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 23, 8, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 9, 23, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 23, LITTLE_ENDIAN,
						tmp);
		}
		break;
	}
	case 64:
	{
		union doubleIEEE754 u;
		uint64_t tmp;

		if (!rbo) {
			*(double *) ptr = v;
			break;
		}
		u.v = v;
		/*
		 * Need to reverse byte order. Write the opposite from our
		 * architecture.
		 */
		if (__FLOAT_WORD_ORDER == LITTLE_ENDIAN) {
			/* Write big endian */
			tmp = bitfield_unsigned_read(ptr, 63, 1, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 1, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 52, 11, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 1, 11, BIG_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 0, 52, LITTLE_ENDIAN);
			bitfield_unsigned_write(&u.bits, 12, 52, BIG_ENDIAN,
						tmp);
		} else {
			/* Write little endian */
			tmp = bitfield_unsigned_read(ptr, 0, 1, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 63, 1, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 1, 11, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 52, 11, LITTLE_ENDIAN,
						tmp);
			tmp = bitfield_unsigned_read(ptr, 12, 52, BIG_ENDIAN);
			bitfield_unsigned_write(&u.bits, 0, 52, LITTLE_ENDIAN,
						tmp);
		}
		break;
	}
	default:
		printf("float write unavailable for size %u\n", len);
		assert(0);
	}
end:
	return len;
}
