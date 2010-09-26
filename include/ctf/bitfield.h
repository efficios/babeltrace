#ifndef _CTF_BITFIELD_H
#define _CTF_BITFIELD_H

/*
 * Common Trace Format
 *
 * Bitfields read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <stdint.h>	/* C99 5.2.4.2 Numerical limits */
#include <limits.h>	/* C99 5.2.4.2 Numerical limits */
#include <endian.h>	/* Non-standard BIG_ENDIAN, LITTLE_ENDIAN, BYTE_ORDER */
#include <assert.h>

/* We can't shift a int from 32 bit, >> 32 on int is undefined */
#define _ctf_piecewise_rshift(v, shift)					\
({									\
	unsigned long sb = (shift) / (sizeof(v) * CHAR_BIT - 1);	\
	unsigned long final = (shift) % (sizeof(v) * CHAR_BIT - 1);	\
	typeof(v) _v = (v);						\
									\
	for (; sb; sb--)						\
		_v >>= sizeof(v) * CHAR_BIT - 1;			\
	_v >>= final;							\
})

/*
 * ctf_bitfield_write - write integer to a bitfield in native endianness
 *
 * Save integer to the bitfield, which starts at the "start" bit, has "len"
 * bits.
 * The inside of a bitfield is from high bits to low bits.
 * Uses native endianness.
 * For unsigned "v", pad MSB with 0 if bitfield is larger than v.
 * For signed "v", sign-extend v if bitfield is larger than v.
 *
 * On little endian, bytes are placed from the less significant to the most
 * significant. Also, consecutive bitfields are placed from lower bits to higher
 * bits.
 *
 * On big endian, bytes are places from most significant to less significant.
 * Also, consecutive bitfields are placed from higher to lower bits.
 */

#define _ctf_bitfield_write_le(ptr, _start, _length, _v)		\
do {									\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long start = (_start), length = (_length);		\
	typeof(_v) v = (_v);						\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
									\
	if (!length)							\
		break;							\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
									\
	/* Trim v high bits */						\
	if (length < sizeof(v) * CHAR_BIT)				\
		v &= ~((~(typeof(v)) 0) << length);			\
									\
	/* We can now append v with a simple "or", shift it piece-wise */ \
	this_unit = start_unit;						\
	if (start_unit == end_unit - 1) {				\
		mask = ~((~(typeof(*(ptr))) 0) << (start % ts));	\
		if (end % ts)						\
			mask |= (~(typeof(*(ptr))) 0) << (end % ts);	\
		cmask = (typeof(*(ptr))) v << (start % ts);		\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		break;							\
	}								\
	if (start % ts) {						\
		cshift = start % ts;					\
		mask = ~((~(typeof(*(ptr))) 0) << cshift);		\
		cmask = (typeof(*(ptr))) v << cshift;			\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		v = _ctf_piecewise_rshift(v, ts - cshift);		\
		start += ts - cshift;					\
		this_unit++;						\
	}								\
	for (; this_unit < end_unit - 1; this_unit++) {			\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
		v = _ctf_piecewise_rshift(v, ts);			\
		start += ts;						\
	}								\
	if (end % ts) {							\
		mask = (~(typeof(*(ptr))) 0) << (end % ts);		\
		cmask = (typeof(*(ptr))) v;				\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
	} else								\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
} while (0)

#define _ctf_bitfield_write_be(ptr, _start, _length, _v)		\
do {									\
	typeof(_v) v = (_v);						\
	unsigned long start = _start, length = _length;			\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
									\
	if (!length)							\
		break;							\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
									\
	/* Trim v high bits */						\
	if (length < sizeof(v) * CHAR_BIT)				\
		v &= ~((~(typeof(v)) 0) << length);			\
									\
	/* We can now append v with a simple "or", shift it piece-wise */ \
	this_unit = end_unit - 1;					\
	if (start_unit == end_unit - 1) {				\
		mask = ~((~(typeof(*(ptr))) 0) << ((ts - (end % ts)) % ts)); \
		if (start % ts)						\
			mask |= (~((typeof(*(ptr))) 0)) << (ts - (start % ts)); \
		cmask = (typeof(*(ptr))) v << ((ts - (end % ts)) % ts);	\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		break;							\
	}								\
	if (end % ts) {							\
		cshift = end % ts;					\
		mask = ~((~(typeof(*(ptr))) 0) << (ts - cshift));	\
		cmask = (typeof(*(ptr))) v << (ts - cshift);		\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
		v = _ctf_piecewise_rshift(v, cshift);			\
		end -= cshift;						\
		this_unit--;						\
	}								\
	for (; (long)this_unit >= (long)start_unit + 1; this_unit--) {	\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
		v = _ctf_piecewise_rshift(v, ts);			\
		end -= ts;						\
	}								\
	if (start % ts) {						\
		mask = (~(typeof(*(ptr))) 0) << (ts - (start % ts));	\
		cmask = (typeof(*(ptr))) v;				\
		(ptr)[this_unit] &= mask;				\
		(ptr)[this_unit] |= cmask;				\
	} else								\
		(ptr)[this_unit] = (typeof(*(ptr))) v;			\
} while (0)

/*
 * ctf_bitfield_write - write integer to a bitfield in native endianness
 * ctf_bitfield_write_le - write integer to a bitfield in little endian
 * ctf_bitfield_write_be - write integer to a bitfield in big endian
 */

#if (BYTE_ORDER == LITTLE_ENDIAN)

#define ctf_bitfield_write(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_le(ptr, _start, _length, _v)

#define ctf_bitfield_write_le(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_le(ptr, _start, _length, _v)
	
#define ctf_bitfield_write_be(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_be((uint8_t *) (ptr), _start, _length, _v)

#elif (BYTE_ORDER == BIG_ENDIAN)

#define ctf_bitfield_write(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_be(ptr, _start, _length, _v)

#define ctf_bitfield_write_le(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_le((uint8_t *) (ptr), _start, _length, _v)
	
#define ctf_bitfield_write_be(ptr, _start, _length, _v)			\
	_ctf_bitfield_write_be(ptr, _start, _length, _v)

#else /* (BYTE_ORDER == PDP_ENDIAN) */

#error "Byte order not supported"

#endif

#if 0
#define _ctf_bitfield_read_le(ptr, _start, _length, _vptr)		\
do {									\
	typeof(_vptr) vptr = (_vptr);					\
	unsigned long start = _start, length = _length;			\
	unsigned long start_unit, end_unit, this_unit;			\
	unsigned long end, cshift; /* cshift is "complement shift" */	\
	typeof(*(ptr)) mask, cmask;					\
	unsigned long ts = sizeof(typeof(*(ptr))) * CHAR_BIT; /* type size */ \
									\
	if (!length)							\
		break;							\
									\
	end = start + length;						\
	start_unit = start / ts;					\
	end_unit = (end + (ts - 1)) / ts;				\
} while (0)

#endif //0

/*
 * Read a bitfield byte-wise. This function is arch-agnostic.
 */

static inline
uint64_t _ctf_bitfield_read_64(const unsigned char *ptr,
			       unsigned long start, unsigned long len,
			       int byte_order, int signedness)
{
	long start_unit, end_unit, this_unit;
	unsigned long end, cshift; /* cshift is "complement shift" */
	unsigned long ts = sizeof(unsigned char) * CHAR_BIT;
	unsigned char mask, cmask;
	uint64_t v = 0;

	if (!len)
		return 0;
	end = start + len;
	start_unit = start / ts;
	end_unit = (end + (ts - 1)) / ts;

	/*
	 * We can now fill v piece-wise, from lower bits to upper bits.
	 * We read the bitfield in the opposite direction it was written.
	 */
	switch (byte_order) {
	case LITTLE_ENDIAN:
		this_unit = end_unit - 1;
		if (signedness) {
			if (ptr[this_unit] & (1U << ((end % ts ? : ts) - 1)))
				v = ~(uint64_t) 0;
		}
		if (start_unit == end_unit - 1) {
			mask = (~(unsigned char) 0) << (end % ts ? : ts);
			mask |= ~((~(unsigned char) 0) << (start % ts));
			cmask = ptr[this_unit];
			cmask &= ~mask;
			cmask >>= (start % ts);
			v <<= end - start;
			v |= cmask;
			break;
		}
		if (end % ts) {
			cshift = (end % ts ? : ts);
			mask = (~(unsigned char) 0) << cshift;
			cmask = ptr[this_unit];
			cmask &= ~mask;
			v <<= cshift;
			v |= (uint64_t) cmask;
			end -= cshift;
			this_unit--;
		}
		for (; this_unit >= start_unit + 1; this_unit--) {
			v <<= ts;
			v |= (uint64_t) ptr[this_unit];
			end -= ts;
		}
		if (start % ts) {
			cmask = ptr[this_unit] >> (start % ts);
			v <<= ts - (start % ts);
			v |= (uint64_t) cmask;
		} else {
			v <<= ts;
			v |= (uint64_t) ptr[this_unit];
		}
		break;
	case BIG_ENDIAN:
		this_unit = start_unit;
		if (signedness) {
			if (ptr[this_unit] & (1U << (ts - (start % ts) - 1)))
				v = ~(uint64_t) 0;
		}
		if (start_unit == end_unit - 1) {
			mask = (~(unsigned char) 0) << (ts - (start % ts));
			mask |= ~((~(unsigned char) 0) << ((ts - (end % ts)) % ts));
			cmask = ptr[this_unit];
			cmask &= ~mask;
			cmask >>= (ts - (end % ts)) % ts;
			v <<= end - start;
			v |= (uint64_t) cmask;
			break;
		}
		if (start % ts) {
			mask = (~(unsigned char) 0) << (ts - (start % ts));
			cmask = ptr[this_unit];
			cmask &= ~mask;
			v <<= ts - (start % ts);
			v |= (uint64_t) cmask;
			start += ts - (start % ts);
			this_unit++;
		}
		for (; this_unit < end_unit - 1; this_unit++) {
			v <<= ts;
			v |= (uint64_t) ptr[this_unit];
			start += ts;
		}
		if (end % ts) {
			cmask = ptr[this_unit];
			cmask >>= (ts - (end % ts)) % ts;
			v <<= (end % ts);
			v |= (uint64_t) cmask;
		} else {
			v <<= ts;
			v |= (uint64_t) ptr[this_unit];
		}
		break;
	default:
		assert(0);	/* TODO: support PDP_ENDIAN */
	}
	return v;
}

static inline
uint64_t ctf_bitfield_unsigned_read(const uint8_t *ptr,
				    unsigned long start, unsigned long len,
				    int byte_order)
{
	return (uint64_t) _ctf_bitfield_read_64(ptr, start, len, byte_order, 0);
}

static inline
int64_t ctf_bitfield_signed_read(const uint8_t *ptr,
				 unsigned long start, unsigned long len,
				 int byte_order)
{
	return (int64_t) _ctf_bitfield_read_64(ptr, start, len, byte_order, 1);
}

#endif /* _CTF_BITFIELD_H */
